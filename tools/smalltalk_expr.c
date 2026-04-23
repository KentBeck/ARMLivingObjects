#include "bootstrap_compiler.h"
#include "primitives.h"
#include "smalltalk_world.h"
#include "test_defs.h"

#include <ctype.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CLI_HEAP_SIZE (32 * 1024 * 1024)
#define MAX_EXPR_LINE 4096
#define FAILURE_LOG_PATH "smalltalk_expr_failures.log"

static uint8_t heap[CLI_HEAP_SIZE] __attribute__((aligned(8)));
static sigjmp_buf trap_env;
static volatile sig_atomic_t trap_active = 0;
static volatile sig_atomic_t trap_signal = 0;

static void cli_trap_handler(int signum)
{
    if (trap_active)
    {
        trap_signal = signum;
        siglongjmp(trap_env, 1);
    }
    signal(signum, SIG_DFL);
    raise(signum);
}

static void install_trap_handlers(struct sigaction *old_trap, struct sigaction *old_segv)
{
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = cli_trap_handler;
    sigemptyset(&action.sa_mask);
    sigaction(SIGTRAP, &action, old_trap);
    sigaction(SIGSEGV, &action, old_segv);
}

static void restore_trap_handlers(const struct sigaction *old_trap, const struct sigaction *old_segv)
{
    sigaction(SIGTRAP, old_trap, NULL);
    sigaction(SIGSEGV, old_segv, NULL);
}

void debug_mnu(uint64_t selector)
{
    fprintf(stderr, "Message not understood: 0x%llx\n", (unsigned long long)selector);
}

void debug_mnu_context(uint64_t selector, uint64_t *current_cm, uint64_t selector_index)
{
    (void)current_cm;
    fprintf(stderr, "Message not understood: selector=0x%llx literal=%llu\n",
            (unsigned long long)selector, (unsigned long long)selector_index);
}

void debug_oom(void)
{
    fprintf(stderr, "Out of object memory\n");
}

void debug_unknown_prim(uint64_t prim_index)
{
    fprintf(stderr, "Unknown primitive: %llu\n", (unsigned long long)prim_index);
}

void debug_error(uint64_t message, uint64_t *fp, uint64_t *class_table)
{
    (void)fp;
    (void)class_table;
    fprintf(stderr, "VM error: 0x%llx\n", (unsigned long long)message);
}

static void init_context(TestContext *ctx, SmalltalkWorld *world)
{
    memset(ctx, 0, sizeof(*ctx));
    ctx->om = world->om;
    ctx->class_class = world->class_class;
    ctx->smallint_class = world->smallint_class;
    ctx->block_class = world->block_class;
    ctx->undefined_object_class = world->undefined_class;
    ctx->character_class = world->character_class;
    ctx->string_class = world->string_class;
    ctx->symbol_class = world->symbol_class;
    ctx->context_class = world->context_class;
    ctx->symbol_table = world->symbol_table;
    ctx->class_table = world->class_table;
}

static int install_runtime_sources(SmalltalkWorld *world)
{
    struct RuntimeSource
    {
        const char *path;
        int has_class_declaration;
    };
    const struct RuntimeSource files[] = {
        {"src/smalltalk/Object.st", 1},
        {"src/smalltalk/SmallInteger.st", 1},
        {"src/smalltalk/True.st", 1},
        {"src/smalltalk/False.st", 1},
        {"src/smalltalk/UndefinedObject.st", 1},
        {"src/smalltalk/Character.st", 0},
        {"src/smalltalk/BlockClosure.st", 0},
        {"src/smalltalk/Array.st", 1},
        {"src/smalltalk/String.st", 1},
        {"src/smalltalk/Symbol.st", 0},
        {"src/smalltalk/Association.st", 0},
        {"src/smalltalk/Dictionary.st", 0},
        {"src/smalltalk/Context.st", 0},
    };
    size_t count = sizeof(files) / sizeof(files[0]);
    for (size_t index = 0; index < count; index++)
    {
        int installed = files[index].has_class_declaration
                            ? smalltalk_world_install_existing_class_file(world, files[index].path) != NULL
                            : smalltalk_world_install_st_file(world, files[index].path);
        if (!installed)
        {
            fprintf(stderr, "failed to install %s\n", files[index].path);
            return 0;
        }
    }
    return 1;
}

