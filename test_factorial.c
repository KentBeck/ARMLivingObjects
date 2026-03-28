#include "test_defs.h"

void test_factorial(TestContext *ctx)
{
    uint64_t *om=ctx->om;
    uint64_t *class_class=ctx->class_class;
    uint64_t *smallint_class=ctx->smallint_class;
    uint64_t *block_class=ctx->block_class;
    uint64_t *test_class=ctx->test_class;
    uint64_t receiver=ctx->receiver;
    uint64_t method=ctx->method;
    uint64_t *class_table=ctx->class_table;
    uint64_t *stack=ctx->stack;
    (void)om;(void)class_class;(void)smallint_class;
    (void)block_class;(void)test_class;(void)receiver;
    (void)method;(void)class_table;(void)stack;
    uint64_t *sp;
    uint64_t *fp;
    uint64_t result;

    // --- Section 13: Factorial ---
    // SmallInteger >> factorial
    //     ^ (self = 1) ifTrue: [1] ifFalse: [self * (self - 1) factorial]
    {
        uint64_t sel_eq2 = tag_smallint(53);
        uint64_t sel_minus2 = tag_smallint(51);
        uint64_t sel_mul2 = tag_smallint(54);
        uint64_t sel_value2 = tag_smallint(60);
        uint64_t sel_ifTF2 = tag_smallint(70);
        uint64_t sel_fact = tag_smallint(80);

        // --- True block: [1] ---
        uint64_t *tb_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 16);
        uint8_t *tb = (uint8_t *)&OBJ_FIELD(tb_bc, 0);
        tb[0] = BC_PUSH_LITERAL;
        WRITE_U32(&tb[1], 0);
        tb[5] = BC_RETURN;

        uint64_t *tb_lits = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 1);
        OBJ_FIELD(tb_lits, 0) = tag_smallint(1);
        uint64_t *tb_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(tb_cm, CM_PRIMITIVE) = tag_smallint(0);
        OBJ_FIELD(tb_cm, CM_NUM_ARGS) = tag_smallint(0);
        OBJ_FIELD(tb_cm, CM_NUM_TEMPS) = tag_smallint(0);
        OBJ_FIELD(tb_cm, CM_LITERALS) = (uint64_t)tb_lits;
        OBJ_FIELD(tb_cm, CM_BYTECODES) = (uint64_t)tb_bc;

        // --- False block: [self * (self - 1) factorial] ---
        // PUSH_SELF, PUSH_SELF, PUSH_LITERAL 0 (=1), SEND #- 1,
        // SEND #factorial 0, SEND #* 1, RETURN
        uint64_t *fb_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 40);
        uint8_t *fb = (uint8_t *)&OBJ_FIELD(fb_bc, 0);
        fb[0] = BC_PUSH_SELF;
        fb[1] = BC_PUSH_SELF;
        fb[2] = BC_PUSH_LITERAL;
        WRITE_U32(&fb[3], 0); // literal 0 = 1
        fb[7] = BC_SEND_MESSAGE;
        WRITE_U32(&fb[8], 1);  // selector 1 = sel_minus
        WRITE_U32(&fb[12], 1); // 1 arg
        fb[16] = BC_SEND_MESSAGE;
        WRITE_U32(&fb[17], 2); // selector 2 = sel_fact
        WRITE_U32(&fb[21], 0); // 0 args
        fb[25] = BC_SEND_MESSAGE;
        WRITE_U32(&fb[26], 3); // selector 3 = sel_mul
        WRITE_U32(&fb[30], 1); // 1 arg
        fb[34] = BC_RETURN;

        uint64_t *fb_lits = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 4);
        OBJ_FIELD(fb_lits, 0) = tag_smallint(1);
        OBJ_FIELD(fb_lits, 1) = sel_minus2;
        OBJ_FIELD(fb_lits, 2) = sel_fact;
        OBJ_FIELD(fb_lits, 3) = sel_mul2;
        uint64_t *fb_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(fb_cm, CM_PRIMITIVE) = tag_smallint(0);
        OBJ_FIELD(fb_cm, CM_NUM_ARGS) = tag_smallint(0);
        OBJ_FIELD(fb_cm, CM_NUM_TEMPS) = tag_smallint(0);
        OBJ_FIELD(fb_cm, CM_LITERALS) = (uint64_t)fb_lits;
        OBJ_FIELD(fb_cm, CM_BYTECODES) = (uint64_t)fb_bc;

        // --- factorial method ---
        // PUSH_SELF, PUSH_LITERAL 0 (=1), SEND #= 1,
        // PUSH_CLOSURE 1 (true block), PUSH_CLOSURE 2 (false block),
        // SEND #ifTrue:ifFalse: 2, RETURN
        uint64_t *fact_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 40);
        uint8_t *fbc = (uint8_t *)&OBJ_FIELD(fact_bc, 0);
        fbc[0] = BC_PUSH_SELF;
        fbc[1] = BC_PUSH_LITERAL;
        WRITE_U32(&fbc[2], 0); // literal 0 = 1
        fbc[6] = BC_SEND_MESSAGE;
        WRITE_U32(&fbc[7], 1);  // selector 1 = sel_eq
        WRITE_U32(&fbc[11], 1); // 1 arg
        fbc[15] = BC_PUSH_CLOSURE;
        WRITE_U32(&fbc[16], 2); // literal 2 = tb_cm
        fbc[20] = BC_PUSH_CLOSURE;
        WRITE_U32(&fbc[21], 3); // literal 3 = fb_cm
        fbc[25] = BC_SEND_MESSAGE;
        WRITE_U32(&fbc[26], 4); // selector 4 = sel_ifTF
        WRITE_U32(&fbc[30], 2); // 2 args
        fbc[34] = BC_RETURN;

        uint64_t *fact_lits = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 5);
        OBJ_FIELD(fact_lits, 0) = tag_smallint(1);
        OBJ_FIELD(fact_lits, 1) = sel_eq2;
        OBJ_FIELD(fact_lits, 2) = (uint64_t)tb_cm;
        OBJ_FIELD(fact_lits, 3) = (uint64_t)fb_cm;
        OBJ_FIELD(fact_lits, 4) = sel_ifTF2;
        uint64_t *fact_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(fact_cm, CM_PRIMITIVE) = tag_smallint(0);
        OBJ_FIELD(fact_cm, CM_NUM_ARGS) = tag_smallint(0);
        OBJ_FIELD(fact_cm, CM_NUM_TEMPS) = tag_smallint(0);
        OBJ_FIELD(fact_cm, CM_LITERALS) = (uint64_t)fact_lits;
        OBJ_FIELD(fact_cm, CM_BYTECODES) = (uint64_t)fact_bc;

        // Add #factorial to SmallInteger's method dict (rebuild with 12 entries)
        uint64_t *old_md = (uint64_t *)OBJ_FIELD(smallint_class, CLASS_METHOD_DICT);
        uint64_t *new_md = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 12);
        for (int i = 0; i < 10; i++)
            OBJ_FIELD(new_md, i) = OBJ_FIELD(old_md, i);
        OBJ_FIELD(new_md, 10) = sel_fact;
        OBJ_FIELD(new_md, 11) = (uint64_t)fact_cm;
        OBJ_FIELD(smallint_class, CLASS_METHOD_DICT) = (uint64_t)new_md;

        // Caller: PUSH_LITERAL 0 (= N), SEND #factorial 0, HALT
        uint64_t *run_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 16);
        uint8_t *rb = (uint8_t *)&OBJ_FIELD(run_bc, 0);
        rb[0] = BC_PUSH_LITERAL;
        WRITE_U32(&rb[1], 0);
        rb[5] = BC_SEND_MESSAGE;
        WRITE_U32(&rb[6], 1);  // selector 1 = sel_fact
        WRITE_U32(&rb[10], 0); // 0 args
        rb[14] = BC_HALT;

        // 1 factorial = 1
        uint64_t *run_lits = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 2);
        OBJ_FIELD(run_lits, 0) = tag_smallint(1);
        OBJ_FIELD(run_lits, 1) = sel_fact;
        uint64_t *run_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(run_cm, CM_PRIMITIVE) = tag_smallint(0);
        OBJ_FIELD(run_cm, CM_NUM_ARGS) = tag_smallint(0);
        OBJ_FIELD(run_cm, CM_NUM_TEMPS) = tag_smallint(0);
        OBJ_FIELD(run_cm, CM_LITERALS) = (uint64_t)run_lits;
        OBJ_FIELD(run_cm, CM_BYTECODES) = (uint64_t)run_bc;

        sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
        fp = (uint64_t *)0xCAFE;
        stack_push(&sp, stack, receiver);
        activate_method(&sp, &fp, 0, (uint64_t)run_cm, 0, 0);
        result = interpret(&sp, &fp,
                                    (uint8_t *)&OBJ_FIELD(run_bc, 0),
                                    class_table, om, NULL);
        ASSERT_EQ(ctx, result, tag_smallint(1), "1 factorial = 1");

        // 2 factorial = 2
        uint64_t *run_lits2 = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 2);
        OBJ_FIELD(run_lits2, 0) = tag_smallint(2);
        OBJ_FIELD(run_lits2, 1) = sel_fact;
        uint64_t *run_cm2 = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(run_cm2, CM_PRIMITIVE) = tag_smallint(0);
        OBJ_FIELD(run_cm2, CM_NUM_ARGS) = tag_smallint(0);
        OBJ_FIELD(run_cm2, CM_NUM_TEMPS) = tag_smallint(0);
        OBJ_FIELD(run_cm2, CM_LITERALS) = (uint64_t)run_lits2;
        OBJ_FIELD(run_cm2, CM_BYTECODES) = (uint64_t)run_bc;

        sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
        fp = (uint64_t *)0xCAFE;
        stack_push(&sp, stack, receiver);
        activate_method(&sp, &fp, 0, (uint64_t)run_cm2, 0, 0);
        result = interpret(&sp, &fp,
                           (uint8_t *)&OBJ_FIELD(run_bc, 0),
                           class_table, om, NULL);
        ASSERT_EQ(ctx, result, tag_smallint(2), "2 factorial = 2");

        // 5 factorial = 120
        uint64_t *run_lits5 = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 2);
        OBJ_FIELD(run_lits5, 0) = tag_smallint(5);
        OBJ_FIELD(run_lits5, 1) = sel_fact;
        uint64_t *run_cm5 = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(run_cm5, CM_PRIMITIVE) = tag_smallint(0);
        OBJ_FIELD(run_cm5, CM_NUM_ARGS) = tag_smallint(0);
        OBJ_FIELD(run_cm5, CM_NUM_TEMPS) = tag_smallint(0);
        OBJ_FIELD(run_cm5, CM_LITERALS) = (uint64_t)run_lits5;
        OBJ_FIELD(run_cm5, CM_BYTECODES) = (uint64_t)run_bc;

        sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
        fp = (uint64_t *)0xCAFE;
        stack_push(&sp, stack, receiver);
        activate_method(&sp, &fp, 0, (uint64_t)run_cm5, 0, 0);
        result = interpret(&sp, &fp,
                           (uint8_t *)&OBJ_FIELD(run_bc, 0),
                           class_table, om, NULL);
        ASSERT_EQ(ctx, result, tag_smallint(120), "5 factorial = 120");

        // 10 factorial = 3628800
        uint64_t *run_lits10 = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 2);
        OBJ_FIELD(run_lits10, 0) = tag_smallint(10);
        OBJ_FIELD(run_lits10, 1) = sel_fact;
        uint64_t *run_cm10 = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(run_cm10, CM_PRIMITIVE) = tag_smallint(0);
        OBJ_FIELD(run_cm10, CM_NUM_ARGS) = tag_smallint(0);
        OBJ_FIELD(run_cm10, CM_NUM_TEMPS) = tag_smallint(0);
        OBJ_FIELD(run_cm10, CM_LITERALS) = (uint64_t)run_lits10;
        OBJ_FIELD(run_cm10, CM_BYTECODES) = (uint64_t)run_bc;

        sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
        fp = (uint64_t *)0xCAFE;
        stack_push(&sp, stack, receiver);
        activate_method(&sp, &fp, 0, (uint64_t)run_cm10, 0, 0);
        result = interpret(&sp, &fp,
                           (uint8_t *)&OBJ_FIELD(run_bc, 0),
                           class_table, om, NULL);
        ASSERT_EQ(ctx, result, tag_smallint(3628800), "10 factorial = 3628800");
    }


    ctx->smallint_class=smallint_class;
    memcpy(ctx->class_table,class_table,sizeof(ctx->class_table));
}
