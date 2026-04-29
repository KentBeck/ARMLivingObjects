// Self-hosting progression: run real Smalltalk source files end-to-end.
//
// Step 1 (this file): verify Token.st runs — specifically that Token class>>eof
// returns a Token instance with type ivar bound to #eof. This proves class-side
// instance creation plus instance-side initialization work.
//
// Future steps: ReadStream, Tokenizer, Parser, CodeGenerator.

#include "test_defs.h"
#include "smalltalk_world.h"
#include "primitives.h"
#include "bootstrap_compiler.h"

#include <unistd.h>

static int byte_object_equals_cstring(uint64_t value, const char *text)
{
    if (!is_object_ptr(value))
    {
        return 0;
    }
    uint64_t *object = (uint64_t *)value;
    size_t len = strlen(text);
    return OBJ_FORMAT(object) == FORMAT_BYTES &&
           OBJ_SIZE(object) == (uint64_t)len &&
           memcmp(&OBJ_FIELD(object, 0), text, len) == 0;
}

static int bytecode_contains(uint64_t *bytecodes, uint64_t count, uint8_t opcode)
{
    uint8_t *bytes = (uint8_t *)&OBJ_FIELD(bytecodes, 0);
    for (uint64_t index = 0; index < count; index++)
    {
        if (bytes[index] == opcode)
        {
            return 1;
        }
    }
    return 0;
}

static void print_smalltalk_suite_progress(const char *class_name, int pass_count, int failure_count)
{
    int index;
    printf("[smalltalk] %s ", class_name);
    for (index = 0; index < pass_count; index++)
    {
        putchar('.');
    }
    for (index = 0; index < failure_count; index++)
    {
        putchar('F');
    }
    printf(" (%d passed, %d failed)\n", pass_count, failure_count);
    fflush(stdout);
}

static void run_smalltalk_self_test(TestContext *ctx, SmalltalkWorld *world,
                                    const char *class_name, int expected_tests)
{
    uint64_t *test_class = smalltalk_world_lookup_class(world, class_name);
    Oop test_result;
    Oop run_count;
    Oop pass_count;
    Oop failure_count;
    Oop last_reason;
    Oop last_backtrace;

    ASSERT_EQ(ctx, test_class != NULL, 1, "runtime: Smalltalk test class available to suite runner");
    test_result = sw_send0(world, ctx, (Oop)test_class, world->class_class, "selfTest");
    ASSERT_EQ(ctx, is_object_ptr(test_result), 1, "runtime: Smalltalk suite returns a TestResult");
    smalltalk_world_put_global(world, "CurrentSmalltalkSuiteResult", test_result);
    test_result = smalltalk_world_lookup_global(world, "CurrentSmalltalkSuiteResult");
    ASSERT_EQ(ctx, is_object_ptr(test_result), 1, "runtime: Smalltalk suite result stays rooted");

    run_count = sw_send0(world, ctx, test_result, NULL, "runCount");
    pass_count = sw_send0(world, ctx, test_result, NULL, "passCount");
    failure_count = sw_send0(world, ctx, test_result, NULL, "failureCount");
    last_reason = sw_send0(world, ctx, test_result, NULL, "lastReason");
    last_backtrace = sw_send0(world, ctx, test_result, NULL, "lastBacktrace");

    print_smalltalk_suite_progress(class_name,
                                   (int)untag_smallint(pass_count),
                                   (int)untag_smallint(failure_count));

    ASSERT_EQ(ctx, run_count, tag_smallint(expected_tests), "runtime: Smalltalk suite runCount matches");
    ASSERT_EQ(ctx, pass_count, tag_smallint(expected_tests), "runtime: Smalltalk suite passCount matches");
    ASSERT_EQ(ctx, failure_count, tag_smallint(0), "runtime: Smalltalk suite has no failures");
    ASSERT_EQ(ctx, last_reason, tagged_nil(), "runtime: passing Smalltalk suite leaves lastReason nil");
    ASSERT_EQ(ctx, last_backtrace, tagged_nil(), "runtime: passing Smalltalk suite leaves lastBacktrace nil");
}

static int copy_symbol_to_cstring(Oop symbol_oop, char *buffer, size_t capacity)
{
    ObjPtr symbol;
    uint64_t size;

    if (!is_object_ptr(symbol_oop) || capacity == 0)
    {
        return 0;
    }

    symbol = (ObjPtr)symbol_oop;
    size = OBJ_SIZE(symbol);
    if (size + 1 > capacity)
    {
        return 0;
    }

    memcpy(buffer, &OBJ_FIELD(symbol, 0), (size_t)size);
    buffer[size] = '\0';
    return 1;
}

static void run_smalltalk_direct_tests(TestContext *ctx, SmalltalkWorld *world,
                                       const char *class_name, int expected_tests)
{
    uint64_t *test_class = smalltalk_world_lookup_class(world, class_name);
    Oop selectors;
    Oop selector_count;
    int pass_count = 0;

    ASSERT_EQ(ctx, test_class != NULL, 1, "runtime: direct Smalltalk test class available");
    selectors = sw_send0(world, ctx, (Oop)test_class, world->class_class, "testSelectors");
    ASSERT_EQ(ctx, is_object_ptr(selectors), 1, "runtime: direct Smalltalk selectors array exists");
    selector_count = sw_send0(world, ctx, selectors, NULL, "size");
    ASSERT_EQ(ctx, selector_count, tag_smallint(expected_tests),
              "runtime: direct Smalltalk test selector count");

    for (int index = 1; index <= expected_tests; index++)
    {
        Oop selector = sw_send1(world, ctx, selectors, NULL, "at:", tag_smallint(index));
        Oop test_case = sw_send0(world, ctx, (Oop)test_class, world->class_class, "new");
        char selector_name[128];
        Oop result;

        ASSERT_EQ(ctx, is_object_ptr(selector), 1, "runtime: direct Smalltalk test selector exists");
        ASSERT_EQ(ctx, is_object_ptr(test_case), 1, "runtime: direct Smalltalk test instance exists");
        ASSERT_EQ(ctx, copy_symbol_to_cstring(selector, selector_name, sizeof(selector_name)), 1,
                  "runtime: direct Smalltalk selector name copied");

        result = sw_send0(world, ctx, test_case, NULL, selector_name);
        ASSERT_EQ(ctx, result, TAGGED_TRUE, "runtime: direct Smalltalk test passes");
        pass_count++;
    }

    print_smalltalk_suite_progress(class_name, pass_count, 0);
}

#ifdef ALO_INTERPRETER_C
static SmalltalkWorld *trap_world = NULL;

static Oop sw_send0_capture_receiver(SmalltalkWorld *world, TestContext *ctx, Oop receiver,
                                     ObjPtr receiver_class, const char *selector,
                                     Oop *updated_receiver)
{
    ObjPtr dispatch_class = is_object_ptr(receiver) ? (ObjPtr)OBJ_CLASS((ObjPtr)receiver) : receiver_class;
    Oop selector_oop = intern_cstring_symbol(world->om, selector);
    Oop method_oop = class_lookup(dispatch_class, selector_oop);
    ObjPtr method = (ObjPtr)method_oop;
    ObjPtr bytecodes = (ObjPtr)OBJ_FIELD(method, CM_BYTECODES);
    uint64_t num_temps = (uint64_t)untag_smallint(OBJ_FIELD(method, CM_NUM_TEMPS));
    Oop *sp = ctx->stack + STACK_WORDS;
    ObjPtr fp = (ObjPtr)0xCAFE;
    stack_push(&sp, ctx->stack, receiver);
    activate_method(&sp, &fp, 0, (Oop)method, 0, num_temps);
    Oop *receiver_slot = (Oop *)(fp + FRAME_RECEIVER);
    Oop result =
        interpret(&sp, &fp, (uint8_t *)&OBJ_FIELD(bytecodes, 0), world->class_table, world->om, NULL);
    *updated_receiver = *receiver_slot;
    return result;
}

static Oop sw_send1_capture_receiver(SmalltalkWorld *world, TestContext *ctx, Oop receiver,
                                     ObjPtr receiver_class, const char *selector, Oop arg,
                                     Oop *updated_receiver)
{
    ObjPtr dispatch_class = is_object_ptr(receiver) ? (ObjPtr)OBJ_CLASS((ObjPtr)receiver) : receiver_class;
    Oop selector_oop = intern_cstring_symbol(world->om, selector);
    Oop method_oop = class_lookup(dispatch_class, selector_oop);
    ObjPtr method = (ObjPtr)method_oop;
    ObjPtr bytecodes = (ObjPtr)OBJ_FIELD(method, CM_BYTECODES);
    uint64_t num_temps = (uint64_t)untag_smallint(OBJ_FIELD(method, CM_NUM_TEMPS));
    Oop *sp = ctx->stack + STACK_WORDS;
    ObjPtr fp = (ObjPtr)0xCAFE;
    stack_push(&sp, ctx->stack, receiver);
    stack_push(&sp, ctx->stack, arg);
    activate_method(&sp, &fp, 0, (Oop)method, 1, num_temps);
    Oop *receiver_slot = (Oop *)(fp + FRAME_RECEIVER);
    Oop result =
        interpret(&sp, &fp, (uint8_t *)&OBJ_FIELD(bytecodes, 0), world->class_table, world->om, NULL);
    *updated_receiver = *receiver_slot;
    return result;
}

