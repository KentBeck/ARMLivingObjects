#include "test_defs.h"

void test_dispatch(TestContext *ctx)
{
    uint64_t *om = ctx->om;
    uint64_t *class_class = ctx->class_class;
    uint64_t *smallint_class = ctx->smallint_class;
    uint64_t *block_class = ctx->block_class;
    uint64_t *test_class = ctx->test_class;
    uint64_t receiver = ctx->receiver;
    uint64_t method = ctx->method;
    uint64_t *class_table = ctx->class_table;
    uint64_t *stack = ctx->stack;
    (void)om;
    (void)class_class;
    (void)smallint_class;
    (void)block_class;
    (void)test_class;
    (void)receiver;
    (void)method;
    (void)class_table;
    (void)stack;
    uint64_t *sp;
    uint64_t *fp;
    uint64_t ip;
    uint64_t caller_fp_val = 0xBEEF;
    uint64_t caller_ip_val = 0xDEAD;
    uint64_t result;

// --- Section 10: Bytecode Dispatch Loop ---

// Helper: write a 4-byte little-endian uint32 into bytecodes
#define WRITE_U32(ptr, val)    \
    do                         \
    {                          \
        uint32_t _v = (val);   \
        memcpy((ptr), &_v, 4); \
    } while (0)

    // Test: dispatch a single PUSH_LITERAL and HALT
    // Method with 1 literal (tag_smallint(42)), bytecodes: PUSH_LITERAL 0, HALT
    {
        uint64_t *d_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 16);
        uint8_t *dbc = (uint8_t *)&OBJ_FIELD(d_bc, 0);
        dbc[0] = BC_PUSH_LITERAL;
        WRITE_U32(&dbc[1], 0); // literal index 0
        dbc[5] = BC_HALT;

        uint64_t *d_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(d_cm, CM_PRIMITIVE) = tag_smallint(0);
        OBJ_FIELD(d_cm, CM_NUM_ARGS) = tag_smallint(0);
        OBJ_FIELD(d_cm, CM_NUM_TEMPS) = tag_smallint(0);
        uint64_t *_lits_2 = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 1);
        OBJ_FIELD(_lits_2, 0) = tag_smallint(42);
        OBJ_FIELD(d_cm, CM_LITERALS) = (uint64_t)_lits_2;
        OBJ_FIELD(d_cm, CM_BYTECODES) = (uint64_t)d_bc;

        sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
        fp = 0;
        stack_push(&sp, stack, receiver);
        activate_method(&sp, &fp, 0, (uint64_t)d_cm, 0, 0);
        result = interpret(&sp, &fp, dbc, class_table, om, NULL);
        ASSERT_EQ(ctx, result, tag_smallint(42), "dispatch: PUSH_LITERAL 0 + HALT");
    }

    // Test: dispatch PUSH_LITERAL then RETURN_STACK_TOP: value returned
    {
        uint64_t *d_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 16);
        uint8_t *dbc = (uint8_t *)&OBJ_FIELD(d_bc, 0);
        dbc[0] = BC_PUSH_LITERAL;
        WRITE_U32(&dbc[1], 0);
        dbc[5] = BC_RETURN;

        uint64_t *d_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(d_cm, CM_PRIMITIVE) = tag_smallint(0);
        OBJ_FIELD(d_cm, CM_NUM_ARGS) = tag_smallint(0);
        OBJ_FIELD(d_cm, CM_NUM_TEMPS) = tag_smallint(0);
        uint64_t *_lits_3 = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 1);
        OBJ_FIELD(_lits_3, 0) = tag_smallint(99);
        OBJ_FIELD(d_cm, CM_LITERALS) = (uint64_t)_lits_3;
        OBJ_FIELD(d_cm, CM_BYTECODES) = (uint64_t)d_bc;

        sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
        fp = (uint64_t *)caller_fp_val;
        stack_push(&sp, stack, receiver);
        activate_method(&sp, &fp, caller_ip_val, (uint64_t)d_cm, 0, 0);
        result = interpret(&sp, &fp, dbc, class_table, om, NULL);
        ASSERT_EQ(ctx, result, tag_smallint(99),
                  "dispatch: PUSH_LITERAL + RETURN returns value");
    }

    // Test: dispatch PUSH_SELF then RETURN_STACK_TOP
    {
        uint64_t *d_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 8);
        uint8_t *dbc = (uint8_t *)&OBJ_FIELD(d_bc, 0);
        dbc[0] = BC_PUSH_SELF;
        dbc[1] = BC_RETURN;

        uint64_t *d_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(d_cm, CM_PRIMITIVE) = tag_smallint(0);
        OBJ_FIELD(d_cm, CM_NUM_ARGS) = tag_smallint(0);
        OBJ_FIELD(d_cm, CM_NUM_TEMPS) = tag_smallint(0);
        OBJ_FIELD(d_cm, CM_LITERALS) = tagged_nil();
        OBJ_FIELD(d_cm, CM_BYTECODES) = (uint64_t)d_bc;

        sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
        fp = (uint64_t *)caller_fp_val;
        stack_push(&sp, stack, receiver);
        activate_method(&sp, &fp, caller_ip_val, (uint64_t)d_cm, 0, 0);
        result = interpret(&sp, &fp, dbc, class_table, om, NULL);
        ASSERT_EQ(ctx, result, receiver, "dispatch: PUSH_SELF + RETURN");
    }

    // Test: dispatch PUSH_TEMP, PUSH_TEMP sequence + HALT
    {
        uint64_t *d_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 16);
        uint8_t *dbc = (uint8_t *)&OBJ_FIELD(d_bc, 0);
        dbc[0] = BC_PUSH_TEMP;
        WRITE_U32(&dbc[1], 0); // temp 0
        dbc[5] = BC_PUSH_TEMP;
        WRITE_U32(&dbc[6], 1); // temp 1
        dbc[10] = BC_HALT;

        uint64_t *d_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(d_cm, CM_PRIMITIVE) = tag_smallint(0);
        OBJ_FIELD(d_cm, CM_NUM_ARGS) = tag_smallint(0);
        OBJ_FIELD(d_cm, CM_NUM_TEMPS) = tag_smallint(2);
        OBJ_FIELD(d_cm, CM_LITERALS) = tagged_nil();
        OBJ_FIELD(d_cm, CM_BYTECODES) = (uint64_t)d_bc;

        sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
        fp = 0;
        stack_push(&sp, stack, receiver);
        activate_method(&sp, &fp, 0, (uint64_t)d_cm, 0, 2);
        // Store values into temps manually
        frame_store_temp(fp, 0, tag_smallint(10));
        frame_store_temp(fp, 1, tag_smallint(20));
        result = interpret(&sp, &fp, dbc, class_table, om, NULL);
        ASSERT_EQ(ctx, result, tag_smallint(20), "dispatch: PUSH_TEMP 0, PUSH_TEMP 1 + HALT");
    }

    // Test: dispatch STORE_TEMP then PUSH_TEMP: round-trip
    {
        uint64_t *d_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 24);
        uint8_t *dbc = (uint8_t *)&OBJ_FIELD(d_bc, 0);
        dbc[0] = BC_PUSH_LITERAL;
        WRITE_U32(&dbc[1], 0); // push literal 0 (= 77)
        dbc[5] = BC_STORE_TEMP;
        WRITE_U32(&dbc[6], 0); // store into temp 0
        dbc[10] = BC_PUSH_TEMP;
        WRITE_U32(&dbc[11], 0); // push temp 0
        dbc[15] = BC_RETURN;

        uint64_t *d_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(d_cm, CM_PRIMITIVE) = tag_smallint(0);
        OBJ_FIELD(d_cm, CM_NUM_ARGS) = tag_smallint(0);
        OBJ_FIELD(d_cm, CM_NUM_TEMPS) = tag_smallint(1);
        uint64_t *_lits_4 = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 1);
        OBJ_FIELD(_lits_4, 0) = tag_smallint(77);
        OBJ_FIELD(d_cm, CM_LITERALS) = (uint64_t)_lits_4;
        OBJ_FIELD(d_cm, CM_BYTECODES) = (uint64_t)d_bc;

        sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
        fp = (uint64_t *)caller_fp_val;
        stack_push(&sp, stack, receiver);
        activate_method(&sp, &fp, caller_ip_val, (uint64_t)d_cm, 0, 1);
        result = interpret(&sp, &fp, dbc, class_table, om, NULL);
        ASSERT_EQ(ctx, result, tag_smallint(77),
                  "dispatch: PUSH_LIT, STORE_TEMP, PUSH_TEMP, RETURN");
    }

    // Test: dispatch JUMP: IP advances to target
    // Bytecodes: JUMP +7 (skip over next PUSH_LITERAL), PUSH_LITERAL 0 (=111), PUSH_LITERAL 1 (=222), HALT
    // JUMP at offset 0, operand at 1..4, target = 0+7 = 7
    // skipped: PUSH_LITERAL at offset 5, operand at 6..9
    // PUSH_LITERAL at offset 10 is actually at target 7? No...
    // Let me recalculate: JUMP=1byte + operand=4bytes = 5 bytes.
    // PUSH_LITERAL=1byte + operand=4bytes = 5 bytes.
    // So: JUMP(offset=10), skip PUSH_LITERAL(111), land at PUSH_LITERAL(222), HALT
    {
        uint64_t *d_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 24);
        uint8_t *dbc = (uint8_t *)&OBJ_FIELD(d_bc, 0);
        dbc[0] = BC_JUMP;
        WRITE_U32(&dbc[1], 10);   // jump to offset 10
        dbc[5] = BC_PUSH_LITERAL; // offset 5 — skipped
        WRITE_U32(&dbc[6], 0);
        dbc[10] = BC_PUSH_LITERAL; // offset 10 — jump target
        WRITE_U32(&dbc[11], 1);
        dbc[15] = BC_HALT;

        uint64_t *d_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(d_cm, CM_PRIMITIVE) = tag_smallint(0);
        OBJ_FIELD(d_cm, CM_NUM_ARGS) = tag_smallint(0);
        OBJ_FIELD(d_cm, CM_NUM_TEMPS) = tag_smallint(0);
        uint64_t *_lits_5 = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 2);
        OBJ_FIELD(_lits_5, 0) = tag_smallint(111);
        OBJ_FIELD(_lits_5, 1) = tag_smallint(222);
        OBJ_FIELD(d_cm, CM_LITERALS) = (uint64_t)_lits_5;
        OBJ_FIELD(d_cm, CM_BYTECODES) = (uint64_t)d_bc;

        sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
        fp = 0;
        stack_push(&sp, stack, receiver);
        activate_method(&sp, &fp, 0, (uint64_t)d_cm, 0, 0);
        result = interpret(&sp, &fp, dbc, class_table, om, NULL);
        ASSERT_EQ(ctx, result, tag_smallint(222), "dispatch: JUMP skips to literal 1");
    }

    // Test: dispatch JUMP_IF_TRUE with tagged true: jumps
    {
        uint64_t *d_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 24);
        uint8_t *dbc = (uint8_t *)&OBJ_FIELD(d_bc, 0);
        dbc[0] = BC_PUSH_LITERAL; // push true
        WRITE_U32(&dbc[1], 0);
        dbc[5] = BC_JUMP_IF_TRUE;
        WRITE_U32(&dbc[6], 15);    // jump to offset 15
        dbc[10] = BC_PUSH_LITERAL; // skipped (push 111)
        WRITE_U32(&dbc[11], 1);
        dbc[15] = BC_PUSH_LITERAL; // jump target (push 222)
        WRITE_U32(&dbc[16], 2);
        dbc[20] = BC_HALT;

        uint64_t *d_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(d_cm, CM_PRIMITIVE) = tag_smallint(0);
        OBJ_FIELD(d_cm, CM_NUM_ARGS) = tag_smallint(0);
        OBJ_FIELD(d_cm, CM_NUM_TEMPS) = tag_smallint(0);
        uint64_t *_lits_6 = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 3);
        OBJ_FIELD(_lits_6, 0) = tagged_true();
        OBJ_FIELD(_lits_6, 1) = tag_smallint(111);
        OBJ_FIELD(_lits_6, 2) = tag_smallint(222);
        OBJ_FIELD(d_cm, CM_LITERALS) = (uint64_t)_lits_6;
        OBJ_FIELD(d_cm, CM_BYTECODES) = (uint64_t)d_bc;

        sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
        fp = 0;
        stack_push(&sp, stack, receiver);
        activate_method(&sp, &fp, 0, (uint64_t)d_cm, 0, 0);
        result = interpret(&sp, &fp, dbc, class_table, om, NULL);
        ASSERT_EQ(ctx, result, tag_smallint(222), "dispatch: JUMP_IF_TRUE with true jumps");
    }

    // Test: dispatch JUMP_IF_TRUE with tagged false: falls through
    {
        uint64_t *d_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 24);
        uint8_t *dbc = (uint8_t *)&OBJ_FIELD(d_bc, 0);
        dbc[0] = BC_PUSH_LITERAL; // push false
        WRITE_U32(&dbc[1], 0);
        dbc[5] = BC_JUMP_IF_TRUE;
        WRITE_U32(&dbc[6], 15);
        dbc[10] = BC_PUSH_LITERAL; // falls through (push 111)
        WRITE_U32(&dbc[11], 1);
        dbc[15] = BC_HALT; // stops here after fall-through

        uint64_t *d_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(d_cm, CM_PRIMITIVE) = tag_smallint(0);
        OBJ_FIELD(d_cm, CM_NUM_ARGS) = tag_smallint(0);
        OBJ_FIELD(d_cm, CM_NUM_TEMPS) = tag_smallint(0);
        uint64_t *_lits_7 = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 2);
        OBJ_FIELD(_lits_7, 0) = tagged_false();
        OBJ_FIELD(_lits_7, 1) = tag_smallint(111);
        OBJ_FIELD(d_cm, CM_LITERALS) = (uint64_t)_lits_7;
        OBJ_FIELD(d_cm, CM_BYTECODES) = (uint64_t)d_bc;

        sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
        fp = 0;
        stack_push(&sp, stack, receiver);
        activate_method(&sp, &fp, 0, (uint64_t)d_cm, 0, 0);
        result = interpret(&sp, &fp, dbc, class_table, om, NULL);
        ASSERT_EQ(ctx, result, tag_smallint(111),
                  "dispatch: JUMP_IF_TRUE with false falls through");
    }

    // Test: dispatch JUMP_IF_FALSE with tagged false: jumps
    {
        uint64_t *d_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 24);
        uint8_t *dbc = (uint8_t *)&OBJ_FIELD(d_bc, 0);
        dbc[0] = BC_PUSH_LITERAL;
        WRITE_U32(&dbc[1], 0); // push false
        dbc[5] = BC_JUMP_IF_FALSE;
        WRITE_U32(&dbc[6], 15);
        dbc[10] = BC_PUSH_LITERAL;
        WRITE_U32(&dbc[11], 1); // skipped
        dbc[15] = BC_PUSH_LITERAL;
        WRITE_U32(&dbc[16], 2); // jump target
        dbc[20] = BC_HALT;

        uint64_t *d_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(d_cm, CM_PRIMITIVE) = tag_smallint(0);
        OBJ_FIELD(d_cm, CM_NUM_ARGS) = tag_smallint(0);
        OBJ_FIELD(d_cm, CM_NUM_TEMPS) = tag_smallint(0);
        uint64_t *_lits_8 = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 3);
        OBJ_FIELD(_lits_8, 0) = tagged_false();
        OBJ_FIELD(_lits_8, 1) = tag_smallint(111);
        OBJ_FIELD(_lits_8, 2) = tag_smallint(333);
        OBJ_FIELD(d_cm, CM_LITERALS) = (uint64_t)_lits_8;
        OBJ_FIELD(d_cm, CM_BYTECODES) = (uint64_t)d_bc;

        sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
        fp = 0;
        stack_push(&sp, stack, receiver);
        activate_method(&sp, &fp, 0, (uint64_t)d_cm, 0, 0);
        result = interpret(&sp, &fp, dbc, class_table, om, NULL);
        ASSERT_EQ(ctx, result, tag_smallint(333), "dispatch: JUMP_IF_FALSE with false jumps");
    }

    // Test: dispatch JUMP_IF_FALSE with tagged true: falls through
    {
        uint64_t *d_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 24);
        uint8_t *dbc = (uint8_t *)&OBJ_FIELD(d_bc, 0);
        dbc[0] = BC_PUSH_LITERAL;
        WRITE_U32(&dbc[1], 0); // push true
        dbc[5] = BC_JUMP_IF_FALSE;
        WRITE_U32(&dbc[6], 15);
        dbc[10] = BC_PUSH_LITERAL;
        WRITE_U32(&dbc[11], 1); // falls through
        dbc[15] = BC_HALT;

        uint64_t *d_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(d_cm, CM_PRIMITIVE) = tag_smallint(0);
        OBJ_FIELD(d_cm, CM_NUM_ARGS) = tag_smallint(0);
        OBJ_FIELD(d_cm, CM_NUM_TEMPS) = tag_smallint(0);
        uint64_t *_lits_9 = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 2);
        OBJ_FIELD(_lits_9, 0) = tagged_true();
        OBJ_FIELD(_lits_9, 1) = tag_smallint(444);
        OBJ_FIELD(d_cm, CM_LITERALS) = (uint64_t)_lits_9;
        OBJ_FIELD(d_cm, CM_BYTECODES) = (uint64_t)d_bc;

        sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
        fp = 0;
        stack_push(&sp, stack, receiver);
        activate_method(&sp, &fp, 0, (uint64_t)d_cm, 0, 0);
        result = interpret(&sp, &fp, dbc, class_table, om, NULL);
        ASSERT_EQ(ctx, result, tag_smallint(444),
                  "dispatch: JUMP_IF_FALSE with true falls through");
    }

    // --- Section 11: Message Send ---

    // Create a class with a method that returns self (^self)
    // Method: PUSH_SELF, RETURN_STACK_TOP
    {
        // bytecodes for "^self"
        uint64_t *self_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 8);
        uint8_t *sbc = (uint8_t *)&OBJ_FIELD(self_bc, 0);
        sbc[0] = BC_PUSH_SELF;
        sbc[1] = BC_RETURN;

        // CompiledMethod: 0 args, 0 temps, 0 literals, bytecodes
        uint64_t *self_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(self_cm, CM_PRIMITIVE) = tag_smallint(0);
        OBJ_FIELD(self_cm, CM_NUM_ARGS) = tag_smallint(0);
        OBJ_FIELD(self_cm, CM_NUM_TEMPS) = tag_smallint(0);
        OBJ_FIELD(self_cm, CM_LITERALS) = tagged_nil();
        OBJ_FIELD(self_cm, CM_BYTECODES) = (uint64_t)self_bc;

        // selector for #yourself = tagged SmallInt 10
        uint64_t sel_yourself = tag_smallint(10);

        // method dict with one entry
        uint64_t *send_md = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 2);
        OBJ_FIELD(send_md, 0) = sel_yourself;
        OBJ_FIELD(send_md, 1) = (uint64_t)self_cm;

        // class with this method dict
        uint64_t *send_class = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 4);
        OBJ_FIELD(send_class, CLASS_SUPERCLASS) = tagged_nil();
        OBJ_FIELD(send_class, CLASS_METHOD_DICT) = (uint64_t)send_md;
        OBJ_FIELD(send_class, CLASS_INST_SIZE) = tag_smallint(0);
        OBJ_FIELD(send_class, CLASS_INST_FORMAT) = tag_smallint(FORMAT_FIELDS);

        // instance of this class
        uint64_t *send_obj = om_alloc(om, (uint64_t)send_class, FORMAT_FIELDS, 0);

        // Caller method: PUSH_SELF, SEND #yourself (0 args), RETURN
        // The caller's literal 0 = sel_yourself
        uint64_t *caller_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 16);
        uint8_t *cbc = (uint8_t *)&OBJ_FIELD(caller_bc, 0);
        cbc[0] = BC_PUSH_SELF; // push receiver as send target
        cbc[1] = BC_SEND_MESSAGE;
        WRITE_U32(&cbc[2], 0); // selector index 0 (= sel_yourself)
        WRITE_U32(&cbc[6], 0); // arg count 0
        cbc[10] = BC_RETURN;

        uint64_t *caller_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(caller_cm, CM_PRIMITIVE) = tag_smallint(0);
        OBJ_FIELD(caller_cm, CM_NUM_ARGS) = tag_smallint(0);
        OBJ_FIELD(caller_cm, CM_NUM_TEMPS) = tag_smallint(0);
        uint64_t *_lits_10 = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 1);
        OBJ_FIELD(_lits_10, 0) = sel_yourself;
        OBJ_FIELD(caller_cm, CM_LITERALS) = (uint64_t)_lits_10;
        OBJ_FIELD(caller_cm, CM_BYTECODES) = (uint64_t)caller_bc;

        sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
        fp = (uint64_t *)0xCAFE; // caller FP sentinel
        stack_push(&sp, stack, (uint64_t)send_obj);
        activate_method(&sp, &fp, 0, (uint64_t)caller_cm, 0, 0);
        result = interpret(&sp, &fp, (uint8_t *)&OBJ_FIELD(caller_bc, 0), class_table, om, NULL);
        ASSERT_EQ(ctx, result, (uint64_t)send_obj,
                  "SEND 0-arg: self yourself returns self");
    }

    // Test: send a 1-arg message
    // Method: ^arg (push temp 0 which is the arg, return)
    // Wait — args are above the frame. In our frame layout, arg0 is at FP+2*W.
    // For dispatch, we'd need a PUSH_ARG bytecode or unified temp/arg indexing.
    // For now, test with a method that just returns self to keep it simple.

    // Test: full scenario: create object, send message, method pushes inst var, returns
    {
        // Class with 1 inst var
        uint64_t *pt_class = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 4);
        OBJ_FIELD(pt_class, CLASS_SUPERCLASS) = tagged_nil();
        OBJ_FIELD(pt_class, CLASS_INST_SIZE) = tag_smallint(1);
        OBJ_FIELD(pt_class, CLASS_INST_FORMAT) = tag_smallint(FORMAT_FIELDS);

        // Method #x: push inst var 0, return
        uint64_t *x_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 16);
        uint8_t *xbc = (uint8_t *)&OBJ_FIELD(x_bc, 0);
        xbc[0] = BC_PUSH_INST_VAR;
        WRITE_U32(&xbc[1], 0); // field 0
        xbc[5] = BC_RETURN;

        uint64_t *x_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(x_cm, CM_PRIMITIVE) = tag_smallint(0);
        OBJ_FIELD(x_cm, CM_NUM_ARGS) = tag_smallint(0);
        OBJ_FIELD(x_cm, CM_NUM_TEMPS) = tag_smallint(0);
        OBJ_FIELD(x_cm, CM_LITERALS) = tagged_nil();
        OBJ_FIELD(x_cm, CM_BYTECODES) = (uint64_t)x_bc;

        uint64_t sel_x = tag_smallint(20);
        uint64_t *pt_md = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 2);
        OBJ_FIELD(pt_md, 0) = sel_x;
        OBJ_FIELD(pt_md, 1) = (uint64_t)x_cm;
        OBJ_FIELD(pt_class, CLASS_METHOD_DICT) = (uint64_t)pt_md;

        // Instance with x=42
        uint64_t *pt_obj = om_alloc(om, (uint64_t)pt_class, FORMAT_FIELDS, 1);
        OBJ_FIELD(pt_obj, 0) = tag_smallint(42);

        // Caller: push self, send #x, return
        uint64_t *c2_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 16);
        uint8_t *c2b = (uint8_t *)&OBJ_FIELD(c2_bc, 0);
        c2b[0] = BC_PUSH_SELF;
        c2b[1] = BC_SEND_MESSAGE;
        WRITE_U32(&c2b[2], 0); // selector index 0
        WRITE_U32(&c2b[6], 0); // 0 args
        c2b[10] = BC_RETURN;

        uint64_t *c2_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(c2_cm, CM_PRIMITIVE) = tag_smallint(0);
        OBJ_FIELD(c2_cm, CM_NUM_ARGS) = tag_smallint(0);
        OBJ_FIELD(c2_cm, CM_NUM_TEMPS) = tag_smallint(0);
        uint64_t *_lits_11 = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 1);
        OBJ_FIELD(_lits_11, 0) = sel_x;
        OBJ_FIELD(c2_cm, CM_LITERALS) = (uint64_t)_lits_11;
        OBJ_FIELD(c2_cm, CM_BYTECODES) = (uint64_t)c2_bc;

        sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
        fp = (uint64_t *)0xCAFE;
        stack_push(&sp, stack, (uint64_t)pt_obj);
        activate_method(&sp, &fp, 0, (uint64_t)c2_cm, 0, 0);
        result = interpret(&sp, &fp, (uint8_t *)&OBJ_FIELD(c2_bc, 0), class_table, om, NULL);
        ASSERT_EQ(ctx, result, tag_smallint(42),
                  "SEND: obj x returns inst var 0 (42)");
    }

    // Test: send a 1-arg message
    // Class with method #add: that pushes inst var 0, pushes arg, adds, returns
    // But we don't have SmallInteger + as a bytecode yet.
    // Simpler: method #identity: that just returns the arg (push arg 0, return)
    // Arg 0 is at FP+2*W. We need to access it. In our frame layout,
    // arg 0 (last pushed) is at FP+2*W = frame_arg(fp, 0).
    // We can use PUSH_TEMP with a negative trick... but our PUSH_TEMP only
    // reads below FP. We need args to be accessible.
    // Solution: treat arg indices as temp indices where temp index for arg N
    // = -(N+1) mapped above the frame. Actually, the simplest thing:
    // push the arg from the caller side as a literal in the callee.
    // No — let's just have the callee return self for now and test that
    // the 1-arg send correctly pops both arg and receiver.
    {
        // Method #withArg: just returns self (ignores arg)
        uint64_t *wa_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 8);
        uint8_t *wabc = (uint8_t *)&OBJ_FIELD(wa_bc, 0);
        wabc[0] = BC_PUSH_SELF;
        wabc[1] = BC_RETURN;

        uint64_t *wa_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(wa_cm, CM_PRIMITIVE) = tag_smallint(0);
        OBJ_FIELD(wa_cm, CM_NUM_ARGS) = tag_smallint(1);
        OBJ_FIELD(wa_cm, CM_NUM_TEMPS) = tag_smallint(0);
        OBJ_FIELD(wa_cm, CM_LITERALS) = tagged_nil();
        OBJ_FIELD(wa_cm, CM_BYTECODES) = (uint64_t)wa_bc;

        uint64_t sel_withArg = tag_smallint(30);
        uint64_t *wa_md = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 2);
        OBJ_FIELD(wa_md, 0) = sel_withArg;
        OBJ_FIELD(wa_md, 1) = (uint64_t)wa_cm;

        uint64_t *wa_class = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 4);
        OBJ_FIELD(wa_class, CLASS_SUPERCLASS) = tagged_nil();
        OBJ_FIELD(wa_class, CLASS_METHOD_DICT) = (uint64_t)wa_md;
        OBJ_FIELD(wa_class, CLASS_INST_SIZE) = tag_smallint(0);
        OBJ_FIELD(wa_class, CLASS_INST_FORMAT) = tag_smallint(FORMAT_FIELDS);

        uint64_t *wa_obj = om_alloc(om, (uint64_t)wa_class, FORMAT_FIELDS, 0);

        // Caller: push self, push literal(arg value), send #withArg: 1 arg, return
        uint64_t *c3_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 24);
        uint8_t *c3b = (uint8_t *)&OBJ_FIELD(c3_bc, 0);
        c3b[0] = BC_PUSH_SELF;    // receiver for send
        c3b[1] = BC_PUSH_LITERAL; // arg value
        WRITE_U32(&c3b[2], 0);    // literal 0 = tag_smallint(777)
        c3b[6] = BC_SEND_MESSAGE;
        WRITE_U32(&c3b[7], 1);  // selector index 1 = sel_withArg
        WRITE_U32(&c3b[11], 1); // 1 arg
        c3b[15] = BC_RETURN;

        uint64_t *c3_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(c3_cm, CM_PRIMITIVE) = tag_smallint(0);
        OBJ_FIELD(c3_cm, CM_NUM_ARGS) = tag_smallint(0);
        OBJ_FIELD(c3_cm, CM_NUM_TEMPS) = tag_smallint(0);
        uint64_t *_lits_12 = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 2);
        OBJ_FIELD(_lits_12, 0) = tag_smallint(777);
        OBJ_FIELD(_lits_12, 1) = sel_withArg;
        OBJ_FIELD(c3_cm, CM_LITERALS) = (uint64_t)_lits_12;
        OBJ_FIELD(c3_cm, CM_BYTECODES) = (uint64_t)c3_bc;

        sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
        fp = (uint64_t *)0xCAFE;
        stack_push(&sp, stack, (uint64_t)wa_obj);
        activate_method(&sp, &fp, 0, (uint64_t)c3_cm, 0, 0);
        result = interpret(&sp, &fp,
                           (uint8_t *)&OBJ_FIELD(c3_bc, 0), class_table, om, NULL);
        ASSERT_EQ(ctx, result, (uint64_t)wa_obj,
                  "SEND 1-arg: self withArg: 777 returns self");
    }

    // Test: send to superclass — method found in superclass
    {
        // Parent class has method #greet that returns literal 0 (= 999)
        uint64_t *greet_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 16);
        uint8_t *gbc = (uint8_t *)&OBJ_FIELD(greet_bc, 0);
        gbc[0] = BC_PUSH_LITERAL;
        WRITE_U32(&gbc[1], 0);
        gbc[5] = BC_RETURN;

        uint64_t *greet_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(greet_cm, CM_PRIMITIVE) = tag_smallint(0);
        OBJ_FIELD(greet_cm, CM_NUM_ARGS) = tag_smallint(0);
        OBJ_FIELD(greet_cm, CM_NUM_TEMPS) = tag_smallint(0);
        uint64_t *_lits_13 = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 1);
        OBJ_FIELD(_lits_13, 0) = tag_smallint(999);
        OBJ_FIELD(greet_cm, CM_LITERALS) = (uint64_t)_lits_13;
        OBJ_FIELD(greet_cm, CM_BYTECODES) = (uint64_t)greet_bc;

        uint64_t sel_greet = tag_smallint(30);
        uint64_t *par_md = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 2);
        OBJ_FIELD(par_md, 0) = sel_greet;
        OBJ_FIELD(par_md, 1) = (uint64_t)greet_cm;

        uint64_t *par_class = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 4);
        OBJ_FIELD(par_class, CLASS_SUPERCLASS) = tagged_nil();
        OBJ_FIELD(par_class, CLASS_METHOD_DICT) = (uint64_t)par_md;
        OBJ_FIELD(par_class, CLASS_INST_SIZE) = tag_smallint(0);
        OBJ_FIELD(par_class, CLASS_INST_FORMAT) = tag_smallint(FORMAT_FIELDS);

        // Child class: empty method dict, superclass = par_class
        uint64_t *child_cls = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 4);
        OBJ_FIELD(child_cls, CLASS_SUPERCLASS) = (uint64_t)par_class;
        OBJ_FIELD(child_cls, CLASS_METHOD_DICT) = tagged_nil();
        OBJ_FIELD(child_cls, CLASS_INST_SIZE) = tag_smallint(0);
        OBJ_FIELD(child_cls, CLASS_INST_FORMAT) = tag_smallint(FORMAT_FIELDS);

        // Instance of child class
        uint64_t *child_obj = om_alloc(om, (uint64_t)child_cls, FORMAT_FIELDS, 0);

        // Caller: push self, send #greet, return
        uint64_t *sc_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 16);
        uint8_t *scb = (uint8_t *)&OBJ_FIELD(sc_bc, 0);
        scb[0] = BC_PUSH_SELF;
        scb[1] = BC_SEND_MESSAGE;
        WRITE_U32(&scb[2], 0); // selector index 0
        WRITE_U32(&scb[6], 0); // 0 args
        scb[10] = BC_RETURN;

        uint64_t *sc_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(sc_cm, CM_PRIMITIVE) = tag_smallint(0);
        OBJ_FIELD(sc_cm, CM_NUM_ARGS) = tag_smallint(0);
        OBJ_FIELD(sc_cm, CM_NUM_TEMPS) = tag_smallint(0);
        uint64_t *_lits_14 = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 1);
        OBJ_FIELD(_lits_14, 0) = sel_greet;
        OBJ_FIELD(sc_cm, CM_LITERALS) = (uint64_t)_lits_14;
        OBJ_FIELD(sc_cm, CM_BYTECODES) = (uint64_t)sc_bc;

        sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
        fp = (uint64_t *)0xCAFE;
        stack_push(&sp, stack, (uint64_t)child_obj);
        activate_method(&sp, &fp, 0, (uint64_t)sc_cm, 0, 0);
        result = interpret(&sp, &fp, (uint8_t *)&OBJ_FIELD(sc_bc, 0), class_table, om, NULL);
        ASSERT_EQ(ctx, result, tag_smallint(999),
                  "SEND superclass: child sends #greet, found in parent");
    }

    // --- Section 12: Primitives ---

    // Install methods on SmallInteger class
    {
        uint64_t sel_plus = tag_smallint(50);
        uint64_t sel_minus = tag_smallint(51);
        uint64_t sel_lt = tag_smallint(52);
        uint64_t sel_eq = tag_smallint(53);
        uint64_t sel_mul = tag_smallint(54);

        // Dummy bytecodes (primitives short-circuit, these are never reached)
        uint64_t *prim_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 2);

        // +
        uint64_t *plus_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(plus_cm, CM_PRIMITIVE) = tag_smallint(PRIM_SMALLINT_ADD);
        OBJ_FIELD(plus_cm, CM_NUM_ARGS) = tag_smallint(1);
        OBJ_FIELD(plus_cm, CM_NUM_TEMPS) = tag_smallint(0);
        OBJ_FIELD(plus_cm, CM_LITERALS) = tagged_nil();
        OBJ_FIELD(plus_cm, CM_BYTECODES) = (uint64_t)prim_bc;

        // -
        uint64_t *sub_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(sub_cm, CM_PRIMITIVE) = tag_smallint(PRIM_SMALLINT_SUB);
        OBJ_FIELD(sub_cm, CM_NUM_ARGS) = tag_smallint(1);
        OBJ_FIELD(sub_cm, CM_NUM_TEMPS) = tag_smallint(0);
        OBJ_FIELD(sub_cm, CM_LITERALS) = tagged_nil();
        OBJ_FIELD(sub_cm, CM_BYTECODES) = (uint64_t)prim_bc;

        // <
        uint64_t *lt_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(lt_cm, CM_PRIMITIVE) = tag_smallint(PRIM_SMALLINT_LT);
        OBJ_FIELD(lt_cm, CM_NUM_ARGS) = tag_smallint(1);
        OBJ_FIELD(lt_cm, CM_NUM_TEMPS) = tag_smallint(0);
        OBJ_FIELD(lt_cm, CM_LITERALS) = tagged_nil();
        OBJ_FIELD(lt_cm, CM_BYTECODES) = (uint64_t)prim_bc;

        // =
        uint64_t *eq_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(eq_cm, CM_PRIMITIVE) = tag_smallint(PRIM_SMALLINT_EQ);
        OBJ_FIELD(eq_cm, CM_NUM_ARGS) = tag_smallint(1);
        OBJ_FIELD(eq_cm, CM_NUM_TEMPS) = tag_smallint(0);
        OBJ_FIELD(eq_cm, CM_LITERALS) = tagged_nil();
        OBJ_FIELD(eq_cm, CM_BYTECODES) = (uint64_t)prim_bc;

        // *
        uint64_t *mul_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(mul_cm, CM_PRIMITIVE) = tag_smallint(PRIM_SMALLINT_MUL);
        OBJ_FIELD(mul_cm, CM_NUM_ARGS) = tag_smallint(1);
        OBJ_FIELD(mul_cm, CM_NUM_TEMPS) = tag_smallint(0);
        OBJ_FIELD(mul_cm, CM_LITERALS) = tagged_nil();
        OBJ_FIELD(mul_cm, CM_BYTECODES) = (uint64_t)prim_bc;

        // Method dict with all 5 selectors
        uint64_t *si_md = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 10);
        OBJ_FIELD(si_md, 0) = sel_plus;
        OBJ_FIELD(si_md, 1) = (uint64_t)plus_cm;
        OBJ_FIELD(si_md, 2) = sel_minus;
        OBJ_FIELD(si_md, 3) = (uint64_t)sub_cm;
        OBJ_FIELD(si_md, 4) = sel_lt;
        OBJ_FIELD(si_md, 5) = (uint64_t)lt_cm;
        OBJ_FIELD(si_md, 6) = sel_eq;
        OBJ_FIELD(si_md, 7) = (uint64_t)eq_cm;
        OBJ_FIELD(si_md, 8) = sel_mul;
        OBJ_FIELD(si_md, 9) = (uint64_t)mul_cm;

        OBJ_FIELD(smallint_class, CLASS_METHOD_DICT) = (uint64_t)si_md;

        // Test: 3 + 4 = 7  via dispatch loop
        // Caller: PUSH_LITERAL 0 (=3), PUSH_LITERAL 1 (=4), SEND #+ 1 arg, HALT
        uint64_t *add_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 24);
        uint8_t *abc = (uint8_t *)&OBJ_FIELD(add_bc, 0);
        abc[0] = BC_PUSH_LITERAL;
        WRITE_U32(&abc[1], 0); // literal 0 = 3
        abc[5] = BC_PUSH_LITERAL;
        WRITE_U32(&abc[6], 1); // literal 1 = 4
        abc[10] = BC_SEND_MESSAGE;
        WRITE_U32(&abc[11], 2); // selector index 2 = sel_plus
        WRITE_U32(&abc[15], 1); // 1 arg
        abc[19] = BC_HALT;

        uint64_t *add_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(add_cm, CM_PRIMITIVE) = tag_smallint(0);
        OBJ_FIELD(add_cm, CM_NUM_ARGS) = tag_smallint(0);
        OBJ_FIELD(add_cm, CM_NUM_TEMPS) = tag_smallint(0);
        uint64_t *_lits_15 = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 3);
        OBJ_FIELD(_lits_15, 0) = tag_smallint(3);
        OBJ_FIELD(_lits_15, 1) = tag_smallint(4);
        OBJ_FIELD(_lits_15, 2) = sel_plus;
        OBJ_FIELD(add_cm, CM_LITERALS) = (uint64_t)_lits_15;
        OBJ_FIELD(add_cm, CM_BYTECODES) = (uint64_t)add_bc;

        sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
        fp = (uint64_t *)0xCAFE;
        stack_push(&sp, stack, receiver);
        activate_method(&sp, &fp, 0, (uint64_t)add_cm, 0, 0);
        result = interpret(&sp, &fp,
                           (uint8_t *)&OBJ_FIELD(add_bc, 0), class_table, om, NULL);
        ASSERT_EQ(ctx, result, tag_smallint(7), "primitive: 3 + 4 = 7 via dispatch");

        // Test: 10 - 3 = 7 via dispatch
        uint64_t *sub_bc2 = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 24);
        uint8_t *sbc2 = (uint8_t *)&OBJ_FIELD(sub_bc2, 0);
        sbc2[0] = BC_PUSH_LITERAL;
        WRITE_U32(&sbc2[1], 0);
        sbc2[5] = BC_PUSH_LITERAL;
        WRITE_U32(&sbc2[6], 1);
        sbc2[10] = BC_SEND_MESSAGE;
        WRITE_U32(&sbc2[11], 2); // selector = sel_minus
        WRITE_U32(&sbc2[15], 1);
        sbc2[19] = BC_HALT;

        uint64_t *sub_cm2 = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(sub_cm2, CM_PRIMITIVE) = tag_smallint(0);
        OBJ_FIELD(sub_cm2, CM_NUM_ARGS) = tag_smallint(0);
        OBJ_FIELD(sub_cm2, CM_NUM_TEMPS) = tag_smallint(0);
        uint64_t *_lits_16 = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 3);
        OBJ_FIELD(_lits_16, 0) = tag_smallint(10);
        OBJ_FIELD(_lits_16, 1) = tag_smallint(3);
        OBJ_FIELD(_lits_16, 2) = sel_minus;
        OBJ_FIELD(sub_cm2, CM_LITERALS) = (uint64_t)_lits_16;
        OBJ_FIELD(sub_cm2, CM_BYTECODES) = (uint64_t)sub_bc2;

        sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
        fp = (uint64_t *)0xCAFE;
        stack_push(&sp, stack, receiver);
        activate_method(&sp, &fp, 0, (uint64_t)sub_cm2, 0, 0);
        result = interpret(&sp, &fp,
                           (uint8_t *)&OBJ_FIELD(sub_bc2, 0), class_table, om, NULL);
        ASSERT_EQ(ctx, result, tag_smallint(7), "primitive: 10 - 3 = 7 via dispatch");

        // Test: 3 < 5 = true via dispatch
        uint64_t *lt_bc2 = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 24);
        uint8_t *lbc2 = (uint8_t *)&OBJ_FIELD(lt_bc2, 0);
        lbc2[0] = BC_PUSH_LITERAL;
        WRITE_U32(&lbc2[1], 0);
        lbc2[5] = BC_PUSH_LITERAL;
        WRITE_U32(&lbc2[6], 1);
        lbc2[10] = BC_SEND_MESSAGE;
        WRITE_U32(&lbc2[11], 2);
        WRITE_U32(&lbc2[15], 1);
        lbc2[19] = BC_HALT;

        uint64_t *lt_cm2 = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(lt_cm2, CM_PRIMITIVE) = tag_smallint(0);
        OBJ_FIELD(lt_cm2, CM_NUM_ARGS) = tag_smallint(0);
        OBJ_FIELD(lt_cm2, CM_NUM_TEMPS) = tag_smallint(0);
        uint64_t *_lits_17 = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 3);
        OBJ_FIELD(_lits_17, 0) = tag_smallint(3);
        OBJ_FIELD(_lits_17, 1) = tag_smallint(5);
        OBJ_FIELD(_lits_17, 2) = sel_lt;
        OBJ_FIELD(lt_cm2, CM_LITERALS) = (uint64_t)_lits_17;
        OBJ_FIELD(lt_cm2, CM_BYTECODES) = (uint64_t)lt_bc2;

        sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
        fp = (uint64_t *)0xCAFE;
        stack_push(&sp, stack, receiver);
        activate_method(&sp, &fp, 0, (uint64_t)lt_cm2, 0, 0);
        result = interpret(&sp, &fp,
                           (uint8_t *)&OBJ_FIELD(lt_bc2, 0), class_table, om, NULL);
        ASSERT_EQ(ctx, result, tagged_true(), "primitive: 3 < 5 = true via dispatch");

        // Test: 42 = 42 → true via dispatch
        uint64_t *eq_bc2 = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 24);
        uint8_t *ebc2 = (uint8_t *)&OBJ_FIELD(eq_bc2, 0);
        ebc2[0] = BC_PUSH_LITERAL;
        WRITE_U32(&ebc2[1], 0);
        ebc2[5] = BC_PUSH_LITERAL;
        WRITE_U32(&ebc2[6], 1);
        ebc2[10] = BC_SEND_MESSAGE;
        WRITE_U32(&ebc2[11], 2);
        WRITE_U32(&ebc2[15], 1);
        ebc2[19] = BC_HALT;

        uint64_t *eq_cm2 = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(eq_cm2, CM_PRIMITIVE) = tag_smallint(0);
        OBJ_FIELD(eq_cm2, CM_NUM_ARGS) = tag_smallint(0);
        OBJ_FIELD(eq_cm2, CM_NUM_TEMPS) = tag_smallint(0);
        uint64_t *_lits_18 = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 3);
        OBJ_FIELD(_lits_18, 0) = tag_smallint(42);
        OBJ_FIELD(_lits_18, 1) = tag_smallint(42);
        OBJ_FIELD(_lits_18, 2) = sel_eq;
        OBJ_FIELD(eq_cm2, CM_LITERALS) = (uint64_t)_lits_18;
        OBJ_FIELD(eq_cm2, CM_BYTECODES) = (uint64_t)eq_bc2;

        sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
        fp = (uint64_t *)0xCAFE;
        stack_push(&sp, stack, receiver);
        activate_method(&sp, &fp, 0, (uint64_t)eq_cm2, 0, 0);
        result = interpret(&sp, &fp,
                           (uint8_t *)&OBJ_FIELD(eq_bc2, 0), class_table, om, NULL);
        ASSERT_EQ(ctx, result, tagged_true(), "primitive: 42 = 42 = true via dispatch");

        // Test: 6 * 7 = 42 via dispatch
        uint64_t *mul_bc2 = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 24);
        uint8_t *mbc2 = (uint8_t *)&OBJ_FIELD(mul_bc2, 0);
        mbc2[0] = BC_PUSH_LITERAL;
        WRITE_U32(&mbc2[1], 0);
        mbc2[5] = BC_PUSH_LITERAL;
        WRITE_U32(&mbc2[6], 1);
        mbc2[10] = BC_SEND_MESSAGE;
        WRITE_U32(&mbc2[11], 2); // selector index 2 = sel_mul
        WRITE_U32(&mbc2[15], 1);
        mbc2[19] = BC_HALT;

        uint64_t *_lits_mul = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 3);
        OBJ_FIELD(_lits_mul, 0) = tag_smallint(6);
        OBJ_FIELD(_lits_mul, 1) = tag_smallint(7);
        OBJ_FIELD(_lits_mul, 2) = sel_mul;
        uint64_t *mul_cm2 = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(mul_cm2, CM_PRIMITIVE) = tag_smallint(0);
        OBJ_FIELD(mul_cm2, CM_NUM_ARGS) = tag_smallint(0);
        OBJ_FIELD(mul_cm2, CM_NUM_TEMPS) = tag_smallint(0);
        OBJ_FIELD(mul_cm2, CM_LITERALS) = (uint64_t)_lits_mul;
        OBJ_FIELD(mul_cm2, CM_BYTECODES) = (uint64_t)mul_bc2;

        sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
        fp = (uint64_t *)0xCAFE;
        stack_push(&sp, stack, receiver);
        activate_method(&sp, &fp, 0, (uint64_t)mul_cm2, 0, 0);
        result = interpret(&sp, &fp,
                           (uint8_t *)&OBJ_FIELD(mul_bc2, 0), class_table, om, NULL);
        ASSERT_EQ(ctx, result, tag_smallint(42), "primitive: 6 * 7 = 42 via dispatch");
    }

    ctx->smallint_class = smallint_class;

    // --- basicNew primitive ---
    // Send basicNew to a class, get back a new instance with correct class and size
    {
        uint64_t sel_basicNew = tag_smallint(70);

        // Create a class with instSize=2
        uint64_t *my_class = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 4);
        OBJ_FIELD(my_class, CLASS_SUPERCLASS) = tagged_nil();
        OBJ_FIELD(my_class, CLASS_INST_SIZE) = tag_smallint(2);
        OBJ_FIELD(my_class, CLASS_METHOD_DICT) = tagged_nil();
        OBJ_FIELD(my_class, CLASS_INST_FORMAT) = tag_smallint(FORMAT_FIELDS);

        // Add basicNew to class_class's method dict (since oop_class(my_class) = class_class)
        uint64_t *prim_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 2);
        uint64_t *bn_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(bn_cm, CM_PRIMITIVE) = tag_smallint(PRIM_BASIC_NEW);
        OBJ_FIELD(bn_cm, CM_NUM_ARGS) = tag_smallint(0);
        OBJ_FIELD(bn_cm, CM_NUM_TEMPS) = tag_smallint(0);
        OBJ_FIELD(bn_cm, CM_LITERALS) = tagged_nil();
        OBJ_FIELD(bn_cm, CM_BYTECODES) = (uint64_t)prim_bc;

        // Extend class_class's method dict to include basicNew
        uint64_t old_md_val = OBJ_FIELD(class_class, CLASS_METHOD_DICT);
        uint64_t *old_cc_md = (old_md_val != tagged_nil() && (old_md_val & 3) == 0)
                                  ? (uint64_t *)old_md_val
                                  : NULL;
        uint64_t old_cc_md_size = old_cc_md ? OBJ_SIZE(old_cc_md) : 0;
        uint64_t *new_cc_md = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, old_cc_md_size + 2);
        for (uint64_t i = 0; i < old_cc_md_size; i++)
            OBJ_FIELD(new_cc_md, i) = OBJ_FIELD(old_cc_md, i);
        OBJ_FIELD(new_cc_md, old_cc_md_size) = sel_basicNew;
        OBJ_FIELD(new_cc_md, old_cc_md_size + 1) = (uint64_t)bn_cm;
        OBJ_FIELD(class_class, CLASS_METHOD_DICT) = (uint64_t)new_cc_md;

        // Caller: PUSH_SELF, SEND #basicNew 0, HALT
        uint64_t *bn_caller_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 16);
        uint8_t *bnbc = (uint8_t *)&OBJ_FIELD(bn_caller_bc, 0);
        bnbc[0] = BC_PUSH_SELF;
        bnbc[1] = BC_SEND_MESSAGE;
        WRITE_U32(&bnbc[2], 0); // selector lit index 0
        WRITE_U32(&bnbc[6], 0); // 0 args
        bnbc[10] = BC_HALT;

        uint64_t *bn_lits = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 1);
        OBJ_FIELD(bn_lits, 0) = sel_basicNew;

        uint64_t *bn_caller_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(bn_caller_cm, CM_PRIMITIVE) = tag_smallint(0);
        OBJ_FIELD(bn_caller_cm, CM_NUM_ARGS) = tag_smallint(0);
        OBJ_FIELD(bn_caller_cm, CM_NUM_TEMPS) = tag_smallint(0);
        OBJ_FIELD(bn_caller_cm, CM_LITERALS) = (uint64_t)bn_lits;
        OBJ_FIELD(bn_caller_cm, CM_BYTECODES) = (uint64_t)bn_caller_bc;

        sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
        fp = (uint64_t *)0xCAFE;
        // receiver is the class itself (we're sending basicNew to a class)
        stack_push(&sp, stack, (uint64_t)my_class);

        activate_method(&sp, &fp, 0, (uint64_t)bn_caller_cm, 0, 0);
        result = interpret(&sp, &fp,
                           (uint8_t *)&OBJ_FIELD(bn_caller_bc, 0),
                           class_table, om, NULL);

        // Result should be a new object pointer (tag 00)
        ASSERT_EQ(ctx, result & 3, 0, "basicNew: result is object pointer");
        uint64_t *new_obj = (uint64_t *)result;
        ASSERT_EQ(ctx, OBJ_CLASS(new_obj), (uint64_t)my_class,
                  "basicNew: class is correct");
        ASSERT_EQ(ctx, OBJ_FORMAT(new_obj), FORMAT_FIELDS,
                  "basicNew: format is fields");
        ASSERT_EQ(ctx, OBJ_SIZE(new_obj), 2,
                  "basicNew: size matches instSize");
        // Fields should be initialized to nil
        ASSERT_EQ(ctx, OBJ_FIELD(new_obj, 0), tagged_nil(),
                  "basicNew: field 0 = nil");
        ASSERT_EQ(ctx, OBJ_FIELD(new_obj, 1), tagged_nil(),
                  "basicNew: field 1 = nil");
    }
}
