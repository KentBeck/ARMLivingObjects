#include "test_defs.h"

static void md_append(uint64_t *om, uint64_t *class_class, uint64_t *klass, uint64_t selector, uint64_t method)
{
    uint64_t md_val = OBJ_FIELD(klass, CLASS_METHOD_DICT);
    uint64_t *old_md = (md_val != tagged_nil() && (md_val & 3) == 0) ? (uint64_t *)md_val : NULL;
    uint64_t old_size = old_md ? OBJ_SIZE(old_md) : 0;
    uint64_t *new_md = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, old_size + 2);
    for (uint64_t i = 0; i < old_size; i++)
    {
        OBJ_FIELD(new_md, i) = OBJ_FIELD(old_md, i);
    }
    OBJ_FIELD(new_md, old_size) = selector;
    OBJ_FIELD(new_md, old_size + 1) = method;
    OBJ_FIELD(klass, CLASS_METHOD_DICT) = (uint64_t)new_md;
}

static uint64_t *make_bytes_obj(uint64_t *om, uint64_t *class_class, uint64_t *string_class, const char *bytes, uint64_t n)
{
    uint64_t *obj = om_alloc(om, (uint64_t)string_class, FORMAT_BYTES, n);
    uint8_t *dst = (uint8_t *)&OBJ_FIELD(obj, 0);
    memcpy(dst, bytes, (size_t)n);
    return obj;
}

