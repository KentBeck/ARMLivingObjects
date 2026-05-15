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
#include "smalltalk_test_support.h"

#include <unistd.h>

typedef struct RuntimeCheckpointHeader
{
    uint64_t magic;
    uint64_t used_size;
    uint64_t page_bytes;
    uint64_t page_count;
    uint64_t generation;
    uint64_t symbol_table_offset;
    uint64_t symbol_class_offset;
    uint64_t context_class_offset;
    uint64_t smalltalk_dict_offset;
    uint64_t class_table_offset;
    uint64_t metadata_checksum;
} RuntimeCheckpointHeader;

static uint64_t runtime_checkpoint_page_data_offset(uint64_t page_count, uint64_t page_id)
{
    return sizeof(RuntimeCheckpointHeader) + (5 * page_count * sizeof(uint64_t)) +
           (page_id * OM_PAGE_BYTES);
}

static uint64_t runtime_first_free_tail_page(Om om)
{
    uint64_t current_page = om_page_id_for_address(om, om[0]);
    uint64_t page_count = om_page_count(om);

    for (uint64_t page_id = current_page + 1; page_id < page_count; page_id++)
    {
        if (om_page_state(om, page_id) == OM_PAGE_STATE_FREE &&
            om_page_used_bytes(om, page_id) == 0)
        {
            return page_id;
        }
    }
    return UINT64_MAX;
}

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

typedef enum RuntimeExprExpectedKind
{
    RUNTIME_EXPR_EXPECT_SMALLINT = 0,
    RUNTIME_EXPR_EXPECT_TRUE,
    RUNTIME_EXPR_EXPECT_FALSE
} RuntimeExprExpectedKind;

typedef struct RuntimeExpressionSpec
{
    char name[64];
    char expression[256];
    RuntimeExprExpectedKind expected_kind;
    int64_t expected_smallint;
} RuntimeExpressionSpec;

