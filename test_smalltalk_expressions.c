#include "test_defs.h"
#include "bootstrap_compiler.h"

typedef enum
{
    EXPR_EXPECT_SMALLINT = 0,
    EXPR_EXPECT_TRUE,
    EXPR_EXPECT_FALSE
} ExprExpectedKind;

typedef struct
{
    const char *name;
    const char *expression;
    ExprExpectedKind expected_kind;
    int64_t expected_smallint;
} ExpressionSpec;

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

    BClassBinding bindings[1] = {
        {"ExprSpec", expr_class},
    };

    const ExpressionSpec specs[] = {
        {"simple add", "1 + 2", EXPR_EXPECT_SMALLINT, 3},
        {"paren precedence", "(1 + 2) * 3", EXPR_EXPECT_SMALLINT, 9},
        {"chained send", "1 + 2 + 3", EXPR_EXPECT_SMALLINT, 6},
        {"less than true", "2 < 3", EXPR_EXPECT_TRUE, 0},
        {"less than false", "5 < 1", EXPR_EXPECT_FALSE, 0},
        {"equality true", "7 = 7", EXPR_EXPECT_TRUE, 0},
        {"equality false", "7 = 8", EXPR_EXPECT_FALSE, 0},
    };

    for (int index = 0; index < (int)(sizeof(specs) / sizeof(specs[0])); index++)
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