static int install_smalltalk_compiler_sources(SmalltalkWorld *world)
{
    const char *class_files[] = {
        "src/smalltalk/Token.st",
        "src/smalltalk/ReadStream.st",
        "src/smalltalk/Tokenizer.st",
    };
    size_t class_count = sizeof(class_files) / sizeof(class_files[0]);

    for (size_t index = 0; index < class_count; index++)
    {
        if (smalltalk_world_install_class_file(world, class_files[index]) == NULL)
        {
            fprintf(stderr, "failed to install %s\n", class_files[index]);
            return 0;
        }
    }
    if (!bc_compile_and_install_classes_file(world->om, world->class_class,
                                             world->string_class, world->array_class,
                                             world->association_class, NULL, 0,
                                             "src/smalltalk/ASTNodes.st"))
    {
        fprintf(stderr, "failed to install src/smalltalk/ASTNodes.st\n");
        return 0;
    }
    if (smalltalk_world_install_class_file(world, "src/smalltalk/Parser.st") == NULL)
    {
        fprintf(stderr, "failed to install src/smalltalk/Parser.st\n");
        return 0;
    }
    if (smalltalk_world_install_class_file(world, "src/smalltalk/CodeGenerator.st") == NULL)
    {
        fprintf(stderr, "failed to install src/smalltalk/CodeGenerator.st\n");
        return 0;
    }
    if (smalltalk_world_install_class_file(world, "src/smalltalk/Compiler.st") == NULL)
    {
        fprintf(stderr, "failed to install src/smalltalk/Compiler.st\n");
        return 0;
    }
    return 1;
}

static Oop run_method(SmalltalkWorld *world, TestContext *ctx, ObjPtr method, Oop receiver)
{
    ObjPtr bytecodes = (ObjPtr)OBJ_FIELD(method, CM_BYTECODES);
    uint64_t num_args = (uint64_t)untag_smallint(OBJ_FIELD(method, CM_NUM_ARGS));
    uint64_t num_temps = (uint64_t)untag_smallint(OBJ_FIELD(method, CM_NUM_TEMPS));
    uint64_t *sp = (uint64_t *)((uint8_t *)ctx->stack + STACK_WORDS * sizeof(uint64_t));
    uint64_t *fp = (uint64_t *)0xCAFE;

    stack_push(&sp, ctx->stack, receiver);
    activate_method(&sp, &fp, 0, (Oop)method, num_args, num_temps);
    return interpret(&sp, &fp, (uint8_t *)&OBJ_FIELD(bytecodes, 0),
                     world->class_table, world->om, NULL);
}

static Oop sw_send0_capture_receiver(SmalltalkWorld *world, TestContext *ctx, Oop receiver,
                                     ObjPtr receiver_class, const char *selector,
                                     Oop *updated_receiver)
{
    ObjPtr dispatch_class = is_object_ptr(receiver) ? (ObjPtr)OBJ_CLASS((ObjPtr)receiver) : receiver_class;
    Oop sel_oop = intern_cstring_symbol(world->om, selector);
    Oop method_oop = class_lookup(dispatch_class, sel_oop);
    ObjPtr method = (ObjPtr)method_oop;
    ObjPtr bytecodes = (ObjPtr)OBJ_FIELD(method, CM_BYTECODES);
    uint64_t num_temps = (uint64_t)untag_smallint(OBJ_FIELD(method, CM_NUM_TEMPS));
    uint64_t *sp = (uint64_t *)((uint8_t *)ctx->stack + STACK_WORDS * sizeof(uint64_t));
    uint64_t *fp = (uint64_t *)0xCAFE;

    stack_push(&sp, ctx->stack, receiver);
    activate_method(&sp, &fp, 0, (Oop)method, 0, num_temps);
    Oop *receiver_slot = (Oop *)(fp + FRAME_RECEIVER);
    Oop result = interpret(&sp, &fp, (uint8_t *)&OBJ_FIELD(bytecodes, 0),
                           world->class_table, world->om, NULL);
    *updated_receiver = *receiver_slot;
    return result;
}

