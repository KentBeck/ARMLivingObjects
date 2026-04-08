#include "test_defs.h"
#include "bootstrap_compiler.h"
#include <ctype.h>

static int read_file(const char *path, char *buf, size_t cap)
{
    FILE *f = fopen(path, "rb");
    if (!f)
    {
        return 0;
    }
    size_t n = fread(buf, 1, cap - 1, f);
    fclose(f);
    buf[n] = '\0';
    return 1;
}

typedef enum
{
    EXPR_EXPECT_SMALLINT = 0,
    EXPR_EXPECT_TRUE,
    EXPR_EXPECT_FALSE
} ExprExpectedKind;

typedef struct
{
    char name[64];
    char expression[256];
    ExprExpectedKind expected_kind;
    int64_t expected_smallint;
} ExpressionSpec;

static void trim_in_place(char *text)
{
    size_t len = strlen(text);
    size_t start = 0;
    while (start < len && isspace((unsigned char)text[start]))
    {
        start++;
    }
    size_t end = len;
    while (end > start && isspace((unsigned char)text[end - 1]))
    {
        end--;
    }
    if (start > 0)
    {
        memmove(text, text + start, end - start);
    }
    text[end - start] = '\0';
}

static int parse_expected_value(const char *text, ExprExpectedKind *kind, int64_t *smallint_value)
{
    if (strcmp(text, "true") == 0)
    {
        *kind = EXPR_EXPECT_TRUE;
        *smallint_value = 0;
        return 1;
    }
    if (strcmp(text, "false") == 0)
    {
        *kind = EXPR_EXPECT_FALSE;
        *smallint_value = 0;
        return 1;
    }

    char *end = NULL;
    long long parsed = strtoll(text, &end, 10);
    if (end == text || *end != '\0')
    {
        return 0;
    }
    *kind = EXPR_EXPECT_SMALLINT;
    *smallint_value = (int64_t)parsed;
    return 1;
}

static int load_expression_specs(const char *path, ExpressionSpec *specs, int max_specs, int *out_count)
{
    FILE *file = fopen(path, "rb");
    if (!file)
    {
        return 0;
    }

    char line[1024];
    int count = 0;
    while (fgets(line, sizeof(line), file) != NULL)
    {
        trim_in_place(line);
        if (line[0] == '\0' || line[0] == '#')
        {
            continue;
        }

        char *first_sep = strstr(line, "|");
        if (!first_sep)
        {
            fclose(file);
            return 0;
        }
        *first_sep = '\0';

        char *second_field = first_sep + 1;
        char *second_sep = strstr(second_field, "|");
        if (!second_sep)
        {
            fclose(file);
            return 0;
        }
        *second_sep = '\0';
        char *third_field = second_sep + 1;

        trim_in_place(line);
        trim_in_place(second_field);
        trim_in_place(third_field);

        if (line[0] == '\0' || second_field[0] == '\0' || third_field[0] == '\0' || count >= max_specs)
        {
            fclose(file);
            return 0;
        }

        ExpressionSpec *spec = &specs[count];
        memset(spec, 0, sizeof(*spec));
        strncpy(spec->name, line, sizeof(spec->name) - 1);
        strncpy(spec->expression, second_field, sizeof(spec->expression) - 1);
        if (!parse_expected_value(third_field, &spec->expected_kind, &spec->expected_smallint))
        {
            fclose(file);
            return 0;
        }

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

static uint64_t selector_token(const char *selector)
{
    uint32_t hash = 2166136261u;
    for (const unsigned char *current = (const unsigned char *)selector; *current != '\0'; current++)
    {
        hash ^= (uint32_t)(*current);
        hash *= 16777619u;
    }
    return tag_smallint((int64_t)(hash & 0x1FFFFFFF));
}

static void md_append(uint64_t *om, uint64_t *class_class, uint64_t *klass, const char *selector, uint64_t method)
{
    uint64_t md_val = OBJ_FIELD(klass, CLASS_METHOD_DICT);
    uint64_t *old_md = (md_val != tagged_nil() && (md_val & 3) == 0) ? (uint64_t *)md_val : NULL;
    uint64_t old_size = old_md ? OBJ_SIZE(old_md) : 0;
    uint64_t *new_md = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, old_size + 2);

    for (uint64_t index = 0; index < old_size; index++)
    {
        OBJ_FIELD(new_md, index) = OBJ_FIELD(old_md, index);
    }
    OBJ_FIELD(new_md, old_size) = selector_token(selector);
    OBJ_FIELD(new_md, old_size + 1) = method;
    OBJ_FIELD(klass, CLASS_METHOD_DICT) = (uint64_t)new_md;
}

static uint64_t *make_primitive_cm(uint64_t *om, uint64_t *class_class, int prim, int num_args)
{
    uint64_t *prim_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 1);
    ((uint8_t *)&OBJ_FIELD(prim_bc, 0))[0] = BC_HALT;

    uint64_t *cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
    OBJ_FIELD(cm, CM_PRIMITIVE) = tag_smallint(prim);
    OBJ_FIELD(cm, CM_NUM_ARGS) = tag_smallint(num_args);
    OBJ_FIELD(cm, CM_NUM_TEMPS) = tag_smallint(0);
    OBJ_FIELD(cm, CM_LITERALS) = tagged_nil();
    OBJ_FIELD(cm, CM_BYTECODES) = (uint64_t)prim_bc;
    return cm;
}