void test_string_dispatch(TestContext *ctx)
{
    uint64_t *om = ctx->om;
    uint64_t *class_class = ctx->class_class;
    uint64_t *smallint_class = ctx->smallint_class;
    uint64_t *string_class = ctx->string_class;
    uint64_t *class_table = ctx->class_table;
    uint64_t *stack = ctx->stack;
    uint64_t *sp;
    uint64_t *fp;
    uint64_t result;

    uint64_t sel_lt = tag_smallint(310);
    uint64_t sel_plus = tag_smallint(311);
    uint64_t sel_size = tag_smallint(72);
    uint64_t sel_at = tag_smallint(90);
    uint64_t sel_at_put = tag_smallint(91);
    uint64_t sel_basicNewSize = tag_smallint(71);
    uint64_t sel_new_size = tag_smallint(312);
    uint64_t sel_comma = tag_smallint(313);

    uint64_t *prim_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 1);

    uint64_t *cm_lt = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
    OBJ_FIELD(cm_lt, CM_PRIMITIVE) = tag_smallint(PRIM_SMALLINT_LT);
    OBJ_FIELD(cm_lt, CM_NUM_ARGS) = tag_smallint(1);
    OBJ_FIELD(cm_lt, CM_NUM_TEMPS) = tag_smallint(0);
    OBJ_FIELD(cm_lt, CM_LITERALS) = tagged_nil();
    OBJ_FIELD(cm_lt, CM_BYTECODES) = (uint64_t)prim_bc;

    uint64_t *cm_plus = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
    OBJ_FIELD(cm_plus, CM_PRIMITIVE) = tag_smallint(PRIM_SMALLINT_ADD);
    OBJ_FIELD(cm_plus, CM_NUM_ARGS) = tag_smallint(1);
    OBJ_FIELD(cm_plus, CM_NUM_TEMPS) = tag_smallint(0);
    OBJ_FIELD(cm_plus, CM_LITERALS) = tagged_nil();
    OBJ_FIELD(cm_plus, CM_BYTECODES) = (uint64_t)prim_bc;

    md_append(om, class_class, smallint_class, sel_lt, (uint64_t)cm_lt);
    md_append(om, class_class, smallint_class, sel_plus, (uint64_t)cm_plus);

    uint64_t *cm_basic_new_size = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
    OBJ_FIELD(cm_basic_new_size, CM_PRIMITIVE) = tag_smallint(PRIM_BASIC_NEW_SIZE);
    OBJ_FIELD(cm_basic_new_size, CM_NUM_ARGS) = tag_smallint(1);
    OBJ_FIELD(cm_basic_new_size, CM_NUM_TEMPS) = tag_smallint(0);
    OBJ_FIELD(cm_basic_new_size, CM_LITERALS) = tagged_nil();
    OBJ_FIELD(cm_basic_new_size, CM_BYTECODES) = (uint64_t)prim_bc;
    md_append(om, class_class, class_class, sel_basicNewSize, (uint64_t)cm_basic_new_size);

    {
        uint64_t *bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 20);
        uint8_t *p = (uint8_t *)&OBJ_FIELD(bc, 0);
        p[0] = BC_PUSH_SELF;
        p[1] = BC_PUSH_ARG;
        WRITE_U32(&p[2], 0);
        p[6] = BC_SEND_MESSAGE;
        WRITE_U32(&p[7], 0);
        WRITE_U32(&p[11], 1);
        p[15] = BC_RETURN;

        uint64_t *lits = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 1);
        OBJ_FIELD(lits, 0) = sel_basicNewSize;

        uint64_t *cm_new_size = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(cm_new_size, CM_PRIMITIVE) = tag_smallint(0);
        OBJ_FIELD(cm_new_size, CM_NUM_ARGS) = tag_smallint(1);
        OBJ_FIELD(cm_new_size, CM_NUM_TEMPS) = tag_smallint(0);
        OBJ_FIELD(cm_new_size, CM_LITERALS) = (uint64_t)lits;
        OBJ_FIELD(cm_new_size, CM_BYTECODES) = (uint64_t)bc;

        md_append(om, class_class, class_class, sel_new_size, (uint64_t)cm_new_size);
    }

    {
        uint64_t *caller_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 20);
        uint8_t *p = (uint8_t *)&OBJ_FIELD(caller_bc, 0);
        p[0] = BC_PUSH_SELF;
        p[1] = BC_PUSH_LITERAL;
        WRITE_U32(&p[2], 1);
        p[6] = BC_SEND_MESSAGE;
        WRITE_U32(&p[7], 0);
        WRITE_U32(&p[11], 1);
        p[15] = BC_HALT;

        uint64_t *lits = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 2);
        OBJ_FIELD(lits, 0) = sel_new_size;
        OBJ_FIELD(lits, 1) = tag_smallint(5);

        uint64_t *caller_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(caller_cm, CM_PRIMITIVE) = tag_smallint(0);
        OBJ_FIELD(caller_cm, CM_NUM_ARGS) = tag_smallint(0);
        OBJ_FIELD(caller_cm, CM_NUM_TEMPS) = tag_smallint(0);
        OBJ_FIELD(caller_cm, CM_LITERALS) = (uint64_t)lits;
        OBJ_FIELD(caller_cm, CM_BYTECODES) = (uint64_t)caller_bc;

        sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
        fp = (uint64_t *)0xCAFE;
        stack_push(&sp, stack, (uint64_t)string_class);
        activate_method(&sp, &fp, 0, (uint64_t)caller_cm, 0, 0);
        result = interpret(&sp, &fp, (uint8_t *)&OBJ_FIELD(caller_bc, 0), class_table, om, NULL);

        ASSERT_EQ(ctx, result & 3, 0, "dispatch new:: result is object ptr");
        ASSERT_EQ(ctx, OBJ_CLASS((uint64_t *)result), (uint64_t)string_class,
                  "dispatch new:: class is String");
        ASSERT_EQ(ctx, OBJ_FORMAT((uint64_t *)result), FORMAT_BYTES,
                  "dispatch new:: format is bytes");
        ASSERT_EQ(ctx, OBJ_SIZE((uint64_t *)result), 5,
                  "dispatch new:: size is 5");
    }

    {
        uint64_t *comma_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 220);
        uint8_t *p = (uint8_t *)&OBJ_FIELD(comma_bc, 0);
        int i = 0;