static Oop sw_send1_capture_receiver(SmalltalkWorld *world, TestContext *ctx, Oop receiver,
                                     ObjPtr receiver_class, const char *selector, Oop arg,
                                     Oop *updated_receiver)
{
    ObjPtr dispatch_class = is_object_ptr(receiver) ? (ObjPtr)OBJ_CLASS((ObjPtr)receiver) : receiver_class;
    Oop sel_oop = intern_cstring_symbol(world->om, selector);
    Oop method_oop = class_lookup(dispatch_class, sel_oop);
    ObjPtr method = (ObjPtr)method_oop;
    ObjPtr bytecodes = (ObjPtr)OBJ_FIELD(method, CM_BYTECODES);
    uint64_t num_temps = (uint64_t)untag_smallint(OBJ_FIELD(method, CM_NUM_TEMPS));
    uint64_t *sp = (uint64_t *)((uint8_t *)ctx->stack + STACK_WORDS * sizeof(uint64_t));
    uint64_t *fp = (uint64_t *)0xCAFE;

    stack_push(&sp, ctx->stack, receiver);
    stack_push(&sp, ctx->stack, arg);
    activate_method(&sp, &fp, 0, (Oop)method, 1, num_temps);
    Oop *receiver_slot = (Oop *)(fp + FRAME_RECEIVER);
    Oop result = interpret(&sp, &fp, (uint8_t *)&OBJ_FIELD(bytecodes, 0),
                           world->class_table, world->om, NULL);
    *updated_receiver = *receiver_slot;
    return result;
}

static ObjPtr lookup_class_or_null(SmalltalkWorld *world, const char *name)
{
    return smalltalk_world_lookup_class(world, name);
}

static int is_instance_of(Oop value, ObjPtr klass)
{
    return is_object_ptr(value) && OBJ_CLASS((ObjPtr)value) == (Oop)klass;
}

static ObjPtr materialize_smalltalk_codegen_method(SmalltalkWorld *world, ObjPtr generator,
                                                   ObjPtr code_generator_class)
{
    int64_t bytecode_count = untag_smallint(OBJ_FIELD(generator, 1));
    int64_t literal_count = untag_smallint(OBJ_FIELD(generator, 3));
    int64_t temp_count = untag_smallint(OBJ_FIELD(generator, 5));
    int64_t arg_count = untag_smallint(OBJ_FIELD(generator, 7));
    if (bytecode_count < 0 || literal_count < 0 || temp_count < 0 || arg_count < 0)
    {
        return NULL;
    }

    ObjPtr source_bytecodes = (ObjPtr)OBJ_FIELD(generator, 0);
    ObjPtr source_literals = (ObjPtr)OBJ_FIELD(generator, 2);
    ObjPtr bytecodes = om_alloc(world->om, (Oop)world->class_class, FORMAT_BYTES,
                                (uint64_t)(bytecode_count > 0 ? bytecode_count : 1));
    if (bytecodes == NULL)
    {
        return NULL;
    }
    if (bytecode_count > 0)
    {
        memcpy(&OBJ_FIELD(bytecodes, 0), &OBJ_FIELD(source_bytecodes, 0), (size_t)bytecode_count);
    }
    else
    {
        ((uint8_t *)&OBJ_FIELD(bytecodes, 0))[0] = 0;
    }

    ObjPtr literals = NULL;
    if (literal_count > 0)
    {
        literals = om_alloc(world->om, (Oop)world->class_class, FORMAT_INDEXABLE, (uint64_t)literal_count);
        if (literals == NULL)
        {
            return NULL;
        }
        for (int64_t index = 0; index < literal_count; index++)
        {
            Oop literal = OBJ_FIELD(source_literals, index);
            if (is_instance_of(literal, code_generator_class))
            {
                literal = (Oop)materialize_smalltalk_codegen_method(world, (ObjPtr)literal,
                                                                    code_generator_class);
                if (!is_object_ptr(literal))
                {
                    return NULL;
                }
            }
            OBJ_FIELD(literals, index) = literal;
        }
    }

    ObjPtr method = om_alloc(world->om, (Oop)world->class_class, FORMAT_FIELDS, 5);
    if (method == NULL)
    {
        return NULL;
    }
    OBJ_FIELD(method, CM_PRIMITIVE) = tag_smallint(PRIM_NONE);
    OBJ_FIELD(method, CM_NUM_ARGS) = tag_smallint(arg_count);
    OBJ_FIELD(method, CM_NUM_TEMPS) = tag_smallint(temp_count);
    OBJ_FIELD(method, CM_LITERALS) = literals != NULL ? (Oop)literals : tagged_nil();
    OBJ_FIELD(method, CM_BYTECODES) = (Oop)bytecodes;
    return method;
}