static uint64_t *materialize_codegen_method(SmalltalkWorld *world, uint64_t *generator)
{
    int64_t bytecode_count = untag_smallint(OBJ_FIELD(generator, 1));
    int64_t literal_count = untag_smallint(OBJ_FIELD(generator, 3));
    int64_t temp_count = untag_smallint(OBJ_FIELD(generator, 5));
    int64_t arg_count = untag_smallint(OBJ_FIELD(generator, 7));
    if (bytecode_count < 0 || literal_count < 0 || temp_count < 0 || arg_count < 0)
    {
        return NULL;
    }

    uint64_t *source_bytecodes = (uint64_t *)OBJ_FIELD(generator, 0);
    uint64_t *source_literals = (uint64_t *)OBJ_FIELD(generator, 2);
    uint64_t *bytecodes = om_alloc(world->om, (uint64_t)world->class_class,
                                   FORMAT_BYTES, (uint64_t)bytecode_count);
    if (bytecodes == NULL)
    {
        return NULL;
    }
    memcpy(&OBJ_FIELD(bytecodes, 0), &OBJ_FIELD(source_bytecodes, 0), (size_t)bytecode_count);

    uint64_t *literals = NULL;
    if (literal_count > 0)
    {
        literals = om_alloc(world->om, (uint64_t)world->class_class,
                            FORMAT_INDEXABLE, (uint64_t)literal_count);
        if (literals == NULL)
        {
            return NULL;
        }
        for (int64_t index = 0; index < literal_count; index++)
        {
            OBJ_FIELD(literals, index) = OBJ_FIELD(source_literals, index);
        }
    }

    uint64_t *method = om_alloc(world->om, (uint64_t)world->class_class, FORMAT_FIELDS, 6);
    if (method == NULL)
    {
        return NULL;
    }
    OBJ_FIELD(method, CM_PRIMITIVE) = tag_smallint(PRIM_NONE);
    OBJ_FIELD(method, CM_NUM_ARGS) = tag_smallint(arg_count);
    OBJ_FIELD(method, CM_NUM_TEMPS) = tag_smallint(temp_count);
    OBJ_FIELD(method, CM_LITERALS) = literals != NULL ? (uint64_t)literals : tagged_nil();
    OBJ_FIELD(method, CM_BYTECODES) = (uint64_t)bytecodes;
    OBJ_FIELD(method, CM_SOURCE) = tagged_nil();
    return method;
}

static uint64_t run_materialized_method(SmalltalkWorld *world, TestContext *ctx,
                                        uint64_t *method, uint64_t receiver)
{
    ObjPtr bytecodes = (ObjPtr)OBJ_FIELD(method, CM_BYTECODES);
    uint64_t num_args = (uint64_t)untag_smallint(OBJ_FIELD(method, CM_NUM_ARGS));
    uint64_t num_temps = (uint64_t)untag_smallint(OBJ_FIELD(method, CM_NUM_TEMPS));
    Oop *sp = ctx->stack + STACK_WORDS;
    ObjPtr fp = (ObjPtr)0xCAFE;
    stack_push(&sp, ctx->stack, receiver);
    activate_method(&sp, &fp, 0, (Oop)method, num_args, num_temps);
    return interpret(&sp, &fp, (uint8_t *)&OBJ_FIELD(bytecodes, 0), world->class_table, world->om, NULL);
}

static void trap_codegen_add_literal_nil(TestContext *ctx)
{
    ObjPtr code_generator_class = smalltalk_world_lookup_class(trap_world, "CodeGenerator");
    Oop generator = sw_send0(trap_world, ctx, (Oop)code_generator_class,
                             trap_world->class_class, "new");
    (void)sw_send1(trap_world, ctx, generator, NULL, "addLiteral:", tagged_nil());
}

static void trap_codegen_resolve_nil(TestContext *ctx)
{
    ObjPtr code_generator_class = smalltalk_world_lookup_class(trap_world, "CodeGenerator");
    Oop generator = sw_send0(trap_world, ctx, (Oop)code_generator_class,
                             trap_world->class_class, "new");
    Oop nil_name = (Oop)sw_make_string(trap_world, "nil");
    (void)sw_send1(trap_world, ctx, generator, NULL, "resolveVariable:", nil_name);
}

static void trap_compiler_compile_nil(TestContext *ctx)
{
    ObjPtr compiler_class = smalltalk_world_lookup_class(trap_world, "Compiler");
    Oop nil_source = (Oop)sw_make_string(trap_world, "nil");
    (void)sw_send1(trap_world, ctx, (Oop)compiler_class, trap_world->class_class,
                   "compileExpression:", nil_source);
}