#define EMIT(op) p[i++] = (uint8_t)(op)
#define EMIT_U32(v)       \
    do                    \
    {                     \
        WRITE_U32(&p[i], (v)); \
        i += 4;           \
    } while (0)

        EMIT(BC_PUSH_LITERAL);
        EMIT_U32(0);
        EMIT(BC_PUSH_LITERAL);
        EMIT_U32(1);
        EMIT(BC_SEND_MESSAGE);
        EMIT_U32(2);
        EMIT_U32(1);
        EMIT(BC_STORE_TEMP);
        EMIT_U32(0);

        EMIT(BC_PUSH_TEMP);
        EMIT_U32(0);
        EMIT(BC_PUSH_LITERAL);
        EMIT_U32(3);
        EMIT(BC_PUSH_SELF);
        EMIT(BC_PUSH_LITERAL);
        EMIT_U32(3);
        EMIT(BC_SEND_MESSAGE);
        EMIT_U32(4);
        EMIT_U32(1);
        EMIT(BC_SEND_MESSAGE);
        EMIT_U32(5);
        EMIT_U32(2);
        EMIT(BC_POP);

        EMIT(BC_PUSH_TEMP);
        EMIT_U32(0);
        EMIT(BC_PUSH_LITERAL);
        EMIT_U32(6);
        EMIT(BC_PUSH_SELF);
        EMIT(BC_PUSH_LITERAL);
        EMIT_U32(6);
        EMIT(BC_SEND_MESSAGE);
        EMIT_U32(4);
        EMIT_U32(1);
        EMIT(BC_SEND_MESSAGE);
        EMIT_U32(5);
        EMIT_U32(2);
        EMIT(BC_POP);

        EMIT(BC_PUSH_TEMP);
        EMIT_U32(0);
        EMIT(BC_PUSH_LITERAL);
        EMIT_U32(7);
        EMIT(BC_PUSH_ARG);
        EMIT_U32(0);
        EMIT(BC_PUSH_LITERAL);
        EMIT_U32(3);
        EMIT(BC_SEND_MESSAGE);
        EMIT_U32(4);
        EMIT_U32(1);
        EMIT(BC_SEND_MESSAGE);
        EMIT_U32(5);
        EMIT_U32(2);
        EMIT(BC_POP);

        EMIT(BC_PUSH_TEMP);
        EMIT_U32(0);
        EMIT(BC_PUSH_LITERAL);
        EMIT_U32(8);
        EMIT(BC_PUSH_ARG);
        EMIT_U32(0);
        EMIT(BC_PUSH_LITERAL);
        EMIT_U32(6);
        EMIT(BC_SEND_MESSAGE);
        EMIT_U32(4);
        EMIT_U32(1);
        EMIT(BC_SEND_MESSAGE);
        EMIT_U32(5);
        EMIT_U32(2);
        EMIT(BC_POP);

        EMIT(BC_PUSH_TEMP);
        EMIT_U32(0);
        EMIT(BC_PUSH_LITERAL);
        EMIT_U32(1);
        EMIT(BC_PUSH_ARG);
        EMIT_U32(0);
        EMIT(BC_PUSH_LITERAL);
        EMIT_U32(7);
        EMIT(BC_SEND_MESSAGE);
        EMIT_U32(4);
        EMIT_U32(1);
        EMIT(BC_SEND_MESSAGE);
        EMIT_U32(5);
        EMIT_U32(2);
        EMIT(BC_POP);

        EMIT(BC_PUSH_TEMP);
        EMIT_U32(0);
        EMIT(BC_RETURN);