static ObjPtr materialize_c_compiled_method(SmalltalkWorld *world, const BCompiledMethodDef *method_def)
{
    ObjPtr literals = NULL;
    if (method_def->body.literal_count > 0)
    {
        literals = om_alloc(world->om, (Oop)world->class_class, FORMAT_INDEXABLE,
                            (uint64_t)method_def->body.literal_count);
        if (literals == NULL)
        {
            return NULL;
        }
        for (int index = 0; index < method_def->body.literal_count; index++)
        {
            BToken literal = method_def->body.literals[index];
            Oop value = tagged_nil();
            switch (literal.type)
            {
                case BTOK_INTEGER:
                    value = tag_smallint(literal.int_value);
                    break;
                case BTOK_CHARACTER:
                    value = tag_character((uint64_t)literal.int_value);
                    break;
                case BTOK_STRING:
                {
                    ObjPtr string = sw_make_string(world, literal.text);
                    value = (Oop)string;
                    break;
                }
                case BTOK_SYMBOL:
                case BTOK_SELECTOR:
                    value = intern_cstring_symbol(world->om, literal.text);
                    break;
                default:
                    return NULL;
            }
            OBJ_FIELD(literals, index) = value;
        }
    }

    ObjPtr bytecodes = om_alloc(world->om, (Oop)world->class_class, FORMAT_BYTES,
                                (uint64_t)(method_def->body.bytecode_count > 0
                                               ? method_def->body.bytecode_count
                                               : 1));
    if (bytecodes == NULL)
    {
        return NULL;
    }
    if (method_def->body.bytecode_count > 0)
    {
        memcpy(&OBJ_FIELD(bytecodes, 0), method_def->body.bytecodes,
               (size_t)method_def->body.bytecode_count);
    }
    else
    {
        ((uint8_t *)&OBJ_FIELD(bytecodes, 0))[0] = 0;
    }

    ObjPtr method = om_alloc(world->om, (Oop)world->class_class, FORMAT_FIELDS, 5);
    if (method == NULL)
    {
        return NULL;
    }
    OBJ_FIELD(method, CM_PRIMITIVE) = tag_smallint(method_def->primitive_index >= 0
                                                       ? method_def->primitive_index
                                                       : PRIM_NONE);
    OBJ_FIELD(method, CM_NUM_ARGS) = tag_smallint(method_def->header.arg_count);
    OBJ_FIELD(method, CM_NUM_TEMPS) = tag_smallint(method_def->body.temp_count);
    OBJ_FIELD(method, CM_LITERALS) = literals != NULL ? (Oop)literals : tagged_nil();
    OBJ_FIELD(method, CM_BYTECODES) = (Oop)bytecodes;
    return method;
}

static int is_bytes_instance_of(SmalltalkWorld *world, Oop value, ObjPtr klass)
{
    if (!is_object_ptr(value))
    {
        return 0;
    }
    ObjPtr object = (ObjPtr)value;
    return OBJ_CLASS(object) == (Oop)klass && OBJ_FORMAT(object) == FORMAT_BYTES;
}

static void print_bytes_escaped(ObjPtr object, FILE *out)
{
    uint8_t *bytes = (uint8_t *)&OBJ_FIELD(object, 0);
    uint64_t size = OBJ_SIZE(object);
    for (uint64_t index = 0; index < size; index++)
    {
        uint8_t byte = bytes[index];
        if (byte == '\\' || byte == '"')
        {
            fputc('\\', out);
            fputc(byte, out);
        }
        else if (isprint(byte))
        {
            fputc(byte, out);
        }
        else
        {
            fprintf(out, "\\x%02x", byte);
        }
    }
}