static uint64_t *make_byte_string(uint64_t *om, uint64_t *string_class, const char *text)
{
    uint64_t size = (uint64_t)strlen(text);
    uint64_t *obj = om_alloc(om, (uint64_t)string_class, FORMAT_BYTES, size);
    memcpy((uint8_t *)&OBJ_FIELD(obj, 0), text, size);
    return obj;
}

static uint64_t *make_class_with_ivars(uint64_t *om, uint64_t *class_class, uint64_t *string_class,
                                       uint64_t *superclass, const char **ivars, uint64_t ivar_count)
{
    uint64_t *klass = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
    OBJ_FIELD(klass, CLASS_SUPERCLASS) = superclass ? (uint64_t)superclass : tagged_nil();
    OBJ_FIELD(klass, CLASS_METHOD_DICT) = tagged_nil();
    OBJ_FIELD(klass, CLASS_INST_SIZE) = tag_smallint((int64_t)ivar_count);
    OBJ_FIELD(klass, CLASS_INST_FORMAT) = tag_smallint(FORMAT_FIELDS);
    if (ivar_count == 0)
    {
        OBJ_FIELD(klass, CLASS_INST_VARS) = tagged_nil();
        return klass;
    }

    uint64_t *ivar_array = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, ivar_count);
    for (uint64_t index = 0; index < ivar_count; index++)
    {
        OBJ_FIELD(ivar_array, index) = (uint64_t)make_byte_string(om, string_class, ivars[index]);
    }
    OBJ_FIELD(klass, CLASS_INST_VARS) = (uint64_t)ivar_array;
    return klass;
}

static uint64_t send_selector0(uint64_t *stack, uint64_t *class_table, uint64_t *om,
                               uint64_t receiver, uint64_t *receiver_class, const char *selector)
{
    uint64_t method_oop = class_lookup(receiver_class, selector_token(selector));
    uint64_t *compiled_method = (uint64_t *)method_oop;
    uint64_t *bytecodes = (uint64_t *)OBJ_FIELD(compiled_method, CM_BYTECODES);
    uint64_t *sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
    uint64_t *fp = (uint64_t *)0xCAFE;
    stack_push(&sp, stack, receiver);
    activate_method(&sp, &fp, 0, (uint64_t)compiled_method, 0, 0);
    return interpret(&sp, &fp, (uint8_t *)&OBJ_FIELD(bytecodes, 0), class_table, om, NULL);
}

static uint64_t send_selector1(uint64_t *stack, uint64_t *class_table, uint64_t *om,
                               uint64_t receiver, uint64_t *receiver_class, const char *selector,
                               uint64_t arg)
{
    uint64_t method_oop = class_lookup(receiver_class, selector_token(selector));
    uint64_t *compiled_method = (uint64_t *)method_oop;
    uint64_t *bytecodes = (uint64_t *)OBJ_FIELD(compiled_method, CM_BYTECODES);
    uint64_t *sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
    uint64_t *fp = (uint64_t *)0xCAFE;
    stack_push(&sp, stack, receiver);
    stack_push(&sp, stack, arg);
    activate_method(&sp, &fp, 0, (uint64_t)compiled_method, 1, 0);
    return interpret(&sp, &fp, (uint8_t *)&OBJ_FIELD(bytecodes, 0), class_table, om, NULL);
}