#undef EMIT
#undef EMIT_U32

        uint64_t *comma_lits = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 9);
        OBJ_FIELD(comma_lits, 0) = (uint64_t)string_class;
        OBJ_FIELD(comma_lits, 1) = tag_smallint(5);
        OBJ_FIELD(comma_lits, 2) = sel_new_size;
        OBJ_FIELD(comma_lits, 3) = tag_smallint(1);
        OBJ_FIELD(comma_lits, 4) = sel_at;
        OBJ_FIELD(comma_lits, 5) = sel_at_put;
        OBJ_FIELD(comma_lits, 6) = tag_smallint(2);
        OBJ_FIELD(comma_lits, 7) = tag_smallint(3);
        OBJ_FIELD(comma_lits, 8) = tag_smallint(4);

        uint64_t *cm_comma = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(cm_comma, CM_PRIMITIVE) = tag_smallint(0);
        OBJ_FIELD(cm_comma, CM_NUM_ARGS) = tag_smallint(1);
        OBJ_FIELD(cm_comma, CM_NUM_TEMPS) = tag_smallint(1);
        OBJ_FIELD(cm_comma, CM_LITERALS) = (uint64_t)comma_lits;
        OBJ_FIELD(cm_comma, CM_BYTECODES) = (uint64_t)comma_bc;
        md_append(om, class_class, string_class, sel_comma, (uint64_t)cm_comma);

        uint64_t *s1 = make_bytes_obj(om, class_class, string_class, "he", 2);
        uint64_t *s2 = make_bytes_obj(om, class_class, string_class, "llo", 3);

        uint64_t *caller_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 20);
        p = (uint8_t *)&OBJ_FIELD(caller_bc, 0);
        p[0] = BC_PUSH_SELF;
        p[1] = BC_PUSH_LITERAL;
        WRITE_U32(&p[2], 1);
        p[6] = BC_SEND_MESSAGE;
        WRITE_U32(&p[7], 0);
        WRITE_U32(&p[11], 1);
        p[15] = BC_HALT;

        uint64_t *caller_lits = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 2);
        OBJ_FIELD(caller_lits, 0) = sel_comma;
        OBJ_FIELD(caller_lits, 1) = (uint64_t)s2;

        uint64_t *caller_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(caller_cm, CM_PRIMITIVE) = tag_smallint(0);
        OBJ_FIELD(caller_cm, CM_NUM_ARGS) = tag_smallint(0);
        OBJ_FIELD(caller_cm, CM_NUM_TEMPS) = tag_smallint(0);
        OBJ_FIELD(caller_cm, CM_LITERALS) = (uint64_t)caller_lits;
        OBJ_FIELD(caller_cm, CM_BYTECODES) = (uint64_t)caller_bc;

        sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
        fp = (uint64_t *)0xCAFE;
        stack_push(&sp, stack, (uint64_t)s1);
        activate_method(&sp, &fp, 0, (uint64_t)caller_cm, 0, 0);
        result = interpret(&sp, &fp, (uint8_t *)&OBJ_FIELD(caller_bc, 0), class_table, om, NULL);

        ASSERT_EQ(ctx, result & 3, 0, "dispatch comma: result is object ptr");
        uint64_t *r = (uint64_t *)result;
        ASSERT_EQ(ctx, OBJ_CLASS(r), (uint64_t)string_class, "dispatch comma: class is String");
        ASSERT_EQ(ctx, OBJ_FORMAT(r), FORMAT_BYTES, "dispatch comma: format is bytes");
        ASSERT_EQ(ctx, OBJ_SIZE(r), 5, "dispatch comma: size is 5");

        uint8_t *rb = (uint8_t *)&OBJ_FIELD(r, 0);
        ASSERT_EQ(ctx, rb[0], (uint8_t)'h', "dispatch comma: byte 1");
        ASSERT_EQ(ctx, rb[1], (uint8_t)'e', "dispatch comma: byte 2");
        ASSERT_EQ(ctx, rb[2], (uint8_t)'l', "dispatch comma: byte 3");
        ASSERT_EQ(ctx, rb[3], (uint8_t)'l', "dispatch comma: byte 4");
        ASSERT_EQ(ctx, rb[4], (uint8_t)'o', "dispatch comma: byte 5");
    }
}