static void print_oop(SmalltalkWorld *world, Oop value, FILE *out, int depth)
{
    if (is_smallint(value))
    {
        fprintf(out, "%lld", (long long)untag_smallint(value));
    }
    else if (value == tagged_nil())
    {
        fputs("nil", out);
    }
    else if (value == tagged_true())
    {
        fputs("true", out);
    }
    else if (value == tagged_false())
    {
        fputs("false", out);
    }
    else if (is_character(value))
    {
        uint64_t code = untag_character(value);
        if (isprint((int)code))
        {
            fprintf(out, "$%c", (int)code);
        }
        else
        {
            fprintf(out, "$<%llu>", (unsigned long long)code);
        }
    }
    else if (is_bytes_instance_of(world, value, world->string_class))
    {
        fputc('"', out);
        print_bytes_escaped((ObjPtr)value, out);
        fputc('"', out);
    }
    else if (is_bytes_instance_of(world, value, world->symbol_class))
    {
        fputc('#', out);
        print_bytes_escaped((ObjPtr)value, out);
    }
    else if (is_object_ptr(value) && OBJ_CLASS((ObjPtr)value) == (Oop)world->array_class &&
             OBJ_FORMAT((ObjPtr)value) == FORMAT_INDEXABLE)
    {
        ObjPtr array = (ObjPtr)value;
        uint64_t size = OBJ_SIZE(array);
        fputc('{', out);
        for (uint64_t index = 0; index < size; index++)
        {
            if (index > 0)
            {
                fputs(". ", out);
            }
            if (depth > 3)
            {
                fputs("...", out);
            }
            else
            {
                print_oop(world, OBJ_FIELD(array, index), out, depth + 1);
            }
        }
        fputc('}', out);
    }
    else if (is_object_ptr(value))
    {
        ObjPtr object = (ObjPtr)value;
        fprintf(out, "<object %p class=%p size=%llu>",
                (void *)object, (void *)OBJ_CLASS(object),
                (unsigned long long)OBJ_SIZE(object));
    }
    else
    {
        fprintf(out, "<oop 0x%llx>", (unsigned long long)value);
    }
}

static void dump_bytecodes(ObjPtr method, FILE *out)
{
    ObjPtr bytecodes = (ObjPtr)OBJ_FIELD(method, CM_BYTECODES);
    uint8_t *bytes = (uint8_t *)&OBJ_FIELD(bytecodes, 0);
    uint64_t count = OBJ_SIZE(bytecodes);
    for (uint64_t index = 0; index < count; index++)
    {
        if (index > 0)
        {
            fputc(' ', out);
        }
        fprintf(out, "%u", (unsigned)bytes[index]);
    }
}

static int oop_structurally_equal(SmalltalkWorld *world, Oop left, Oop right, int depth);

static int array_literals_equal(SmalltalkWorld *world, ObjPtr left, ObjPtr right, int depth)
{
    if (left == NULL || right == NULL)
    {
        return left == right;
    }
    if (OBJ_SIZE(left) != OBJ_SIZE(right))
    {
        return 0;
    }
    for (uint64_t index = 0; index < OBJ_SIZE(left); index++)
    {
        if (!oop_structurally_equal(world, OBJ_FIELD(left, index), OBJ_FIELD(right, index), depth + 1))
        {
            return 0;
        }
    }
    return 1;
}

static int method_structurally_equal(SmalltalkWorld *world, ObjPtr left, ObjPtr right, int depth)
{
    if (OBJ_FIELD(left, CM_PRIMITIVE) != OBJ_FIELD(right, CM_PRIMITIVE) ||
        OBJ_FIELD(left, CM_NUM_ARGS) != OBJ_FIELD(right, CM_NUM_ARGS) ||
        OBJ_FIELD(left, CM_NUM_TEMPS) != OBJ_FIELD(right, CM_NUM_TEMPS))
    {
        return 0;
    }

    ObjPtr left_bytecodes = (ObjPtr)OBJ_FIELD(left, CM_BYTECODES);
    ObjPtr right_bytecodes = (ObjPtr)OBJ_FIELD(right, CM_BYTECODES);
    if (OBJ_SIZE(left_bytecodes) != OBJ_SIZE(right_bytecodes) ||
        memcmp(&OBJ_FIELD(left_bytecodes, 0), &OBJ_FIELD(right_bytecodes, 0),
               (size_t)OBJ_SIZE(left_bytecodes)) != 0)
    {
        return 0;
    }

    Oop left_literals_oop = OBJ_FIELD(left, CM_LITERALS);
    Oop right_literals_oop = OBJ_FIELD(right, CM_LITERALS);
    if (left_literals_oop == tagged_nil() || right_literals_oop == tagged_nil())
    {
        return left_literals_oop == right_literals_oop;
    }
    return array_literals_equal(world, (ObjPtr)left_literals_oop, (ObjPtr)right_literals_oop, depth + 1);
}