static int runtime_load_expression_specs(const char *path,
                                         RuntimeExpressionSpec *specs,
                                         int max_specs,
                                         int *out_count)
{
    FILE *file = fopen(path, "rb");
    char line[1024];
    int count = 0;

    if (file == NULL)
    {
        return 0;
    }

    while (fgets(line, sizeof(line), file) != NULL)
    {
        int parsed_kind = 0;
        char *first_sep;
        char *second_field;
        char *second_sep;
        char *third_field;
        RuntimeExpressionSpec *spec;

        stt_trim_in_place(line);
        if (line[0] == '\0' || line[0] == '#')
        {
            continue;
        }

        first_sep = strstr(line, "|");
        if (first_sep == NULL)
        {
            fclose(file);
            return 0;
        }
        *first_sep = '\0';

        second_field = first_sep + 1;
        second_sep = strstr(second_field, "|");
        if (second_sep == NULL)
        {
            fclose(file);
            return 0;
        }
        *second_sep = '\0';
        third_field = second_sep + 1;

        stt_trim_in_place(line);
        stt_trim_in_place(second_field);
        stt_trim_in_place(third_field);

        if (line[0] == '\0' || second_field[0] == '\0' || third_field[0] == '\0' || count >= max_specs)
        {
            fclose(file);
            return 0;
        }

        spec = &specs[count];
        memset(spec, 0, sizeof(*spec));
        strncpy(spec->name, line, sizeof(spec->name) - 1);
        strncpy(spec->expression, second_field, sizeof(spec->expression) - 1);
        if (!stt_parse_expected_value(third_field, &parsed_kind, &spec->expected_smallint))
        {
            fclose(file);
            return 0;
        }
        spec->expected_kind = (RuntimeExprExpectedKind)parsed_kind;
        count++;
    }

    fclose(file);
    if (count == 0)
    {
        return 0;
    }
    *out_count = count;
    return 1;
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

static void install_rooted_method(TestContext *ctx,
                                  SmalltalkWorld *world,
                                  const char *class_name,
                                  const char *selector_name,
                                  const char *temp_global_name,
                                  Oop method_oop)
{
    ObjPtr klass;
    Oop selector_oop;
    Oop old_md_oop;
    ObjPtr old_md;
    uint64_t old_size;
    ObjPtr new_md;

    smalltalk_world_put_global(world, temp_global_name, method_oop);
    method_oop = smalltalk_world_lookup_global(world, temp_global_name);
    ASSERT_EQ(ctx, is_object_ptr(method_oop), 1, "runtime: rooted compiled method stays live");

    selector_oop = intern_cstring_symbol(world->om, selector_name);
    klass = smalltalk_world_lookup_class(world, class_name);
    ASSERT_EQ(ctx, klass != NULL, 1, "runtime: target class exists for rooted install");

    old_md_oop = OBJ_FIELD(klass, CLASS_METHOD_DICT);
    old_md = (old_md_oop != tagged_nil() && is_object_ptr(old_md_oop)) ? (ObjPtr)old_md_oop : NULL;
    old_size = old_md != NULL ? OBJ_SIZE(old_md) : 0;
    new_md = om_alloc(world->om, (Oop)world->class_class, FORMAT_INDEXABLE, old_size + 2);
    ASSERT_EQ(ctx, new_md != NULL, 1, "runtime: rooted method dict allocation succeeds");

    klass = smalltalk_world_lookup_class(world, class_name);
    ASSERT_EQ(ctx, klass != NULL, 1, "runtime: target class survives rooted install allocation");
    method_oop = smalltalk_world_lookup_global(world, temp_global_name);
    ASSERT_EQ(ctx, is_object_ptr(method_oop), 1, "runtime: rooted method survives method dict allocation");
    old_md_oop = OBJ_FIELD(klass, CLASS_METHOD_DICT);
    old_md = (old_md_oop != tagged_nil() && is_object_ptr(old_md_oop)) ? (ObjPtr)old_md_oop : NULL;
    old_size = old_md != NULL ? OBJ_SIZE(old_md) : 0;

    for (uint64_t index = 0; index < old_size; index++)
    {
        OBJ_FIELD(new_md, index) = OBJ_FIELD(old_md, index);
    }
    OBJ_FIELD(new_md, old_size) = selector_oop;
    OBJ_FIELD(new_md, old_size + 1) = method_oop;
    OBJ_FIELD(klass, CLASS_METHOD_DICT) = (Oop)new_md;
    smalltalk_world_put_global(world, temp_global_name, tagged_nil());
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

static void run_smalltalk_direct_selector(TestContext *ctx, SmalltalkWorld *world,
                                          const char *class_name, const char *selector_name)
{
    uint64_t *test_class = smalltalk_world_lookup_class(world, class_name);
    Oop test_case;
    Oop result;

    ASSERT_EQ(ctx, test_class != NULL, 1, "runtime: direct Smalltalk selector class available");
    test_case = sw_send0(world, ctx, (Oop)test_class, world->class_class, "new");
    ASSERT_EQ(ctx, is_object_ptr(test_case), 1, "runtime: direct Smalltalk selector instance exists");
    result = sw_send0(world, ctx, test_case, NULL, selector_name);
    ASSERT_EQ(ctx, result, TAGGED_TRUE, "runtime: direct Smalltalk selector passes");
}

static void run_smalltalk_suite_builder(TestContext *ctx, SmalltalkWorld *world,
                                        const char *class_name, int expected_tests)
{
    uint64_t *suite_class = smalltalk_world_lookup_class(world, class_name);
    uint64_t *test_result_class = smalltalk_world_lookup_class(world, "TestResult");
    Oop suite;
    Oop tests;
    Oop suite_size;
    Oop test_result;
    Oop run_count;
    Oop pass_count;
    Oop failure_count;
    Oop last_reason;
    Oop last_backtrace;

    ASSERT_EQ(ctx, suite_class != NULL, 1, "runtime: Smalltalk suite class available");
    ASSERT_EQ(ctx, test_result_class != NULL, 1, "runtime: TestResult class available for suite execution");
    suite = sw_send0(world, ctx, (Oop)suite_class, world->class_class, "suite");
    ASSERT_EQ(ctx, is_object_ptr(suite), 1, "runtime: Smalltalk suite builder returns a suite");
    smalltalk_world_put_global(world, "CurrentSmalltalkSuite", suite);
    suite = smalltalk_world_lookup_global(world, "CurrentSmalltalkSuite");
    ASSERT_EQ(ctx, is_object_ptr(suite), 1, "runtime: Smalltalk suite stays rooted");
    tests = sw_send0(world, ctx, suite, NULL, "tests");
    smalltalk_world_put_global(world, "CurrentSmalltalkSuiteTests", tests);
    tests = smalltalk_world_lookup_global(world, "CurrentSmalltalkSuiteTests");
    suite_size = sw_send0(world, ctx, suite, NULL, "size");
    ASSERT_EQ(ctx, is_object_ptr(tests), 1, "runtime: suite exposes test storage");
    ASSERT_EQ(ctx, suite_size, tag_smallint(expected_tests), "runtime: suite size matches expectation");
    test_result = sw_send0(world, ctx, (Oop)test_result_class, world->class_class, "new");
    ASSERT_EQ(ctx, is_object_ptr(test_result), 1, "runtime: suite TestResult exists");
    smalltalk_world_put_global(world, "CurrentSmalltalkSuiteResult", test_result);
    test_result = smalltalk_world_lookup_global(world, "CurrentSmalltalkSuiteResult");
    ASSERT_EQ(ctx, is_object_ptr(test_result), 1, "runtime: suite TestResult stays rooted");

    for (int index = 1; index <= expected_tests; index++)
    {
        Oop test_case = sw_send1(world, ctx, tests, NULL, "at:", tag_smallint(index));
        ASSERT_EQ(ctx, is_object_ptr(test_case), 1, "runtime: suite test case exists");
        sw_send1(world, ctx, test_case, NULL, "runOn:", test_result);
    }

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
    OopRootSet roots = {0};
    uint64_t generator_root = oop_roots_add(&roots, (Oop)generator);
    uint64_t bytecodes_root = UINT64_MAX;
    uint64_t literals_root = UINT64_MAX;
    uint64_t method_root = UINT64_MAX;
    uint64_t *code_generator_class = smalltalk_world_lookup_class(world, "CodeGenerator");
    uint64_t *generator_ptr = oop_roots_ptr(&roots, generator_root);
    int64_t bytecode_count = untag_smallint(OBJ_FIELD(generator_ptr, 1));
    int64_t literal_count = untag_smallint(OBJ_FIELD(generator_ptr, 3));
    int64_t temp_count = untag_smallint(OBJ_FIELD(generator_ptr, 5));
    int64_t arg_count = untag_smallint(OBJ_FIELD(generator_ptr, 7));
    int64_t primitive_index = untag_smallint(OBJ_FIELD(generator_ptr, 11));
    if (bytecode_count < 0 || literal_count < 0 || temp_count < 0 || arg_count < 0)
    {
        return NULL;
    }

    uint64_t *bytecodes = om_alloc(world->om, (uint64_t)world->class_class,
                                   FORMAT_BYTES, (uint64_t)bytecode_count);
    if (bytecodes == NULL)
    {
        return NULL;
    }
    bytecodes_root = oop_roots_add(&roots, (Oop)bytecodes);
    generator_ptr = oop_roots_ptr(&roots, generator_root);
    bytecodes = oop_roots_ptr(&roots, bytecodes_root);
    memcpy(&OBJ_FIELD(bytecodes, 0),
           &OBJ_FIELD((uint64_t *)OBJ_FIELD(generator_ptr, 0), 0),
           (size_t)bytecode_count);

    uint64_t *literals = NULL;
    if (literal_count > 0)
    {
        literals = om_alloc(world->om, (uint64_t)world->class_class,
                            FORMAT_INDEXABLE, (uint64_t)literal_count);
        if (literals == NULL)
        {
            return NULL;
        }
        literals_root = oop_roots_add(&roots, (Oop)literals);
        generator_ptr = oop_roots_ptr(&roots, generator_root);
        literals = oop_roots_ptr(&roots, literals_root);
        for (int64_t index = 0; index < literal_count; index++)
        {
            Oop literal = OBJ_FIELD((uint64_t *)OBJ_FIELD(generator_ptr, 2), index);
            if (is_object_ptr(literal) &&
                code_generator_class != NULL &&
                OBJ_CLASS((uint64_t *)literal) == (uint64_t)code_generator_class)
            {
                literal = (Oop)materialize_codegen_method(world, (uint64_t *)literal);
                if (!is_object_ptr(literal))
                {
                    return NULL;
                }
                generator_ptr = oop_roots_ptr(&roots, generator_root);
                literals = oop_roots_ptr(&roots, literals_root);
            }
            OBJ_FIELD(literals, index) = literal;
        }
    }

    uint64_t *method = om_alloc(world->om, (uint64_t)world->class_class, FORMAT_FIELDS, 6);
    if (method == NULL)
    {
        return NULL;
    }
    method_root = oop_roots_add(&roots, (Oop)method);
    method = oop_roots_ptr(&roots, method_root);
    bytecodes = oop_roots_ptr(&roots, bytecodes_root);
    literals = literals_root == UINT64_MAX ? NULL : oop_roots_ptr(&roots, literals_root);
    OBJ_FIELD(method, CM_PRIMITIVE) = tag_smallint(primitive_index >= 0 ? primitive_index : PRIM_NONE);
    OBJ_FIELD(method, CM_NUM_ARGS) = tag_smallint(arg_count);
    OBJ_FIELD(method, CM_NUM_TEMPS) = tag_smallint(temp_count);
    OBJ_FIELD(method, CM_LITERALS) = literals != NULL ? (uint64_t)literals : tagged_nil();
    OBJ_FIELD(method, CM_BYTECODES) = (uint64_t)bytecodes;
    OBJ_FIELD(method, CM_SOURCE) = tagged_nil();
    return method;
}

static uint64_t *compile_smalltalk_method(SmalltalkWorld *world,
                                          TestContext *ctx,
                                          uint64_t *compiler_class,
                                          const char *method_source)
{
    uint64_t source = (uint64_t)sw_make_string(world, method_source);
    uint64_t generator = sw_send1(world, ctx, (uint64_t)compiler_class, world->class_class,
                                  "compileMethod:", source);

    if (!is_object_ptr(generator))
    {
        return NULL;
    }
    return materialize_codegen_method(world, (uint64_t *)generator);
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

    static BCompiledMethodDef methods[1];
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
    static uint8_t world_buf[32 * 1024 * 1024] __attribute__((aligned(8)));
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
    ASSERT_EQ(ctx, smalltalk_world_install_st_file(&world, "src/smalltalk/Association.st"), 1,
              "runtime: Association.st installs methods onto the existing Association class");
    ASSERT_EQ(ctx, smalltalk_world_install_st_file(&world, "src/smalltalk/Dictionary.st"), 1,
              "runtime: Dictionary.st installs methods onto the existing Dictionary class");
    ASSERT_EQ(ctx, smalltalk_world_install_st_file(&world, "src/smalltalk/Context.st"),
              1, "runtime: Context.st installs methods onto the existing Context class");
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
    ASSERT_EQ(ctx, OBJ_SIZE(method_gen_ptr), 12,
              "runtime: method CodeGenerator has expected ivar slots");
    ASSERT_EQ(ctx, OBJ_FIELD(method_gen_ptr, 1), tag_smallint(7),
              "runtime: Smalltalk compiler emits explicit return plus trailing method return");
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
            {"1 + 2", 0, tag_smallint(3), NULL},
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
    {
        RuntimeExpressionSpec specs[128];
        int spec_count = 0;
        uint64_t *expr_spec_class;
        uint64_t *expr_helper_method;
        uint64_t *expr_helper_delegate_method;

        ASSERT_EQ(ctx,
                  runtime_load_expression_specs("tests/ExpressionSpecs.txt", specs, 128, &spec_count),
                  1,
                  "runtime: ExpressionSpecs.txt loads for Smalltalk compiler corpus");
        expr_spec_class = smalltalk_world_define_class(&world, "ExprSpec", world.object_class, NULL, 0, FORMAT_FIELDS);
        ASSERT_EQ(ctx, expr_spec_class != NULL, 1, "runtime: ExprSpec class defined for Smalltalk compiler corpus");
        {
            uint64_t *live_parser_class = smalltalk_world_lookup_class(&world, "Parser");
            uint64_t parser = sw_send1(&world, ctx, (uint64_t)live_parser_class, world.class_class,
                                       "on:", (uint64_t)sw_make_string(&world, "bar ^ self foo"));
            uint64_t method_ast = sw_send0(&world, ctx, parser, NULL, "parseMethod");
            uint64_t temporaries = sw_send0(&world, ctx, method_ast, NULL, "temporaries");
            ASSERT_EQ(ctx, sw_send0(&world, ctx, temporaries, NULL, "size"), tag_smallint(0),
                      "runtime: Parser parseMethod keeps unary return method temporaries empty");
        }

        expr_helper_method = compile_smalltalk_method(&world, ctx, compiler_class,
                                                      "foo\n"
                                                      "    ^ 7");
        ASSERT_EQ(ctx, expr_helper_method != NULL, 1,
                  "runtime: Smalltalk compiler compiles ExprSpec>>foo");
        install_rooted_method(ctx, &world, "ExprSpec", "foo",
                              "CurrentCompilerInstalledMethod", (Oop)expr_helper_method);

        expr_helper_delegate_method = compile_smalltalk_method(&world, ctx, compiler_class,
                                                               "bar\n"
                                                               "    ^ self foo");
        ASSERT_EQ(ctx, expr_helper_delegate_method != NULL, 1,
                  "runtime: Smalltalk compiler compiles ExprSpec>>bar");
        install_rooted_method(ctx, &world, "ExprSpec", "bar",
                              "CurrentCompilerInstalledMethod", (Oop)expr_helper_delegate_method);
        {
            uint64_t *helper_receiver = om_alloc(world.om, (uint64_t)expr_spec_class, FORMAT_FIELDS, 0);
            ASSERT_EQ(ctx, helper_receiver != NULL, 1, "runtime: helper ExprSpec receiver allocates");
            ASSERT_EQ(ctx, sw_send0(&world, ctx, (uint64_t)helper_receiver, expr_spec_class, "foo"),
                      tag_smallint(7), "runtime: ExprSpec>>foo executes");
            ASSERT_EQ(ctx, sw_send0(&world, ctx, (uint64_t)helper_receiver, expr_spec_class, "bar"),
                      tag_smallint(7), "runtime: ExprSpec>>bar executes");
        }

        for (int index = 0; index < spec_count; index++)
        {
            char selector[32];
            char method_source_buf[1024];
            uint64_t *expr_method;
            uint64_t *expr_receiver;
            uint64_t expr_result;

            snprintf(selector, sizeof(selector), "expr%d", index + 1);
            snprintf(method_source_buf, sizeof(method_source_buf),
                     "%s\n"
                     "    ^ %s",
                     selector, specs[index].expression);
            expr_method = compile_smalltalk_method(&world, ctx, compiler_class, method_source_buf);
            ASSERT_EQ(ctx, expr_method != NULL, 1, specs[index].name);
            install_rooted_method(ctx, &world, "ExprSpec", selector,
                                  "CurrentCompilerInstalledMethod", (Oop)expr_method);

            expr_receiver = om_alloc(world.om, (uint64_t)expr_spec_class, FORMAT_FIELDS, 0);
            ASSERT_EQ(ctx, expr_receiver != NULL, 1, "runtime: ExprSpec receiver allocates");
            expr_result = sw_send0(&world, ctx, (uint64_t)expr_receiver, expr_spec_class, selector);

            if (specs[index].expected_kind == RUNTIME_EXPR_EXPECT_SMALLINT)
            {
                ASSERT_EQ(ctx, expr_result, tag_smallint(specs[index].expected_smallint), specs[index].name);
            }
            else if (specs[index].expected_kind == RUNTIME_EXPR_EXPECT_TRUE)
            {
                ASSERT_EQ(ctx, expr_result, tagged_true(), specs[index].name);
            }
            else
            {
                ASSERT_EQ(ctx, expr_result, tagged_false(), specs[index].name);
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
    {
        uint64_t *visit_assignment_method = compile_smalltalk_method(
            &world, ctx, compiler_class,
            "visitAssignment: aNode\n"
            "    self visitNode: aNode value.\n"
            "    self emitDuplicate.\n"
            "    self storeVariable: aNode variable name.\n"
            "    ^ self");
        uint64_t assignment_gen;
        uint64_t temp_names;
        uint64_t x_string;
        uint64_t variable_node;
        uint64_t literal_node;
        uint64_t assignment_node;
        uint64_t assignment_result;

        ASSERT_EQ(ctx, visit_assignment_method != NULL, 1,
                  "runtime: Smalltalk compiler compiles real CodeGenerator>>visitAssignment:");
        install_rooted_method(ctx, &world, "CodeGenerator", "visitAssignment:",
                              "CurrentCompilerInstalledMethod", (Oop)visit_assignment_method);
        assignment_gen = sw_send0(&world, ctx, (uint64_t)smalltalk_world_lookup_class(&world, "CodeGenerator"),
                                  world.class_class, "new");
        ASSERT_EQ(ctx, is_object_ptr(assignment_gen), 1,
                  "runtime: CodeGenerator new allocates receiver for compiler-installed visitAssignment:");
        temp_names = sw_send1(&world, ctx, (uint64_t)world.array_class, world.class_class,
                              "new:", tag_smallint(1));
        ASSERT_EQ(ctx, is_object_ptr(temp_names), 1,
                  "runtime: Array new: allocates temp name storage for compiler-installed visitAssignment:");
        x_string = (uint64_t)sw_make_string(&world, "x");
        (void)sw_send2(&world, ctx, temp_names, world.array_class, "at:put:",
                       tag_smallint(1), x_string);
        ASSERT_EQ(ctx, byte_object_equals_cstring(
                              sw_send1(&world, ctx, temp_names, world.array_class, "at:", tag_smallint(1)),
                              "x"),
                  1,
                  "runtime: temp names array stores assignment variable");
        ASSERT_EQ(ctx, sw_send1(&world, ctx, assignment_gen, NULL, "temporaries:", temp_names),
                  assignment_gen,
                  "runtime: compiler-installed visitAssignment: configures temporaries");
        variable_node = sw_send1(&world, ctx, (uint64_t)smalltalk_world_lookup_class(&world, "VariableNode"),
                                 world.class_class, "name:", x_string);
        literal_node = sw_send1(&world, ctx, (uint64_t)smalltalk_world_lookup_class(&world, "LiteralNode"),
                                world.class_class, "value:", tag_smallint(1));
        assignment_node = sw_send2(&world, ctx,
                                   (uint64_t)smalltalk_world_lookup_class(&world, "AssignmentNode"),
                                   world.class_class, "variable:value:", variable_node, literal_node);
        ASSERT_EQ(ctx, is_object_ptr(assignment_node), 1,
                  "runtime: AssignmentNode allocates input AST for compiler-installed visitAssignment:");
        assignment_result = sw_send1(&world, ctx, assignment_gen,
                                     smalltalk_world_lookup_class(&world, "CodeGenerator"),
                                     "visitAssignment:", assignment_node);
        ASSERT_EQ(ctx, assignment_result, assignment_gen,
                  "runtime: compiler-installed CodeGenerator>>visitAssignment: returns self");
        ASSERT_EQ(ctx, sw_send0(&world, ctx, assignment_gen,
                                smalltalk_world_lookup_class(&world, "CodeGenerator"),
                                "bytecodeCount"),
                  tag_smallint(11),
                  "runtime: compiler-installed CodeGenerator>>visitAssignment: emits literal push, dup, and store");
        ASSERT_EQ(ctx, sw_send0(&world, ctx, assignment_gen,
                                smalltalk_world_lookup_class(&world, "CodeGenerator"),
                                "literalCount"),
                  tag_smallint(1),
                  "runtime: compiler-installed CodeGenerator>>visitAssignment: records assignment literal");
    }
    {
        uint64_t *visit_statements_method = compile_smalltalk_method(
            &world, ctx, compiler_class,
            "visitStatements: stmts from: index last: lastIndex\n"
            "    | stmt |\n"
            "    stmt := stmts at: index.\n"
            "    self visitNode: stmt.\n"
            "    index = lastIndex\n"
            "        ifTrue: [^ self]\n"
            "        ifFalse: [\n"
            "            self emitPop.\n"
            "            ^ self visitStatements: stmts from: index + 1 last: lastIndex\n"
            "        ]");
        uint64_t sequence_gen;
        uint64_t stmts;
        uint64_t literal_one_node;
        uint64_t literal_two_node;
        uint64_t sequence_node;
        uint64_t sequence_result;

        ASSERT_EQ(ctx, visit_statements_method != NULL, 1,
                  "runtime: Smalltalk compiler compiles real CodeGenerator>>visitStatements:from:last:");
        install_rooted_method(ctx, &world, "CodeGenerator", "visitStatements:from:last:",
                              "CurrentCompilerInstalledMethod", (Oop)visit_statements_method);
        sequence_gen = sw_send0(&world, ctx, (uint64_t)smalltalk_world_lookup_class(&world, "CodeGenerator"),
                                world.class_class, "new");
        ASSERT_EQ(ctx, is_object_ptr(sequence_gen), 1,
                  "runtime: CodeGenerator new allocates receiver for compiler-installed visitStatements:from:last:");
        literal_one_node = sw_send1(&world, ctx, (uint64_t)smalltalk_world_lookup_class(&world, "LiteralNode"),
                                    world.class_class, "value:", tag_smallint(1));
        literal_two_node = sw_send1(&world, ctx, (uint64_t)smalltalk_world_lookup_class(&world, "LiteralNode"),
                                    world.class_class, "value:", tag_smallint(2));
        stmts = sw_send2(&world, ctx, (uint64_t)world.array_class, world.class_class, "with:with:",
                         literal_one_node, literal_two_node);
        ASSERT_EQ(ctx, is_object_ptr(stmts), 1,
                  "runtime: Array with:with: builds statement list for compiler-installed visitStatements:from:last:");
        sequence_node = sw_send1(&world, ctx, (uint64_t)smalltalk_world_lookup_class(&world, "SequenceNode"),
                                 world.class_class, "statements:", stmts);
        ASSERT_EQ(ctx, is_object_ptr(sequence_node), 1,
                  "runtime: SequenceNode statements: builds input AST for compiler-installed visitStatements:from:last:");
        sequence_result = sw_send1(&world, ctx, sequence_gen,
                                   smalltalk_world_lookup_class(&world, "CodeGenerator"),
                                   "visitSequence:", sequence_node);
        ASSERT_EQ(ctx, sequence_result, sequence_gen,
                  "runtime: compiler-installed CodeGenerator>>visitStatements:from:last: returns self through visitSequence:");
        ASSERT_EQ(ctx, sw_send0(&world, ctx, sequence_gen,
                                smalltalk_world_lookup_class(&world, "CodeGenerator"),
                                "bytecodeCount"),
                  tag_smallint(11),
                  "runtime: compiler-installed CodeGenerator>>visitStatements:from:last: emits pop between statements");
        ASSERT_EQ(ctx, sw_send0(&world, ctx, sequence_gen,
                                smalltalk_world_lookup_class(&world, "CodeGenerator"),
                                "literalCount"),
                  tag_smallint(2),
                  "runtime: compiler-installed CodeGenerator>>visitStatements:from:last: records both literals");
    }
    {
        uint64_t *visit_return_method = compile_smalltalk_method(
            &world, ctx, compiler_class,
            "visitReturn: aNode\n"
            "    self visitNode: aNode expression.\n"
            "    inBlock\n"
            "        ifTrue: [self emitReturnNonLocal]\n"
            "        ifFalse: [self emitReturn].\n"
            "    ^ self");
        uint64_t return_gen;
        uint64_t return_expression;
        uint64_t return_node;
        uint64_t return_result;

        ASSERT_EQ(ctx, visit_return_method != NULL, 1,
                  "runtime: Smalltalk compiler compiles real CodeGenerator>>visitReturn:");
        install_rooted_method(ctx, &world, "CodeGenerator", "visitReturn:",
                              "CurrentCompilerInstalledMethod", (Oop)visit_return_method);
        return_gen = sw_send0(&world, ctx, (uint64_t)smalltalk_world_lookup_class(&world, "CodeGenerator"),
                              world.class_class, "new");
        ASSERT_EQ(ctx, is_object_ptr(return_gen), 1,
                  "runtime: CodeGenerator new allocates receiver for compiler-installed visitReturn:");
        return_expression = sw_send1(&world, ctx, (uint64_t)smalltalk_world_lookup_class(&world, "LiteralNode"),
                                     world.class_class, "value:", tag_smallint(1));
        return_node = sw_send1(&world, ctx, (uint64_t)smalltalk_world_lookup_class(&world, "ReturnNode"),
                               world.class_class, "expression:", return_expression);
        ASSERT_EQ(ctx, is_object_ptr(return_node), 1,
                  "runtime: ReturnNode allocates input AST for compiler-installed visitReturn:");
        return_result = sw_send1(&world, ctx, return_gen,
                                 smalltalk_world_lookup_class(&world, "CodeGenerator"),
                                 "visitReturn:", return_node);
        ASSERT_EQ(ctx, return_result, return_gen,
                  "runtime: compiler-installed CodeGenerator>>visitReturn: returns self");
        ASSERT_EQ(ctx, sw_send0(&world, ctx, return_gen,
                                smalltalk_world_lookup_class(&world, "CodeGenerator"),
                                "bytecodeCount"),
                  tag_smallint(6),
                  "runtime: compiler-installed CodeGenerator>>visitReturn: emits literal push and return");
        ASSERT_EQ(ctx, sw_send0(&world, ctx, return_gen,
                                smalltalk_world_lookup_class(&world, "CodeGenerator"),
                                "literalCount"),
                  tag_smallint(1),
                  "runtime: compiler-installed CodeGenerator>>visitReturn: records return literal");
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
    ASSERT_EQ(ctx, smalltalk_world_install_class_file(&world, "src/smalltalk/TestFailure.st") != NULL,
              1, "runtime: TestFailure.st defines class and installs methods");
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
    ASSERT_EQ(ctx, smalltalk_world_install_class_file(&world, "src/smalltalk/Transaction.st") != NULL,
              1, "runtime: Transaction.st defines class and installs methods");
    ASSERT_EQ(ctx, smalltalk_world_install_class_file(&world, "src/smalltalk/Image.st") != NULL,
              1, "runtime: Image.st defines class and installs methods");
    {
        uint64_t *transaction_class = smalltalk_world_lookup_class(&world, "Transaction");
        uint64_t *image_class = smalltalk_world_lookup_class(&world, "Image");
        ASSERT_EQ(ctx, transaction_class != NULL, 1, "runtime: Transaction in Smalltalk dict");
        ASSERT_EQ(ctx, image_class != NULL, 1, "runtime: Image in Smalltalk dict");
        ASSERT_EQ(ctx, class_lookup((uint64_t *)OBJ_CLASS(transaction_class),
                                    intern_cstring_symbol(world.om, "atomic:")) != 0,
                  1, "runtime: Transaction has class-side atomic:");
        ASSERT_EQ(ctx, class_lookup((uint64_t *)OBJ_CLASS(transaction_class),
                                    intern_cstring_symbol(world.om, "readOnly:")) != 0,
                  1, "runtime: Transaction has class-side readOnly:");
        ASSERT_EQ(ctx, class_lookup((uint64_t *)OBJ_CLASS(image_class),
                                    intern_cstring_symbol(world.om, "checkpointTo:")) != 0,
                  1, "runtime: Image has class-side checkpointTo:");
        ASSERT_EQ(ctx, class_lookup((uint64_t *)OBJ_CLASS(image_class),
                                    intern_cstring_symbol(world.om, "restartFrom:valueOfGlobal:")) != 0,
                  1, "runtime: Image has class-side restartFrom:valueOfGlobal:");
#ifdef ALO_INTERPRETER_C
        {
            const char *checkpoint_path = "/tmp/arlo_incremental_pages.image";
            static uint8_t checkpoint_world_buf[8 * 1024 * 1024] __attribute__((aligned(8)));
            SmalltalkWorld checkpoint_world;
            TestContext checkpoint_ctx = *ctx;
            uint64_t *image_class;
            uint64_t *probe_clean = NULL;
            uint64_t *page_filler = NULL;
            uint64_t *probe_dirty = NULL;
            uint64_t clean_page;
            uint64_t dirty_page;
            uint64_t filler_fields;
            Oop checkpoint_path_oop;
            FILE *file;
            RuntimeCheckpointHeader header;
            RuntimeCheckpointHeader second_header;
            uint8_t clean_before[OM_PAGE_BYTES];
            uint8_t clean_after[OM_PAGE_BYTES];
            uint8_t dirty_before[OM_PAGE_BYTES];
            uint8_t dirty_after[OM_PAGE_BYTES];
            uint64_t target_page;

            unlink(checkpoint_path);
            smalltalk_world_init(&checkpoint_world, checkpoint_world_buf, sizeof(checkpoint_world_buf));
            ASSERT_EQ(ctx, smalltalk_world_install_class_file(&checkpoint_world, "src/smalltalk/Exception.st") != NULL,
                      1, "runtime: incremental checkpoint world installs Exception");
            ASSERT_EQ(ctx, smalltalk_world_install_class_file(&checkpoint_world, "src/smalltalk/Error.st") != NULL,
                      1, "runtime: incremental checkpoint world installs Error");
            ASSERT_EQ(ctx, smalltalk_world_install_class_file(&checkpoint_world, "src/smalltalk/Image.st") != NULL,
                      1, "runtime: incremental checkpoint world installs Image");
            image_class = smalltalk_world_lookup_class(&checkpoint_world, "Image");
            ASSERT_EQ(ctx, image_class != NULL, 1, "runtime: incremental checkpoint world has Image");

            target_page = runtime_first_free_tail_page(checkpoint_world.om);
            ASSERT_EQ(ctx, target_page != UINT64_MAX, 1,
                      "runtime: found a free tail page for incremental checkpoint probes");
            checkpoint_world.om[0] = om_page_start(checkpoint_world.om, target_page);
            probe_clean = om_alloc(checkpoint_world.om, (uint64_t)checkpoint_world.class_class, FORMAT_FIELDS, 1);
            ASSERT_EQ(ctx, probe_clean != NULL, 1, "runtime: clean-page probe allocated");
            clean_page = om_page_id_for_address(checkpoint_world.om, (uint64_t)probe_clean);
            ASSERT_EQ(ctx, clean_page, target_page, "runtime: clean-page probe lands on reserved page");
            filler_fields = (OM_PAGE_BYTES - om_page_used_bytes(checkpoint_world.om, clean_page)) / WORD_BYTES;
            ASSERT_EQ(ctx, filler_fields > 5, 1,
                      "runtime: reserved clean page has room for filler and dirty probe");
            filler_fields -= 5;
            page_filler = om_alloc(checkpoint_world.om, (uint64_t)checkpoint_world.class_class, FORMAT_FIELDS, filler_fields);
            ASSERT_EQ(ctx, page_filler != NULL, 1, "runtime: clean page filler allocated");
            probe_dirty = om_alloc(checkpoint_world.om, (uint64_t)checkpoint_world.class_class, FORMAT_FIELDS, 1);
            ASSERT_EQ(ctx, probe_dirty != NULL, 1, "runtime: dirty-page probe allocated");

            dirty_page = om_page_id_for_address(checkpoint_world.om, (uint64_t)probe_dirty);
            ASSERT_EQ(ctx, dirty_page, clean_page + 1,
                      "runtime: dirty probe lands on following page");

            OBJ_FIELD(probe_clean, 0) = tag_smallint(111);
            OBJ_FIELD(probe_dirty, 0) = tag_smallint(222);
            om_mark_object_dirty(checkpoint_world.om, probe_clean);
            om_mark_object_dirty(checkpoint_world.om, page_filler);
            om_mark_object_dirty(checkpoint_world.om, probe_dirty);

            checkpoint_path_oop = (Oop)sw_make_string(&checkpoint_world, checkpoint_path);
            ASSERT_EQ(ctx, sw_send1(&checkpoint_world, &checkpoint_ctx, (Oop)image_class, checkpoint_world.class_class,
                                    "checkpointTo:", checkpoint_path_oop),
                      checkpoint_path_oop,
                      "runtime: first checkpoint succeeds");
            ASSERT_EQ(ctx, image_checkpoint_validate(checkpoint_path), 1,
                      "runtime: first incremental checkpoint validates");

            file = fopen(checkpoint_path, "rb");
            ASSERT_EQ(ctx, file != NULL, 1, "runtime: checkpoint file created");
            ASSERT_EQ(ctx, fread(&header, sizeof(header), 1, file), (size_t)1,
                      "runtime: checkpoint header readable");
            ASSERT_EQ(ctx, header.generation, (uint64_t)1,
                      "runtime: first checkpoint generation starts at one");
            ASSERT_EQ(ctx, fseek(file, (long)runtime_checkpoint_page_data_offset(header.page_count, clean_page), SEEK_SET), 0,
                      "runtime: seek to clean page body succeeds");
            ASSERT_EQ(ctx, fread(clean_before, 1, OM_PAGE_BYTES, file), (size_t)OM_PAGE_BYTES,
                      "runtime: clean page body readable");
            ASSERT_EQ(ctx, fseek(file, (long)runtime_checkpoint_page_data_offset(header.page_count, dirty_page), SEEK_SET), 0,
                      "runtime: seek to dirty page body succeeds");
            ASSERT_EQ(ctx, fread(dirty_before, 1, OM_PAGE_BYTES, file), (size_t)OM_PAGE_BYTES,
                      "runtime: dirty page body readable");
            fclose(file);

            OBJ_FIELD(probe_dirty, 0) = tag_smallint(999);
            om_mark_object_dirty(checkpoint_world.om, probe_dirty);
            ASSERT_EQ(ctx, om_dirty_page_count(checkpoint_world.om), (uint64_t)1,
                      "runtime: only the changed page is dirty before second checkpoint");

            ASSERT_EQ(ctx, sw_send1(&checkpoint_world, &checkpoint_ctx, (Oop)image_class, checkpoint_world.class_class,
                                    "checkpointTo:", checkpoint_path_oop),
                      checkpoint_path_oop,
                      "runtime: second checkpoint succeeds");
            ASSERT_EQ(ctx, om_dirty_page_count(checkpoint_world.om), (uint64_t)0,
                      "runtime: checkpoint clears dirty pages");
            ASSERT_EQ(ctx, image_checkpoint_validate(checkpoint_path), 1,
                      "runtime: second incremental checkpoint validates");

            file = fopen(checkpoint_path, "rb");
            ASSERT_EQ(ctx, file != NULL, 1, "runtime: second checkpoint file readable");
            ASSERT_EQ(ctx, fread(&second_header, sizeof(second_header), 1, file), (size_t)1,
                      "runtime: second checkpoint header readable");
            ASSERT_EQ(ctx, second_header.generation, header.generation + 1,
                      "runtime: second checkpoint increments generation");
            ASSERT_EQ(ctx, fseek(file, (long)runtime_checkpoint_page_data_offset(second_header.page_count, clean_page), SEEK_SET), 0,
                      "runtime: seek to clean page body after second checkpoint succeeds");
            ASSERT_EQ(ctx, fread(clean_after, 1, OM_PAGE_BYTES, file), (size_t)OM_PAGE_BYTES,
                      "runtime: clean page body after second checkpoint readable");
            ASSERT_EQ(ctx, fseek(file, (long)runtime_checkpoint_page_data_offset(second_header.page_count, dirty_page), SEEK_SET), 0,
                      "runtime: seek to dirty page body after second checkpoint succeeds");
            ASSERT_EQ(ctx, fread(dirty_after, 1, OM_PAGE_BYTES, file), (size_t)OM_PAGE_BYTES,
                      "runtime: dirty page body after second checkpoint readable");
            fclose(file);

            ASSERT_EQ(ctx, memcmp(clean_before, clean_after, OM_PAGE_BYTES), 0,
                      "runtime: clean page body unchanged across incremental checkpoint");
            ASSERT_EQ(ctx, memcmp(dirty_before, dirty_after, OM_PAGE_BYTES) != 0, 1,
                      "runtime: dirty page body rewritten across incremental checkpoint");
            unlink(checkpoint_path);
            smalltalk_world_teardown(&checkpoint_world);
        }

        {
            const char *checkpoint_path = "/tmp/arlo_incremental_multipage.image";
            static uint8_t checkpoint_world_buf[8 * 1024 * 1024] __attribute__((aligned(8)));
            SmalltalkWorld checkpoint_world;
            TestContext checkpoint_ctx = *ctx;
            uint64_t *image_class;
            uint64_t *clean_probe = NULL;
            uint64_t *clean_filler = NULL;
            uint64_t *multi_fields = NULL;
            uint64_t clean_page;
            uint64_t first_multi_page;
            uint64_t last_multi_page;
            uint64_t filler_fields;
            Oop checkpoint_path_oop;
            FILE *file;
            RuntimeCheckpointHeader header;
            RuntimeCheckpointHeader second_header;
            uint8_t clean_before[OM_PAGE_BYTES];
            uint8_t clean_after[OM_PAGE_BYTES];
            uint8_t multi_first_before[OM_PAGE_BYTES];
            uint8_t multi_first_after[OM_PAGE_BYTES];
            uint8_t multi_last_before[OM_PAGE_BYTES];
            uint8_t multi_last_after[OM_PAGE_BYTES];
            uint64_t target_page;

            unlink(checkpoint_path);
            smalltalk_world_init(&checkpoint_world, checkpoint_world_buf, sizeof(checkpoint_world_buf));
            ASSERT_EQ(ctx, smalltalk_world_install_class_file(&checkpoint_world, "src/smalltalk/Exception.st") != NULL,
                      1, "runtime: multipage checkpoint world installs Exception");
            ASSERT_EQ(ctx, smalltalk_world_install_class_file(&checkpoint_world, "src/smalltalk/Error.st") != NULL,
                      1, "runtime: multipage checkpoint world installs Error");
            ASSERT_EQ(ctx, smalltalk_world_install_class_file(&checkpoint_world, "src/smalltalk/Image.st") != NULL,
                      1, "runtime: multipage checkpoint world installs Image");
            image_class = smalltalk_world_lookup_class(&checkpoint_world, "Image");
            ASSERT_EQ(ctx, image_class != NULL, 1, "runtime: multipage checkpoint world has Image");

            target_page = runtime_first_free_tail_page(checkpoint_world.om);
            ASSERT_EQ(ctx, target_page != UINT64_MAX, 1,
                      "runtime: found a free tail page before multipage object");
            checkpoint_world.om[0] = om_page_start(checkpoint_world.om, target_page);
            clean_probe = om_alloc(checkpoint_world.om, (uint64_t)checkpoint_world.class_class, FORMAT_FIELDS, 1);
            ASSERT_EQ(ctx, clean_probe != NULL, 1, "runtime: clean-page probe before multipage object allocated");
            clean_page = om_page_id_for_address(checkpoint_world.om, (uint64_t)clean_probe);
            ASSERT_EQ(ctx, clean_page, target_page, "runtime: clean-page probe lands on reserved page");
            filler_fields = (OM_PAGE_BYTES - om_page_used_bytes(checkpoint_world.om, clean_page)) / WORD_BYTES;
            ASSERT_EQ(ctx, filler_fields > 0, 1,
                      "runtime: reserved clean page has room before multipage object");
            clean_filler = om_alloc(checkpoint_world.om, (uint64_t)checkpoint_world.class_class, FORMAT_FIELDS, filler_fields);
            ASSERT_EQ(ctx, clean_filler != NULL, 1, "runtime: clean-page filler allocated");

            multi_fields = om_alloc(checkpoint_world.om, (uint64_t)checkpoint_world.class_class, FORMAT_FIELDS, 1200);
            ASSERT_EQ(ctx, multi_fields != NULL, 1, "runtime: multipage fields object allocated");
            ASSERT_EQ(ctx, om_object_spans_pages(checkpoint_world.om, multi_fields), (uint64_t)1,
                      "runtime: fields object spans multiple pages");

            OBJ_FIELD(clean_probe, 0) = tag_smallint(123);
            OBJ_FIELD(multi_fields, 0) = tag_smallint(1);
            OBJ_FIELD(multi_fields, 700) = tag_smallint(700);
            OBJ_FIELD(multi_fields, 1199) = tag_smallint(1199);

            first_multi_page = om_page_id_for_address(checkpoint_world.om, (uint64_t)multi_fields);
            last_multi_page = om_page_id_for_address(checkpoint_world.om,
                                                     ((uint64_t)multi_fields + om_object_bytes(multi_fields)) - 1);
            ASSERT_EQ(ctx, first_multi_page > clean_page, 1,
                      "runtime: multipage object starts after the unrelated clean page");
            ASSERT_EQ(ctx, last_multi_page > first_multi_page, 1,
                      "runtime: multipage object reaches a later continuation page");

            om_mark_object_dirty(checkpoint_world.om, clean_probe);
            om_mark_object_dirty(checkpoint_world.om, clean_filler);
            om_mark_object_dirty(checkpoint_world.om, multi_fields);

            checkpoint_path_oop = (Oop)sw_make_string(&checkpoint_world, checkpoint_path);
            ASSERT_EQ(ctx, sw_send1(&checkpoint_world, &checkpoint_ctx, (Oop)image_class, checkpoint_world.class_class,
                                    "checkpointTo:", checkpoint_path_oop),
                      checkpoint_path_oop,
                      "runtime: first multipage checkpoint succeeds");
            ASSERT_EQ(ctx, image_checkpoint_validate(checkpoint_path), 1,
                      "runtime: first multipage checkpoint validates");

            file = fopen(checkpoint_path, "rb");
            ASSERT_EQ(ctx, file != NULL, 1, "runtime: multipage checkpoint file created");
            ASSERT_EQ(ctx, fread(&header, sizeof(header), 1, file), (size_t)1,
                      "runtime: multipage checkpoint header readable");
            ASSERT_EQ(ctx, header.generation, (uint64_t)1,
                      "runtime: first multipage checkpoint generation starts at one");
            ASSERT_EQ(ctx, fseek(file, (long)runtime_checkpoint_page_data_offset(header.page_count, clean_page), SEEK_SET), 0,
                      "runtime: seek to clean page for multipage proof succeeds");
            ASSERT_EQ(ctx, fread(clean_before, 1, OM_PAGE_BYTES, file), (size_t)OM_PAGE_BYTES,
                      "runtime: clean page body before multipage change readable");
            ASSERT_EQ(ctx, fseek(file, (long)runtime_checkpoint_page_data_offset(header.page_count, first_multi_page), SEEK_SET), 0,
                      "runtime: seek to first multipage body succeeds");
            ASSERT_EQ(ctx, fread(multi_first_before, 1, OM_PAGE_BYTES, file), (size_t)OM_PAGE_BYTES,
                      "runtime: first multipage body before change readable");
            ASSERT_EQ(ctx, fseek(file, (long)runtime_checkpoint_page_data_offset(header.page_count, last_multi_page), SEEK_SET), 0,
                      "runtime: seek to last multipage body succeeds");
            ASSERT_EQ(ctx, fread(multi_last_before, 1, OM_PAGE_BYTES, file), (size_t)OM_PAGE_BYTES,
                      "runtime: last multipage body before change readable");
            fclose(file);

            OBJ_FIELD(multi_fields, 1199) = tag_smallint(4321);
            om_mark_field_dirty(checkpoint_world.om, multi_fields, 1199);
            ASSERT_EQ(ctx, om_page_is_dirty(checkpoint_world.om, clean_page), (uint64_t)0,
                      "runtime: unrelated clean page stays clean");
            ASSERT_EQ(ctx, om_page_is_dirty(checkpoint_world.om, first_multi_page), (uint64_t)0,
                      "runtime: untouched first multipage page stays clean");
            ASSERT_EQ(ctx, om_page_is_dirty(checkpoint_world.om, last_multi_page), (uint64_t)1,
                      "runtime: last multipage page marked dirty");
            ASSERT_EQ(ctx, om_dirty_page_count(checkpoint_world.om), (uint64_t)1,
                      "runtime: later multipage field dirties only one page");

            ASSERT_EQ(ctx, sw_send1(&checkpoint_world, &checkpoint_ctx, (Oop)image_class, checkpoint_world.class_class,
                                    "checkpointTo:", checkpoint_path_oop),
                      checkpoint_path_oop,
                      "runtime: second multipage checkpoint succeeds");
            ASSERT_EQ(ctx, image_checkpoint_validate(checkpoint_path), 1,
                      "runtime: second multipage checkpoint validates");

            file = fopen(checkpoint_path, "rb");
            ASSERT_EQ(ctx, file != NULL, 1, "runtime: second multipage checkpoint readable");
            ASSERT_EQ(ctx, fread(&second_header, sizeof(second_header), 1, file), (size_t)1,
                      "runtime: second multipage header readable");
            ASSERT_EQ(ctx, second_header.generation, header.generation + 1,
                      "runtime: second multipage checkpoint increments generation");
            ASSERT_EQ(ctx, fseek(file, (long)runtime_checkpoint_page_data_offset(second_header.page_count, clean_page), SEEK_SET), 0,
                      "runtime: seek to clean page after multipage change succeeds");
            ASSERT_EQ(ctx, fread(clean_after, 1, OM_PAGE_BYTES, file), (size_t)OM_PAGE_BYTES,
                      "runtime: clean page body after multipage change readable");
            ASSERT_EQ(ctx, fseek(file, (long)runtime_checkpoint_page_data_offset(second_header.page_count, first_multi_page), SEEK_SET), 0,
                      "runtime: seek to first multipage body after change succeeds");
            ASSERT_EQ(ctx, fread(multi_first_after, 1, OM_PAGE_BYTES, file), (size_t)OM_PAGE_BYTES,
                      "runtime: first multipage body after change readable");
            ASSERT_EQ(ctx, fseek(file, (long)runtime_checkpoint_page_data_offset(second_header.page_count, last_multi_page), SEEK_SET), 0,
                      "runtime: seek to last multipage body after change succeeds");
            ASSERT_EQ(ctx, fread(multi_last_after, 1, OM_PAGE_BYTES, file), (size_t)OM_PAGE_BYTES,
                      "runtime: last multipage body after change readable");
            fclose(file);

            ASSERT_EQ(ctx, memcmp(clean_before, clean_after, OM_PAGE_BYTES), 0,
                      "runtime: unrelated clean page remains byte-identical");
            ASSERT_EQ(ctx, memcmp(multi_last_before, multi_last_after, OM_PAGE_BYTES) != 0, 1,
                      "runtime: tail continuation page of multipage object rewritten");
            unlink(checkpoint_path);
            smalltalk_world_teardown(&checkpoint_world);
        }
#endif
    }

    ASSERT_EQ(ctx, smalltalk_world_install_class_file(&world, "tests/fixtures/ContextTest.st") != NULL,
              1, "runtime: ContextTest.st defines class and installs methods");
    ASSERT_EQ(ctx, smalltalk_world_install_class_file(&world, "tests/fixtures/BlockActivationTest.st") != NULL,
              1, "runtime: BlockActivationTest.st defines class and installs methods");
    ASSERT_EQ(ctx, smalltalk_world_install_class_file(&world, "tests/fixtures/DefaultActionException.st") != NULL,
              1, "runtime: DefaultActionException.st defines class and installs methods");
    ASSERT_EQ(ctx, smalltalk_world_install_class_file(&world, "tests/fixtures/MultipleFailureTest.st") != NULL,
              1, "runtime: MultipleFailureTest.st defines class and installs methods");
    ASSERT_EQ(ctx, smalltalk_world_define_class(&world, "CompilerTest",
                                                smalltalk_world_lookup_class(&world, "TestCase"),
                                                NULL, 0, FORMAT_FIELDS) != NULL,
              1, "runtime: CompilerTest class defined");
    ASSERT_EQ(ctx, smalltalk_world_install_class_file(&world, "tests/fixtures/ExceptionHandlingTest.st") != NULL,
              1, "runtime: ExceptionHandlingTest.st defines class and installs methods");
    ASSERT_EQ(ctx, smalltalk_world_install_class_file(&world, "tests/fixtures/TransactionTest.st") != NULL,
              1, "runtime: TransactionTest.st defines class and installs methods");
    ASSERT_EQ(ctx, smalltalk_world_install_class_file(&world, "tests/fixtures/DurableTransactionTest.st") != NULL,
              1, "runtime: DurableTransactionTest.st defines class and installs methods");
    {
        uint64_t *context_test_class = smalltalk_world_lookup_class(&world, "ContextTest");
        uint64_t *block_activation_test_class = smalltalk_world_lookup_class(&world, "BlockActivationTest");
        uint64_t *default_action_exception_class = smalltalk_world_lookup_class(&world, "DefaultActionException");
        uint64_t *multiple_failure_test_class = smalltalk_world_lookup_class(&world, "MultipleFailureTest");
        uint64_t *compiler_test_class = smalltalk_world_lookup_class(&world, "CompilerTest");
        uint64_t *exception_handling_test_class = smalltalk_world_lookup_class(&world, "ExceptionHandlingTest");
        uint64_t *transaction_test_class = smalltalk_world_lookup_class(&world, "TransactionTest");
        uint64_t *durable_transaction_test_class = smalltalk_world_lookup_class(&world, "DurableTransactionTest");
        uint64_t *exception_class = smalltalk_world_lookup_class(&world, "Exception");
        uint64_t *error_class = smalltalk_world_lookup_class(&world, "Error");
        uint64_t *test_failure_class = smalltalk_world_lookup_class(&world, "TestFailure");
        ASSERT_EQ(ctx, context_test_class != NULL, 1, "runtime: ContextTest in Smalltalk dict");
        ASSERT_EQ(ctx, block_activation_test_class != NULL, 1, "runtime: BlockActivationTest in Smalltalk dict");
        ASSERT_EQ(ctx, default_action_exception_class != NULL, 1, "runtime: DefaultActionException in Smalltalk dict");
        ASSERT_EQ(ctx, multiple_failure_test_class != NULL, 1, "runtime: MultipleFailureTest in Smalltalk dict");
        ASSERT_EQ(ctx, compiler_test_class != NULL, 1, "runtime: CompilerTest in Smalltalk dict");
        ASSERT_EQ(ctx, exception_handling_test_class != NULL, 1, "runtime: ExceptionHandlingTest in Smalltalk dict");
        ASSERT_EQ(ctx, transaction_test_class != NULL, 1, "runtime: TransactionTest in Smalltalk dict");
        ASSERT_EQ(ctx, durable_transaction_test_class != NULL, 1,
                  "runtime: DurableTransactionTest in Smalltalk dict");
        ASSERT_EQ(ctx, exception_class != NULL, 1, "runtime: Exception in Smalltalk dict");
        ASSERT_EQ(ctx, error_class != NULL, 1, "runtime: Error in Smalltalk dict");
        ASSERT_EQ(ctx, test_failure_class != NULL, 1, "runtime: TestFailure in Smalltalk dict");
        ASSERT_EQ(ctx, class_lookup(context_test_class, intern_cstring_symbol(world.om, "runOn:")) != 0,
                  1, "runtime: ContextTest inherits runOn:");
        ASSERT_EQ(ctx, class_lookup(block_activation_test_class, intern_cstring_symbol(world.om, "runOn:")) != 0,
                  1, "runtime: BlockActivationTest inherits runOn:");
        ASSERT_EQ(ctx, class_lookup(multiple_failure_test_class, intern_cstring_symbol(world.om, "runOn:")) != 0,
                  1, "runtime: MultipleFailureTest inherits runOn:");
        ASSERT_EQ(ctx, class_lookup(compiler_test_class, intern_cstring_symbol(world.om, "runOn:")) != 0,
                  1, "runtime: CompilerTest inherits runOn:");
        ASSERT_EQ(ctx, class_lookup(exception_handling_test_class, intern_cstring_symbol(world.om, "runOn:")) != 0,
                  1, "runtime: ExceptionHandlingTest inherits runOn:");
        ASSERT_EQ(ctx, class_lookup(transaction_test_class, intern_cstring_symbol(world.om, "runOn:")) != 0,
                  1, "runtime: TransactionTest inherits runOn:");
        ASSERT_EQ(ctx, class_lookup(durable_transaction_test_class, intern_cstring_symbol(world.om, "runOn:")) != 0,
                  1, "runtime: DurableTransactionTest inherits runOn:");
#ifdef ALO_INTERPRETER_C
        ASSERT_EQ(ctx, sw_send1(&world, ctx, (Oop)exception_class, world.class_class,
                                "handlesSignalClass:", (Oop)error_class), TAGGED_TRUE,
                  "runtime: Exception matches Error through Smalltalk protocol");
        ASSERT_EQ(ctx, sw_send1(&world, ctx, (Oop)error_class, world.class_class,
                                "handlesSignalClass:", (Oop)exception_class), TAGGED_FALSE,
                  "runtime: Error does not match Exception through Smalltalk protocol");
#endif
    }
    ASSERT_EQ(ctx, smalltalk_world_install_class_file(&world, "src/smalltalk/WriteStream.st") != NULL,
              1, "runtime: WriteStream.st defines class and installs methods");
    ASSERT_EQ(ctx, smalltalk_world_install_st_file(&world, "tests/fixtures/CompilerTest.st"),
              1, "runtime: CompilerTest.st installs methods onto CompilerTest");
    {
        uint64_t *write_stream_compiled_method = compile_smalltalk_method(
            &world, ctx, compiler_class,
            "nextPut: aByte\n"
            "    | one |\n"
            "    one := String new: 1.\n"
            "    one at: 1 put: aByte.\n"
            "    collection := collection , one.\n"
            "    ^ aByte");
        uint64_t stream;
        uint64_t contents;
        uint64_t put_result;

        ASSERT_EQ(ctx, write_stream_compiled_method != NULL, 1,
                  "runtime: Smalltalk compiler compiles real WriteStream>>nextPut:");
        install_rooted_method(ctx, &world, "WriteStream", "nextPut:",
                              "CurrentCompilerInstalledMethod", (Oop)write_stream_compiled_method);
        ASSERT_EQ(ctx, smalltalk_world_lookup_class(&world, "WriteStream") != NULL, 1,
                  "runtime: WriteStream class stays available for compiler-installed method");
        stream = sw_send1(&world, ctx, (uint64_t)smalltalk_world_lookup_class(&world, "WriteStream"),
                          world.class_class, "on:", (uint64_t)sw_make_string(&world, ""));
        ASSERT_EQ(ctx, is_object_ptr(stream), 1,
                  "runtime: WriteStream on: creates receiver for compiler-installed nextPut:");
        put_result = sw_send1(&world, ctx, stream, smalltalk_world_lookup_class(&world, "WriteStream"),
                              "nextPut:", tag_smallint('A'));
        ASSERT_EQ(ctx, put_result, tag_smallint('A'),
                  "runtime: compiler-installed WriteStream>>nextPut: returns the byte");
        contents = sw_send0(&world, ctx, stream, smalltalk_world_lookup_class(&world, "WriteStream"),
                            "contents");
        ASSERT_EQ(ctx, byte_object_equals_cstring(contents, "A"), 1,
                  "runtime: compiler-installed WriteStream>>nextPut: updates contents");
    }
    {
        uint64_t *suite_add_method = compile_smalltalk_method(
            &world, ctx, compiler_class,
            "add: aTest\n"
            "    tests == nil\n"
            "        ifTrue: [self initialize]\n"
            "        ifFalse: [self].\n"
            "    tally = tests size\n"
            "        ifTrue: [self growStorage]\n"
            "        ifFalse: [self].\n"
            "    tally := tally + 1.\n"
            "    tests at: tally put: aTest.\n"
            "    ^ aTest");
        uint64_t *suite_grow_storage_method = compile_smalltalk_method(
            &world, ctx, compiler_class,
            "growStorage\n"
            "    | grown |\n"
            "    grown := Array new: tests size + tests size.\n"
            "    self copyTestsFrom: tests to: grown startingAt: 1.\n"
            "    tests := grown.\n"
            "    ^ self");
        uint64_t *suite_copy_tests_method = compile_smalltalk_method(
            &world, ctx, compiler_class,
            "copyTestsFrom: oldStorage to: newStorage startingAt: index\n"
            "    ^ (index < (tally + 1))\n"
            "        ifTrue: [\n"
            "            newStorage at: index put: (oldStorage at: index).\n"
            "            self copyTestsFrom: oldStorage to: newStorage startingAt: index + 1\n"
            "        ]\n"
            "        ifFalse: [newStorage]");
        uint64_t suite;
        uint64_t last_test_case;
        uint64_t tests_array;
        uint64_t stored_test_case;

        ASSERT_EQ(ctx, suite_add_method != NULL, 1,
                  "runtime: Smalltalk compiler compiles real TestSuite>>add:");
        ASSERT_EQ(ctx, suite_grow_storage_method != NULL, 1,
                  "runtime: Smalltalk compiler compiles real TestSuite>>growStorage");
        ASSERT_EQ(ctx, suite_copy_tests_method != NULL, 1,
                  "runtime: Smalltalk compiler compiles real TestSuite>>copyTestsFrom:to:startingAt:");
        install_rooted_method(ctx, &world, "TestSuite", "add:",
                              "CurrentCompilerInstalledMethod", (Oop)suite_add_method);
        install_rooted_method(ctx, &world, "TestSuite", "growStorage",
                              "CurrentCompilerInstalledMethod", (Oop)suite_grow_storage_method);
        install_rooted_method(ctx, &world, "TestSuite", "copyTestsFrom:to:startingAt:",
                              "CurrentCompilerInstalledMethod", (Oop)suite_copy_tests_method);
        ASSERT_EQ(ctx, smalltalk_world_lookup_class(&world, "TestSuite") != NULL, 1,
                  "runtime: TestSuite available for compiler-installed add:");
        ASSERT_EQ(ctx, smalltalk_world_lookup_class(&world, "CompilerTest") != NULL, 1,
                  "runtime: CompilerTest available for compiler-installed add:");
        suite = sw_send0(&world, ctx, (uint64_t)smalltalk_world_lookup_class(&world, "TestSuite"),
                         world.class_class, "new");
        ASSERT_EQ(ctx, is_object_ptr(suite), 1,
                  "runtime: TestSuite new allocates receiver for compiler-installed add:");
        for (int index = 0; index < 5; index++)
        {
            uint64_t test_case = sw_send0(&world, ctx, (uint64_t)smalltalk_world_lookup_class(&world, "CompilerTest"),
                                          world.class_class, "new");
            ASSERT_EQ(ctx, is_object_ptr(test_case), 1,
                      "runtime: CompilerTest new allocates case for compiler-installed add:");
            last_test_case = test_case;
            ASSERT_EQ(ctx, sw_send1(&world, ctx, suite, smalltalk_world_lookup_class(&world, "TestSuite"),
                                    "add:", test_case),
                      test_case,
                      "runtime: compiler-installed TestSuite>>add: returns added test");
        }
        ASSERT_EQ(ctx, sw_send0(&world, ctx, suite, smalltalk_world_lookup_class(&world, "TestSuite"), "size"),
                  tag_smallint(5),
                  "runtime: compiler-installed TestSuite>>add: grows and updates tally");
        tests_array = sw_send0(&world, ctx, suite, smalltalk_world_lookup_class(&world, "TestSuite"), "tests");
        stored_test_case = sw_send1(&world, ctx, tests_array, world.array_class, "at:", tag_smallint(5));
        ASSERT_EQ(ctx, stored_test_case, last_test_case,
                  "runtime: compiler-installed TestSuite>>add: stores added test after growth");
    }
    {
        uint64_t *suite_run_on_method = compile_smalltalk_method(
            &world, ctx, compiler_class,
            "runOn: aResult\n"
            "    | index |\n"
            "    index := 1.\n"
            "    [index < (tally + 1)] whileTrue: [\n"
            "        (tests at: index) runOn: aResult.\n"
            "        index := index + 1\n"
            "    ].\n"
            "    ^ aResult");
        uint64_t suite;
        uint64_t result;
        uint64_t test_case;
        uint64_t selector = intern_cstring_symbol(world.om, "testCompileExpressionLiteralShape");
        uint64_t run_result;

        ASSERT_EQ(ctx, suite_run_on_method != NULL, 1,
                  "runtime: Smalltalk compiler compiles real TestSuite>>runOn:");
        install_rooted_method(ctx, &world, "TestSuite", "runOn:",
                              "CurrentCompilerInstalledMethod", (Oop)suite_run_on_method);
        ASSERT_EQ(ctx, smalltalk_world_lookup_class(&world, "TestSuite") != NULL, 1,
                  "runtime: TestSuite class remains available");
        ASSERT_EQ(ctx, smalltalk_world_lookup_class(&world, "TestResult") != NULL, 1,
                  "runtime: TestResult class remains available");
        ASSERT_EQ(ctx, smalltalk_world_lookup_class(&world, "CompilerTest") != NULL, 1,
                  "runtime: CompilerTest class remains available");
        suite = sw_send0(&world, ctx, (uint64_t)smalltalk_world_lookup_class(&world, "TestSuite"),
                         world.class_class, "new");
        result = sw_send0(&world, ctx, (uint64_t)smalltalk_world_lookup_class(&world, "TestResult"),
                          world.class_class, "new");
        test_case = sw_send0(&world, ctx, (uint64_t)smalltalk_world_lookup_class(&world, "CompilerTest"),
                             world.class_class, "new");
        ASSERT_EQ(ctx, is_object_ptr(suite), 1, "runtime: TestSuite new allocates suite");
        ASSERT_EQ(ctx, is_object_ptr(result), 1, "runtime: TestResult new allocates result");
        ASSERT_EQ(ctx, is_object_ptr(test_case), 1, "runtime: CompilerTest new allocates test case");
        ASSERT_EQ(ctx,
                  sw_send1(&world, ctx, test_case, smalltalk_world_lookup_class(&world, "CompilerTest"),
                           "selector:", selector),
                  selector, "runtime: CompilerTest selector: stores test selector");
        ASSERT_EQ(ctx, sw_send1(&world, ctx, suite, smalltalk_world_lookup_class(&world, "TestSuite"),
                                "add:", test_case),
                  test_case, "runtime: TestSuite add: accepts compiler test case");
        run_result = sw_send1(&world, ctx, suite, smalltalk_world_lookup_class(&world, "TestSuite"),
                              "runOn:", result);
        ASSERT_EQ(ctx, run_result, result,
                  "runtime: compiler-installed TestSuite>>runOn: returns result");
        ASSERT_EQ(ctx, sw_send0(&world, ctx, result, smalltalk_world_lookup_class(&world, "TestResult"),
                                "runCount"),
                  tag_smallint(1),
                  "runtime: compiler-installed TestSuite>>runOn: executes one test");
        ASSERT_EQ(ctx, sw_send0(&world, ctx, result, smalltalk_world_lookup_class(&world, "TestResult"),
                                "passCount"),
                  tag_smallint(1),
                  "runtime: compiler-installed TestSuite>>runOn: records one pass");
        ASSERT_EQ(ctx, sw_send0(&world, ctx, result, smalltalk_world_lookup_class(&world, "TestResult"),
                                "failureCount"),
                  tag_smallint(0),
                  "runtime: compiler-installed TestSuite>>runOn: records no failures");
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
        run_smalltalk_direct_selector(ctx, &world, "CompilerTest", "testCompileExpressionLiteralShape");
        run_smalltalk_direct_selector(ctx, &world, "CompilerTest", "testCompileExpressionBinarySendShape");
        run_smalltalk_direct_selector(ctx, &world, "CompilerTest", "testCompileMethodBinaryReturnShape");
        run_smalltalk_direct_selector(ctx, &world, "CompilerTest", "testCompileExpressionKeywordSendShape");
        run_smalltalk_direct_selector(ctx, &world, "CompilerTest", "testCompileMethodUnarySendReturnShape");
        run_smalltalk_direct_selector(ctx, &world, "CompilerTest", "testCompileMethodConditionalNonLocalReturnShape");
        run_smalltalk_direct_selector(ctx, &world, "CompilerTest", "testCompileMethodPrimitiveShape");
        run_smalltalk_direct_selector(ctx, &world, "CompilerTest", "testCompileMethodPrimitiveFallbackBodyShape");
        run_smalltalk_direct_selector(ctx, &world, "CompilerTest", "testCompileMethodTemporariesShape");
        run_smalltalk_direct_selector(ctx, &world, "CompilerTest", "testCompileMethodGlobalReferenceShape");
        run_smalltalk_direct_selector(ctx, &world, "CompilerTest",
                                      "testCompileMethodRealWriteStreamNextPutShape");
        run_smalltalk_direct_selector(ctx, &world, "CompilerTest",
                                      "testCompileMethodRealTestSuiteRunOnShape");
        run_smalltalk_direct_selector(ctx, &world, "CompilerTest",
                                      "testCompileMethodRealTestSuiteAddShape");
        run_smalltalk_direct_selector(ctx, &world, "CompilerTest",
                                      "testCompileMethodRealSmalltalkSelfTestSuiteAddTestsShape");
        run_smalltalk_direct_selector(ctx, &world, "CompilerTest",
                                      "testCompileMethodRealParserExpressionOrAssignmentShape");
        run_smalltalk_direct_selector(ctx, &world, "CompilerTest",
                                      "testCompileMethodRealCodeGeneratorVisitAssignmentShape");
        run_smalltalk_direct_selector(ctx, &world, "CompilerTest",
                                      "testCompileMethodRealParserTemporariesShape");
        run_smalltalk_direct_selector(ctx, &world, "CompilerTest",
                                      "testCompileMethodRealCodeGeneratorVisitStatementsShape");
        run_smalltalk_direct_selector(ctx, &world, "ContextTest",
                                      "testStringConcatenationProducesExpectedContents");
        run_smalltalk_direct_selector(ctx, &world, "ContextTest",
                                      "testArrayAtPutRoundTripsValue");
        run_smalltalk_direct_selector(ctx, &world, "ContextTest",
                                      "testStringAtPutRoundTripsByteValue");
        run_smalltalk_direct_selector(ctx, &world, "ContextTest",
                                      "testSymbolEqualityDistinguishesSameAndDifferentSymbols");
        run_smalltalk_direct_selector(ctx, &world, "ContextTest",
                                      "testStringEqualityDistinguishesSameAndDifferentContents");
        run_smalltalk_direct_selector(ctx, &world, "ContextTest",
                                      "testStringAsSymbolInternsContents");
        run_smalltalk_direct_selector(ctx, &world, "ContextTest",
                                      "testStringHashIsStableAndMatchesEqualContents");
        run_smalltalk_direct_selector(ctx, &world, "TransactionTest", "testAtomicCommitsObjectChanges");
        run_smalltalk_direct_selector(ctx, &world, "TransactionTest", "testAtomicRollsBackOnError");
        run_smalltalk_direct_selector(ctx, &world, "TransactionTest", "testAtomicReturnsBlockValue");
        run_smalltalk_direct_selector(ctx, &world, "TransactionTest",
                                      "testReadOnlyEvaluatesWithoutMutationProtocol");
        run_smalltalk_direct_selector(ctx, &world, "TransactionTest",
                                      "testReadOnlyDiscardsObjectChanges");
        run_smalltalk_direct_selector(ctx, &world, "TransactionTest",
                                      "testNestedAtomicRollsBackWithOuterTransaction");
        run_smalltalk_direct_selector(ctx, &world, "DurableTransactionTest",
                                      "testDurableUsesSameProgrammingModelAsAtomic");
        run_smalltalk_direct_selector(ctx, &world, "DurableTransactionTest",
                                      "testDurableReturnsBlockValue");
        run_smalltalk_direct_selector(ctx, &world, "DurableTransactionTest",
                                      "testDurableRollsBackOnError");
        run_smalltalk_direct_selector(ctx, &world, "DurableTransactionTest",
                                      "testDurableCommitSurvivesCheckpointAndRestart");
        run_smalltalk_direct_selector(ctx, &world, "DurableTransactionTest",
                                      "testDurableCommitReplaysWithoutPostCommitCheckpoint");
#else
        run_smalltalk_direct_tests(ctx, &world, "ContextTest", 13);
        run_smalltalk_direct_tests(ctx, &world, "BlockActivationTest", 6);
#endif
    }
    {
        uint64_t *parse_expr_or_assignment_method = compile_smalltalk_method(
            &world, ctx, compiler_class,
            "parseExpressionOrAssignment\n"
            "    | expr nextToken |\n"
            "    expr := self parseExpression.\n"
            "    expr isVariable\n"
            "        ifTrue: [0]\n"
            "        ifFalse: [^ expr].\n"
            "\n"
            "    nextToken := tokenizer peek.\n"
            "    (nextToken isSpecialText: ':=')\n"
            "        ifTrue: [0]\n"
            "        ifFalse: [^ expr].\n"
            "\n"
            "    tokenizer next.\n"
            "    ^ AssignmentNode variable: expr value: self parseExpression");
        uint64_t parser;
        uint64_t assignment;
        uint64_t assignment_variable;
        uint64_t assignment_value;

        ASSERT_EQ(ctx, parse_expr_or_assignment_method != NULL, 1,
                  "runtime: Smalltalk compiler compiles real Parser>>parseExpressionOrAssignment");
        install_rooted_method(ctx, &world, "Parser", "parseExpressionOrAssignment",
                              "CurrentCompilerInstalledMethod", (Oop)parse_expr_or_assignment_method);
        parser = sw_send1(&world, ctx, (uint64_t)smalltalk_world_lookup_class(&world, "Parser"),
                          world.class_class, "on:", (uint64_t)sw_make_string(&world, "x := 1"));
        ASSERT_EQ(ctx, is_object_ptr(parser), 1,
                  "runtime: Parser on: allocates receiver for compiler-installed parseExpressionOrAssignment");
        assignment = sw_send0(&world, ctx, parser, smalltalk_world_lookup_class(&world, "Parser"),
                              "parseExpressionOrAssignment");
        ASSERT_EQ(ctx, is_object_ptr(assignment), 1,
                  "runtime: compiler-installed Parser>>parseExpressionOrAssignment returns AST node");
        ASSERT_EQ(ctx, sw_send0(&world, ctx, assignment, NULL, "isAssignment"), tagged_true(),
                  "runtime: compiler-installed Parser>>parseExpressionOrAssignment keeps assignment node");
        assignment_variable = sw_send0(&world, ctx,
                                       sw_send0(&world, ctx, assignment, NULL, "variable"),
                                       NULL, "name");
        ASSERT_EQ(ctx, byte_object_equals_cstring(assignment_variable, "x"), 1,
                  "runtime: compiler-installed Parser>>parseExpressionOrAssignment keeps assignment variable");
        assignment_value = sw_send0(&world, ctx,
                                    sw_send0(&world, ctx, assignment, NULL, "value"),
                                    NULL, "value");
        ASSERT_EQ(ctx, assignment_value, tag_smallint(1),
                  "runtime: compiler-installed Parser>>parseExpressionOrAssignment keeps assignment value");
    }
    {
        uint64_t *parse_temporaries_method = compile_smalltalk_method(
            &world, ctx, compiler_class,
            "parseTemporaries\n"
            "    | token temps |\n"
            "    token := tokenizer next.\n"
            "    (token text = '|')\n"
            "        ifTrue: [\n"
            "            temps := Array new: 16.\n"
            "            ^ self collectTemporaries: temps at: 1\n"
            "        ]\n"
            "        ifFalse: [\n"
            "            tokenizer unread: token.\n"
            "            ^ Array new: 0\n"
            "        ]");
        uint64_t parser;
        uint64_t temps;

        ASSERT_EQ(ctx, parse_temporaries_method != NULL, 1,
                  "runtime: Smalltalk compiler compiles real Parser>>parseTemporaries");
        install_rooted_method(ctx, &world, "Parser", "parseTemporaries",
                              "CurrentCompilerInstalledMethod", (Oop)parse_temporaries_method);
        parser = sw_send1(&world, ctx, (uint64_t)smalltalk_world_lookup_class(&world, "Parser"),
                          world.class_class, "on:", (uint64_t)sw_make_string(&world, "| alpha beta |"));
        ASSERT_EQ(ctx, is_object_ptr(parser), 1,
                  "runtime: Parser on: allocates receiver for compiler-installed parseTemporaries");
        temps = sw_send0(&world, ctx, parser, smalltalk_world_lookup_class(&world, "Parser"),
                         "parseTemporaries");
        ASSERT_EQ(ctx, is_object_ptr(temps), 1,
                  "runtime: compiler-installed Parser>>parseTemporaries returns temp array");
        ASSERT_EQ(ctx, sw_send0(&world, ctx, temps, world.array_class, "size"), tag_smallint(2),
                  "runtime: compiler-installed Parser>>parseTemporaries keeps two temp names");
        ASSERT_EQ(ctx, byte_object_equals_cstring(
                              sw_send1(&world, ctx, temps, world.array_class, "at:", tag_smallint(1)),
                              "alpha"),
                  1,
                  "runtime: compiler-installed Parser>>parseTemporaries keeps first temp name");
        ASSERT_EQ(ctx, byte_object_equals_cstring(
                              sw_send1(&world, ctx, temps, world.array_class, "at:", tag_smallint(2)),
                              "beta"),
                  1,
                  "runtime: compiler-installed Parser>>parseTemporaries keeps second temp name");
    }
    {
        uint64_t *parse_statement_method = compile_smalltalk_method(
            &world, ctx, compiler_class,
            "parseStatement\n"
            "    | token |\n"
            "    token := tokenizer peek.\n"
            "\n"
            "    (token isSpecialText: '^')\n"
            "        ifTrue: [\n"
            "            tokenizer next.\n"
            "            ^ ReturnNode expression: self parseExpression\n"
            "        ]\n"
            "        ifFalse: [0].\n"
            "\n"
            "    ^ self parseExpressionOrAssignment");
        uint64_t parser;
        uint64_t statement;
        uint64_t expression_value;

        ASSERT_EQ(ctx, parse_statement_method != NULL, 1,
                  "runtime: Smalltalk compiler compiles real Parser>>parseStatement");
        install_rooted_method(ctx, &world, "Parser", "parseStatement",
                              "CurrentCompilerInstalledMethod", (Oop)parse_statement_method);
        parser = sw_send1(&world, ctx, (uint64_t)smalltalk_world_lookup_class(&world, "Parser"),
                          world.class_class, "on:", (uint64_t)sw_make_string(&world, "^ 1"));
        ASSERT_EQ(ctx, is_object_ptr(parser), 1,
                  "runtime: Parser on: allocates receiver for compiler-installed parseStatement");
        statement = sw_send0(&world, ctx, parser, smalltalk_world_lookup_class(&world, "Parser"),
                             "parseStatement");
        ASSERT_EQ(ctx, is_object_ptr(statement), 1,
                  "runtime: compiler-installed Parser>>parseStatement returns AST node");
        ASSERT_EQ(ctx, sw_send0(&world, ctx, statement, NULL, "isReturn"), tagged_true(),
                  "runtime: compiler-installed Parser>>parseStatement keeps return node");
        expression_value = sw_send0(&world, ctx,
                                    sw_send0(&world, ctx, statement, NULL, "expression"),
                                    NULL, "value");
        ASSERT_EQ(ctx, expression_value, tag_smallint(1),
                  "runtime: compiler-installed Parser>>parseStatement keeps return expression");
    }
    {
        uint64_t *parse_method_body_method = compile_smalltalk_method(
            &world, ctx, compiler_class,
            "parseMethodBody\n"
            "    | temps statements |\n"
            "    primitiveIndex := self parseOptionalPrimitiveIndex.\n"
            "    temps := self parseTemporaries.\n"
            "    statements := self parseStatements.\n"
            "    ^ SequenceNode temporaries: temps statements: statements");
        uint64_t parser;
        uint64_t body;
        uint64_t temps;
        uint64_t statements;

        ASSERT_EQ(ctx, parse_method_body_method != NULL, 1,
                  "runtime: Smalltalk compiler compiles real Parser>>parseMethodBody");
        install_rooted_method(ctx, &world, "Parser", "parseMethodBody",
                              "CurrentCompilerInstalledMethod", (Oop)parse_method_body_method);
        parser = sw_send1(&world, ctx, (uint64_t)smalltalk_world_lookup_class(&world, "Parser"),
                          world.class_class, "on:", (uint64_t)sw_make_string(&world, "| alpha | 1"));
        ASSERT_EQ(ctx, is_object_ptr(parser), 1,
                  "runtime: Parser on: allocates receiver for compiler-installed parseMethodBody");
        body = sw_send0(&world, ctx, parser, smalltalk_world_lookup_class(&world, "Parser"),
                        "parseMethodBody");
        ASSERT_EQ(ctx, is_object_ptr(body), 1,
                  "runtime: compiler-installed Parser>>parseMethodBody returns sequence node");
        ASSERT_EQ(ctx, sw_send0(&world, ctx, body, NULL, "isSequence"), tagged_true(),
                  "runtime: compiler-installed Parser>>parseMethodBody keeps sequence node");
        temps = sw_send0(&world, ctx, body, NULL, "temporaries");
        statements = sw_send0(&world, ctx, body, NULL, "statements");
        ASSERT_EQ(ctx, sw_send0(&world, ctx, temps, world.array_class, "size"), tag_smallint(1),
                  "runtime: compiler-installed Parser>>parseMethodBody keeps one temporary");
        ASSERT_EQ(ctx, sw_send0(&world, ctx, statements, world.array_class, "size"), tag_smallint(1),
                  "runtime: compiler-installed Parser>>parseMethodBody keeps one statement");
        ASSERT_EQ(ctx, sw_send0(&world, ctx, parser, smalltalk_world_lookup_class(&world, "Parser"),
                                "primitiveIndex"),
                  tag_smallint(-1),
                  "runtime: compiler-installed Parser>>parseMethodBody keeps default primitive index");
    }
    {
        uint64_t *visit_sequence_method = compile_smalltalk_method(
            &world, ctx, compiler_class,
            "visitSequence: aNode\n"
            "    self temporaries: aNode temporaries.\n"
            "    aNode statements size > 0\n"
            "        ifTrue: [self visitStatements: aNode statements from: 1 last: aNode statements size]\n"
            "        ifFalse: [0].\n"
            "    ^ self");
        uint64_t sequence_gen;
        uint64_t temps;
        uint64_t stmts;
        uint64_t literal_one_node;
        uint64_t literal_two_node;
        uint64_t sequence_node;
        uint64_t sequence_result;

        ASSERT_EQ(ctx, visit_sequence_method != NULL, 1,
                  "runtime: Smalltalk compiler compiles real CodeGenerator>>visitSequence:");
        install_rooted_method(ctx, &world, "CodeGenerator", "visitSequence:",
                              "CurrentCompilerInstalledMethod", (Oop)visit_sequence_method);
        sequence_gen = sw_send0(&world, ctx, (uint64_t)smalltalk_world_lookup_class(&world, "CodeGenerator"),
                                world.class_class, "new");
        ASSERT_EQ(ctx, is_object_ptr(sequence_gen), 1,
                  "runtime: CodeGenerator new allocates receiver for compiler-installed visitSequence:");
        temps = sw_send1(&world, ctx, (uint64_t)world.array_class, world.class_class, "with:",
                         (uint64_t)sw_make_string(&world, "tmp"));
        literal_one_node = sw_send1(&world, ctx, (uint64_t)smalltalk_world_lookup_class(&world, "LiteralNode"),
                                    world.class_class, "value:", tag_smallint(1));
        literal_two_node = sw_send1(&world, ctx, (uint64_t)smalltalk_world_lookup_class(&world, "LiteralNode"),
                                    world.class_class, "value:", tag_smallint(2));
        stmts = sw_send2(&world, ctx, (uint64_t)world.array_class, world.class_class, "with:with:",
                         literal_one_node, literal_two_node);
        sequence_node = sw_send2(&world, ctx, (uint64_t)smalltalk_world_lookup_class(&world, "SequenceNode"),
                                 world.class_class, "temporaries:statements:", temps, stmts);
        ASSERT_EQ(ctx, is_object_ptr(sequence_node), 1,
                  "runtime: SequenceNode temporaries:statements: builds input AST for compiler-installed visitSequence:");
        sequence_result = sw_send1(&world, ctx, sequence_gen,
                                   smalltalk_world_lookup_class(&world, "CodeGenerator"),
                                   "visitSequence:", sequence_node);
        ASSERT_EQ(ctx, sequence_result, sequence_gen,
                  "runtime: compiler-installed CodeGenerator>>visitSequence: returns self");
        ASSERT_EQ(ctx, sw_send0(&world, ctx, sequence_gen,
                                smalltalk_world_lookup_class(&world, "CodeGenerator"),
                                "tempCount"),
                  tag_smallint(1),
                  "runtime: compiler-installed CodeGenerator>>visitSequence: configures temporaries");
        ASSERT_EQ(ctx, sw_send0(&world, ctx, sequence_gen,
                                smalltalk_world_lookup_class(&world, "CodeGenerator"),
                                "bytecodeCount"),
                  tag_smallint(11),
                  "runtime: compiler-installed CodeGenerator>>visitSequence: emits delegated statement bytecodes");
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
        ASSERT_EQ(ctx, OBJ_SIZE(moved_method_gen_ptr), 12,
                  "runtime: rooted compiler result remains a CodeGenerator after GC");
        ASSERT_EQ(ctx, OBJ_FIELD(moved_method_gen_ptr, 1), tag_smallint(7),
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