static uint64_t *materialize_c_compiled_method(SmalltalkWorld *world, const BCompiledMethodDef *method_def)
{
    uint64_t *literals = NULL;
    if (method_def->body.literal_count > 0)
    {
        literals = om_alloc(world->om, (uint64_t)world->class_class,
                            FORMAT_INDEXABLE, (uint64_t)method_def->body.literal_count);
        if (literals == NULL)
        {
            return NULL;
        }
        for (int index = 0; index < method_def->body.literal_count; index++)
        {
            BToken literal = method_def->body.literals[index];
            uint64_t value = tagged_nil();
            switch (literal.type)
            {
                case BTOK_INTEGER:
                    value = tag_smallint(literal.int_value);
                    break;
                case BTOK_CHARACTER:
                    value = tag_character((uint64_t)literal.int_value);
                    break;
                case BTOK_STRING:
                    value = (uint64_t)sw_make_string(world, literal.text);
                    break;
                case BTOK_SYMBOL:
                case BTOK_SELECTOR:
                    value = intern_cstring_symbol(world->om, literal.text);
                    break;
                case BTOK_IDENTIFIER:
                    if (strcmp(literal.text, "nil") == 0)
                    {
                        value = tagged_nil();
                        break;
                    }
                    if (strcmp(literal.text, "true") == 0)
                    {
                        value = tagged_true();
                        break;
                    }
                    if (strcmp(literal.text, "false") == 0)
                    {
                        value = tagged_false();
                        break;
                    }
                    return NULL;
                default:
                    return NULL;
            }
            OBJ_FIELD(literals, index) = value;
        }
    }

    uint64_t *bytecodes = om_alloc(world->om, (uint64_t)world->class_class,
                                   FORMAT_BYTES,
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

    uint64_t *method = om_alloc(world->om, (uint64_t)world->class_class, FORMAT_FIELDS, 6);
    if (method == NULL)
    {
        return NULL;
    }
    OBJ_FIELD(method, CM_PRIMITIVE) = tag_smallint(method_def->primitive_index >= 0
                                                       ? method_def->primitive_index
                                                       : PRIM_NONE);
    OBJ_FIELD(method, CM_NUM_ARGS) = tag_smallint(method_def->header.arg_count);
    OBJ_FIELD(method, CM_NUM_TEMPS) = tag_smallint(method_def->body.temp_count);
    OBJ_FIELD(method, CM_LITERALS) = literals != NULL ? (uint64_t)literals : tagged_nil();
    OBJ_FIELD(method, CM_BYTECODES) = (uint64_t)bytecodes;
    OBJ_FIELD(method, CM_SOURCE) = method_def->method_source[0] != '\0'
                                       ? (uint64_t)sw_make_string(world, method_def->method_source)
                                       : tagged_nil();
    return method;
}

static int oop_structurally_equal(SmalltalkWorld *world, uint64_t left, uint64_t right, int depth);

static int array_literals_equal(SmalltalkWorld *world, uint64_t *left, uint64_t *right, int depth)
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

static int method_structurally_equal(SmalltalkWorld *world, uint64_t *left, uint64_t *right, int depth)
{
    if (OBJ_FIELD(left, CM_PRIMITIVE) != OBJ_FIELD(right, CM_PRIMITIVE) ||
        OBJ_FIELD(left, CM_NUM_ARGS) != OBJ_FIELD(right, CM_NUM_ARGS) ||
        OBJ_FIELD(left, CM_NUM_TEMPS) != OBJ_FIELD(right, CM_NUM_TEMPS))
    {
        return 0;
    }

    uint64_t *left_bytecodes = (uint64_t *)OBJ_FIELD(left, CM_BYTECODES);
    uint64_t *right_bytecodes = (uint64_t *)OBJ_FIELD(right, CM_BYTECODES);
    if (OBJ_SIZE(left_bytecodes) != OBJ_SIZE(right_bytecodes) ||
        memcmp(&OBJ_FIELD(left_bytecodes, 0), &OBJ_FIELD(right_bytecodes, 0),
               (size_t)OBJ_SIZE(left_bytecodes)) != 0)
    {
        return 0;
    }

    uint64_t left_literals_oop = OBJ_FIELD(left, CM_LITERALS);
    uint64_t right_literals_oop = OBJ_FIELD(right, CM_LITERALS);
    if (left_literals_oop == tagged_nil() || right_literals_oop == tagged_nil())
    {
        return left_literals_oop == right_literals_oop;
    }
    return array_literals_equal(world, (uint64_t *)left_literals_oop, (uint64_t *)right_literals_oop, depth + 1);
}

static int oop_structurally_equal(SmalltalkWorld *world, uint64_t left, uint64_t right, int depth)
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

    uint64_t *left_obj = (uint64_t *)left;
    uint64_t *right_obj = (uint64_t *)right;
    if (OBJ_CLASS(left_obj) == (uint64_t)world->class_class && OBJ_SIZE(left_obj) == 5 &&
        OBJ_CLASS(right_obj) == (uint64_t)world->class_class && OBJ_SIZE(right_obj) == 5)
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

static uint64_t *compile_c_expression_method(SmalltalkWorld *world, const char *selector, const char *expression)
{
    char source[512];
    snprintf(source, sizeof(source),
             "!ExprValidation methodsFor: 'tests'!\n"
             "%s\n"
             "    ^ %s\n"
             "!\n",
             selector, expression);

    BCompiledMethodDef methods[1];
    int method_count = 0;
    if (!bc_compile_source_methods(source, methods, 1, &method_count) || method_count != 1)
    {
        return NULL;
    }
    return materialize_c_compiled_method(world, &methods[0]);
}
#endif

void test_smalltalk_runtime(TestContext *ctx)
{
    static uint8_t world_buf[16 * 1024 * 1024] __attribute__((aligned(8)));
    SmalltalkWorld world;
    smalltalk_world_init(&world, world_buf, sizeof(world_buf));

    uint64_t *point_file_class =
        smalltalk_world_install_class_file(&world, "tests/fixtures/PointFile.st");
    ASSERT_EQ(ctx, point_file_class != NULL, 1,
              "runtime: class file loader creates PointFile");
    ASSERT_EQ(ctx, (uint64_t)smalltalk_world_lookup_class(&world, "PointFile"),
              (uint64_t)point_file_class,
              "runtime: class file loader registers PointFile");
    ASSERT_EQ(ctx, class_lookup(point_file_class, intern_cstring_symbol(world.om, "y")) != 0,
              1, "runtime: class file loader installs PointFile methods");

    ASSERT_EQ(ctx, smalltalk_world_install_class_file(&world, "src/smalltalk/Token.st") != NULL,
              1, "runtime: Token.st defines class and installs methods");

    // --- `Token eof` should return a Token instance with type = #eof. ---
    uint64_t *token_class = smalltalk_world_lookup_class(&world, "Token");
    ASSERT_EQ(ctx, token_class != NULL, 1, "runtime: Token in Smalltalk dict");

    uint64_t eof_tok = sw_send0(&world, ctx, (uint64_t)token_class, world.class_class, "eof");
    ASSERT_EQ(ctx, is_object_ptr(eof_tok), 1, "runtime: Token eof returns an object");
    uint64_t *eof_ptr = (uint64_t *)eof_tok;
    ASSERT_EQ(ctx, OBJ_SIZE(eof_ptr), 3, "runtime: Token eof has 3 ivar slots");

    // type ivar (slot 0) should be the interned Symbol #eof.
    uint64_t eof_type = OBJ_FIELD(eof_ptr, 0);
    uint64_t expected_eof_sym = intern_cstring_symbol(world.om, "eof");
    ASSERT_EQ(ctx, eof_type, expected_eof_sym, "runtime: Token eof type ivar is #eof");

    // --- Step 2: ReadStream.st. Build stream on a String, call next/peek/atEnd. ---
    // ReadStream uses ifTrue:ifFalse: which is defined on True/False in True.st/False.st.
    uint64_t *true_class = smalltalk_world_install_existing_class_file(&world, "src/smalltalk/True.st");
    ASSERT_EQ(ctx, true_class != NULL, 1,
              "runtime: True.st declaration matches existing class and installs methods");
    ASSERT_EQ(ctx, (uint64_t)true_class, (uint64_t)world.true_class,
              "runtime: True.st attaches to the existing True class");
    uint64_t *false_class = smalltalk_world_install_existing_class_file(&world, "src/smalltalk/False.st");
    ASSERT_EQ(ctx, false_class != NULL, 1,
              "runtime: False.st declaration matches existing class and installs methods");
    ASSERT_EQ(ctx, (uint64_t)false_class, (uint64_t)world.false_class,
              "runtime: False.st attaches to the existing False class");
    {
        uint64_t if_tf_sel = intern_cstring_symbol(world.om, "ifTrue:ifFalse:");
        uint64_t true_if_tf = class_lookup(true_class, if_tf_sel);
        uint64_t false_if_tf = class_lookup(false_class, if_tf_sel);
        ASSERT_EQ(ctx, true_if_tf != 0, 1, "runtime: True understands ifTrue:ifFalse:");
        ASSERT_EQ(ctx, false_if_tf != 0, 1, "runtime: False understands ifTrue:ifFalse:");
        uint64_t *true_if_tf_bc = (uint64_t *)OBJ_FIELD((uint64_t *)true_if_tf, CM_BYTECODES);
        uint64_t *false_if_tf_bc = (uint64_t *)OBJ_FIELD((uint64_t *)false_if_tf, CM_BYTECODES);
        uint64_t true_if_tf_len = OBJ_SIZE(true_if_tf_bc);
        uint64_t false_if_tf_len = OBJ_SIZE(false_if_tf_bc);
        ASSERT_EQ(ctx, ((uint8_t *)&OBJ_FIELD(true_if_tf_bc, 0))[true_if_tf_len - 1], BC_RETURN,
                  "runtime: True ifTrue:ifFalse: ends with local return");
        ASSERT_EQ(ctx, ((uint8_t *)&OBJ_FIELD(false_if_tf_bc, 0))[false_if_tf_len - 1], BC_RETURN,
                  "runtime: False ifTrue:ifFalse: ends with local return");
    }

    ASSERT_EQ(ctx, smalltalk_world_install_class_file(&world, "src/smalltalk/ReadStream.st") != NULL,
              1, "runtime: ReadStream.st defines class and installs methods");

    uint64_t *rs_class = smalltalk_world_lookup_class(&world, "ReadStream");
    ASSERT_EQ(ctx, rs_class != NULL, 1, "runtime: ReadStream in Smalltalk dict");

    // `ReadStream on: 'abc'` → a ReadStream whose collection is that string and position is 1.
    uint64_t abc = (uint64_t)sw_make_string(&world, "abc");
    uint64_t rs = sw_send1(&world, ctx, (uint64_t)rs_class, world.class_class, "on:", abc);
    ASSERT_EQ(ctx, is_object_ptr(rs), 1, "runtime: ReadStream on: returns an object");
    uint64_t *rs_ptr = (uint64_t *)rs;
    ASSERT_EQ(ctx, OBJ_SIZE(rs_ptr), 3, "runtime: ReadStream has 3 ivar slots");
    ASSERT_EQ(ctx, OBJ_FIELD(rs_ptr, 1), tag_smallint(1), "runtime: ReadStream position starts at 1");

    // Re-fetch the class (GC may have moved it during installs above).
    uint64_t *rs_class_live = smalltalk_world_lookup_class(&world, "ReadStream");
    // `stream next` should return the first byte ('a' = 97 as a SmallInteger,
    // because collection is a byte String and at: returns byte values).
    uint64_t first = sw_send0(&world, ctx, rs, rs_class_live, "next");
    ASSERT_EQ(ctx, first, tag_smallint('a'), "runtime: ReadStream next returns first byte");
    ASSERT_EQ(ctx, OBJ_FIELD(rs_ptr, 1), tag_smallint(2), "runtime: ReadStream position advances to 2");

    // `stream peek` returns next byte without advancing.
    uint64_t peeked = sw_send0(&world, ctx, rs, rs_class_live, "peek");
    ASSERT_EQ(ctx, peeked, tag_smallint('b'), "runtime: ReadStream peek returns second byte");
    ASSERT_EQ(ctx, OBJ_FIELD(rs_ptr, 1), tag_smallint(2), "runtime: ReadStream peek does not advance");

    // --- Step 3: Tokenizer.st end-to-end. Tokenize "1" and inspect the result. ---
    // The Tokenizer uses isNil on Character results (Object.st/UndefinedObject.st)
    // and `>`/`<=` on integers (SmallInteger.st adds these helpers).
    uint64_t *object_class = smalltalk_world_install_existing_class_file(&world, "src/smalltalk/Object.st");
    ASSERT_EQ(ctx, object_class != NULL, 1,
              "runtime: Object.st declaration matches existing class and installs methods");
    ASSERT_EQ(ctx, (uint64_t)object_class, (uint64_t)world.object_class,
              "runtime: Object.st attaches to the existing Object class");
    uint64_t *undefined_class = smalltalk_world_install_existing_class_file(&world, "src/smalltalk/UndefinedObject.st");
    ASSERT_EQ(ctx, undefined_class != NULL, 1,
              "runtime: UndefinedObject.st declaration matches existing class and installs methods");
    ASSERT_EQ(ctx, (uint64_t)undefined_class, (uint64_t)world.undefined_class,
              "runtime: UndefinedObject.st attaches to the existing UndefinedObject class");
    uint64_t *smallint_class = smalltalk_world_install_existing_class_file(&world, "src/smalltalk/SmallInteger.st");
    ASSERT_EQ(ctx, smallint_class != NULL, 1,
              "runtime: SmallInteger.st declaration matches existing class and installs methods");
    ASSERT_EQ(ctx, (uint64_t)smallint_class, (uint64_t)world.smallint_class,
              "runtime: SmallInteger.st attaches to the existing SmallInteger class");
    ASSERT_EQ(ctx, smalltalk_world_install_st_file(&world, "src/smalltalk/Class.st"), 1,
              "runtime: Class.st installs methods onto the existing Class class");
    uint64_t *array_class = smalltalk_world_install_existing_class_file(&world, "src/smalltalk/Array.st");
    ASSERT_EQ(ctx, array_class != NULL, 1,
              "runtime: Array.st declaration matches existing class and installs methods");
    ASSERT_EQ(ctx, (uint64_t)array_class, (uint64_t)world.array_class,
              "runtime: Array.st attaches to the existing Array class");
    uint64_t *string_class = smalltalk_world_install_existing_class_file(&world, "src/smalltalk/String.st");
    ASSERT_EQ(ctx, string_class != NULL, 1,
              "runtime: String.st declaration matches existing class and installs methods");
    ASSERT_EQ(ctx, (uint64_t)string_class, (uint64_t)world.string_class,
              "runtime: String.st attaches to the existing String class");
    ASSERT_EQ(ctx, smalltalk_world_install_class_file(&world, "src/smalltalk/Stdio.st") != NULL,
              1, "runtime: Stdio.st defines class and installs methods");
    ASSERT_EQ(ctx, smalltalk_world_install_st_file(&world, "src/smalltalk/Dictionary.st"), 1,
              "runtime: Dictionary.st installs methods onto the existing Dictionary class");
    uint64_t *dictionary_class = world.dictionary_class;
    uint64_t hello = (uint64_t)sw_make_string(&world, "ab");
    uint64_t suffix = (uint64_t)sw_make_string(&world, "cd");
    uint64_t combined = sw_send1(&world, ctx, hello, NULL, ",", suffix);
    ASSERT_EQ(ctx, byte_object_equals_cstring(combined, "abcd"), 1,
              "runtime: String.st concatenation executes with whileTrue:");

    {
        uint64_t *dictionary_class_live = smalltalk_world_lookup_class(&world, "Dictionary");
        uint64_t smalltalk_method = class_lookup((uint64_t *)OBJ_CLASS(dictionary_class_live),
                                                 intern_cstring_symbol(world.om, "smalltalk"));
        uint64_t globals;
        uint64_t source;
        uint64_t object_name;
        uint64_t yourself_selector = intern_cstring_symbol(world.om, "yourself");

        ASSERT_EQ(ctx, smalltalk_method != 0, 1,
                  "runtime: Dictionary class-side smalltalk method is installed");
        ASSERT_EQ(ctx, OBJ_FIELD((uint64_t *)smalltalk_method, CM_PRIMITIVE),
                  tag_smallint(PRIM_SMALLTALK_GLOBALS),
                  "runtime: Dictionary class-side smalltalk method uses live-image primitive");
        ASSERT_EQ(ctx, global_smalltalk_dictionary != NULL, 1,
                  "runtime: global Smalltalk dictionary is available to live-image primitives");
        globals = prim_smalltalk_globals();
        ASSERT_EQ(ctx, is_object_ptr(globals), 1,
                  "runtime: Dictionary smalltalk returns a live Smalltalk dictionary");
        ASSERT_EQ(ctx, OBJ_CLASS((uint64_t *)globals), (uint64_t)dictionary_class_live,
                  "runtime: Dictionary smalltalk returns a Dictionary instance");

        ASSERT_EQ(ctx, prim_class_name((uint64_t)smalltalk_world_lookup_class(&world, "Object")),
                  intern_cstring_symbol(world.om, "Object"),
                  "runtime: Class name returns live class symbol");
        ASSERT_EQ(ctx, prim_class_superclass((uint64_t)smalltalk_world_lookup_class(&world, "Object")),
                  tagged_nil(),
                  "runtime: Object superclass is nil");
        ASSERT_EQ(ctx, prim_class_includes_selector((uint64_t)smalltalk_world_lookup_class(&world, "Object"),
                                                    yourself_selector),
                  tagged_true(),
                  "runtime: Class includesSelector: sees installed method");

        object_name = (uint64_t)sw_make_string(&world, "Object");
        source = prim_method_source_for_class_selector(object_name, yourself_selector, world.om);
        ASSERT_EQ(ctx, byte_object_equals_cstring(source, "yourself\n    ^ self"), 1,
                  "runtime: Class sourceAtSelector: returns live method source");
        ASSERT_EQ(ctx,
                  byte_object_equals_cstring(
                      OBJ_FIELD((uint64_t *)class_lookup(smalltalk_world_lookup_class(&world, "Object"),
                                                         yourself_selector),
                                CM_SOURCE),
                      "yourself\n    ^ self"),
                  1,
                  "runtime: installed CompiledMethod retains full source");
    }

    {
        uint64_t *stdio_class = smalltalk_world_lookup_class(&world, "Stdio");
        int read_pipe[2];
        int write_pipe[2];
        char write_buffer[8] = {0};
        ssize_t written_bytes;
        uint64_t input;
        uint64_t output;

        ASSERT_EQ(ctx, stdio_class != NULL, 1, "runtime: Stdio in Smalltalk dict");
        ASSERT_EQ(ctx, pipe(read_pipe), 0, "runtime: Stdio read pipe setup succeeds");
        ASSERT_EQ(ctx, write(read_pipe[1], "lsp", 3), 3, "runtime: Stdio read pipe accepts bytes");
        close(read_pipe[1]);
        input = sw_send2(&world, ctx, (uint64_t)stdio_class, world.class_class,
                         "read:count:", tag_smallint(read_pipe[0]), tag_smallint(8));
        close(read_pipe[0]);
        ASSERT_EQ(ctx, byte_object_equals_cstring(input, "lsp"), 1,
                  "runtime: Stdio class>>read:count: returns bytes from fd");

        ASSERT_EQ(ctx, pipe(write_pipe), 0, "runtime: Stdio write pipe setup succeeds");
        output = sw_send2(&world, ctx, (uint64_t)stdio_class, world.class_class,
                          "write:string:", tag_smallint(write_pipe[1]),
                          (uint64_t)sw_make_string(&world, "json"));
        close(write_pipe[1]);
        written_bytes = read(write_pipe[0], write_buffer, sizeof(write_buffer));
        close(write_pipe[0]);
        ASSERT_EQ(ctx, output, tag_smallint(4),
                  "runtime: Stdio class>>write:string: reports bytes written");
        ASSERT_EQ(ctx, written_bytes, 4,
                  "runtime: Stdio class>>write:string: writes expected byte count");
        ASSERT_EQ(ctx, memcmp(write_buffer, "json", 4) == 0, 1,
                  "runtime: Stdio class>>write:string: writes expected bytes");
    }

    ASSERT_EQ(ctx, smalltalk_world_install_class_file(&world, "src/smalltalk/Tokenizer.st") != NULL,
              1, "runtime: Tokenizer.st defines class and installs methods");

    uint64_t *tokenizer_class = smalltalk_world_lookup_class(&world, "Tokenizer");
    ASSERT_EQ(ctx, tokenizer_class != NULL, 1, "runtime: Tokenizer in Smalltalk dict");

    // `Tokenizer on: '1'` → fresh tokenizer, source ivar is '1', stream is a
    // ReadStream on that source, buffered is nil.
    uint64_t src = (uint64_t)sw_make_string(&world, "1");
    uint64_t tokenizer = sw_send1(&world, ctx, (uint64_t)tokenizer_class, world.class_class, "on:", src);
    ASSERT_EQ(ctx, is_object_ptr(tokenizer), 1, "runtime: Tokenizer on: returns an object");
    // Ivars should be populated: source = input string, stream = ReadStream, buffered = nil.
    uint64_t *tok_ptr = (uint64_t *)tokenizer;
    ASSERT_EQ(ctx, OBJ_SIZE(tok_ptr), 3, "runtime: Tokenizer has 3 ivars");
    ASSERT_EQ(ctx, OBJ_FIELD(tok_ptr, 0), src, "runtime: Tokenizer source ivar is input string");
    ASSERT_EQ(ctx, is_object_ptr(OBJ_FIELD(tok_ptr, 1)), 1, "runtime: Tokenizer stream ivar is a ReadStream");
    ASSERT_EQ(ctx, OBJ_FIELD(tok_ptr, 2), tagged_nil(), "runtime: Tokenizer buffered ivar is nil");

    uint64_t first_tok_oop = sw_send0(&world, ctx, tokenizer, NULL, "next");
    ASSERT_EQ(ctx, is_object_ptr(first_tok_oop), 1, "runtime: Tokenizer next returns a Token");
    uint64_t *first_tok = (uint64_t *)first_tok_oop;
    ASSERT_EQ(ctx, OBJ_SIZE(first_tok), 3, "runtime: Token has 3 ivars");
    // type ivar (slot 0) should be #integer; value ivar (slot 2) should be 1.
    uint64_t integer_sym = intern_cstring_symbol(world.om, "integer");
    ASSERT_EQ(ctx, OBJ_FIELD(first_tok, 0), integer_sym,
              "runtime: Tokenizer produced a Token with type #integer");
    ASSERT_EQ(ctx, OBJ_FIELD(first_tok, 2), tag_smallint(1),
              "runtime: Tokenizer produced a Token with value 1");

    uint64_t src15 = (uint64_t)sw_make_string(&world, "15");
    uint64_t tokenizer15 = sw_send1(&world, ctx, (uint64_t)tokenizer_class, world.class_class, "on:", src15);
    ASSERT_EQ(ctx, is_object_ptr(tokenizer15), 1, "runtime: Tokenizer on: '15' returns an object");
    uint64_t tok15_oop = sw_send0(&world, ctx, tokenizer15, NULL, "next");
    ASSERT_EQ(ctx, is_object_ptr(tok15_oop), 1, "runtime: Tokenizer next returns a multi-digit Token");
    uint64_t *tok15 = (uint64_t *)tok15_oop;
    ASSERT_EQ(ctx, OBJ_FIELD(tok15, 0), integer_sym,
              "runtime: Tokenizer produced #integer for multi-digit input");
    ASSERT_EQ(ctx, OBJ_FIELD(tok15, 2), tag_smallint(15),
              "runtime: Tokenizer produced a Token with value 15");
    ASSERT_EQ(ctx, byte_object_equals_cstring(OBJ_FIELD(tok15, 1), "15"), 1,
              "runtime: Tokenizer preserved multi-digit token text");

#ifdef ALO_INTERPRETER_C
    OopRootSet compiler_roots = {0};
    uint64_t method_gen_root = 0;
    int have_method_gen_root = 0;

    ASSERT_EQ(ctx,
              bc_compile_and_install_classes_file(world.om, world.class_class,
                                                  world.string_class, world.array_class,
                                                  world.association_class,
                                                  NULL, 0, "src/smalltalk/ASTNodes.st"),
              1, "runtime: ASTNodes.st defines classes and installs methods");
    ASSERT_EQ(ctx, smalltalk_world_install_class_file(&world, "src/smalltalk/Parser.st") != NULL,
              1, "runtime: Parser.st defines class and installs methods");
    ASSERT_EQ(ctx, smalltalk_world_install_class_file(&world, "src/smalltalk/CodeGenerator.st") != NULL,
              1, "runtime: CodeGenerator.st defines class and installs methods");
    ASSERT_EQ(ctx, smalltalk_world_install_class_file(&world, "src/smalltalk/Compiler.st") != NULL,
              1, "runtime: Compiler.st defines class and installs methods");
    ASSERT_EQ(ctx, smalltalk_world_install_class_file(&world, "src/smalltalk/LSPMethodSpan.st") != NULL,
              1, "runtime: LSPMethodSpan.st defines class and installs methods");
    ASSERT_EQ(ctx, smalltalk_world_install_class_file(&world, "src/smalltalk/LSPSourceIndex.st") != NULL,
              1, "runtime: LSPSourceIndex.st defines class and installs methods");
    ASSERT_EQ(ctx, smalltalk_world_install_class_file(&world, "src/smalltalk/LSPDocument.st") != NULL,
              1, "runtime: LSPDocument.st defines class and installs methods");
    {
        uint64_t *parser_class = smalltalk_world_lookup_class(&world, "Parser");
        uint64_t *method_node_class = smalltalk_world_lookup_class(&world, "MethodNode");
        uint64_t binary_method_source = (uint64_t)sw_make_string(&world, "sum ^ 1 + 2");
        uint64_t parser = sw_send1(&world, ctx, (uint64_t)parser_class, world.class_class,
                                   "on:", binary_method_source);
        ASSERT_EQ(ctx, is_object_ptr(parser), 1,
                  "runtime: Parser on: returns parser for binary method");
        uint64_t method_ast = sw_send0(&world, ctx, parser, NULL, "parseMethod");
        ASSERT_EQ(ctx, is_object_ptr(method_ast), 1,
                  "runtime: Parser parseMethod returns object for binary method");
        ASSERT_EQ(ctx, OBJ_CLASS((uint64_t *)method_ast), (uint64_t)method_node_class,
                  "runtime: Parser parseMethod returns MethodNode for binary method");
        uint64_t method_args = sw_send0(&world, ctx, method_ast, NULL, "arguments");
        ASSERT_EQ(ctx, is_object_ptr(method_args), 1,
                  "runtime: MethodNode arguments returns array for binary method");
    }

    uint64_t *compiler_class = smalltalk_world_lookup_class(&world, "Compiler");
    ASSERT_EQ(ctx, compiler_class != NULL, 1, "runtime: Compiler in Smalltalk dict");
    uint64_t method_source = (uint64_t)sw_make_string(&world, "answer ^ 1");
    uint64_t method_gen = sw_send1(&world, ctx, (uint64_t)compiler_class, world.class_class,
                                   "compileMethod:", method_source);
    ASSERT_EQ(ctx, is_object_ptr(method_gen), 1,
              "runtime: Compiler compileMethod: returns a CodeGenerator object");
    method_gen_root = oop_roots_add(&compiler_roots, method_gen);
    have_method_gen_root = 1;
    uint64_t *method_gen_ptr = oop_roots_ptr(&compiler_roots, method_gen_root);
    ASSERT_EQ(ctx, OBJ_SIZE(method_gen_ptr), 11,
              "runtime: method CodeGenerator has expected ivar slots");
    ASSERT_EQ(ctx, OBJ_FIELD(method_gen_ptr, 1), tag_smallint(6),
              "runtime: Smalltalk compiler emits six bytes for answer ^ 1");
    ASSERT_EQ(ctx, OBJ_FIELD(method_gen_ptr, 3), tag_smallint(1),
              "runtime: Smalltalk compiler records one literal for answer ^ 1");
    uint64_t *generated_bytecodes = (uint64_t *)OBJ_FIELD(method_gen_ptr, 0);
    ASSERT_EQ(ctx, is_object_ptr((uint64_t)generated_bytecodes), 1,
              "runtime: generated method bytecodes are stored in a byte object");
    uint8_t *generated_bytes = (uint8_t *)&OBJ_FIELD(generated_bytecodes, 0);
    ASSERT_EQ(ctx, generated_bytes[0], BC_PUSH_LITERAL,
              "runtime: Smalltalk compiler emits push literal first");
    ASSERT_EQ(ctx, generated_bytes[5], BC_RETURN,
              "runtime: Smalltalk compiler emits return after literal");
    uint64_t *generated_literals = (uint64_t *)OBJ_FIELD(method_gen_ptr, 2);
    ASSERT_EQ(ctx, is_object_ptr((uint64_t)generated_literals), 1,
              "runtime: generated method literals are stored in an array");
    ASSERT_EQ(ctx, OBJ_FIELD(generated_literals, 0), tag_smallint(1),
              "runtime: Smalltalk compiler records literal 1");

    uint64_t *materialized_method = materialize_codegen_method(&world, method_gen_ptr);
    ASSERT_EQ(ctx, materialized_method != NULL, 1,
              "runtime: Smalltalk compiler output materializes as a CompiledMethod");
    uint64_t materialized_result =
        run_materialized_method(&world, ctx, materialized_method, tagged_nil());
    ASSERT_EQ(ctx, materialized_result, tag_smallint(1),
              "runtime: materialized Smalltalk-compiled method executes");

    {
        struct
        {
            const char *expression;
            int expected_kind;
            uint64_t expected;
            const char *expected_text;
        } expression_specs[] = {
            {"nil", 0, tagged_nil(), NULL},
            {"true", 0, tagged_true(), NULL},
            {"false", 0, tagged_false(), NULL},
            {"1", 0, tag_smallint(1), NULL},
            {"15", 0, tag_smallint(15), NULL},
            {"#foo", 0, intern_cstring_symbol(world.om, "foo"), NULL},
            {"'hi'", 1, 0, "hi"},
            {"$A", 0, tag_character('A'), NULL},
        };

        for (uint64_t index = 0; index < sizeof(expression_specs) / sizeof(expression_specs[0]); index++)
        {
            uint64_t expr_source = (uint64_t)sw_make_string(&world, expression_specs[index].expression);
            uint64_t expr_gen = sw_send1(&world, ctx, (uint64_t)compiler_class, world.class_class,
                                         "compileExpression:", expr_source);
            ASSERT_EQ(ctx, is_object_ptr(expr_gen), 1,
                      "runtime: Compiler compileExpression: returns a CodeGenerator object");
            uint64_t *expr_method = materialize_codegen_method(&world, (uint64_t *)expr_gen);
            ASSERT_EQ(ctx, expr_method != NULL, 1,
                      "runtime: compileExpression materializes into a CompiledMethod");

            char selector[32];
            snprintf(selector, sizeof(selector), "expr%llu", (unsigned long long)(index + 1));
            uint64_t *c_method = compile_c_expression_method(&world, selector, expression_specs[index].expression);
            ASSERT_EQ(ctx, c_method != NULL, 1,
                      "runtime: C compiler materializes expression comparison method");
            ASSERT_EQ(ctx, method_structurally_equal(&world, c_method, expr_method, 0), 1,
                      expression_specs[index].expression);

            uint64_t expr_result = run_materialized_method(&world, ctx, expr_method, tagged_nil());
            if (expression_specs[index].expected_kind == 1)
            {
                ASSERT_EQ(ctx, byte_object_equals_cstring(expr_result, expression_specs[index].expected_text), 1,
                          expression_specs[index].expression);
            }
            else
            {
                ASSERT_EQ(ctx, expr_result, expression_specs[index].expected,
                          expression_specs[index].expression);
            }
        }
    }

    uint64_t *code_generator_class = smalltalk_world_lookup_class(&world, "CodeGenerator");
    ASSERT_EQ(ctx, code_generator_class != NULL, 1, "runtime: CodeGenerator in Smalltalk dict");
    {
        uint64_t visit_message_selector = intern_cstring_symbol(world.om, "visitMessage:");
        uint64_t visit_message_method = class_lookup(code_generator_class, visit_message_selector);
        ASSERT_EQ(ctx, is_object_ptr(visit_message_method), 1,
                  "runtime: CodeGenerator understands visitMessage:");
        uint64_t *visit_message_bytecodes = (uint64_t *)OBJ_FIELD((uint64_t *)visit_message_method, CM_BYTECODES);
        uint64_t visit_message_len = OBJ_SIZE(visit_message_bytecodes);
        uint8_t *visit_message_bytes = (uint8_t *)&OBJ_FIELD(visit_message_bytecodes, 0);
        ASSERT_EQ(ctx, visit_message_bytes[visit_message_len - 2], BC_PUSH_SELF,
                  "runtime: visitMessage: keeps trailing push self");
        ASSERT_EQ(ctx, visit_message_bytes[visit_message_len - 1], BC_RETURN,
                  "runtime: visitMessage: keeps trailing return");
    }
    {
        uint64_t direct_gen = sw_send0(&world, ctx, (uint64_t)code_generator_class,
                                       world.class_class, "new");
        uint64_t direct_result = sw_send2(&world, ctx, direct_gen, NULL,
                                          "emitSendMessage:argc:",
                                          tag_smallint(2), tag_smallint(1));
        ASSERT_EQ(ctx, is_object_ptr(direct_result), 1,
                  "runtime: emitSendMessage:argc: returns a generator object");
        ASSERT_EQ(ctx, direct_result, direct_gen,
                  "runtime: emitSendMessage:argc: returns self");
        ASSERT_EQ(ctx, OBJ_FIELD((uint64_t *)direct_gen, 1), tag_smallint(9),
                  "runtime: emitSendMessage:argc: appends nine bytes");
    }
    trap_world = &world;
    ASSERT_EQ(ctx, (uint64_t)run_trap_test(ctx, trap_codegen_add_literal_nil), 0,
              "runtime: CodeGenerator addLiteral: nil executes");
    ASSERT_EQ(ctx, (uint64_t)run_trap_test(ctx, trap_codegen_resolve_nil), 0,
              "runtime: CodeGenerator resolveVariable: 'nil' executes");
    uint64_t *parser_class = smalltalk_world_lookup_class(&world, "Parser");
    ASSERT_EQ(ctx, parser_class != NULL, 1, "runtime: Parser in Smalltalk dict");
    uint64_t *message_node_class = smalltalk_world_lookup_class(&world, "MessageNode");
    ASSERT_EQ(ctx, message_node_class != NULL, 1, "runtime: MessageNode in Smalltalk dict");
    {
        uint64_t nil_source = (uint64_t)sw_make_string(&world, "nil");
        uint64_t nil_parser = sw_send1(&world, ctx, (uint64_t)parser_class, world.class_class,
                                       "on:", nil_source);
        uint64_t nil_ast = sw_send0(&world, ctx, nil_parser, NULL, "parseExpression");
        ASSERT_EQ(ctx, is_object_ptr(nil_ast), 1,
                  "runtime: Parser parseExpression returns a node for nil");
    }
    ASSERT_EQ(ctx, (uint64_t)run_trap_test(ctx, trap_compiler_compile_nil), 0,
              "runtime: Compiler compileExpression: 'nil' executes");
    {
        uint64_t *lsp_document_class = smalltalk_world_lookup_class(&world, "LSPDocument");
        uint64_t *lsp_method_span_class = smalltalk_world_lookup_class(&world, "LSPMethodSpan");
        uint64_t *method_node_class = smalltalk_world_lookup_class(&world, "MethodNode");
        ASSERT_EQ(ctx, lsp_document_class != NULL, 1, "runtime: LSPDocument in Smalltalk dict");
        ASSERT_EQ(ctx, lsp_method_span_class != NULL, 1, "runtime: LSPMethodSpan in Smalltalk dict");
        ASSERT_EQ(ctx, untag_smallint(OBJ_FIELD(lsp_document_class, CLASS_INST_SIZE)), 5,
                  "runtime: LSPDocument class declaration has five instance variables");

        uint64_t method_text = (uint64_t)sw_make_string(&world, "answer ^ 1");
        uint64_t method_uri = (uint64_t)sw_make_string(&world, "memory://Answer.st");
        uint64_t doc = sw_send2(&world, ctx, (uint64_t)lsp_document_class, world.class_class,
                                "uri:text:", method_uri, method_text);
        ASSERT_EQ(ctx, is_object_ptr(doc), 1,
                  "runtime: LSPDocument class-side constructor returns object");
        ASSERT_EQ(ctx, sw_send0(&world, ctx, doc, NULL, "uri"), method_uri,
                  "runtime: LSPDocument stores URI");
        ASSERT_EQ(ctx, sw_send0(&world, ctx, doc, NULL, "text"), method_text,
                  "runtime: LSPDocument stores source text");
        ASSERT_EQ(ctx, sw_send0(&world, ctx, doc, NULL, "version"), tag_smallint(1),
                  "runtime: LSPDocument starts at version 1");
        ASSERT_EQ(ctx, is_object_ptr(sw_send0(&world, ctx, doc, NULL, "sourceIndex")), 1,
                  "runtime: LSPDocument creates a source index object");

        uint64_t ast0 = sw_send0(&world, ctx, doc, NULL, "ast");
        ASSERT_EQ(ctx, ast0, tagged_nil(),
                  "runtime: LSPDocument starts without cached AST");

        uint64_t ast1 = sw_send0(&world, ctx, doc, NULL, "parseMethodAst");
        ASSERT_EQ(ctx, is_object_ptr(ast1), 1,
                  "runtime: LSPDocument parseMethodAst returns AST");
        ASSERT_EQ(ctx, OBJ_CLASS((uint64_t *)ast1), (uint64_t)method_node_class,
                  "runtime: LSPDocument parseMethodAst returns MethodNode");
        uint64_t ast2 = sw_send0(&world, ctx, doc, NULL, "parseMethodAst");
        ASSERT_EQ(ctx, ast1, ast2,
                  "runtime: LSPDocument reuses cached AST");

        uint64_t answer_class_name = intern_cstring_symbol(world.om, "Answer");
        uint64_t answer_selector = intern_cstring_symbol(world.om, "answer");
        uint64_t span = sw_send2(&world, ctx, (uint64_t)lsp_method_span_class, world.class_class,
                                 "className:selector:",
                                 answer_class_name,
                                 answer_selector);
        ASSERT_EQ(ctx, is_object_ptr(span), 1,
                  "runtime: LSPMethodSpan class-side constructor returns object");
        ASSERT_EQ(ctx, sw_send0(&world, ctx, span, NULL, "className"), answer_class_name,
                  "runtime: LSPMethodSpan stores class name");
        ASSERT_EQ(ctx, sw_send0(&world, ctx, span, NULL, "selector"), answer_selector,
                  "runtime: LSPMethodSpan stores selector");
        ASSERT_EQ(ctx, sw_send2(&world, ctx, span, NULL,
                                "start:stop:",
                                tag_smallint(1),
                                tag_smallint(10)),
                  span,
                  "runtime: LSPMethodSpan start:stop: returns self");
        ASSERT_EQ(ctx, sw_send1(&world, ctx, span, NULL,
                                "source:",
                                method_text),
                  method_text,
                  "runtime: LSPMethodSpan source: stores source text");
        ASSERT_EQ(ctx, sw_send0(&world, ctx, span, NULL, "source"), method_text,
                  "runtime: span keeps source text");

        {
            uint64_t updated_text = (uint64_t)sw_make_string(&world, "answer ^ 2");
            uint64_t update_result = sw_send2(&world, ctx, doc, NULL,
                                              "updateText:version:",
                                              updated_text, tag_smallint(2));
            ASSERT_EQ(ctx, update_result, doc,
                      "runtime: LSPDocument updateText:version: returns self");
            ASSERT_EQ(ctx, sw_send0(&world, ctx, doc, NULL, "version"), tag_smallint(2),
                      "runtime: LSPDocument updates version");
            ASSERT_EQ(ctx, sw_send0(&world, ctx, doc, NULL, "text"), updated_text,
                      "runtime: LSPDocument updates text");
            ASSERT_EQ(ctx, sw_send0(&world, ctx, doc, NULL, "ast"), tagged_nil(),
                      "runtime: LSPDocument clears cached AST on update");
            ASSERT_EQ(ctx, is_object_ptr(sw_send0(&world, ctx, doc, NULL, "sourceIndex")), 1,
                      "runtime: LSPDocument replaces source index on update");
        }
    }
    {
        uint64_t expr_source = (uint64_t)sw_make_string(&world, "1 + 2");
        uint64_t expr_parser = sw_send1(&world, ctx, (uint64_t)parser_class, world.class_class,
                                        "on:", expr_source);
        uint64_t expr_ast = sw_send0(&world, ctx, expr_parser, NULL, "parseExpression");
        ASSERT_EQ(ctx, is_object_ptr(expr_ast), 1,
                  "runtime: Parser parses binary expression for direct CodeGenerator stepping");

        uint64_t step_gen = sw_send0(&world, ctx, (uint64_t)code_generator_class,
                                     world.class_class, "new");
        ASSERT_EQ(ctx, is_object_ptr(step_gen), 1,
                  "runtime: CodeGenerator new returns object for direct visitMessage stepping");

        expr_source = (uint64_t)sw_make_string(&world, "1 + 2");
        expr_parser = sw_send1(&world, ctx, (uint64_t)parser_class, world.class_class,
                               "on:", expr_source);
        expr_ast = sw_send0(&world, ctx, expr_parser, NULL, "parseExpression");
        ASSERT_EQ(ctx, OBJ_CLASS((uint64_t *)expr_ast), (uint64_t)message_node_class,
                  "runtime: binary expression parses as MessageNode");
        uint64_t expr_receiver = OBJ_FIELD((uint64_t *)expr_ast, 0);
        uint64_t step_result =
            sw_send1(&world, ctx, step_gen, NULL, "visitNode:", expr_receiver);
        ASSERT_EQ(ctx, is_object_ptr(step_result), 1,
                  "runtime: visit receiver node returns generator");
        step_gen = step_result;
        ASSERT_EQ(ctx, OBJ_FIELD((uint64_t *)step_gen, 1), tag_smallint(5),
                  "runtime: visit receiver node emits one push literal");
        ASSERT_EQ(ctx, OBJ_FIELD((uint64_t *)step_gen, 3), tag_smallint(1),
                  "runtime: visit receiver node records receiver literal");

        expr_source = (uint64_t)sw_make_string(&world, "1 + 2");
        expr_parser = sw_send1(&world, ctx, (uint64_t)parser_class, world.class_class,
                               "on:", expr_source);
        expr_ast = sw_send0(&world, ctx, expr_parser, NULL, "parseExpression");
        uint64_t expr_arguments = OBJ_FIELD((uint64_t *)expr_ast, 2);
        ASSERT_EQ(ctx, is_object_ptr(expr_arguments), 1,
                  "runtime: binary expression stores arguments in an object");
        ASSERT_EQ(ctx, OBJ_CLASS((uint64_t *)expr_arguments), (uint64_t)world.array_class,
                  "runtime: binary expression arguments are an Array");
        uint64_t expr_argc = tag_smallint((int64_t)OBJ_SIZE((uint64_t *)expr_arguments));
        ASSERT_EQ(ctx, expr_argc, tag_smallint(1),
                  "runtime: binary expression exposes one argument");
        ASSERT_EQ(ctx, is_object_ptr(OBJ_FIELD((uint64_t *)expr_arguments, 0)), 1,
                  "runtime: binary expression first argument is an AST node");
        ASSERT_EQ(ctx, OBJ_FIELD((uint64_t *)OBJ_FIELD((uint64_t *)expr_arguments, 0), 0), tag_smallint(2),
                  "runtime: binary expression first argument literal is 2");
        step_result =
            sw_send2(&world, ctx, step_gen, NULL, "visitMessageArgs:from:",
                     expr_arguments, tag_smallint(1));
        ASSERT_EQ(ctx, is_object_ptr(step_result), 1,
                  "runtime: visitMessageArgs:from: returns generator");
        step_gen = step_result;
        ASSERT_EQ(ctx, OBJ_FIELD((uint64_t *)step_gen, 1), tag_smallint(10),
                  "runtime: visitMessageArgs:from: emits argument bytecodes");
        ASSERT_EQ(ctx, OBJ_FIELD((uint64_t *)step_gen, 3), tag_smallint(2),
                  "runtime: visitMessageArgs:from: records argument literal");
        ASSERT_EQ(ctx, OBJ_FIELD((uint64_t *)OBJ_FIELD((uint64_t *)step_gen, 2), 0), tag_smallint(1),
                  "runtime: visitMessageArgs:from: keeps receiver literal first");
        ASSERT_EQ(ctx, OBJ_FIELD((uint64_t *)OBJ_FIELD((uint64_t *)step_gen, 2), 1), tag_smallint(2),
                  "runtime: visitMessageArgs:from: stores argument literal second");

        expr_source = (uint64_t)sw_make_string(&world, "1 + 2");
        expr_parser = sw_send1(&world, ctx, (uint64_t)parser_class, world.class_class,
                               "on:", expr_source);
        expr_ast = sw_send0(&world, ctx, expr_parser, NULL, "parseExpression");
        uint64_t expr_selector = OBJ_FIELD((uint64_t *)expr_ast, 1);
        ASSERT_EQ(ctx, byte_object_equals_cstring(expr_selector, "+"), 1,
                  "runtime: binary expression selector is '+'");
        uint64_t selector_index =
            sw_send1(&world, ctx, step_gen, NULL, "addSelectorLiteral:", expr_selector);
        ASSERT_EQ(ctx, selector_index, tag_smallint(2),
                  "runtime: addSelectorLiteral: returns selector literal index");
        ASSERT_EQ(ctx, OBJ_FIELD((uint64_t *)step_gen, 3), tag_smallint(3),
                  "runtime: addSelectorLiteral: appends selector literal");
        ASSERT_EQ(ctx, byte_object_equals_cstring(OBJ_FIELD((uint64_t *)OBJ_FIELD((uint64_t *)step_gen, 2), 2), "+"), 1,
                  "runtime: addSelectorLiteral: stores '+' as third literal");

        step_result =
            sw_send2(&world, ctx, step_gen, NULL, "emitSendMessage:argc:",
                     selector_index, expr_argc);
        ASSERT_EQ(ctx, is_object_ptr(step_result), 1,
                  "runtime: emitSendMessage:argc: after selector setup returns generator");
        step_gen = step_result;
        ASSERT_EQ(ctx, OBJ_FIELD((uint64_t *)step_gen, 1), tag_smallint(19),
                  "runtime: emitSendMessage:argc: appends message send bytecodes");

        expr_source = (uint64_t)sw_make_string(&world, "1 + 2");
        expr_parser = sw_send1(&world, ctx, (uint64_t)parser_class, world.class_class,
                               "on:", expr_source);
        expr_ast = sw_send0(&world, ctx, expr_parser, NULL, "parseExpression");
        step_result =
            sw_send1(&world, ctx, step_gen, NULL, "visitMessage:", expr_ast);
        ASSERT_EQ(ctx, is_object_ptr(step_result), 1,
                  "runtime: visitMessage: returns generator for binary expression");
    }
    uint64_t loop_gen = sw_send0(&world, ctx, (uint64_t)code_generator_class,
                                 world.class_class, "new");
    ASSERT_EQ(ctx, is_object_ptr(loop_gen), 1,
              "runtime: CodeGenerator new returns an object for whileTrue:");
    uint64_t loop_source = (uint64_t)sw_make_string(&world, "[1 < 0] whileTrue: [1]");
    uint64_t loop_parser = sw_send1(&world, ctx, (uint64_t)parser_class, world.class_class,
                                    "on:", loop_source);
    ASSERT_EQ(ctx, is_object_ptr(loop_parser), 1, "runtime: Parser on: creates parser for whileTrue:");
    uint64_t loop_ast = sw_send0(&world, ctx, loop_parser, NULL, "parseExpression");
    ASSERT_EQ(ctx, is_object_ptr(loop_ast), 1, "runtime: Parser parses whileTrue: expression");
    uint64_t moved_loop_gen = loop_gen;
    (void)sw_send1_capture_receiver(&world, ctx, loop_gen, NULL,
                                    "visitNode:", loop_ast, &moved_loop_gen);
    loop_gen = moved_loop_gen;
    (void)sw_send0_capture_receiver(&world, ctx, loop_gen, NULL,
                                    "emitReturn", &moved_loop_gen);
    loop_gen = moved_loop_gen;
    ASSERT_EQ(ctx, is_object_ptr(loop_gen), 1,
              "runtime: Smalltalk CodeGenerator compiles whileTrue: on the receiver");
    uint64_t *loop_gen_ptr = (uint64_t *)loop_gen;
    uint64_t loop_bytecode_count = (uint64_t)untag_smallint(OBJ_FIELD(loop_gen_ptr, 1));
    uint64_t *loop_bytecodes = (uint64_t *)OBJ_FIELD(loop_gen_ptr, 0);
    ASSERT_EQ(ctx, bytecode_contains(loop_bytecodes, loop_bytecode_count, BC_JUMP_IF_FALSE), 1,
              "runtime: Smalltalk compiler emits whileTrue: false jump");
#endif

    ASSERT_EQ(ctx, smalltalk_world_install_st_file(&world, "src/smalltalk/Context.st"),
              1, "runtime: Context.st installs methods onto the existing Context class");
    ASSERT_EQ(ctx, class_lookup(world.context_class, intern_cstring_symbol(world.om, "sender")) != 0,
              1, "runtime: Context.st installs sender");
    ASSERT_EQ(ctx, smalltalk_world_install_existing_class_file(&world, "src/smalltalk/Object.st") != NULL,
              1, "runtime: Object.st installs methods onto the existing Object class");

    ASSERT_EQ(ctx, smalltalk_world_install_class_file(&world, "src/smalltalk/TestResult.st") != NULL,
              1, "runtime: TestResult.st defines class and installs methods");
    uint64_t *test_result_class = smalltalk_world_lookup_class(&world, "TestResult");
    ASSERT_EQ(ctx, test_result_class != NULL, 1, "runtime: TestResult in Smalltalk dict");
    ASSERT_EQ(ctx, untag_smallint(OBJ_FIELD(test_result_class, CLASS_INST_SIZE)), 8,
              "runtime: TestResult.st class declaration has eight instance variables");
    ASSERT_EQ(ctx, class_lookup(test_result_class, intern_cstring_symbol(world.om, "failureBacktraces")) != 0,
              1, "runtime: TestResult.st installs failureBacktraces");

    ASSERT_EQ(ctx, smalltalk_world_install_class_file(&world, "src/smalltalk/Exception.st") != NULL,
              1, "runtime: Exception.st defines class and installs methods");
    ASSERT_EQ(ctx, smalltalk_world_install_class_file(&world, "src/smalltalk/Error.st") != NULL,
              1, "runtime: Error.st defines class and installs methods");
    ASSERT_EQ(ctx, smalltalk_world_install_existing_class_file(&world, "src/smalltalk/True.st") != NULL,
              1, "runtime: True.st installs methods onto the existing True class");
    ASSERT_EQ(ctx, smalltalk_world_install_existing_class_file(&world, "src/smalltalk/False.st") != NULL,
              1, "runtime: False.st installs methods onto the existing False class");
    {
        ASSERT_EQ(ctx, class_lookup(world.true_class, intern_cstring_symbol(world.om, "ifTrue:ifFalse:")) != 0, 1,
                  "runtime: True class handles ifTrue:ifFalse:");
        ASSERT_EQ(ctx, class_lookup(world.false_class, intern_cstring_symbol(world.om, "ifTrue:ifFalse:")) != 0, 1,
                  "runtime: False class handles ifTrue:ifFalse:");
    }

    ASSERT_EQ(ctx, smalltalk_world_install_class_file(&world, "src/smalltalk/TestCase.st") != NULL,
              1, "runtime: TestCase.st defines class and installs methods");
    uint64_t *test_case_class = smalltalk_world_lookup_class(&world, "TestCase");
    ASSERT_EQ(ctx, test_case_class != NULL, 1, "runtime: TestCase in Smalltalk dict");
    ASSERT_EQ(ctx, untag_smallint(OBJ_FIELD(test_case_class, CLASS_INST_SIZE)), 2,
              "runtime: TestCase.st class declaration has two instance variables");
    ASSERT_EQ(ctx, class_lookup(test_case_class, intern_cstring_symbol(world.om, "runOn:")) != 0,
              1, "runtime: TestCase.st installs runOn:");

    ASSERT_EQ(ctx, smalltalk_world_install_class_file(&world, "src/smalltalk/TestSuite.st") != NULL,
              1, "runtime: TestSuite.st defines class and installs methods");
    uint64_t *test_suite_class = smalltalk_world_lookup_class(&world, "TestSuite");
    ASSERT_EQ(ctx, test_suite_class != NULL, 1, "runtime: TestSuite in Smalltalk dict");
    ASSERT_EQ(ctx, untag_smallint(OBJ_FIELD(test_suite_class, CLASS_INST_SIZE)), 2,
              "runtime: TestSuite.st class declaration has two instance variables");
    ASSERT_EQ(ctx, class_lookup(test_suite_class, intern_cstring_symbol(world.om, "runOn:")) != 0,
              1, "runtime: TestSuite.st installs runOn:");
    ASSERT_EQ(ctx, class_lookup(test_suite_class, intern_cstring_symbol(world.om, "add:")) != 0,
              1, "runtime: TestSuite.st installs add:");

    ASSERT_EQ(ctx, smalltalk_world_install_st_file(&world, "src/smalltalk/BlockClosure.st"), 1,
              "runtime: BlockClosure.st installs methods onto the existing BlockClosure class");

    ASSERT_EQ(ctx, smalltalk_world_install_class_file(&world, "tests/fixtures/ContextTest.st") != NULL,
              1, "runtime: ContextTest.st defines class and installs methods");
    ASSERT_EQ(ctx, smalltalk_world_install_class_file(&world, "tests/fixtures/BlockActivationTest.st") != NULL,
              1, "runtime: BlockActivationTest.st defines class and installs methods");
    ASSERT_EQ(ctx, smalltalk_world_install_class_file(&world, "tests/fixtures/DefaultActionException.st") != NULL,
              1, "runtime: DefaultActionException.st defines class and installs methods");
    ASSERT_EQ(ctx, smalltalk_world_install_class_file(&world, "tests/fixtures/ExceptionHandlingTest.st") != NULL,
              1, "runtime: ExceptionHandlingTest.st defines class and installs methods");
    {
        uint64_t *context_test_class = smalltalk_world_lookup_class(&world, "ContextTest");
        uint64_t *block_activation_test_class = smalltalk_world_lookup_class(&world, "BlockActivationTest");
        uint64_t *default_action_exception_class = smalltalk_world_lookup_class(&world, "DefaultActionException");
        uint64_t *exception_handling_test_class = smalltalk_world_lookup_class(&world, "ExceptionHandlingTest");
        uint64_t *exception_class = smalltalk_world_lookup_class(&world, "Exception");
        uint64_t *error_class = smalltalk_world_lookup_class(&world, "Error");
        ASSERT_EQ(ctx, context_test_class != NULL, 1, "runtime: ContextTest in Smalltalk dict");
        ASSERT_EQ(ctx, block_activation_test_class != NULL, 1, "runtime: BlockActivationTest in Smalltalk dict");
        ASSERT_EQ(ctx, default_action_exception_class != NULL, 1, "runtime: DefaultActionException in Smalltalk dict");
        ASSERT_EQ(ctx, exception_handling_test_class != NULL, 1, "runtime: ExceptionHandlingTest in Smalltalk dict");
        ASSERT_EQ(ctx, exception_class != NULL, 1, "runtime: Exception in Smalltalk dict");
        ASSERT_EQ(ctx, error_class != NULL, 1, "runtime: Error in Smalltalk dict");
        ASSERT_EQ(ctx, class_lookup(context_test_class, intern_cstring_symbol(world.om, "runOn:")) != 0,
                  1, "runtime: ContextTest inherits runOn:");
        ASSERT_EQ(ctx, class_lookup(block_activation_test_class, intern_cstring_symbol(world.om, "runOn:")) != 0,
                  1, "runtime: BlockActivationTest inherits runOn:");
        ASSERT_EQ(ctx, class_lookup(exception_handling_test_class, intern_cstring_symbol(world.om, "runOn:")) != 0,
                  1, "runtime: ExceptionHandlingTest inherits runOn:");
#ifdef ALO_INTERPRETER_C
        ASSERT_EQ(ctx, sw_send1(&world, ctx, (Oop)exception_class, world.class_class,
                                "handlesSignalClass:", (Oop)error_class), TAGGED_TRUE,
                  "runtime: Exception matches Error through Smalltalk protocol");
        ASSERT_EQ(ctx, sw_send1(&world, ctx, (Oop)error_class, world.class_class,
                                "handlesSignalClass:", (Oop)exception_class), TAGGED_FALSE,
                  "runtime: Error does not match Exception through Smalltalk protocol");
#endif
    }
    ASSERT_EQ(ctx, smalltalk_world_install_class_file(&world, "tests/fixtures/SmalltalkSelfTestSuite.st") != NULL,
              1, "runtime: SmalltalkSelfTestSuite.st defines class and installs methods");
    {
        uint64_t *smalltalk_self_test_suite_class = smalltalk_world_lookup_class(&world, "SmalltalkSelfTestSuite");
        ASSERT_EQ(ctx, smalltalk_self_test_suite_class != NULL, 1,
                  "runtime: SmalltalkSelfTestSuite in Smalltalk dict");
        ASSERT_EQ(ctx, class_lookup((uint64_t *)OBJ_CLASS(smalltalk_self_test_suite_class),
                                    intern_cstring_symbol(world.om, "selfTest")) != 0,
                  1, "runtime: SmalltalkSelfTestSuite has class-side selfTest runner");
        ASSERT_EQ(ctx, class_lookup((uint64_t *)OBJ_CLASS(smalltalk_self_test_suite_class),
                                    intern_cstring_symbol(world.om, "suite")) != 0,
                  1, "runtime: SmalltalkSelfTestSuite has class-side suite builder");
#ifdef ALO_INTERPRETER_C
        run_smalltalk_self_test(ctx, &world, "SmalltalkSelfTestSuite", 23);
#else
        run_smalltalk_direct_tests(ctx, &world, "ContextTest", 6);
        run_smalltalk_direct_tests(ctx, &world, "BlockActivationTest", 6);
#endif
    }
#ifdef ALO_INTERPRETER_C
    if (have_method_gen_root)
    {
        static uint8_t compiler_gc_buf[32 * 1024 * 1024] __attribute__((aligned(8)));
        uint64_t to_space[2];
        om_init(compiler_gc_buf, sizeof(compiler_gc_buf), to_space);

        gc_collect(compiler_roots.roots, compiler_roots.count, world.om, to_space,
                   (uint64_t)world_buf, (uint64_t)(world_buf + sizeof(world_buf)));

        uint64_t moved_method_gen = oop_roots_get(&compiler_roots, method_gen_root);
        uint64_t *moved_method_gen_ptr = (uint64_t *)moved_method_gen;
        ASSERT_EQ(ctx, moved_method_gen >= (uint64_t)compiler_gc_buf, 1,
                  "runtime: rooted compiler result moves to GC to-space lower bound");
        ASSERT_EQ(ctx, moved_method_gen < (uint64_t)(compiler_gc_buf + sizeof(compiler_gc_buf)), 1,
                  "runtime: rooted compiler result moves to GC to-space upper bound");
        ASSERT_EQ(ctx, OBJ_SIZE(moved_method_gen_ptr), 11,
                  "runtime: rooted compiler result remains a CodeGenerator after GC");
        ASSERT_EQ(ctx, OBJ_FIELD(moved_method_gen_ptr, 1), tag_smallint(6),
                  "runtime: rooted compiler result preserves bytecode count after GC");
        ASSERT_EQ(ctx, OBJ_FIELD(moved_method_gen_ptr, 3), tag_smallint(1),
                  "runtime: rooted compiler result preserves literal count after GC");
        uint64_t *moved_bytecodes = (uint64_t *)OBJ_FIELD(moved_method_gen_ptr, 0);
        ASSERT_EQ(ctx, is_object_ptr((uint64_t)moved_bytecodes), 1,
                  "runtime: rooted compiler result preserves bytecodes object after GC");
        uint8_t *moved_bytes = (uint8_t *)&OBJ_FIELD(moved_bytecodes, 0);
        ASSERT_EQ(ctx, moved_bytes[0], BC_PUSH_LITERAL,
                  "runtime: rooted compiler result preserves bytecodes after GC");
    }
#endif

    smalltalk_world_teardown(&world);
}