static int oop_structurally_equal(SmalltalkWorld *world, Oop left, Oop right, int depth)
{
    if (left == right)
    {
        return 1;
    }
    if (depth > 8)
    {
        return 0;
    }
    if (!is_object_ptr(left) || !is_object_ptr(right))
    {
        return 0;
    }

    ObjPtr left_obj = (ObjPtr)left;
    ObjPtr right_obj = (ObjPtr)right;
    if (OBJ_CLASS(left_obj) == (Oop)world->class_class && OBJ_SIZE(left_obj) == 5 &&
        OBJ_CLASS(right_obj) == (Oop)world->class_class && OBJ_SIZE(right_obj) == 5)
    {
        return method_structurally_equal(world, left_obj, right_obj, depth + 1);
    }
    if (OBJ_CLASS(left_obj) != OBJ_CLASS(right_obj) ||
        OBJ_FORMAT(left_obj) != OBJ_FORMAT(right_obj) ||
        OBJ_SIZE(left_obj) != OBJ_SIZE(right_obj))
    {
        return 0;
    }
    if (OBJ_FORMAT(left_obj) == FORMAT_BYTES)
    {
        return memcmp(&OBJ_FIELD(left_obj, 0), &OBJ_FIELD(right_obj, 0),
                      (size_t)OBJ_SIZE(left_obj)) == 0;
    }
    if (OBJ_FORMAT(left_obj) == FORMAT_INDEXABLE)
    {
        return array_literals_equal(world, left_obj, right_obj, depth + 1);
    }
    for (uint64_t index = 0; index < OBJ_SIZE(left_obj); index++)
    {
        if (!oop_structurally_equal(world, OBJ_FIELD(left_obj, index),
                                    OBJ_FIELD(right_obj, index), depth + 1))
        {
            return 0;
        }
    }
    return 1;
}

static FILE *open_failure_log(void)
{
    return fopen(FAILURE_LOG_PATH, "a");
}

static void log_failure_header(FILE *log, const char *expression, const char *reason)
{
    fprintf(log, "expression: %s\nreason: %s\n", expression, reason);
}

static void log_method_dump(SmalltalkWorld *world, const char *label, ObjPtr method, FILE *log)
{
    fprintf(log, "%s primitive=%lld args=%lld temps=%lld\n", label,
            (long long)untag_smallint(OBJ_FIELD(method, CM_PRIMITIVE)),
            (long long)untag_smallint(OBJ_FIELD(method, CM_NUM_ARGS)),
            (long long)untag_smallint(OBJ_FIELD(method, CM_NUM_TEMPS)));
    fprintf(log, "%s bytecodes: ", label);
    dump_bytecodes(method, log);
    fprintf(log, "\n");

    Oop literals_oop = OBJ_FIELD(method, CM_LITERALS);
    fprintf(log, "%s literals:", label);
    if (literals_oop == tagged_nil())
    {
        fprintf(log, " <nil>\n");
        return;
    }
    ObjPtr literals = (ObjPtr)literals_oop;
    for (uint64_t index = 0; index < OBJ_SIZE(literals); index++)
    {
        fprintf(log, "\n  [%llu] ", (unsigned long long)index);
        print_oop(world, OBJ_FIELD(literals, index), log, 0);
    }
    fprintf(log, "\n");
}

static int log_failure(SmalltalkWorld *world, const char *expression, const char *reason,
                       ObjPtr c_method, ObjPtr st_method)
{
    FILE *log = open_failure_log();
    if (log == NULL)
    {
        return 0;
    }
    log_failure_header(log, expression, reason);
    if (c_method != NULL)
    {
        log_method_dump(world, "c", c_method, log);
    }
    if (st_method != NULL)
    {
        log_method_dump(world, "smalltalk", st_method, log);
    }
    fprintf(log, "---\n");
    fclose(log);
    return 1;
}

static char *build_method_source(const char *selector, const char *expression)
{
    const char *prefix = "!ExprCLI methodsFor: 'cli expressions'!\n";
    const char *middle = "\n    ^ ";
    const char *suffix = "\n!\n";
    size_t len = strlen(prefix) + strlen(selector) + strlen(middle) +
                 strlen(expression) + strlen(suffix) + 1;
    char *source = malloc(len);
    if (source == NULL)
    {
        return NULL;
    }
    snprintf(source, len, "%s%s%s%s%s", prefix, selector, middle, expression, suffix);
    return source;
}

