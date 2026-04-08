#include "test_defs.h"
#include "bootstrap_compiler.h"
#include <ctype.h>

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

void test_smalltalk_expressions(TestContext *ctx)
{
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
    OBJ_FIELD(expr_class, CLASS_SUPERCLASS) = tagged_nil();
    OBJ_FIELD(expr_class, CLASS_METHOD_DICT) = tagged_nil();
    OBJ_FIELD(expr_class, CLASS_INST_SIZE) = tag_smallint(0);
    OBJ_FIELD(expr_class, CLASS_INST_FORMAT) = tag_smallint(FORMAT_FIELDS);
    md_append(ctx->om, ctx->class_class, expr_class, "thisContext",
              (uint64_t)make_primitive_cm(ctx->om, ctx->class_class, PRIM_THIS_CONTEXT, 0));

    BClassBinding bindings[1] = {
        {"ExprSpec", expr_class},
    };

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
}