void test_smalltalk_expressions(TestContext *ctx)
{
    char context_src[2048];
    char expression_spec_test_src[2048];
    const char *object_testing_src =
        "!Object methodsFor: 'testing'!\n"
        "isNil\n"
        "    ^ false\n"
        "!\n";
    const char *object_framework_src =
        "!Object methodsFor: 'testing'!\n"
        "isNil\n"
        "    ^ false\n"
        "!\n"
        "!Object methodsFor: 'message sending'!\n"
        "perform: aSelector\n"
        "    <primitive: 17>\n"
        "    ^ self\n"
        "!\n"
        "!Object methodsFor: 'convenience'!\n"
        "yourself\n"
        "    ^ self\n"
        "!\n";
    const char *undefined_object_testing_src =
        "!UndefinedObject methodsFor: 'testing'!\n"
        "isNil\n"
        "    ^ true\n"
        "!\n";
    const char *test_result_runtime_src =
        "!TestResult methodsFor: 'initialization'!\n"
        "initialize\n"
        "    runCount := 0.\n"
        "    passCount := 0.\n"
        "    failureCount := 0.\n"
        "    lastFailure := nil.\n"
        "    lastSelector := nil.\n"
        "    lastReason := nil.\n"
        "    ^ self\n"
        "!\n"
        "!TestResult methodsFor: 'recording'!\n"
        "recordPass: aCase selector: aSelector\n"
        "    runCount := runCount + 1.\n"
        "    passCount := passCount + 1.\n"
        "    lastFailure := nil.\n"
        "    lastSelector := aSelector.\n"
        "    lastReason := nil.\n"
        "    ^ aCase\n"
        "!\n"
        "recordFailure: aCase selector: aSelector reason: aSymbol\n"
        "    runCount := runCount + 1.\n"
        "    failureCount := failureCount + 1.\n"
        "    lastFailure := aCase.\n"
        "    lastSelector := aSelector.\n"
        "    lastReason := aSymbol.\n"
        "    ^ aCase\n"
        "!\n"
        "!TestResult methodsFor: 'accessing'!\n"
        "wasSuccessful\n"
        "    ^ failureCount = 0\n"
        "!\n";

    md_append(ctx->om, ctx->class_class, ctx->smallint_class, "+",
              (uint64_t)make_primitive_cm(ctx->om, ctx->class_class, PRIM_SMALLINT_ADD, 1));
    md_append(ctx->om, ctx->class_class, ctx->smallint_class, "-",
              (uint64_t)make_primitive_cm(ctx->om, ctx->class_class, PRIM_SMALLINT_SUB, 1));
    md_append(ctx->om, ctx->class_class, ctx->smallint_class, "*",
              (uint64_t)make_primitive_cm(ctx->om, ctx->class_class, PRIM_SMALLINT_MUL, 1));
    md_append(ctx->om, ctx->class_class, ctx->smallint_class, "<",
              (uint64_t)make_primitive_cm(ctx->om, ctx->class_class, PRIM_SMALLINT_LT, 1));
    md_append(ctx->om, ctx->class_class, ctx->smallint_class, "=",
              (uint64_t)make_primitive_cm(ctx->om, ctx->class_class, PRIM_SMALLINT_EQ, 1));
    md_append(ctx->om, ctx->class_class, ctx->smallint_class, "asCharacter",
              (uint64_t)make_primitive_cm(ctx->om, ctx->class_class, PRIM_AS_CHARACTER, 0));
    md_append(ctx->om, ctx->class_class, ctx->context_class, "==",
              (uint64_t)make_primitive_cm(ctx->om, ctx->class_class, PRIM_IDENTITY_EQ, 1));

    md_append(ctx->om, ctx->class_class, ctx->character_class, "value",
              (uint64_t)make_primitive_cm(ctx->om, ctx->class_class, PRIM_CHAR_VALUE, 0));
    md_append(ctx->om, ctx->class_class, ctx->character_class, "isLetter",
              (uint64_t)make_primitive_cm(ctx->om, ctx->class_class, PRIM_CHAR_IS_LETTER, 0));
    md_append(ctx->om, ctx->class_class, ctx->character_class, "isDigit",
              (uint64_t)make_primitive_cm(ctx->om, ctx->class_class, PRIM_CHAR_IS_DIGIT, 0));

    uint64_t *expr_meta = om_alloc(ctx->om, (uint64_t)ctx->class_class, FORMAT_FIELDS, 4);
    OBJ_FIELD(expr_meta, CLASS_SUPERCLASS) = tagged_nil();
    OBJ_FIELD(expr_meta, CLASS_METHOD_DICT) = tagged_nil();
    OBJ_FIELD(expr_meta, CLASS_INST_SIZE) = tag_smallint(0);
    OBJ_FIELD(expr_meta, CLASS_INST_FORMAT) = tag_smallint(FORMAT_FIELDS);

    uint64_t *expr_class = om_alloc(ctx->om, (uint64_t)expr_meta, FORMAT_FIELDS, 4);
    OBJ_FIELD(expr_class, CLASS_SUPERCLASS) = (uint64_t)ctx->test_class;
    OBJ_FIELD(expr_class, CLASS_METHOD_DICT) = tagged_nil();
    OBJ_FIELD(expr_class, CLASS_INST_SIZE) = tag_smallint(0);
    OBJ_FIELD(expr_class, CLASS_INST_FORMAT) = tag_smallint(FORMAT_FIELDS);

    BClassBinding bindings[1] = {
        {"ExprSpec", expr_class},
    };
    BClassBinding runtime_bindings[3] = {
        {"Object", ctx->test_class},
        {"Context", ctx->context_class},
        {"UndefinedObject", ctx->undefined_object_class},
    };

    ASSERT_EQ(ctx, read_file("src/smalltalk/Context.st", context_src, sizeof(context_src)), 1,
              "expression Context source loads");
    ASSERT_EQ(ctx,
              bc_compile_and_install_source_methods(ctx->om, ctx->class_class, runtime_bindings, 3, object_testing_src),
              1,
              "expression Object methods install");
    ASSERT_EQ(ctx,
              bc_compile_and_install_source_methods(ctx->om, ctx->class_class, runtime_bindings, 3, context_src),
              1,
              "expression Context methods install");
    ASSERT_EQ(ctx,
              bc_compile_and_install_source_methods(ctx->om, ctx->class_class, runtime_bindings, 3, undefined_object_testing_src),
              1,
              "expression UndefinedObject methods install");
    ASSERT_EQ(ctx, class_lookup(ctx->context_class, selector_token("receiver")) != 0, 1,
              "expression runtime: Context understands receiver");
    ASSERT_EQ(ctx, class_lookup(expr_class, selector_token("isNil")) != 0, 1,
              "expression runtime: ExprSpec receiver understands isNil");

    ExpressionSpec specs[128];
    int spec_count = 0;
    ASSERT_EQ(ctx,
              load_expression_specs("tests/ExpressionSpecs.txt", specs, 128, &spec_count),
              1,
              "expression specs file loads");

    {
        const char *helpers =
            "!ExprSpec methodsFor: 'expression helper methods'!\n"
            "foo\n"
            "    ^ 7\n"
            "!\n"
            "bar\n"
            "    ^ self foo\n"
            "!\n";
        ASSERT_EQ(ctx,
                  bc_compile_and_install_source_methods(ctx->om, ctx->class_class, bindings, 1, helpers),
                  1,
                  "expression helper methods install");
    }

    for (int index = 0; index < spec_count; index++)
    {
        char selector[32];
        char source[1024];
        snprintf(selector, sizeof(selector), "expr%d", index + 1);
        snprintf(source, sizeof(source),
                 "!ExprSpec methodsFor: 'expression specs'!\n"
                 "%s\n"
                 "    ^ %s\n"
                 "!\n",
                 selector, specs[index].expression);

        ASSERT_EQ(ctx,
                  bc_compile_and_install_source_methods(ctx->om, ctx->class_class, bindings, 1, source),
                  1,
                  specs[index].name);

        uint64_t *instance_md = (uint64_t *)OBJ_FIELD(expr_class, CLASS_METHOD_DICT);
        uint64_t method_oop = md_lookup(instance_md, selector_token(selector));
        ASSERT_EQ(ctx, method_oop != 0, 1, "expression method installed");

        uint64_t *expr_receiver = om_alloc(ctx->om, (uint64_t)expr_class, FORMAT_FIELDS, 0);
        uint64_t *compiled_method = (uint64_t *)method_oop;
        uint64_t *bytecodes = (uint64_t *)OBJ_FIELD(compiled_method, CM_BYTECODES);

        uint64_t *sp = (uint64_t *)((uint8_t *)ctx->stack + STACK_WORDS * sizeof(uint64_t));
        uint64_t *fp = (uint64_t *)0xCAFE;
        stack_push(&sp, ctx->stack, (uint64_t)expr_receiver);
        activate_method(&sp, &fp, 0, (uint64_t)compiled_method, 0, 0);

        uint64_t result = interpret(&sp, &fp, (uint8_t *)&OBJ_FIELD(bytecodes, 0), ctx->class_table, ctx->om, NULL);

        if (specs[index].expected_kind == EXPR_EXPECT_SMALLINT)
        {
            ASSERT_EQ(ctx, result, tag_smallint(specs[index].expected_smallint), specs[index].name);
        }
        else if (specs[index].expected_kind == EXPR_EXPECT_TRUE)
        {
            ASSERT_EQ(ctx, result, tagged_true(), specs[index].name);
        }
        else
        {
            ASSERT_EQ(ctx, result, tagged_false(), specs[index].name);
        }
    }

    {
        static uint8_t xunit_om_buffer[262144] __attribute__((aligned(8)));
        uint64_t xunit_om[2];
        const char *test_result_ivars[] = {
            "runCount", "passCount", "failureCount", "lastFailure", "lastSelector", "lastReason"
        };
        om_init(xunit_om_buffer, sizeof(xunit_om_buffer), xunit_om);

        uint64_t *class_class = om_alloc(xunit_om, 0, FORMAT_FIELDS, 5);
        OBJ_CLASS(class_class) = (uint64_t)class_class;
        OBJ_FIELD(class_class, CLASS_SUPERCLASS) = tagged_nil();
        OBJ_FIELD(class_class, CLASS_METHOD_DICT) = tagged_nil();
        OBJ_FIELD(class_class, CLASS_INST_SIZE) = tag_smallint(5);
        OBJ_FIELD(class_class, CLASS_INST_FORMAT) = tag_smallint(FORMAT_FIELDS);
        OBJ_FIELD(class_class, CLASS_INST_VARS) = tagged_nil();

        uint64_t *smallint_class = make_class_with_ivars(xunit_om, class_class, class_class, NULL, NULL, 0);
        uint64_t *block_class = make_class_with_ivars(xunit_om, class_class, class_class, NULL, NULL, 0);
        uint64_t *undefined_object_class = make_class_with_ivars(xunit_om, class_class, class_class, NULL, NULL, 0);
        uint64_t *string_class = make_class_with_ivars(xunit_om, class_class, class_class, NULL, NULL, 0);
        OBJ_FIELD(string_class, CLASS_INST_FORMAT) = tag_smallint(FORMAT_BYTES);
        uint64_t *symbol_class = make_class_with_ivars(xunit_om, class_class, string_class, string_class, NULL, 0);
        OBJ_FIELD(symbol_class, CLASS_INST_FORMAT) = tag_smallint(FORMAT_BYTES);
        uint64_t *character_class = make_class_with_ivars(xunit_om, class_class, string_class, NULL, NULL, 0);

        const char *context_ivars[] = {
            "sender", "ip", "method", "receiver", "home", "closure", "flags", "numArgs", "numTemps"
        };
        uint64_t *context_class = make_class_with_ivars(xunit_om, class_class, string_class, NULL,
                                                        context_ivars, CONTEXT_VAR_BASE);
        uint64_t *object_class = make_class_with_ivars(xunit_om, class_class, string_class, NULL, NULL, 0);
        uint64_t *test_result_class = make_class_with_ivars(xunit_om, class_class, string_class,
                                                            object_class, test_result_ivars, 6);
        uint64_t *expression_spec_test_class = make_class_with_ivars(xunit_om, class_class, string_class,
                                                                     object_class, NULL, 0);

        BClassBinding framework_runtime_bindings[4] = {
            {"Object", object_class},
            {"Context", context_class},
            {"UndefinedObject", undefined_object_class},
            {"TestResult", test_result_class},
        };
        BClassBinding expression_spec_test_binding[1] = {
            {"ExpressionSpecTest", expression_spec_test_class},
        };

        uint64_t *symbol_table = om_alloc(xunit_om, (uint64_t)class_class, FORMAT_INDEXABLE, 20);
        for (int index = 0; index < 20; index++)
        {
            OBJ_FIELD(symbol_table, index) = tagged_nil();
        }

        uint64_t *saved_global_symbol_table = global_symbol_table;
        uint64_t *saved_global_symbol_class = global_symbol_class;
        uint64_t *saved_global_context_class = global_context_class;
        global_symbol_table = symbol_table;
        global_symbol_class = symbol_class;
        global_context_class = context_class;

        ASSERT_EQ(ctx, read_file("src/smalltalk/ExpressionSpecTest.st", expression_spec_test_src, sizeof(expression_spec_test_src)), 1,
                  "xUnit ExpressionSpecTest source loads");

        ASSERT_EQ(ctx,
                  bc_compile_and_install_source_methods(xunit_om, class_class, framework_runtime_bindings, 4, object_framework_src),
                  1,
                  "xUnit Object methods install");
        ASSERT_EQ(ctx,
                  bc_compile_and_install_source_methods(xunit_om, class_class, framework_runtime_bindings, 4, context_src),
                  1,
                  "xUnit Context methods install");
        ASSERT_EQ(ctx,
                  bc_compile_and_install_source_methods(xunit_om, class_class, framework_runtime_bindings, 4, undefined_object_testing_src),
                  1,
                  "xUnit UndefinedObject methods install");
        ASSERT_EQ(ctx,
                  bc_compile_and_install_source_methods(xunit_om, class_class, framework_runtime_bindings, 4, test_result_runtime_src),
                  1,
                  "xUnit TestResult methods install");
        ASSERT_EQ(ctx,
                  bc_compile_and_install_source_methods(xunit_om, class_class, expression_spec_test_binding, 1, expression_spec_test_src),
                  1,
                  "xUnit ExpressionSpecTest methods install");

        md_append(xunit_om, class_class, smallint_class, "+",
                  (uint64_t)make_primitive_cm(xunit_om, class_class, PRIM_SMALLINT_ADD, 1));
        md_append(xunit_om, class_class, smallint_class, "<",
                  (uint64_t)make_primitive_cm(xunit_om, class_class, PRIM_SMALLINT_LT, 1));
        md_append(xunit_om, class_class, smallint_class, "=",
                  (uint64_t)make_primitive_cm(xunit_om, class_class, PRIM_SMALLINT_EQ, 1));

        uint64_t *framework_class_table = om_alloc(xunit_om, (uint64_t)class_class, FORMAT_INDEXABLE, 6);
        OBJ_FIELD(framework_class_table, 0) = (uint64_t)smallint_class;
        OBJ_FIELD(framework_class_table, 1) = (uint64_t)block_class;
        OBJ_FIELD(framework_class_table, 2) = 0;
        OBJ_FIELD(framework_class_table, 3) = 0;
        OBJ_FIELD(framework_class_table, 4) = (uint64_t)character_class;
        OBJ_FIELD(framework_class_table, 5) = (uint64_t)undefined_object_class;

        uint64_t *result_obj = om_alloc(xunit_om, (uint64_t)test_result_class, FORMAT_FIELDS, 6);
        uint64_t *test_case_obj = om_alloc(xunit_om, (uint64_t)expression_spec_test_class, FORMAT_FIELDS, 0);

        send_selector0(ctx->stack, framework_class_table, xunit_om, (uint64_t)result_obj, test_result_class, "initialize");
        ASSERT_EQ(ctx, send_selector0(ctx->stack, framework_class_table, xunit_om, (uint64_t)test_case_obj,
                                      expression_spec_test_class, "testThisContextReceiverIsNil"),
                  tagged_false(),
                  "xUnit migrated test method returns false");
        uint64_t run_result = send_selector1(ctx->stack, framework_class_table, xunit_om, (uint64_t)test_case_obj,
                                             expression_spec_test_class, "runOn:", (uint64_t)result_obj);

        ASSERT_EQ(ctx, run_result, (uint64_t)result_obj,
                  "xUnit suite returns the result object");
        ASSERT_EQ(ctx, OBJ_FIELD(result_obj, 0), tag_smallint(1),
                  "xUnit runCount is 1");
        ASSERT_EQ(ctx, OBJ_FIELD(result_obj, 1), tag_smallint(1),
                  "xUnit passCount is 1");
        ASSERT_EQ(ctx, OBJ_FIELD(result_obj, 2), tag_smallint(0),
                  "xUnit failureCount is 0");
        ASSERT_EQ(ctx, OBJ_CLASS((uint64_t *)OBJ_FIELD(result_obj, 4)), (uint64_t)symbol_class,
                  "xUnit records a Symbol last selector");
        ASSERT_EQ(ctx, send_selector0(ctx->stack, framework_class_table, xunit_om, (uint64_t)result_obj,
                                      test_result_class, "wasSuccessful"),
                  tagged_true(),
                  "xUnit migrated expression test passes");

        global_symbol_table = saved_global_symbol_table;
        global_symbol_class = saved_global_symbol_class;
        global_context_class = saved_global_context_class;
    }
}