static char *build_raw_method_source(const char *selector, const char *expression)
{
    const char *middle = "\n    ^ ";
    size_t len = strlen(selector) + strlen(middle) + strlen(expression) + 1;
    char *source = malloc(len);
    if (source == NULL)
    {
        return NULL;
    }
    snprintf(source, len, "%s%s%s", selector, middle, expression);
    return source;
}

static ObjPtr compile_c_expression_method(SmalltalkWorld *world, ObjPtr receiver_class,
                                          const char *expression, int index)
{
    char selector[32];
    snprintf(selector, sizeof(selector), "expr%d", index);

    char *source = build_method_source(selector, expression);
    if (source == NULL)
    {
        fprintf(stderr, "out of memory while building method source\n");
        return NULL;
    }

    BCompiledMethodDef methods[1];
    int method_count = 0;
    int compiled = bc_compile_source_methods(source, methods, 1, &method_count);
    free(source);
    if (!compiled || method_count != 1)
    {
        fprintf(stderr, "compile failed: %s\n", expression);
        return NULL;
    }
    (void)receiver_class;
    return materialize_c_compiled_method(world, &methods[0]);
}

static ObjPtr compile_smalltalk_expression_method(SmalltalkWorld *world, TestContext *ctx,
                                                  const char *expression, int index)
{
    ObjPtr parser_class = lookup_class_or_null(world, "Parser");
    ObjPtr code_generator_class = lookup_class_or_null(world, "CodeGenerator");
    if (parser_class == NULL || code_generator_class == NULL)
    {
        return NULL;
    }

    (void)intern_cstring_symbol(world->om, "on:");
    (void)intern_cstring_symbol(world->om, "parseMethod");
    (void)intern_cstring_symbol(world->om, "new");
    (void)intern_cstring_symbol(world->om, "arguments");
    (void)intern_cstring_symbol(world->om, "body");
    (void)intern_cstring_symbol(world->om, "arguments:");
    (void)intern_cstring_symbol(world->om, "visitSequence:");
    (void)intern_cstring_symbol(world->om, "emitReturn");

    char selector[32];
    snprintf(selector, sizeof(selector), "expr%d", index);
    char *source = build_raw_method_source(selector, expression);
    if (source == NULL)
    {
        return NULL;
    }
    Oop source_string = (Oop)sw_make_string(world, source);
    free(source);
    Oop parser = sw_send1(world, ctx, (Oop)parser_class, world->class_class,
                          "on:", source_string);
    if (!is_object_ptr(parser))
    {
        return NULL;
    }
    Oop method_ast = sw_send0(world, ctx, parser, NULL, "parseMethod");
    if (!is_object_ptr(method_ast))
    {
        return NULL;
    }
    Oop arguments = sw_send0(world, ctx, method_ast, NULL, "arguments");
    Oop body = sw_send0(world, ctx, method_ast, NULL, "body");
    if (!is_object_ptr(arguments) || !is_object_ptr(body))
    {
        return NULL;
    }
    Oop generator = sw_send0(world, ctx, (Oop)code_generator_class, world->class_class, "new");
    if (!is_object_ptr(generator))
    {
        return NULL;
    }
    Oop moved_generator = generator;
    (void)sw_send1_capture_receiver(world, ctx, generator, NULL, "arguments:",
                                    arguments, &moved_generator);
    generator = moved_generator;
    if (!is_object_ptr(generator))
    {
        return NULL;
    }
    (void)sw_send1_capture_receiver(world, ctx, generator, NULL, "visitSequence:",
                                    body, &moved_generator);
    generator = moved_generator;
    if (!is_object_ptr(generator))
    {
        return NULL;
    }
    (void)sw_send0_capture_receiver(world, ctx, generator, NULL, "emitReturn", &moved_generator);
    generator = moved_generator;
    if (!is_object_ptr(generator))
    {
        return NULL;
    }
    return materialize_smalltalk_codegen_method(world, (ObjPtr)generator, code_generator_class);
}

