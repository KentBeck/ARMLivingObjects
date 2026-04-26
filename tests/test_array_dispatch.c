#include "test_defs.h"
#include "smalltalk_test_support.h"

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

    stt_install_class_new_size_methods(om, class_class, sel_basicNewSize, sel_new_size);
    stt_md_append_oop(om, class_class, array_class, sel_size,
                      (uint64_t)stt_make_primitive_cm(om, class_class, PRIM_SIZE, 0));
    stt_md_append_oop(om, class_class, array_class, sel_at,
                      (uint64_t)stt_make_primitive_cm(om, class_class, PRIM_AT, 1));
    stt_md_append_oop(om, class_class, array_class, sel_at_put,
                      (uint64_t)stt_make_primitive_cm(om, class_class, PRIM_AT_PUT, 2));

    uint64_t *arr;
    {
        result = stt_send_class_new_size(ctx, class_table, om, class_class,
                                         (uint64_t)array_class, sel_new_size, 3);

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
