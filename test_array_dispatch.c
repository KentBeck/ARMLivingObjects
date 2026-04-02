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

void test_array_dispatch(TestContext *ctx)
{
    uint64_t *om = ctx->om;
    uint64_t *class_class = ctx->class_class;
    uint64_t *class_table = ctx->class_table;
    uint64_t *stack = ctx->stack;
    uint64_t *sp;
    uint64_t *fp;
    uint64_t result;

    uint64_t sel_basicNewSize = tag_smallint(71);
    uint64_t sel_new_size = tag_smallint(410);
    uint64_t sel_at = tag_smallint(90);
    uint64_t sel_at_put = tag_smallint(91);
    uint64_t sel_size = tag_smallint(72);

    uint64_t *array_class = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 4);
    OBJ_FIELD(array_class, CLASS_SUPERCLASS) = tagged_nil();
    OBJ_FIELD(array_class, CLASS_METHOD_DICT) = tagged_nil();
    OBJ_FIELD(array_class, CLASS_INST_SIZE) = tag_smallint(0);
    OBJ_FIELD(array_class, CLASS_INST_FORMAT) = tag_smallint(FORMAT_INDEXABLE);

    uint64_t *prim_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 1);

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

    uint64_t *cm_size = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
    OBJ_FIELD(cm_size, CM_PRIMITIVE) = tag_smallint(PRIM_SIZE);
    OBJ_FIELD(cm_size, CM_NUM_ARGS) = tag_smallint(0);
    OBJ_FIELD(cm_size, CM_NUM_TEMPS) = tag_smallint(0);
    OBJ_FIELD(cm_size, CM_LITERALS) = tagged_nil();
    OBJ_FIELD(cm_size, CM_BYTECODES) = (uint64_t)prim_bc;
    md_append(om, class_class, array_class, sel_size, (uint64_t)cm_size);

    uint64_t *cm_at = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
    OBJ_FIELD(cm_at, CM_PRIMITIVE) = tag_smallint(PRIM_AT);
    OBJ_FIELD(cm_at, CM_NUM_ARGS) = tag_smallint(1);
    OBJ_FIELD(cm_at, CM_NUM_TEMPS) = tag_smallint(0);
    OBJ_FIELD(cm_at, CM_LITERALS) = tagged_nil();
    OBJ_FIELD(cm_at, CM_BYTECODES) = (uint64_t)prim_bc;
    md_append(om, class_class, array_class, sel_at, (uint64_t)cm_at);

    uint64_t *cm_at_put = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
    OBJ_FIELD(cm_at_put, CM_PRIMITIVE) = tag_smallint(PRIM_AT_PUT);
    OBJ_FIELD(cm_at_put, CM_NUM_ARGS) = tag_smallint(2);
    OBJ_FIELD(cm_at_put, CM_NUM_TEMPS) = tag_smallint(0);
    OBJ_FIELD(cm_at_put, CM_LITERALS) = tagged_nil();
    OBJ_FIELD(cm_at_put, CM_BYTECODES) = (uint64_t)prim_bc;
    md_append(om, class_class, array_class, sel_at_put, (uint64_t)cm_at_put);

    uint64_t *arr;
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
        OBJ_FIELD(lits, 1) = tag_smallint(3);

        uint64_t *caller_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(caller_cm, CM_PRIMITIVE) = tag_smallint(0);
        OBJ_FIELD(caller_cm, CM_NUM_ARGS) = tag_smallint(0);
        OBJ_FIELD(caller_cm, CM_NUM_TEMPS) = tag_smallint(0);
        OBJ_FIELD(caller_cm, CM_LITERALS) = (uint64_t)lits;
        OBJ_FIELD(caller_cm, CM_BYTECODES) = (uint64_t)caller_bc;

        sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
        fp = (uint64_t *)0xCAFE;
        stack_push(&sp, stack, (uint64_t)array_class);
        activate_method(&sp, &fp, 0, (uint64_t)caller_cm, 0, 0);
        result = interpret(&sp, &fp, (uint8_t *)&OBJ_FIELD(caller_bc, 0), class_table, om, NULL);

        ASSERT_EQ(ctx, result & 3, 0, "dispatch array new:: result is object ptr");
        arr = (uint64_t *)result;
        ASSERT_EQ(ctx, OBJ_CLASS(arr), (uint64_t)array_class, "dispatch array new:: class");
        ASSERT_EQ(ctx, OBJ_FORMAT(arr), FORMAT_INDEXABLE, "dispatch array new:: format");
        ASSERT_EQ(ctx, OBJ_SIZE(arr), 3, "dispatch array new:: size");
    }

    {
        uint64_t *caller_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 24);
        uint8_t *p = (uint8_t *)&OBJ_FIELD(caller_bc, 0);
        p[0] = BC_PUSH_SELF;
        p[1] = BC_PUSH_LITERAL;
        WRITE_U32(&p[2], 1);
        p[6] = BC_PUSH_LITERAL;
        WRITE_U32(&p[7], 2);
        p[11] = BC_SEND_MESSAGE;
        WRITE_U32(&p[12], 0);
        WRITE_U32(&p[16], 2);
        p[20] = BC_HALT;

        uint64_t *lits = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 3);
        OBJ_FIELD(lits, 0) = sel_at_put;
        OBJ_FIELD(lits, 1) = tag_smallint(2);
        OBJ_FIELD(lits, 2) = tag_smallint(77);

        uint64_t *caller_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(caller_cm, CM_PRIMITIVE) = tag_smallint(0);
        OBJ_FIELD(caller_cm, CM_NUM_ARGS) = tag_smallint(0);
        OBJ_FIELD(caller_cm, CM_NUM_TEMPS) = tag_smallint(0);
        OBJ_FIELD(caller_cm, CM_LITERALS) = (uint64_t)lits;
        OBJ_FIELD(caller_cm, CM_BYTECODES) = (uint64_t)caller_bc;

        sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
        fp = (uint64_t *)0xCAFE;
        stack_push(&sp, stack, (uint64_t)arr);
        activate_method(&sp, &fp, 0, (uint64_t)caller_cm, 0, 0);
        result = interpret(&sp, &fp, (uint8_t *)&OBJ_FIELD(caller_bc, 0), class_table, om, NULL);
        (void)result;
        ASSERT_EQ(ctx, OBJ_FIELD(arr, 1), tag_smallint(77), "dispatch array at:put: stores value");
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
        OBJ_FIELD(lits, 0) = sel_at;
        OBJ_FIELD(lits, 1) = tag_smallint(2);

        uint64_t *caller_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(caller_cm, CM_PRIMITIVE) = tag_smallint(0);
        OBJ_FIELD(caller_cm, CM_NUM_ARGS) = tag_smallint(0);
        OBJ_FIELD(caller_cm, CM_NUM_TEMPS) = tag_smallint(0);
        OBJ_FIELD(caller_cm, CM_LITERALS) = (uint64_t)lits;
        OBJ_FIELD(caller_cm, CM_BYTECODES) = (uint64_t)caller_bc;

        sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
        fp = (uint64_t *)0xCAFE;
        stack_push(&sp, stack, (uint64_t)arr);
        activate_method(&sp, &fp, 0, (uint64_t)caller_cm, 0, 0);
        result = interpret(&sp, &fp, (uint8_t *)&OBJ_FIELD(caller_bc, 0), class_table, om, NULL);
        ASSERT_EQ(ctx, result, tag_smallint(77), "dispatch array at: reads stored value");
    }

    {
        uint64_t *caller_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 16);
        uint8_t *p = (uint8_t *)&OBJ_FIELD(caller_bc, 0);
        p[0] = BC_PUSH_SELF;
        p[1] = BC_SEND_MESSAGE;
        WRITE_U32(&p[2], 0);
        WRITE_U32(&p[6], 0);
        p[10] = BC_HALT;

        uint64_t *lits = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 1);
        OBJ_FIELD(lits, 0) = sel_size;

        uint64_t *caller_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(caller_cm, CM_PRIMITIVE) = tag_smallint(0);
        OBJ_FIELD(caller_cm, CM_NUM_ARGS) = tag_smallint(0);
        OBJ_FIELD(caller_cm, CM_NUM_TEMPS) = tag_smallint(0);
        OBJ_FIELD(caller_cm, CM_LITERALS) = (uint64_t)lits;
        OBJ_FIELD(caller_cm, CM_BYTECODES) = (uint64_t)caller_bc;

        sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
        fp = (uint64_t *)0xCAFE;
        stack_push(&sp, stack, (uint64_t)arr);
        activate_method(&sp, &fp, 0, (uint64_t)caller_cm, 0, 0);
        result = interpret(&sp, &fp, (uint8_t *)&OBJ_FIELD(caller_bc, 0), class_table, om, NULL);
        ASSERT_EQ(ctx, result, tag_smallint(3), "dispatch array size returns 3");
    }
}