static int evaluate_expression(SmalltalkWorld *world, TestContext *ctx,
                               ObjPtr receiver_class, Oop receiver,
                               const char *expression, int index)
{
    ObjPtr c_method = compile_c_expression_method(world, receiver_class, expression, index);
    if (c_method == NULL)
    {
        (void)log_failure(world, expression, "c compile failed", NULL, NULL);
        return 0;
    }

    struct sigaction old_trap;
    struct sigaction old_segv;
    ObjPtr smalltalk_method = NULL;
    install_trap_handlers(&old_trap, &old_segv);
    trap_active = 1;
    trap_signal = 0;
    if (sigsetjmp(trap_env, 1) == 0)
    {
        smalltalk_method = compile_smalltalk_expression_method(world, ctx, expression, index);
        trap_active = 0;
        restore_trap_handlers(&old_trap, &old_segv);
    }
    else
    {
        trap_active = 0;
        restore_trap_handlers(&old_trap, &old_segv);
        fprintf(stderr, "smalltalk compile trapped with signal %d: %s\n",
                (int)trap_signal, expression);
        (void)log_failure(world, expression, "smalltalk compile trapped", c_method, NULL);
        return 0;
    }
    if (smalltalk_method == NULL)
    {
        fprintf(stderr, "smalltalk compile failed: %s\n", expression);
        (void)log_failure(world, expression, "smalltalk compile failed", c_method, NULL);
        return 0;
    }

    if (!method_structurally_equal(world, c_method, smalltalk_method, 0))
    {
        fprintf(stderr, "compiler mismatch: %s\n", expression);
        (void)log_failure(world, expression, "compiler mismatch", c_method, smalltalk_method);
        return 0;
    }

    Oop result = tagged_nil();
    install_trap_handlers(&old_trap, &old_segv);
    trap_active = 1;
    trap_signal = 0;
    if (sigsetjmp(trap_env, 1) == 0)
    {
        result = run_method(world, ctx, smalltalk_method, receiver);
        trap_active = 0;
        restore_trap_handlers(&old_trap, &old_segv);
    }
    else
    {
        trap_active = 0;
        restore_trap_handlers(&old_trap, &old_segv);
        fprintf(stderr, "smalltalk execution trapped with signal %d: %s\n",
                (int)trap_signal, expression);
        (void)log_failure(world, expression, "smalltalk execution trapped", c_method, smalltalk_method);
        return 0;
    }
    print_oop(world, result, stdout, 0);
    fputc('\n', stdout);
    return 1;
}

static char *trim_line(char *line)
{
    while (*line != '\0' && isspace((unsigned char)*line))
    {
        line++;
    }
    char *end = line + strlen(line);
    while (end > line && isspace((unsigned char)end[-1]))
    {
        *--end = '\0';
    }
    return line;
}

static int run_cli(int argc, char **argv)
{
    SmalltalkWorld world;
    TestContext ctx;
    smalltalk_world_init(&world, heap, sizeof(heap));
    init_context(&ctx, &world);

    int ok = install_runtime_sources(&world);
    if (ok)
    {
        ok = install_smalltalk_compiler_sources(&world);
    }
    ObjPtr expr_class = NULL;
    Oop receiver = tagged_nil();
    if (ok)
    {
        expr_class = smalltalk_world_define_class(&world, "ExprCLI",
                                                  world.object_class, NULL, 0,
                                                  FORMAT_FIELDS);
        receiver = (Oop)om_alloc(world.om, (Oop)expr_class, FORMAT_FIELDS, 0);
        ok = expr_class != NULL && is_object_ptr(receiver);
    }

    int expression_index = 1;
    if (ok && argc > 1)
    {
        for (int arg = 1; arg < argc; arg++)
        {
            if (!evaluate_expression(&world, &ctx, expr_class, receiver,
                                     argv[arg], expression_index++))
            {
                ok = 0;
                break;
            }
        }
    }
    else if (ok)
    {
        char line[MAX_EXPR_LINE];
        while (fgets(line, sizeof(line), stdin) != NULL)
        {
            char *expression = trim_line(line);
            if (*expression == '\0')
            {
                continue;
            }
            if (!evaluate_expression(&world, &ctx, expr_class, receiver,
                                     expression, expression_index++))
            {
                ok = 0;
                break;
            }
        }
    }

    smalltalk_world_teardown(&world);
    return ok ? 0 : 1;
}

int main(int argc, char **argv)
{
    return run_cli(argc, argv);
}
