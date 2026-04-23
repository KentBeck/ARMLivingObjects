#include "bootstrap_compiler.h"
#include "primitives.h"
#include "smalltalk_world.h"
#include "test_defs.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CLI_HEAP_SIZE (32 * 1024 * 1024)
#define MAX_EXPR_LINE 4096

static uint8_t heap[CLI_HEAP_SIZE] __attribute__((aligned(8)));

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

static int evaluate_expression(SmalltalkWorld *world, TestContext *ctx,
                               ObjPtr receiver_class, Oop receiver,
                               const char *expression, int index)
{
    char selector[32];
    snprintf(selector, sizeof(selector), "expr%d", index);

    char *source = build_method_source(selector, expression);
    if (source == NULL)
    {
        fprintf(stderr, "out of memory while building method source\n");
        return 0;
    }

    int installed = bc_compile_and_install_source_methods(world->om, world->class_class,
                                                         NULL, 0, source);
    free(source);
    if (!installed)
    {
        fprintf(stderr, "compile failed: %s\n", expression);
        return 0;
    }

    Oop selector_oop = intern_cstring_symbol(world->om, selector);
    Oop method_oop = class_lookup(receiver_class, selector_oop);
    if (!is_object_ptr(method_oop))
    {
        fprintf(stderr, "compiled method not found: %s\n", selector);
        return 0;
    }

    Oop result = run_method(world, ctx, (ObjPtr)method_oop, receiver);
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
