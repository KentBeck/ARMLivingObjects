#include "test_defs.h"

static void trap_smallint_add_wrong_receiver(TestContext *ctx)
{
    uint64_t *om = ctx->om;
    uint64_t *class_class = ctx->class_class;
    uint64_t *class_table = ctx->class_table;
    uint64_t *stack = ctx->stack;
    uint64_t *sp;
    uint64_t *fp;

    uint64_t sel_plus = tag_smallint(120);

    uint64_t *prim_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 2);
    uint64_t *plus_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
    OBJ_FIELD(plus_cm, CM_PRIMITIVE) = tag_smallint(PRIM_SMALLINT_ADD);
    OBJ_FIELD(plus_cm, CM_NUM_ARGS) = tag_smallint(1);
    OBJ_FIELD(plus_cm, CM_NUM_TEMPS) = tag_smallint(0);
    OBJ_FIELD(plus_cm, CM_LITERALS) = tagged_nil();
    OBJ_FIELD(plus_cm, CM_BYTECODES) = (uint64_t)prim_bc;

    uint64_t *recv_class = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 4);
    OBJ_FIELD(recv_class, CLASS_SUPERCLASS) = tagged_nil();
    OBJ_FIELD(recv_class, CLASS_INST_SIZE) = tag_smallint(0);
    OBJ_FIELD(recv_class, CLASS_INST_FORMAT) = tag_smallint(FORMAT_FIELDS);
    uint64_t *recv_md = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 2);
    OBJ_FIELD(recv_md, 0) = sel_plus;
    OBJ_FIELD(recv_md, 1) = (uint64_t)plus_cm;
    OBJ_FIELD(recv_class, CLASS_METHOD_DICT) = (uint64_t)recv_md;

    uint64_t *recv_obj = om_alloc(om, (uint64_t)recv_class, FORMAT_FIELDS, 0);

    uint64_t *caller_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 24);
    uint8_t *cbb = (uint8_t *)&OBJ_FIELD(caller_bc, 0);
    cbb[0] = BC_PUSH_SELF;
    cbb[1] = BC_PUSH_LITERAL;
    WRITE_U32(&cbb[2], 0);
    cbb[6] = BC_SEND_MESSAGE;
    WRITE_U32(&cbb[7], 1);
    WRITE_U32(&cbb[11], 1);
    cbb[15] = BC_HALT;

    uint64_t *caller_lits = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 2);
    OBJ_FIELD(caller_lits, 0) = tag_smallint(4);
    OBJ_FIELD(caller_lits, 1) = sel_plus;

    uint64_t *caller_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
    OBJ_FIELD(caller_cm, CM_PRIMITIVE) = tag_smallint(0);
    OBJ_FIELD(caller_cm, CM_NUM_ARGS) = tag_smallint(0);
    OBJ_FIELD(caller_cm, CM_NUM_TEMPS) = tag_smallint(0);
    OBJ_FIELD(caller_cm, CM_LITERALS) = (uint64_t)caller_lits;
    OBJ_FIELD(caller_cm, CM_BYTECODES) = (uint64_t)caller_bc;

    sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
    fp = (uint64_t *)0xCAFE;
    stack_push(&sp, stack, (uint64_t)recv_obj);
    activate_method(&sp, &fp, 0, (uint64_t)caller_cm, 0, 0);
    (void)interpret(&sp, &fp, (uint8_t *)&OBJ_FIELD(caller_bc, 0), class_table, om, NULL);
}

static void trap_char_value_wrong_receiver(TestContext *ctx)
{
    uint64_t *om = ctx->om;
    uint64_t *class_class = ctx->class_class;
    uint64_t *class_table = ctx->class_table;
    uint64_t *stack = ctx->stack;
    uint64_t *sp;
    uint64_t *fp;

    uint64_t sel_value = tag_smallint(122);

    uint64_t *prim_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 2);
    uint64_t *value_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
    OBJ_FIELD(value_cm, CM_PRIMITIVE) = tag_smallint(PRIM_CHAR_VALUE);
    OBJ_FIELD(value_cm, CM_NUM_ARGS) = tag_smallint(0);
    OBJ_FIELD(value_cm, CM_NUM_TEMPS) = tag_smallint(0);
    OBJ_FIELD(value_cm, CM_LITERALS) = tagged_nil();
    OBJ_FIELD(value_cm, CM_BYTECODES) = (uint64_t)prim_bc;

    uint64_t *recv_class = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 4);
    OBJ_FIELD(recv_class, CLASS_SUPERCLASS) = tagged_nil();
    OBJ_FIELD(recv_class, CLASS_INST_SIZE) = tag_smallint(0);
    OBJ_FIELD(recv_class, CLASS_INST_FORMAT) = tag_smallint(FORMAT_FIELDS);
    uint64_t *recv_md = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 2);
    OBJ_FIELD(recv_md, 0) = sel_value;
    OBJ_FIELD(recv_md, 1) = (uint64_t)value_cm;
    OBJ_FIELD(recv_class, CLASS_METHOD_DICT) = (uint64_t)recv_md;

    uint64_t *recv_obj = om_alloc(om, (uint64_t)recv_class, FORMAT_FIELDS, 0);

    uint64_t *caller_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 16);
    uint8_t *cbb = (uint8_t *)&OBJ_FIELD(caller_bc, 0);
    cbb[0] = BC_PUSH_SELF;
    cbb[1] = BC_SEND_MESSAGE;
    WRITE_U32(&cbb[2], 0);
    WRITE_U32(&cbb[6], 0);
    cbb[10] = BC_HALT;

    uint64_t *caller_lits = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 1);
    OBJ_FIELD(caller_lits, 0) = sel_value;

    uint64_t *caller_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
    OBJ_FIELD(caller_cm, CM_PRIMITIVE) = tag_smallint(0);
    OBJ_FIELD(caller_cm, CM_NUM_ARGS) = tag_smallint(0);
    OBJ_FIELD(caller_cm, CM_NUM_TEMPS) = tag_smallint(0);
    OBJ_FIELD(caller_cm, CM_LITERALS) = (uint64_t)caller_lits;
    OBJ_FIELD(caller_cm, CM_BYTECODES) = (uint64_t)caller_bc;

    sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
    fp = (uint64_t *)0xCAFE;
    stack_push(&sp, stack, (uint64_t)recv_obj);
    activate_method(&sp, &fp, 0, (uint64_t)caller_cm, 0, 0);
    (void)interpret(&sp, &fp, (uint8_t *)&OBJ_FIELD(caller_bc, 0), class_table, om, NULL);
}

static void trap_as_character_wrong_receiver(TestContext *ctx)
{
    uint64_t *om = ctx->om;
    uint64_t *class_class = ctx->class_class;
    uint64_t *class_table = ctx->class_table;
    uint64_t *stack = ctx->stack;
    uint64_t *sp;
    uint64_t *fp;

    uint64_t sel_asChar = tag_smallint(123);

    uint64_t *prim_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 2);
    uint64_t *aschar_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
    OBJ_FIELD(aschar_cm, CM_PRIMITIVE) = tag_smallint(PRIM_AS_CHARACTER);
    OBJ_FIELD(aschar_cm, CM_NUM_ARGS) = tag_smallint(0);
    OBJ_FIELD(aschar_cm, CM_NUM_TEMPS) = tag_smallint(0);
    OBJ_FIELD(aschar_cm, CM_LITERALS) = tagged_nil();
    OBJ_FIELD(aschar_cm, CM_BYTECODES) = (uint64_t)prim_bc;

    uint64_t *recv_class = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 4);
    OBJ_FIELD(recv_class, CLASS_SUPERCLASS) = tagged_nil();
    OBJ_FIELD(recv_class, CLASS_INST_SIZE) = tag_smallint(0);
    OBJ_FIELD(recv_class, CLASS_INST_FORMAT) = tag_smallint(FORMAT_FIELDS);
    uint64_t *recv_md = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 2);
    OBJ_FIELD(recv_md, 0) = sel_asChar;
    OBJ_FIELD(recv_md, 1) = (uint64_t)aschar_cm;
    OBJ_FIELD(recv_class, CLASS_METHOD_DICT) = (uint64_t)recv_md;

    uint64_t *recv_obj = om_alloc(om, (uint64_t)recv_class, FORMAT_FIELDS, 0);

    uint64_t *caller_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 16);
    uint8_t *cbb = (uint8_t *)&OBJ_FIELD(caller_bc, 0);
    cbb[0] = BC_PUSH_SELF;
    cbb[1] = BC_SEND_MESSAGE;
    WRITE_U32(&cbb[2], 0);
    WRITE_U32(&cbb[6], 0);
    cbb[10] = BC_HALT;

    uint64_t *caller_lits = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 1);
    OBJ_FIELD(caller_lits, 0) = sel_asChar;

    uint64_t *caller_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
    OBJ_FIELD(caller_cm, CM_PRIMITIVE) = tag_smallint(0);
    OBJ_FIELD(caller_cm, CM_NUM_ARGS) = tag_smallint(0);
    OBJ_FIELD(caller_cm, CM_NUM_TEMPS) = tag_smallint(0);
    OBJ_FIELD(caller_cm, CM_LITERALS) = (uint64_t)caller_lits;
    OBJ_FIELD(caller_cm, CM_BYTECODES) = (uint64_t)caller_bc;

    sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
    fp = (uint64_t *)0xCAFE;
    stack_push(&sp, stack, (uint64_t)recv_obj);
    activate_method(&sp, &fp, 0, (uint64_t)caller_cm, 0, 0);
    (void)interpret(&sp, &fp, (uint8_t *)&OBJ_FIELD(caller_bc, 0), class_table, om, NULL);
}

static void trap_print_char_wrong_receiver(TestContext *ctx)
{
    uint64_t *om = ctx->om;
    uint64_t *class_class = ctx->class_class;
    uint64_t *class_table = ctx->class_table;
    uint64_t *stack = ctx->stack;
    uint64_t *sp;
    uint64_t *fp;

    uint64_t sel_printChar = tag_smallint(124);

    uint64_t *prim_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 2);
    uint64_t *pc_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
    OBJ_FIELD(pc_cm, CM_PRIMITIVE) = tag_smallint(PRIM_PRINT_CHAR);
    OBJ_FIELD(pc_cm, CM_NUM_ARGS) = tag_smallint(0);
    OBJ_FIELD(pc_cm, CM_NUM_TEMPS) = tag_smallint(0);
    OBJ_FIELD(pc_cm, CM_LITERALS) = tagged_nil();
    OBJ_FIELD(pc_cm, CM_BYTECODES) = (uint64_t)prim_bc;

    uint64_t *recv_class = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 4);
    OBJ_FIELD(recv_class, CLASS_SUPERCLASS) = tagged_nil();
    OBJ_FIELD(recv_class, CLASS_INST_SIZE) = tag_smallint(0);
    OBJ_FIELD(recv_class, CLASS_INST_FORMAT) = tag_smallint(FORMAT_FIELDS);
    uint64_t *recv_md = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 2);
    OBJ_FIELD(recv_md, 0) = sel_printChar;
    OBJ_FIELD(recv_md, 1) = (uint64_t)pc_cm;
    OBJ_FIELD(recv_class, CLASS_METHOD_DICT) = (uint64_t)recv_md;

    uint64_t *recv_obj = om_alloc(om, (uint64_t)recv_class, FORMAT_FIELDS, 0);

    uint64_t *caller_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 16);
    uint8_t *cbb = (uint8_t *)&OBJ_FIELD(caller_bc, 0);
    cbb[0] = BC_PUSH_SELF;
    cbb[1] = BC_SEND_MESSAGE;
    WRITE_U32(&cbb[2], 0);
    WRITE_U32(&cbb[6], 0);
    cbb[10] = BC_HALT;

    uint64_t *caller_lits = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 1);
    OBJ_FIELD(caller_lits, 0) = sel_printChar;

    uint64_t *caller_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
    OBJ_FIELD(caller_cm, CM_PRIMITIVE) = tag_smallint(0);
    OBJ_FIELD(caller_cm, CM_NUM_ARGS) = tag_smallint(0);
    OBJ_FIELD(caller_cm, CM_NUM_TEMPS) = tag_smallint(0);
    OBJ_FIELD(caller_cm, CM_LITERALS) = (uint64_t)caller_lits;
    OBJ_FIELD(caller_cm, CM_BYTECODES) = (uint64_t)caller_bc;

    sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
    fp = (uint64_t *)0xCAFE;
    stack_push(&sp, stack, (uint64_t)recv_obj);
    activate_method(&sp, &fp, 0, (uint64_t)caller_cm, 0, 0);
    (void)interpret(&sp, &fp, (uint8_t *)&OBJ_FIELD(caller_bc, 0), class_table, om, NULL);
}

static void trap_block_value_wrong_receiver(TestContext *ctx)
{
    uint64_t *om = ctx->om;
    uint64_t *class_class = ctx->class_class;
    uint64_t *class_table = ctx->class_table;
    uint64_t *stack = ctx->stack;
    uint64_t *sp;
    uint64_t *fp;

    uint64_t sel_value = tag_smallint(125);

    uint64_t *prim_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 2);
    uint64_t *value_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
    OBJ_FIELD(value_cm, CM_PRIMITIVE) = tag_smallint(PRIM_BLOCK_VALUE);
    OBJ_FIELD(value_cm, CM_NUM_ARGS) = tag_smallint(0);
    OBJ_FIELD(value_cm, CM_NUM_TEMPS) = tag_smallint(0);
    OBJ_FIELD(value_cm, CM_LITERALS) = tagged_nil();
    OBJ_FIELD(value_cm, CM_BYTECODES) = (uint64_t)prim_bc;

    uint64_t *recv_class = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 4);
    OBJ_FIELD(recv_class, CLASS_SUPERCLASS) = tagged_nil();
    OBJ_FIELD(recv_class, CLASS_INST_SIZE) = tag_smallint(0);
    OBJ_FIELD(recv_class, CLASS_INST_FORMAT) = tag_smallint(FORMAT_FIELDS);
    uint64_t *recv_md = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 2);
    OBJ_FIELD(recv_md, 0) = sel_value;
    OBJ_FIELD(recv_md, 1) = (uint64_t)value_cm;
    OBJ_FIELD(recv_class, CLASS_METHOD_DICT) = (uint64_t)recv_md;

    uint64_t *recv_obj = om_alloc(om, (uint64_t)recv_class, FORMAT_FIELDS, 0);

    uint64_t *caller_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 16);
    uint8_t *cbb = (uint8_t *)&OBJ_FIELD(caller_bc, 0);
    cbb[0] = BC_PUSH_SELF;
    cbb[1] = BC_SEND_MESSAGE;
    WRITE_U32(&cbb[2], 0);
    WRITE_U32(&cbb[6], 0);
    cbb[10] = BC_HALT;

    uint64_t *caller_lits = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 1);
    OBJ_FIELD(caller_lits, 0) = sel_value;

    uint64_t *caller_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
    OBJ_FIELD(caller_cm, CM_PRIMITIVE) = tag_smallint(0);
    OBJ_FIELD(caller_cm, CM_NUM_ARGS) = tag_smallint(0);
    OBJ_FIELD(caller_cm, CM_NUM_TEMPS) = tag_smallint(0);
    OBJ_FIELD(caller_cm, CM_LITERALS) = (uint64_t)caller_lits;
    OBJ_FIELD(caller_cm, CM_BYTECODES) = (uint64_t)caller_bc;

    sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
    fp = (uint64_t *)0xCAFE;
    stack_push(&sp, stack, (uint64_t)recv_obj);
    activate_method(&sp, &fp, 0, (uint64_t)caller_cm, 0, 0);
    (void)interpret(&sp, &fp, (uint8_t *)&OBJ_FIELD(caller_bc, 0), class_table, om, NULL);
}

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

    // Test: send a 1-arg message and verify the send path pops arg and receiver correctly.
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

        // Wrong argument type should primitive-fail into the Smalltalk body.
        uint64_t old_smallint_md = OBJ_FIELD(smallint_class, CLASS_METHOD_DICT);
        uint64_t old_smallint_slot = OBJ_FIELD(class_table, 0);

        uint64_t sel_plus_fallback = tag_smallint(121);
        uint64_t *fallback_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 6);
        uint8_t *fbb = (uint8_t *)&OBJ_FIELD(fallback_bc, 0);
        fbb[0] = BC_PUSH_LITERAL;
        WRITE_U32(&fbb[1], 0);
        fbb[5] = BC_RETURN;

        uint64_t *fallback_lits = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 1);
        OBJ_FIELD(fallback_lits, 0) = tag_smallint(444);

        uint64_t *fallback_plus_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(fallback_plus_cm, CM_PRIMITIVE) = tag_smallint(PRIM_SMALLINT_ADD);
        OBJ_FIELD(fallback_plus_cm, CM_NUM_ARGS) = tag_smallint(1);
        OBJ_FIELD(fallback_plus_cm, CM_NUM_TEMPS) = tag_smallint(0);
        OBJ_FIELD(fallback_plus_cm, CM_LITERALS) = (uint64_t)fallback_lits;
        OBJ_FIELD(fallback_plus_cm, CM_BYTECODES) = (uint64_t)fallback_bc;

        uint64_t *fallback_si_md = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 2);
        OBJ_FIELD(fallback_si_md, 0) = sel_plus_fallback;
        OBJ_FIELD(fallback_si_md, 1) = (uint64_t)fallback_plus_cm;
        OBJ_FIELD(smallint_class, CLASS_METHOD_DICT) = (uint64_t)fallback_si_md;
        OBJ_FIELD(class_table, 0) = (uint64_t)smallint_class;

        uint64_t *wrong_arg_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 24);
        uint8_t *wab = (uint8_t *)&OBJ_FIELD(wrong_arg_bc, 0);
        wab[0] = BC_PUSH_SELF;
        wab[1] = BC_PUSH_LITERAL;
        WRITE_U32(&wab[2], 0);
        wab[6] = BC_SEND_MESSAGE;
        WRITE_U32(&wab[7], 1);
        WRITE_U32(&wab[11], 1);
        wab[15] = BC_HALT;

        uint64_t *wrong_arg_lits = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 2);
        OBJ_FIELD(wrong_arg_lits, 0) = tagged_nil();
        OBJ_FIELD(wrong_arg_lits, 1) = sel_plus_fallback;

        uint64_t *wrong_arg_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(wrong_arg_cm, CM_PRIMITIVE) = tag_smallint(0);
        OBJ_FIELD(wrong_arg_cm, CM_NUM_ARGS) = tag_smallint(0);
        OBJ_FIELD(wrong_arg_cm, CM_NUM_TEMPS) = tag_smallint(0);
        OBJ_FIELD(wrong_arg_cm, CM_LITERALS) = (uint64_t)wrong_arg_lits;
        OBJ_FIELD(wrong_arg_cm, CM_BYTECODES) = (uint64_t)wrong_arg_bc;

        sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
        fp = (uint64_t *)0xCAFE;
        stack_push(&sp, stack, tag_smallint(7));
        activate_method(&sp, &fp, 0, (uint64_t)wrong_arg_cm, 0, 0);
        result = interpret(&sp, &fp,
                           (uint8_t *)&OBJ_FIELD(wrong_arg_bc, 0), class_table, om, NULL);
        ASSERT_EQ(ctx, result, tag_smallint(444),
                  "primitive failure: SmallInteger>>+ wrong arg falls through to method body");

        uint64_t *overflow_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 24);
        uint8_t *ovb = (uint8_t *)&OBJ_FIELD(overflow_bc, 0);
        ovb[0] = BC_PUSH_SELF;
        ovb[1] = BC_PUSH_LITERAL;
        WRITE_U32(&ovb[2], 0);
        ovb[6] = BC_SEND_MESSAGE;
        WRITE_U32(&ovb[7], 1);
        WRITE_U32(&ovb[11], 1);
        ovb[15] = BC_HALT;

        uint64_t *overflow_lits = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 2);
        OBJ_FIELD(overflow_lits, 0) = tag_smallint(1);
        OBJ_FIELD(overflow_lits, 1) = sel_plus_fallback;

        uint64_t *overflow_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(overflow_cm, CM_PRIMITIVE) = tag_smallint(0);
        OBJ_FIELD(overflow_cm, CM_NUM_ARGS) = tag_smallint(0);
        OBJ_FIELD(overflow_cm, CM_NUM_TEMPS) = tag_smallint(0);
        OBJ_FIELD(overflow_cm, CM_LITERALS) = (uint64_t)overflow_lits;
        OBJ_FIELD(overflow_cm, CM_BYTECODES) = (uint64_t)overflow_bc;

        sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
        fp = (uint64_t *)0xCAFE;
        stack_push(&sp, stack, tag_smallint(INT64_MAX >> 2));
        activate_method(&sp, &fp, 0, (uint64_t)overflow_cm, 0, 0);
        result = interpret(&sp, &fp,
                           (uint8_t *)&OBJ_FIELD(overflow_bc, 0), class_table, om, NULL);
        ASSERT_EQ(ctx, result, tag_smallint(444),
                  "primitive failure: SmallInteger>>+ overflow falls through to method body");

        ASSERT_EQ(ctx, (uint64_t)run_trap_test(ctx, trap_smallint_add_wrong_receiver),
                  (uint64_t)SIGTRAP,
                  "primitive trap: SmallInteger>>+ wrong receiver traps");

        OBJ_FIELD(smallint_class, CLASS_METHOD_DICT) = old_smallint_md;
        OBJ_FIELD(class_table, 0) = old_smallint_slot;
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

    // --- basicNew: on a non-indexable class → error ---
    {
        uint64_t sel_basicNewSize = tag_smallint(71);

        uint64_t *fixed_class = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 4);
        OBJ_FIELD(fixed_class, CLASS_SUPERCLASS) = tagged_nil();
        OBJ_FIELD(fixed_class, CLASS_INST_SIZE) = tag_smallint(2);
        OBJ_FIELD(fixed_class, CLASS_METHOD_DICT) = tagged_nil();
        OBJ_FIELD(fixed_class, CLASS_INST_FORMAT) = tag_smallint(FORMAT_FIELDS);

        uint64_t *fallback_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 6);
        uint8_t *fbb = (uint8_t *)&OBJ_FIELD(fallback_bc, 0);
        fbb[0] = BC_PUSH_LITERAL;
        WRITE_U32(&fbb[1], 0);
        fbb[5] = BC_RETURN;

        uint64_t *fallback_lits = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 1);
        OBJ_FIELD(fallback_lits, 0) = tag_smallint(333);

        uint64_t *bns_fail_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(bns_fail_cm, CM_PRIMITIVE) = tag_smallint(PRIM_BASIC_NEW_SIZE);
        OBJ_FIELD(bns_fail_cm, CM_NUM_ARGS) = tag_smallint(1);
        OBJ_FIELD(bns_fail_cm, CM_NUM_TEMPS) = tag_smallint(0);
        OBJ_FIELD(bns_fail_cm, CM_LITERALS) = (uint64_t)fallback_lits;
        OBJ_FIELD(bns_fail_cm, CM_BYTECODES) = (uint64_t)fallback_bc;

        uint64_t old_md_val = OBJ_FIELD(class_class, CLASS_METHOD_DICT);
        uint64_t *old_md = (old_md_val != tagged_nil() && (old_md_val & 3) == 0)
                               ? (uint64_t *)old_md_val
                               : NULL;
        uint64_t old_md_size = old_md ? OBJ_SIZE(old_md) : 0;
        uint64_t *new_md = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, old_md_size + 2);
        for (uint64_t i = 0; i < old_md_size; i++)
            OBJ_FIELD(new_md, i) = OBJ_FIELD(old_md, i);
        OBJ_FIELD(new_md, old_md_size) = sel_basicNewSize;
        OBJ_FIELD(new_md, old_md_size + 1) = (uint64_t)bns_fail_cm;
        OBJ_FIELD(class_class, CLASS_METHOD_DICT) = (uint64_t)new_md;

        uint64_t *caller_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 20);
        uint8_t *cbb = (uint8_t *)&OBJ_FIELD(caller_bc, 0);
        cbb[0] = BC_PUSH_SELF;
        cbb[1] = BC_PUSH_LITERAL;
        WRITE_U32(&cbb[2], 1);
        cbb[6] = BC_SEND_MESSAGE;
        WRITE_U32(&cbb[7], 0);
        WRITE_U32(&cbb[11], 1);
        cbb[15] = BC_HALT;

        uint64_t *caller_lits = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 2);
        OBJ_FIELD(caller_lits, 0) = sel_basicNewSize;
        OBJ_FIELD(caller_lits, 1) = tag_smallint(5);

        uint64_t *caller_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(caller_cm, CM_PRIMITIVE) = tag_smallint(0);
        OBJ_FIELD(caller_cm, CM_NUM_ARGS) = tag_smallint(0);
        OBJ_FIELD(caller_cm, CM_NUM_TEMPS) = tag_smallint(0);
        OBJ_FIELD(caller_cm, CM_LITERALS) = (uint64_t)caller_lits;
        OBJ_FIELD(caller_cm, CM_BYTECODES) = (uint64_t)caller_bc;

        sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
        fp = (uint64_t *)0xCAFE;
        stack_push(&sp, stack, (uint64_t)fixed_class);
        activate_method(&sp, &fp, 0, (uint64_t)caller_cm, 0, 0);
        result = interpret(&sp, &fp,
                           (uint8_t *)&OBJ_FIELD(caller_bc, 0),
                           class_table, om, NULL);
        ASSERT_EQ(ctx, result, tag_smallint(333),
                  "primitive failure: basicNew: on fixed-size class falls through to method body");
    }

    // --- basicNew: on an indexable class → correct size ---
    {
        uint64_t sel_basicNewSize = tag_smallint(71);

        // Create an indexable class (like Array)
        uint64_t *array_class = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 4);
        OBJ_FIELD(array_class, CLASS_SUPERCLASS) = tagged_nil();
        OBJ_FIELD(array_class, CLASS_INST_SIZE) = tag_smallint(0);
        OBJ_FIELD(array_class, CLASS_METHOD_DICT) = tagged_nil();
        OBJ_FIELD(array_class, CLASS_INST_FORMAT) = tag_smallint(FORMAT_INDEXABLE);

        // Add basicNew: to class_class's method dict
        uint64_t *prim_bc2 = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 2);
        uint64_t *bns_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(bns_cm, CM_PRIMITIVE) = tag_smallint(PRIM_BASIC_NEW_SIZE);
        OBJ_FIELD(bns_cm, CM_NUM_ARGS) = tag_smallint(1);
        OBJ_FIELD(bns_cm, CM_NUM_TEMPS) = tag_smallint(0);
        OBJ_FIELD(bns_cm, CM_LITERALS) = tagged_nil();
        OBJ_FIELD(bns_cm, CM_BYTECODES) = (uint64_t)prim_bc2;

        uint64_t old_md_val2 = OBJ_FIELD(class_class, CLASS_METHOD_DICT);
        uint64_t *old_md2 = (old_md_val2 != tagged_nil() && (old_md_val2 & 3) == 0)
                                ? (uint64_t *)old_md_val2
                                : NULL;
        uint64_t old_md2_size = old_md2 ? OBJ_SIZE(old_md2) : 0;
        uint64_t *new_md2 = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, old_md2_size + 2);
        for (uint64_t i = 0; i < old_md2_size; i++)
            OBJ_FIELD(new_md2, i) = OBJ_FIELD(old_md2, i);
        OBJ_FIELD(new_md2, old_md2_size) = sel_basicNewSize;
        OBJ_FIELD(new_md2, old_md2_size + 1) = (uint64_t)bns_cm;
        OBJ_FIELD(class_class, CLASS_METHOD_DICT) = (uint64_t)new_md2;

        // Caller: PUSH_LITERAL 0 (the size=5), PUSH_SELF, SEND #basicNew: 1, HALT
        uint64_t *bns_caller_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 20);
        uint8_t *bnsbc = (uint8_t *)&OBJ_FIELD(bns_caller_bc, 0);
        bnsbc[0] = BC_PUSH_SELF; // push receiver (array_class)
        bnsbc[1] = BC_PUSH_LITERAL;
        WRITE_U32(&bnsbc[2], 1); // literal 1 = size arg
        bnsbc[6] = BC_SEND_MESSAGE;
        WRITE_U32(&bnsbc[7], 0);  // selector lit index 0
        WRITE_U32(&bnsbc[11], 1); // 1 arg
        bnsbc[15] = BC_HALT;

        uint64_t *bns_lits = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 2);
        OBJ_FIELD(bns_lits, 0) = sel_basicNewSize;
        OBJ_FIELD(bns_lits, 1) = tag_smallint(5); // size argument

        uint64_t *bns_caller_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(bns_caller_cm, CM_PRIMITIVE) = tag_smallint(0);
        OBJ_FIELD(bns_caller_cm, CM_NUM_ARGS) = tag_smallint(0);
        OBJ_FIELD(bns_caller_cm, CM_NUM_TEMPS) = tag_smallint(0);
        OBJ_FIELD(bns_caller_cm, CM_LITERALS) = (uint64_t)bns_lits;
        OBJ_FIELD(bns_caller_cm, CM_BYTECODES) = (uint64_t)bns_caller_bc;

        sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
        fp = (uint64_t *)0xCAFE;
        stack_push(&sp, stack, (uint64_t)array_class);

        activate_method(&sp, &fp, 0, (uint64_t)bns_caller_cm, 0, 0);
        result = interpret(&sp, &fp,
                           (uint8_t *)&OBJ_FIELD(bns_caller_bc, 0),
                           class_table, om, NULL);

        ASSERT_EQ(ctx, result & 3, 0, "basicNew: indexable result is object ptr");
        uint64_t *arr = (uint64_t *)result;
        ASSERT_EQ(ctx, OBJ_CLASS(arr), (uint64_t)array_class,
                  "basicNew: indexable class correct");
        ASSERT_EQ(ctx, OBJ_FORMAT(arr), FORMAT_INDEXABLE,
                  "basicNew: indexable format correct");
        ASSERT_EQ(ctx, OBJ_SIZE(arr), 5,
                  "basicNew: indexable size = 5");
        ASSERT_EQ(ctx, OBJ_FIELD(arr, 0), tagged_nil(),
                  "basicNew: indexable field 0 = nil");
        ASSERT_EQ(ctx, OBJ_FIELD(arr, 4), tagged_nil(),
                  "basicNew: indexable field 4 = nil");
    }

    // --- basicNew: on a byte-indexable class → correct size ---
    {
        uint64_t sel_basicNewSize = tag_smallint(71);

        // Create a byte-indexable class (like String/ByteArray)
        uint64_t *bytearray_class = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 4);
        OBJ_FIELD(bytearray_class, CLASS_SUPERCLASS) = tagged_nil();
        OBJ_FIELD(bytearray_class, CLASS_INST_SIZE) = tag_smallint(0);
        OBJ_FIELD(bytearray_class, CLASS_METHOD_DICT) = tagged_nil();
        OBJ_FIELD(bytearray_class, CLASS_INST_FORMAT) = tag_smallint(FORMAT_BYTES);

        // Caller: PUSH_SELF, PUSH_LITERAL 1 (size=10), SEND #basicNew: 1, HALT
        uint64_t *bnsb_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 20);
        uint8_t *bnsbbc = (uint8_t *)&OBJ_FIELD(bnsb_bc, 0);
        bnsbbc[0] = BC_PUSH_SELF;
        bnsbbc[1] = BC_PUSH_LITERAL;
        WRITE_U32(&bnsbbc[2], 1);
        bnsbbc[6] = BC_SEND_MESSAGE;
        WRITE_U32(&bnsbbc[7], 0);
        WRITE_U32(&bnsbbc[11], 1);
        bnsbbc[15] = BC_HALT;

        uint64_t *bnsb_lits = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 2);
        OBJ_FIELD(bnsb_lits, 0) = sel_basicNewSize;
        OBJ_FIELD(bnsb_lits, 1) = tag_smallint(10); // 10 bytes

        uint64_t *bnsb_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(bnsb_cm, CM_PRIMITIVE) = tag_smallint(0);
        OBJ_FIELD(bnsb_cm, CM_NUM_ARGS) = tag_smallint(0);
        OBJ_FIELD(bnsb_cm, CM_NUM_TEMPS) = tag_smallint(0);
        OBJ_FIELD(bnsb_cm, CM_LITERALS) = (uint64_t)bnsb_lits;
        OBJ_FIELD(bnsb_cm, CM_BYTECODES) = (uint64_t)bnsb_bc;

        sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
        fp = (uint64_t *)0xCAFE;
        stack_push(&sp, stack, (uint64_t)bytearray_class);

        activate_method(&sp, &fp, 0, (uint64_t)bnsb_cm, 0, 0);
        result = interpret(&sp, &fp,
                           (uint8_t *)&OBJ_FIELD(bnsb_bc, 0),
                           class_table, om, NULL);

        ASSERT_EQ(ctx, result & 3, 0, "basicNew: bytes result is object ptr");
        uint64_t *ba = (uint64_t *)result;
        ASSERT_EQ(ctx, OBJ_CLASS(ba), (uint64_t)bytearray_class,
                  "basicNew: bytes class correct");
        ASSERT_EQ(ctx, OBJ_FORMAT(ba), FORMAT_BYTES,
                  "basicNew: bytes format correct");
        ASSERT_EQ(ctx, OBJ_SIZE(ba), 10,
                  "basicNew: bytes size = 10");
    }

    // --- size primitive ---
    {
        uint64_t sel_size = tag_smallint(72);

        uint64_t *size_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 2);
        uint64_t *size_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(size_cm, CM_PRIMITIVE) = tag_smallint(PRIM_SIZE);
        OBJ_FIELD(size_cm, CM_NUM_ARGS) = tag_smallint(0);
        OBJ_FIELD(size_cm, CM_NUM_TEMPS) = tag_smallint(0);
        OBJ_FIELD(size_cm, CM_LITERALS) = tagged_nil();
        OBJ_FIELD(size_cm, CM_BYTECODES) = (uint64_t)size_bc;

        // Create a class with size method
        uint64_t *sz_class = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 4);
        OBJ_FIELD(sz_class, CLASS_SUPERCLASS) = tagged_nil();
        OBJ_FIELD(sz_class, CLASS_INST_SIZE) = tag_smallint(0);
        OBJ_FIELD(sz_class, CLASS_INST_FORMAT) = tag_smallint(FORMAT_INDEXABLE);
        uint64_t *sz_md = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 2);
        OBJ_FIELD(sz_md, 0) = sel_size;
        OBJ_FIELD(sz_md, 1) = (uint64_t)size_cm;
        OBJ_FIELD(sz_class, CLASS_METHOD_DICT) = (uint64_t)sz_md;

        // Create an indexable object with 4 elements
        uint64_t *arr = om_alloc(om, (uint64_t)sz_class, FORMAT_INDEXABLE, 4);

        // Caller: PUSH_SELF, SEND #size 0, HALT
        uint64_t *sz_caller_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 16);
        uint8_t *szbc = (uint8_t *)&OBJ_FIELD(sz_caller_bc, 0);
        szbc[0] = BC_PUSH_SELF;
        szbc[1] = BC_SEND_MESSAGE;
        WRITE_U32(&szbc[2], 0);
        WRITE_U32(&szbc[6], 0);
        szbc[10] = BC_HALT;

        uint64_t *sz_lits = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 1);
        OBJ_FIELD(sz_lits, 0) = sel_size;

        uint64_t *sz_caller_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(sz_caller_cm, CM_PRIMITIVE) = tag_smallint(0);
        OBJ_FIELD(sz_caller_cm, CM_NUM_ARGS) = tag_smallint(0);
        OBJ_FIELD(sz_caller_cm, CM_NUM_TEMPS) = tag_smallint(0);
        OBJ_FIELD(sz_caller_cm, CM_LITERALS) = (uint64_t)sz_lits;
        OBJ_FIELD(sz_caller_cm, CM_BYTECODES) = (uint64_t)sz_caller_bc;

        sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
        fp = (uint64_t *)0xCAFE;
        stack_push(&sp, stack, (uint64_t)arr);

        activate_method(&sp, &fp, 0, (uint64_t)sz_caller_cm, 0, 0);
        result = interpret(&sp, &fp,
                           (uint8_t *)&OBJ_FIELD(sz_caller_bc, 0),
                           class_table, om, NULL);

        ASSERT_EQ(ctx, result, tag_smallint(4),
                  "size: indexable object size = 4");
    }

    // --- == identity primitive ---
    {
        uint64_t sel_eq_eq = tag_smallint(73);

        // Add == to test_class (any class will do)
        uint64_t *eq_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 2);
        uint64_t *eq_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(eq_cm, CM_PRIMITIVE) = tag_smallint(PRIM_IDENTITY_EQ);
        OBJ_FIELD(eq_cm, CM_NUM_ARGS) = tag_smallint(1);
        OBJ_FIELD(eq_cm, CM_NUM_TEMPS) = tag_smallint(0);
        OBJ_FIELD(eq_cm, CM_LITERALS) = tagged_nil();
        OBJ_FIELD(eq_cm, CM_BYTECODES) = (uint64_t)eq_bc;

        uint64_t *eq_class = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 4);
        OBJ_FIELD(eq_class, CLASS_SUPERCLASS) = tagged_nil();
        OBJ_FIELD(eq_class, CLASS_INST_SIZE) = tag_smallint(0);
        OBJ_FIELD(eq_class, CLASS_INST_FORMAT) = tag_smallint(FORMAT_FIELDS);
        uint64_t *eq_md = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 2);
        OBJ_FIELD(eq_md, 0) = sel_eq_eq;
        OBJ_FIELD(eq_md, 1) = (uint64_t)eq_cm;
        OBJ_FIELD(eq_class, CLASS_METHOD_DICT) = (uint64_t)eq_md;

        uint64_t *objX = om_alloc(om, (uint64_t)eq_class, FORMAT_FIELDS, 0);
        uint64_t *objY = om_alloc(om, (uint64_t)eq_class, FORMAT_FIELDS, 0);

        // Caller: PUSH_SELF, PUSH_LITERAL 1, SEND == 1, HALT
        uint64_t *eq_caller_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 20);
        uint8_t *eqbc = (uint8_t *)&OBJ_FIELD(eq_caller_bc, 0);
        eqbc[0] = BC_PUSH_SELF;
        eqbc[1] = BC_PUSH_LITERAL;
        WRITE_U32(&eqbc[2], 1);
        eqbc[6] = BC_SEND_MESSAGE;
        WRITE_U32(&eqbc[7], 0);
        WRITE_U32(&eqbc[11], 1);
        eqbc[15] = BC_HALT;

        // Test 1: objX == objX → true
        uint64_t *eq_lits1 = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 2);
        OBJ_FIELD(eq_lits1, 0) = sel_eq_eq;
        OBJ_FIELD(eq_lits1, 1) = (uint64_t)objX; // same object

        uint64_t *eq_caller_cm1 = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(eq_caller_cm1, CM_PRIMITIVE) = tag_smallint(0);
        OBJ_FIELD(eq_caller_cm1, CM_NUM_ARGS) = tag_smallint(0);
        OBJ_FIELD(eq_caller_cm1, CM_NUM_TEMPS) = tag_smallint(0);
        OBJ_FIELD(eq_caller_cm1, CM_LITERALS) = (uint64_t)eq_lits1;
        OBJ_FIELD(eq_caller_cm1, CM_BYTECODES) = (uint64_t)eq_caller_bc;

        sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
        fp = (uint64_t *)0xCAFE;
        stack_push(&sp, stack, (uint64_t)objX);
        activate_method(&sp, &fp, 0, (uint64_t)eq_caller_cm1, 0, 0);
        result = interpret(&sp, &fp,
                           (uint8_t *)&OBJ_FIELD(eq_caller_bc, 0),
                           class_table, om, NULL);
        ASSERT_EQ(ctx, result, tagged_true(), "==: same object → true");

        // Test 2: objX == objY → false
        uint64_t *eq_lits2 = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 2);
        OBJ_FIELD(eq_lits2, 0) = sel_eq_eq;
        OBJ_FIELD(eq_lits2, 1) = (uint64_t)objY; // different object

        uint64_t *eq_caller_cm2 = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(eq_caller_cm2, CM_PRIMITIVE) = tag_smallint(0);
        OBJ_FIELD(eq_caller_cm2, CM_NUM_ARGS) = tag_smallint(0);
        OBJ_FIELD(eq_caller_cm2, CM_NUM_TEMPS) = tag_smallint(0);
        OBJ_FIELD(eq_caller_cm2, CM_LITERALS) = (uint64_t)eq_lits2;
        OBJ_FIELD(eq_caller_cm2, CM_BYTECODES) = (uint64_t)eq_caller_bc;

        sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
        fp = (uint64_t *)0xCAFE;
        stack_push(&sp, stack, (uint64_t)objX);
        activate_method(&sp, &fp, 0, (uint64_t)eq_caller_cm2, 0, 0);
        result = interpret(&sp, &fp,
                           (uint8_t *)&OBJ_FIELD(eq_caller_bc, 0),
                           class_table, om, NULL);
        ASSERT_EQ(ctx, result, tagged_false(), "==: different object → false");
    }

    // --- basicClass primitive ---
    {
        uint64_t sel_basicClass = tag_smallint(74);

        uint64_t *bc_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 2);
        uint64_t *bc_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(bc_cm, CM_PRIMITIVE) = tag_smallint(PRIM_BASIC_CLASS);
        OBJ_FIELD(bc_cm, CM_NUM_ARGS) = tag_smallint(0);
        OBJ_FIELD(bc_cm, CM_NUM_TEMPS) = tag_smallint(0);
        OBJ_FIELD(bc_cm, CM_LITERALS) = tagged_nil();
        OBJ_FIELD(bc_cm, CM_BYTECODES) = (uint64_t)bc_bc;

        // A class with basicClass in its method dict
        uint64_t *dog_class = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 4);
        OBJ_FIELD(dog_class, CLASS_SUPERCLASS) = tagged_nil();
        OBJ_FIELD(dog_class, CLASS_INST_SIZE) = tag_smallint(0);
        OBJ_FIELD(dog_class, CLASS_INST_FORMAT) = tag_smallint(FORMAT_FIELDS);
        uint64_t *bc_md = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 2);
        OBJ_FIELD(bc_md, 0) = sel_basicClass;
        OBJ_FIELD(bc_md, 1) = (uint64_t)bc_cm;
        OBJ_FIELD(dog_class, CLASS_METHOD_DICT) = (uint64_t)bc_md;

        uint64_t *dog = om_alloc(om, (uint64_t)dog_class, FORMAT_FIELDS, 0);

        // Caller: PUSH_SELF, SEND #basicClass 0, HALT
        uint64_t *bc_caller_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 16);
        uint8_t *bcbc = (uint8_t *)&OBJ_FIELD(bc_caller_bc, 0);
        bcbc[0] = BC_PUSH_SELF;
        bcbc[1] = BC_SEND_MESSAGE;
        WRITE_U32(&bcbc[2], 0);
        WRITE_U32(&bcbc[6], 0);
        bcbc[10] = BC_HALT;

        uint64_t *bc_lits = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 1);
        OBJ_FIELD(bc_lits, 0) = sel_basicClass;

        uint64_t *bc_caller_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(bc_caller_cm, CM_PRIMITIVE) = tag_smallint(0);
        OBJ_FIELD(bc_caller_cm, CM_NUM_ARGS) = tag_smallint(0);
        OBJ_FIELD(bc_caller_cm, CM_NUM_TEMPS) = tag_smallint(0);
        OBJ_FIELD(bc_caller_cm, CM_LITERALS) = (uint64_t)bc_lits;
        OBJ_FIELD(bc_caller_cm, CM_BYTECODES) = (uint64_t)bc_caller_bc;

        sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
        fp = (uint64_t *)0xCAFE;
        stack_push(&sp, stack, (uint64_t)dog);
        activate_method(&sp, &fp, 0, (uint64_t)bc_caller_cm, 0, 0);
        result = interpret(&sp, &fp,
                           (uint8_t *)&OBJ_FIELD(bc_caller_bc, 0),
                           class_table, om, NULL);
        ASSERT_EQ(ctx, result, (uint64_t)dog_class,
                  "basicClass: heap object returns its class");
    }

    // --- thisContext pseudo-variable bytecode ---
    {
        uint64_t *ctx_user_class = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 4);
        OBJ_FIELD(ctx_user_class, CLASS_SUPERCLASS) = tagged_nil();
        OBJ_FIELD(ctx_user_class, CLASS_INST_SIZE) = tag_smallint(0);
        OBJ_FIELD(ctx_user_class, CLASS_INST_FORMAT) = tag_smallint(FORMAT_FIELDS);
        OBJ_FIELD(ctx_user_class, CLASS_METHOD_DICT) = tagged_nil();

        uint64_t *ctx_user = om_alloc(om, (uint64_t)ctx_user_class, FORMAT_FIELDS, 0);

        uint64_t *tc_caller_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 2);
        uint8_t *tcbc = (uint8_t *)&OBJ_FIELD(tc_caller_bc, 0);
        tcbc[0] = BC_PUSH_THIS_CONTEXT;
        tcbc[1] = BC_HALT;

        uint64_t *tc_caller_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(tc_caller_cm, CM_PRIMITIVE) = tag_smallint(0);
        OBJ_FIELD(tc_caller_cm, CM_NUM_ARGS) = tag_smallint(0);
        OBJ_FIELD(tc_caller_cm, CM_NUM_TEMPS) = tag_smallint(0);
        OBJ_FIELD(tc_caller_cm, CM_LITERALS) = tagged_nil();
        OBJ_FIELD(tc_caller_cm, CM_BYTECODES) = (uint64_t)tc_caller_bc;

        sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
        fp = (uint64_t *)0xCAFE;
        stack_push(&sp, stack, (uint64_t)ctx_user);
        activate_method(&sp, &fp, 0, (uint64_t)tc_caller_cm, 0, 0);
        result = interpret(&sp, &fp,
                           (uint8_t *)&OBJ_FIELD(tc_caller_bc, 0),
                           class_table, om, NULL);
        ASSERT_EQ(ctx, is_object_ptr(result), 1, "thisContext: returns a heap context object");
        ASSERT_EQ(ctx, OBJ_CLASS((uint64_t *)result), (uint64_t)ctx->context_class,
                  "thisContext: returns a Context instance");
        ASSERT_EQ(ctx, OBJ_FIELD((uint64_t *)result, CONTEXT_RECEIVER), (uint64_t)ctx_user,
                  "thisContext: context receiver is the active receiver");
        ASSERT_EQ(ctx, OBJ_FIELD((uint64_t *)result, CONTEXT_METHOD), (uint64_t)tc_caller_cm,
                  "thisContext: context method is the active method");

    }

    // --- class method: ^ self basicClass ---
    {
        uint64_t sel_class = tag_smallint(75);
        uint64_t sel_basicClass = tag_smallint(74);

        // basicClass prim method
        uint64_t *bc2_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 2);
        uint64_t *bc2_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(bc2_cm, CM_PRIMITIVE) = tag_smallint(PRIM_BASIC_CLASS);
        OBJ_FIELD(bc2_cm, CM_NUM_ARGS) = tag_smallint(0);
        OBJ_FIELD(bc2_cm, CM_NUM_TEMPS) = tag_smallint(0);
        OBJ_FIELD(bc2_cm, CM_LITERALS) = tagged_nil();
        OBJ_FIELD(bc2_cm, CM_BYTECODES) = (uint64_t)bc2_bc;

        // class method: PUSH_SELF, SEND #basicClass 0, RETURN
        uint64_t *cl_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 16);
        uint8_t *clbc = (uint8_t *)&OBJ_FIELD(cl_bc, 0);
        clbc[0] = BC_PUSH_SELF;
        clbc[1] = BC_SEND_MESSAGE;
        WRITE_U32(&clbc[2], 0); // lit 0 = #basicClass
        WRITE_U32(&clbc[6], 0); // 0 args
        clbc[10] = BC_RETURN;

        uint64_t *cl_lits = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 1);
        OBJ_FIELD(cl_lits, 0) = sel_basicClass;

        uint64_t *cl_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(cl_cm, CM_PRIMITIVE) = tag_smallint(0);
        OBJ_FIELD(cl_cm, CM_NUM_ARGS) = tag_smallint(0);
        OBJ_FIELD(cl_cm, CM_NUM_TEMPS) = tag_smallint(0);
        OBJ_FIELD(cl_cm, CM_LITERALS) = (uint64_t)cl_lits;
        OBJ_FIELD(cl_cm, CM_BYTECODES) = (uint64_t)cl_bc;

        // A class with both #basicClass and #class
        uint64_t *cat_class = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 4);
        OBJ_FIELD(cat_class, CLASS_SUPERCLASS) = tagged_nil();
        OBJ_FIELD(cat_class, CLASS_INST_SIZE) = tag_smallint(0);
        OBJ_FIELD(cat_class, CLASS_INST_FORMAT) = tag_smallint(FORMAT_FIELDS);
        uint64_t *cl_md = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 4);
        OBJ_FIELD(cl_md, 0) = sel_basicClass;
        OBJ_FIELD(cl_md, 1) = (uint64_t)bc2_cm;
        OBJ_FIELD(cl_md, 2) = sel_class;
        OBJ_FIELD(cl_md, 3) = (uint64_t)cl_cm;
        OBJ_FIELD(cat_class, CLASS_METHOD_DICT) = (uint64_t)cl_md;

        uint64_t *cat = om_alloc(om, (uint64_t)cat_class, FORMAT_FIELDS, 0);

        // Caller: PUSH_SELF, SEND #class 0, HALT
        uint64_t *cl_caller_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 16);
        uint8_t *clcbc = (uint8_t *)&OBJ_FIELD(cl_caller_bc, 0);
        clcbc[0] = BC_PUSH_SELF;
        clcbc[1] = BC_SEND_MESSAGE;
        WRITE_U32(&clcbc[2], 0);
        WRITE_U32(&clcbc[6], 0);
        clcbc[10] = BC_HALT;

        uint64_t *cl_caller_lits = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 1);
        OBJ_FIELD(cl_caller_lits, 0) = sel_class;

        uint64_t *cl_caller_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(cl_caller_cm, CM_PRIMITIVE) = tag_smallint(0);
        OBJ_FIELD(cl_caller_cm, CM_NUM_ARGS) = tag_smallint(0);
        OBJ_FIELD(cl_caller_cm, CM_NUM_TEMPS) = tag_smallint(0);
        OBJ_FIELD(cl_caller_cm, CM_LITERALS) = (uint64_t)cl_caller_lits;
        OBJ_FIELD(cl_caller_cm, CM_BYTECODES) = (uint64_t)cl_caller_bc;

        sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
        fp = (uint64_t *)0xCAFE;
        stack_push(&sp, stack, (uint64_t)cat);
        activate_method(&sp, &fp, 0, (uint64_t)cl_caller_cm, 0, 0);
        result = interpret(&sp, &fp,
                           (uint8_t *)&OBJ_FIELD(cl_caller_bc, 0),
                           class_table, om, NULL);
        ASSERT_EQ(ctx, result, (uint64_t)cat_class,
                  "class: Smalltalk method via basicClass returns class");
    }

    // --- basicClass on Character immediate ---
    {
        uint64_t sel_basicClass = tag_smallint(74);

        // Character class should have basicClass method
        uint64_t *char_class = ctx->character_class;
        uint64_t *cbc_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 2);
        uint64_t *cbc_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(cbc_cm, CM_PRIMITIVE) = tag_smallint(PRIM_BASIC_CLASS);
        OBJ_FIELD(cbc_cm, CM_NUM_ARGS) = tag_smallint(0);
        OBJ_FIELD(cbc_cm, CM_NUM_TEMPS) = tag_smallint(0);
        OBJ_FIELD(cbc_cm, CM_LITERALS) = tagged_nil();
        OBJ_FIELD(cbc_cm, CM_BYTECODES) = (uint64_t)cbc_bc;

        uint64_t *cbc_md = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 2);
        OBJ_FIELD(cbc_md, 0) = sel_basicClass;
        OBJ_FIELD(cbc_md, 1) = (uint64_t)cbc_cm;
        OBJ_FIELD(char_class, CLASS_METHOD_DICT) = (uint64_t)cbc_md;

        // Caller: PUSH_LITERAL 0 ($A), SEND #basicClass 0, HALT
        uint64_t *cbc_caller_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 16);
        uint8_t *cbcbc = (uint8_t *)&OBJ_FIELD(cbc_caller_bc, 0);
        cbcbc[0] = BC_PUSH_LITERAL;
        WRITE_U32(&cbcbc[1], 0); // lit 0 = $A
        cbcbc[5] = BC_SEND_MESSAGE;
        WRITE_U32(&cbcbc[6], 1);  // lit 1 = #basicClass
        WRITE_U32(&cbcbc[10], 0); // 0 args
        cbcbc[14] = BC_HALT;

        uint64_t *cbc_lits = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 2);
        OBJ_FIELD(cbc_lits, 0) = tag_character(65); // $A
        OBJ_FIELD(cbc_lits, 1) = sel_basicClass;

        uint64_t *cbc_ccm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(cbc_ccm, CM_PRIMITIVE) = tag_smallint(0);
        OBJ_FIELD(cbc_ccm, CM_NUM_ARGS) = tag_smallint(0);
        OBJ_FIELD(cbc_ccm, CM_NUM_TEMPS) = tag_smallint(0);
        OBJ_FIELD(cbc_ccm, CM_LITERALS) = (uint64_t)cbc_lits;
        OBJ_FIELD(cbc_ccm, CM_BYTECODES) = (uint64_t)cbc_caller_bc;

        sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
        fp = (uint64_t *)0xCAFE;
        stack_push(&sp, stack, tag_smallint(0));
        activate_method(&sp, &fp, 0, (uint64_t)cbc_ccm, 0, 0);
        result = interpret(&sp, &fp,
                           (uint8_t *)&OBJ_FIELD(cbc_caller_bc, 0),
                           class_table, om, NULL);
        ASSERT_EQ(ctx, result, (uint64_t)char_class,
                  "basicClass: Character $A returns Character class");
    }

    // --- basicClass on nil immediate ---
    {
        uint64_t sel_basicClass = tag_smallint(74);

        uint64_t *uo_class = ctx->undefined_object_class;
        uint64_t *uocm_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 2);
        uint64_t *uocm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(uocm, CM_PRIMITIVE) = tag_smallint(PRIM_BASIC_CLASS);
        OBJ_FIELD(uocm, CM_NUM_ARGS) = tag_smallint(0);
        OBJ_FIELD(uocm, CM_NUM_TEMPS) = tag_smallint(0);
        OBJ_FIELD(uocm, CM_LITERALS) = tagged_nil();
        OBJ_FIELD(uocm, CM_BYTECODES) = (uint64_t)uocm_bc;

        uint64_t *uomd = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 2);
        OBJ_FIELD(uomd, 0) = sel_basicClass;
        OBJ_FIELD(uomd, 1) = (uint64_t)uocm;
        OBJ_FIELD(uo_class, CLASS_METHOD_DICT) = (uint64_t)uomd;

        uint64_t *uocaller_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 16);
        uint8_t *uobb = (uint8_t *)&OBJ_FIELD(uocaller_bc, 0);
        uobb[0] = BC_PUSH_LITERAL;
        WRITE_U32(&uobb[1], 0); // nil
        uobb[5] = BC_SEND_MESSAGE;
        WRITE_U32(&uobb[6], 1); // #basicClass
        WRITE_U32(&uobb[10], 0);
        uobb[14] = BC_HALT;

        uint64_t *uolits = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 2);
        OBJ_FIELD(uolits, 0) = tagged_nil();
        OBJ_FIELD(uolits, 1) = sel_basicClass;

        uint64_t *uocaller_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(uocaller_cm, CM_PRIMITIVE) = tag_smallint(0);
        OBJ_FIELD(uocaller_cm, CM_NUM_ARGS) = tag_smallint(0);
        OBJ_FIELD(uocaller_cm, CM_NUM_TEMPS) = tag_smallint(0);
        OBJ_FIELD(uocaller_cm, CM_LITERALS) = (uint64_t)uolits;
        OBJ_FIELD(uocaller_cm, CM_BYTECODES) = (uint64_t)uocaller_bc;

        sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
        fp = (uint64_t *)0xCAFE;
        stack_push(&sp, stack, tag_smallint(0));
        activate_method(&sp, &fp, 0, (uint64_t)uocaller_cm, 0, 0);
        result = interpret(&sp, &fp,
                           (uint8_t *)&OBJ_FIELD(uocaller_bc, 0),
                           class_table, om, NULL);
        ASSERT_EQ(ctx, result, (uint64_t)uo_class,
                  "basicClass: nil returns UndefinedObject class");
    }

    // --- Character>>value primitive ---
    {
        uint64_t sel_value = tag_smallint(81);

        uint64_t *cv_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 2);
        uint64_t *cv_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(cv_cm, CM_PRIMITIVE) = tag_smallint(PRIM_CHAR_VALUE);
        OBJ_FIELD(cv_cm, CM_NUM_ARGS) = tag_smallint(0);
        OBJ_FIELD(cv_cm, CM_NUM_TEMPS) = tag_smallint(0);
        OBJ_FIELD(cv_cm, CM_LITERALS) = tagged_nil();
        OBJ_FIELD(cv_cm, CM_BYTECODES) = (uint64_t)cv_bc;

        // Add value to Character class method dict
        uint64_t *char_class2 = ctx->character_class;
        uint64_t md_val = OBJ_FIELD(char_class2, CLASS_METHOD_DICT);
        uint64_t *md_old = (md_val != tagged_nil() && (md_val & 3) == 0)
                               ? (uint64_t *)md_val
                               : NULL;
        uint64_t md_sz = md_old ? OBJ_SIZE(md_old) : 0;
        uint64_t *md_new = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, md_sz + 2);
        for (uint64_t i = 0; i < md_sz; i++)
            OBJ_FIELD(md_new, i) = OBJ_FIELD(md_old, i);
        OBJ_FIELD(md_new, md_sz) = sel_value;
        OBJ_FIELD(md_new, md_sz + 1) = (uint64_t)cv_cm;
        OBJ_FIELD(char_class2, CLASS_METHOD_DICT) = (uint64_t)md_new;

        // Caller: PUSH_LITERAL 0 ($A), SEND #value 0, HALT
        uint64_t *cv_cbc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 16);
        uint8_t *cvbc = (uint8_t *)&OBJ_FIELD(cv_cbc, 0);
        cvbc[0] = BC_PUSH_LITERAL;
        WRITE_U32(&cvbc[1], 0);
        cvbc[5] = BC_SEND_MESSAGE;
        WRITE_U32(&cvbc[6], 1);
        WRITE_U32(&cvbc[10], 0);
        cvbc[14] = BC_HALT;

        uint64_t *cv_lits = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 2);
        OBJ_FIELD(cv_lits, 0) = tag_character(65);
        OBJ_FIELD(cv_lits, 1) = sel_value;

        uint64_t *cv_ccm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(cv_ccm, CM_PRIMITIVE) = tag_smallint(0);
        OBJ_FIELD(cv_ccm, CM_NUM_ARGS) = tag_smallint(0);
        OBJ_FIELD(cv_ccm, CM_NUM_TEMPS) = tag_smallint(0);
        OBJ_FIELD(cv_ccm, CM_LITERALS) = (uint64_t)cv_lits;
        OBJ_FIELD(cv_ccm, CM_BYTECODES) = (uint64_t)cv_cbc;

        sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
        fp = (uint64_t *)0xCAFE;
        stack_push(&sp, stack, tag_smallint(0));
        activate_method(&sp, &fp, 0, (uint64_t)cv_ccm, 0, 0);
        result = interpret(&sp, &fp,
                           (uint8_t *)&OBJ_FIELD(cv_cbc, 0),
                           class_table, om, NULL);
        ASSERT_EQ(ctx, result, tag_smallint(65),
                  "Character>>value: $A value = 65");

        ASSERT_EQ(ctx, (uint64_t)run_trap_test(ctx, trap_char_value_wrong_receiver),
                  (uint64_t)SIGTRAP,
                  "primitive trap: Character>>value wrong receiver traps");
    }

    // --- SmallInteger>>asCharacter primitive ---
    {
        uint64_t sel_asChar = tag_smallint(82);

        uint64_t *ac_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 2);
        uint64_t *ac_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(ac_cm, CM_PRIMITIVE) = tag_smallint(PRIM_AS_CHARACTER);
        OBJ_FIELD(ac_cm, CM_NUM_ARGS) = tag_smallint(0);
        OBJ_FIELD(ac_cm, CM_NUM_TEMPS) = tag_smallint(0);
        OBJ_FIELD(ac_cm, CM_LITERALS) = tagged_nil();
        OBJ_FIELD(ac_cm, CM_BYTECODES) = (uint64_t)ac_bc;

        // Add asCharacter to SmallInteger class
        uint64_t *si_class = ctx->smallint_class;
        uint64_t si_md_val = OBJ_FIELD(si_class, CLASS_METHOD_DICT);
        uint64_t *si_md_old = (si_md_val != tagged_nil() && (si_md_val & 3) == 0)
                                  ? (uint64_t *)si_md_val
                                  : NULL;
        uint64_t si_md_sz = si_md_old ? OBJ_SIZE(si_md_old) : 0;
        uint64_t *si_md_new = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, si_md_sz + 2);
        for (uint64_t i = 0; i < si_md_sz; i++)
            OBJ_FIELD(si_md_new, i) = OBJ_FIELD(si_md_old, i);
        OBJ_FIELD(si_md_new, si_md_sz) = sel_asChar;
        OBJ_FIELD(si_md_new, si_md_sz + 1) = (uint64_t)ac_cm;
        OBJ_FIELD(si_class, CLASS_METHOD_DICT) = (uint64_t)si_md_new;

        // Caller: PUSH_LITERAL 0 (65), SEND #asCharacter 0, HALT
        uint64_t *ac_cbc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 16);
        uint8_t *acbc = (uint8_t *)&OBJ_FIELD(ac_cbc, 0);
        acbc[0] = BC_PUSH_LITERAL;
        WRITE_U32(&acbc[1], 0);
        acbc[5] = BC_SEND_MESSAGE;
        WRITE_U32(&acbc[6], 1);
        WRITE_U32(&acbc[10], 0);
        acbc[14] = BC_HALT;

        uint64_t *ac_lits = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 2);
        OBJ_FIELD(ac_lits, 0) = tag_smallint(65);
        OBJ_FIELD(ac_lits, 1) = sel_asChar;

        uint64_t *ac_ccm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(ac_ccm, CM_PRIMITIVE) = tag_smallint(0);
        OBJ_FIELD(ac_ccm, CM_NUM_ARGS) = tag_smallint(0);
        OBJ_FIELD(ac_ccm, CM_NUM_TEMPS) = tag_smallint(0);
        OBJ_FIELD(ac_ccm, CM_LITERALS) = (uint64_t)ac_lits;
        OBJ_FIELD(ac_ccm, CM_BYTECODES) = (uint64_t)ac_cbc;

        sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
        fp = (uint64_t *)0xCAFE;
        stack_push(&sp, stack, tag_smallint(0));
        activate_method(&sp, &fp, 0, (uint64_t)ac_ccm, 0, 0);
        result = interpret(&sp, &fp,
                           (uint8_t *)&OBJ_FIELD(ac_cbc, 0),
                           class_table, om, NULL);
        ASSERT_EQ(ctx, result, tag_character(65),
                  "SmallInteger>>asCharacter: 65 asCharacter = $A");
    }

    ASSERT_EQ(ctx, (uint64_t)run_trap_test(ctx, trap_as_character_wrong_receiver),
              (uint64_t)SIGTRAP,
              "primitive trap: SmallInteger>>asCharacter wrong receiver traps");

    // --- Character isLetter, isDigit, asUppercase, asLowercase ---
    {
        uint64_t sel_isLetter = tag_smallint(83);
        uint64_t sel_isDigit = tag_smallint(84);
        uint64_t sel_upper = tag_smallint(85);
        uint64_t sel_lower = tag_smallint(86);

        // Create prim methods
        uint64_t *dummy_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 2);

#define MAKE_PRIM_CM(name, prim_num)                                        \
    uint64_t *name = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5); \
    OBJ_FIELD(name, CM_PRIMITIVE) = tag_smallint(prim_num);                 \
    OBJ_FIELD(name, CM_NUM_ARGS) = tag_smallint(0);                         \
    OBJ_FIELD(name, CM_NUM_TEMPS) = tag_smallint(0);                        \
    OBJ_FIELD(name, CM_LITERALS) = tagged_nil();                            \
    OBJ_FIELD(name, CM_BYTECODES) = (uint64_t)dummy_bc;

        MAKE_PRIM_CM(cm_isLetter, PRIM_CHAR_IS_LETTER)
        MAKE_PRIM_CM(cm_isDigit, PRIM_CHAR_IS_DIGIT)
        MAKE_PRIM_CM(cm_upper, PRIM_CHAR_UPPERCASE)
        MAKE_PRIM_CM(cm_lower, PRIM_CHAR_LOWERCASE)
#undef MAKE_PRIM_CM

        // Add all to Character class
        uint64_t *cc = ctx->character_class;
        uint64_t cc_md_val = OBJ_FIELD(cc, CLASS_METHOD_DICT);
        uint64_t *cc_md_old = (cc_md_val != tagged_nil() && (cc_md_val & 3) == 0)
                                  ? (uint64_t *)cc_md_val
                                  : NULL;
        uint64_t cc_md_sz = cc_md_old ? OBJ_SIZE(cc_md_old) : 0;
        uint64_t *cc_md = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, cc_md_sz + 8);
        for (uint64_t i = 0; i < cc_md_sz; i++)
            OBJ_FIELD(cc_md, i) = OBJ_FIELD(cc_md_old, i);
        OBJ_FIELD(cc_md, cc_md_sz + 0) = sel_isLetter;
        OBJ_FIELD(cc_md, cc_md_sz + 1) = (uint64_t)cm_isLetter;
        OBJ_FIELD(cc_md, cc_md_sz + 2) = sel_isDigit;
        OBJ_FIELD(cc_md, cc_md_sz + 3) = (uint64_t)cm_isDigit;
        OBJ_FIELD(cc_md, cc_md_sz + 4) = sel_upper;
        OBJ_FIELD(cc_md, cc_md_sz + 5) = (uint64_t)cm_upper;
        OBJ_FIELD(cc_md, cc_md_sz + 6) = sel_lower;
        OBJ_FIELD(cc_md, cc_md_sz + 7) = (uint64_t)cm_lower;
        OBJ_FIELD(cc, CLASS_METHOD_DICT) = (uint64_t)cc_md;

// Helper: caller bytecodes PUSH_LITERAL 0, SEND lit1 0, HALT
#define RUN_CHAR_PRIM(char_val, sel_lit, expected, msg)                             \
    {                                                                               \
        uint64_t *_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 16);      \
        uint8_t *_b = (uint8_t *)&OBJ_FIELD(_bc, 0);                                \
        _b[0] = BC_PUSH_LITERAL;                                                    \
        WRITE_U32(&_b[1], 0);                                                       \
        _b[5] = BC_SEND_MESSAGE;                                                    \
        WRITE_U32(&_b[6], 1);                                                       \
        WRITE_U32(&_b[10], 0);                                                      \
        _b[14] = BC_HALT;                                                           \
        uint64_t *_lits = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 2); \
        OBJ_FIELD(_lits, 0) = char_val;                                             \
        OBJ_FIELD(_lits, 1) = sel_lit;                                              \
        uint64_t *_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);      \
        OBJ_FIELD(_cm, CM_PRIMITIVE) = tag_smallint(0);                             \
        OBJ_FIELD(_cm, CM_NUM_ARGS) = tag_smallint(0);                              \
        OBJ_FIELD(_cm, CM_NUM_TEMPS) = tag_smallint(0);                             \
        OBJ_FIELD(_cm, CM_LITERALS) = (uint64_t)_lits;                              \
        OBJ_FIELD(_cm, CM_BYTECODES) = (uint64_t)_bc;                               \
        sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));       \
        fp = (uint64_t *)0xCAFE;                                                    \
        stack_push(&sp, stack, tag_smallint(0));                                    \
        activate_method(&sp, &fp, 0, (uint64_t)_cm, 0, 0);                          \
        result = interpret(&sp, &fp, (uint8_t *)&OBJ_FIELD(_bc, 0),                 \
                           class_table, om, NULL);                                  \
        ASSERT_EQ(ctx, result, expected, msg);                                      \
    }

        // isLetter
        RUN_CHAR_PRIM(tag_character('A'), sel_isLetter, tagged_true(), "isLetter: $A → true")
        RUN_CHAR_PRIM(tag_character('z'), sel_isLetter, tagged_true(), "isLetter: $z → true")
        RUN_CHAR_PRIM(tag_character('5'), sel_isLetter, tagged_false(), "isLetter: $5 → false")
        RUN_CHAR_PRIM(tag_character(' '), sel_isLetter, tagged_false(), "isLetter: space → false")

        // isDigit
        RUN_CHAR_PRIM(tag_character('0'), sel_isDigit, tagged_true(), "isDigit: $0 → true")
        RUN_CHAR_PRIM(tag_character('9'), sel_isDigit, tagged_true(), "isDigit: $9 → true")
        RUN_CHAR_PRIM(tag_character('A'), sel_isDigit, tagged_false(), "isDigit: $A → false")

        // asUppercase
        RUN_CHAR_PRIM(tag_character('a'), sel_upper, tag_character('A'), "asUppercase: $a → $A")
        RUN_CHAR_PRIM(tag_character('z'), sel_upper, tag_character('Z'), "asUppercase: $z → $Z")
        RUN_CHAR_PRIM(tag_character('A'), sel_upper, tag_character('A'), "asUppercase: $A → $A (no change)")
        RUN_CHAR_PRIM(tag_character('5'), sel_upper, tag_character('5'), "asUppercase: $5 → $5 (no change)")

        // asLowercase
        RUN_CHAR_PRIM(tag_character('A'), sel_lower, tag_character('a'), "asLowercase: $A → $a")
        RUN_CHAR_PRIM(tag_character('Z'), sel_lower, tag_character('z'), "asLowercase: $Z → $z")
        RUN_CHAR_PRIM(tag_character('a'), sel_lower, tag_character('a'), "asLowercase: $a → $a (no change)")

        // = (identity works because same encoding)
        uint64_t sel_eq = tag_smallint(87);
        uint64_t *eq_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 2);
        uint64_t *cm_eq = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(cm_eq, CM_PRIMITIVE) = tag_smallint(PRIM_IDENTITY_EQ);
        OBJ_FIELD(cm_eq, CM_NUM_ARGS) = tag_smallint(1);
        OBJ_FIELD(cm_eq, CM_NUM_TEMPS) = tag_smallint(0);
        OBJ_FIELD(cm_eq, CM_LITERALS) = tagged_nil();
        OBJ_FIELD(cm_eq, CM_BYTECODES) = (uint64_t)eq_bc;

        // Add = to Character class
        uint64_t eq_md_val = OBJ_FIELD(cc, CLASS_METHOD_DICT);
        uint64_t *eq_md_old = (uint64_t *)eq_md_val;
        uint64_t eq_md_sz = OBJ_SIZE(eq_md_old);
        uint64_t *eq_md = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, eq_md_sz + 2);
        for (uint64_t i = 0; i < eq_md_sz; i++)
            OBJ_FIELD(eq_md, i) = OBJ_FIELD(eq_md_old, i);
        OBJ_FIELD(eq_md, eq_md_sz) = sel_eq;
        OBJ_FIELD(eq_md, eq_md_sz + 1) = (uint64_t)cm_eq;
        OBJ_FIELD(cc, CLASS_METHOD_DICT) = (uint64_t)eq_md;

        // Test: $A = $A → true (1-arg send)
        {
            uint64_t *_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 20);
            uint8_t *_b = (uint8_t *)&OBJ_FIELD(_bc, 0);
            _b[0] = BC_PUSH_LITERAL;
            WRITE_U32(&_b[1], 0); // $A
            _b[5] = BC_PUSH_LITERAL;
            WRITE_U32(&_b[6], 1); // $A
            _b[10] = BC_SEND_MESSAGE;
            WRITE_U32(&_b[11], 2); // sel = lit 2
            WRITE_U32(&_b[15], 1); // 1 arg
            _b[19] = BC_HALT;
            uint64_t *_lits = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 3);
            OBJ_FIELD(_lits, 0) = tag_character('A');
            OBJ_FIELD(_lits, 1) = tag_character('A');
            OBJ_FIELD(_lits, 2) = sel_eq;
            uint64_t *_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
            OBJ_FIELD(_cm, CM_PRIMITIVE) = tag_smallint(0);
            OBJ_FIELD(_cm, CM_NUM_ARGS) = tag_smallint(0);
            OBJ_FIELD(_cm, CM_NUM_TEMPS) = tag_smallint(0);
            OBJ_FIELD(_cm, CM_LITERALS) = (uint64_t)_lits;
            OBJ_FIELD(_cm, CM_BYTECODES) = (uint64_t)_bc;
            sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
            fp = (uint64_t *)0xCAFE;
            stack_push(&sp, stack, tag_smallint(0));
            activate_method(&sp, &fp, 0, (uint64_t)_cm, 0, 0);
            result = interpret(&sp, &fp, (uint8_t *)&OBJ_FIELD(_bc, 0),
                               class_table, om, NULL);
            ASSERT_EQ(ctx, result, tagged_true(), "Character =: $A = $A → true");
        }

        // Test: $A = $B → false
        {
            uint64_t *_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 20);
            uint8_t *_b = (uint8_t *)&OBJ_FIELD(_bc, 0);
            _b[0] = BC_PUSH_LITERAL;
            WRITE_U32(&_b[1], 0); // $A
            _b[5] = BC_PUSH_LITERAL;
            WRITE_U32(&_b[6], 1); // $B
            _b[10] = BC_SEND_MESSAGE;
            WRITE_U32(&_b[11], 2);
            WRITE_U32(&_b[15], 1);
            _b[19] = BC_HALT;
            uint64_t *_lits = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 3);
            OBJ_FIELD(_lits, 0) = tag_character('A');
            OBJ_FIELD(_lits, 1) = tag_character('B');
            OBJ_FIELD(_lits, 2) = sel_eq;
            uint64_t *_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
            OBJ_FIELD(_cm, CM_PRIMITIVE) = tag_smallint(0);
            OBJ_FIELD(_cm, CM_NUM_ARGS) = tag_smallint(0);
            OBJ_FIELD(_cm, CM_NUM_TEMPS) = tag_smallint(0);
            OBJ_FIELD(_cm, CM_LITERALS) = (uint64_t)_lits;
            OBJ_FIELD(_cm, CM_BYTECODES) = (uint64_t)_bc;
            sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
            fp = (uint64_t *)0xCAFE;
            stack_push(&sp, stack, tag_smallint(0));
            activate_method(&sp, &fp, 0, (uint64_t)_cm, 0, 0);
            result = interpret(&sp, &fp, (uint8_t *)&OBJ_FIELD(_bc, 0),
                               class_table, om, NULL);
            ASSERT_EQ(ctx, result, tagged_false(), "Character =: $A = $B → false");
        }

#undef RUN_CHAR_PRIM
    }

    // --- hash primitive ---
    {
        uint64_t sel_hash = tag_smallint(76);

        uint64_t *h_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 2);
        uint64_t *h_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(h_cm, CM_PRIMITIVE) = tag_smallint(PRIM_HASH);
        OBJ_FIELD(h_cm, CM_NUM_ARGS) = tag_smallint(0);
        OBJ_FIELD(h_cm, CM_NUM_TEMPS) = tag_smallint(0);
        OBJ_FIELD(h_cm, CM_LITERALS) = tagged_nil();
        OBJ_FIELD(h_cm, CM_BYTECODES) = (uint64_t)h_bc;

        uint64_t *h_class = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 4);
        OBJ_FIELD(h_class, CLASS_SUPERCLASS) = tagged_nil();
        OBJ_FIELD(h_class, CLASS_INST_SIZE) = tag_smallint(0);
        OBJ_FIELD(h_class, CLASS_INST_FORMAT) = tag_smallint(FORMAT_FIELDS);
        uint64_t *h_md = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 2);
        OBJ_FIELD(h_md, 0) = sel_hash;
        OBJ_FIELD(h_md, 1) = (uint64_t)h_cm;
        OBJ_FIELD(h_class, CLASS_METHOD_DICT) = (uint64_t)h_md;

        uint64_t *hobj = om_alloc(om, (uint64_t)h_class, FORMAT_FIELDS, 0);

        // Caller: PUSH_SELF, SEND hash 0, HALT
        uint64_t *h_cbc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 16);
        uint8_t *hbc = (uint8_t *)&OBJ_FIELD(h_cbc, 0);
        hbc[0] = BC_PUSH_SELF;
        hbc[1] = BC_SEND_MESSAGE;
        WRITE_U32(&hbc[2], 0);
        WRITE_U32(&hbc[6], 0);
        hbc[10] = BC_HALT;

        uint64_t *h_lits = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 1);
        OBJ_FIELD(h_lits, 0) = sel_hash;

        uint64_t *h_ccm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(h_ccm, CM_PRIMITIVE) = tag_smallint(0);
        OBJ_FIELD(h_ccm, CM_NUM_ARGS) = tag_smallint(0);
        OBJ_FIELD(h_ccm, CM_NUM_TEMPS) = tag_smallint(0);
        OBJ_FIELD(h_ccm, CM_LITERALS) = (uint64_t)h_lits;
        OBJ_FIELD(h_ccm, CM_BYTECODES) = (uint64_t)h_cbc;

        sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
        fp = (uint64_t *)0xCAFE;
        stack_push(&sp, stack, (uint64_t)hobj);
        activate_method(&sp, &fp, 0, (uint64_t)h_ccm, 0, 0);
        result = interpret(&sp, &fp,
                           (uint8_t *)&OBJ_FIELD(h_cbc, 0),
                           class_table, om, NULL);
        // Result should be a tagged SmallInt (tag = 01)
        ASSERT_EQ(ctx, result & 1, 1, "hash: result is SmallInt");
        // Same object → same hash
        sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
        fp = (uint64_t *)0xCAFE;
        stack_push(&sp, stack, (uint64_t)hobj);
        activate_method(&sp, &fp, 0, (uint64_t)h_ccm, 0, 0);
        uint64_t result2 = interpret(&sp, &fp,
                                     (uint8_t *)&OBJ_FIELD(h_cbc, 0),
                                     class_table, om, NULL);
        ASSERT_EQ(ctx, result, result2, "hash: same object → same hash");
    }

    // --- at: format dispatch ---
    {
        uint64_t sel_at = tag_smallint(90);

        // Reuse existing at: prim method
        uint64_t *at_pbc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 2);
        uint64_t *at_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(at_cm, CM_PRIMITIVE) = tag_smallint(PRIM_AT);
        OBJ_FIELD(at_cm, CM_NUM_ARGS) = tag_smallint(1);
        OBJ_FIELD(at_cm, CM_NUM_TEMPS) = tag_smallint(0);
        OBJ_FIELD(at_cm, CM_LITERALS) = tagged_nil();
        OBJ_FIELD(at_cm, CM_BYTECODES) = (uint64_t)at_pbc;

        // --- Test 1: indexable, 1-based ---
        uint64_t *idx_class = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 4);
        OBJ_FIELD(idx_class, CLASS_SUPERCLASS) = tagged_nil();
        OBJ_FIELD(idx_class, CLASS_INST_SIZE) = tag_smallint(0);
        OBJ_FIELD(idx_class, CLASS_INST_FORMAT) = tag_smallint(FORMAT_INDEXABLE);
        uint64_t *idx_md = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 2);
        OBJ_FIELD(idx_md, 0) = sel_at;
        OBJ_FIELD(idx_md, 1) = (uint64_t)at_cm;
        OBJ_FIELD(idx_class, CLASS_METHOD_DICT) = (uint64_t)idx_md;

        uint64_t *arr = om_alloc(om, (uint64_t)idx_class, FORMAT_INDEXABLE, 3);
        OBJ_FIELD(arr, 0) = tag_smallint(10);
        OBJ_FIELD(arr, 1) = tag_smallint(20);
        OBJ_FIELD(arr, 2) = tag_smallint(30);

        // Caller: PUSH_SELF, PUSH_LITERAL 1, SEND at: 1, HALT
        uint64_t *at_cbc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 20);
        uint8_t *atbc = (uint8_t *)&OBJ_FIELD(at_cbc, 0);
        atbc[0] = BC_PUSH_SELF;
        atbc[1] = BC_PUSH_LITERAL;
        WRITE_U32(&atbc[2], 1); // lit 1 = index
        atbc[6] = BC_SEND_MESSAGE;
        WRITE_U32(&atbc[7], 0);  // lit 0 = sel
        WRITE_U32(&atbc[11], 1); // 1 arg
        atbc[15] = BC_HALT;

        // at: 1 → first element (10)
        uint64_t *at_lits1 = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 2);
        OBJ_FIELD(at_lits1, 0) = sel_at;
        OBJ_FIELD(at_lits1, 1) = tag_smallint(1); // 1-based

        uint64_t *at_ccm1 = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(at_ccm1, CM_PRIMITIVE) = tag_smallint(0);
        OBJ_FIELD(at_ccm1, CM_NUM_ARGS) = tag_smallint(0);
        OBJ_FIELD(at_ccm1, CM_NUM_TEMPS) = tag_smallint(0);
        OBJ_FIELD(at_ccm1, CM_LITERALS) = (uint64_t)at_lits1;
        OBJ_FIELD(at_ccm1, CM_BYTECODES) = (uint64_t)at_cbc;

        sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
        fp = (uint64_t *)0xCAFE;
        stack_push(&sp, stack, (uint64_t)arr);
        activate_method(&sp, &fp, 0, (uint64_t)at_ccm1, 0, 0);
        result = interpret(&sp, &fp,
                           (uint8_t *)&OBJ_FIELD(at_cbc, 0),
                           class_table, om, NULL);
        ASSERT_EQ(ctx, result, tag_smallint(10),
                  "at: indexable 1-based: at:1 → 10");

        // at: 3 → third element (30)
        uint64_t *at_lits3 = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 2);
        OBJ_FIELD(at_lits3, 0) = sel_at;
        OBJ_FIELD(at_lits3, 1) = tag_smallint(3);

        uint64_t *at_ccm3 = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(at_ccm3, CM_PRIMITIVE) = tag_smallint(0);
        OBJ_FIELD(at_ccm3, CM_NUM_ARGS) = tag_smallint(0);
        OBJ_FIELD(at_ccm3, CM_NUM_TEMPS) = tag_smallint(0);
        OBJ_FIELD(at_ccm3, CM_LITERALS) = (uint64_t)at_lits3;
        OBJ_FIELD(at_ccm3, CM_BYTECODES) = (uint64_t)at_cbc;

        sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
        fp = (uint64_t *)0xCAFE;
        stack_push(&sp, stack, (uint64_t)arr);
        activate_method(&sp, &fp, 0, (uint64_t)at_ccm3, 0, 0);
        result = interpret(&sp, &fp,
                           (uint8_t *)&OBJ_FIELD(at_cbc, 0),
                           class_table, om, NULL);
        ASSERT_EQ(ctx, result, tag_smallint(30),
                  "at: indexable 1-based: at:3 → 30");

        // --- Test 2: byte-indexable, 1-based, returns SmallInt byte ---
        uint64_t *byte_class = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 4);
        OBJ_FIELD(byte_class, CLASS_SUPERCLASS) = tagged_nil();
        OBJ_FIELD(byte_class, CLASS_INST_SIZE) = tag_smallint(0);
        OBJ_FIELD(byte_class, CLASS_INST_FORMAT) = tag_smallint(FORMAT_BYTES);
        uint64_t *byte_md = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 2);
        OBJ_FIELD(byte_md, 0) = sel_at;
        OBJ_FIELD(byte_md, 1) = (uint64_t)at_cm;
        OBJ_FIELD(byte_class, CLASS_METHOD_DICT) = (uint64_t)byte_md;

        uint64_t *ba = om_alloc(om, (uint64_t)byte_class, FORMAT_BYTES, 4);
        uint8_t *ba_data = (uint8_t *)&OBJ_FIELD(ba, 0);
        ba_data[0] = 65; // 'A'
        ba_data[1] = 66; // 'B'
        ba_data[2] = 67; // 'C'
        ba_data[3] = 68; // 'D'

        // at: 2 → 66 (B)
        uint64_t *at_blits = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 2);
        OBJ_FIELD(at_blits, 0) = sel_at;
        OBJ_FIELD(at_blits, 1) = tag_smallint(2);

        uint64_t *at_bccm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(at_bccm, CM_PRIMITIVE) = tag_smallint(0);
        OBJ_FIELD(at_bccm, CM_NUM_ARGS) = tag_smallint(0);
        OBJ_FIELD(at_bccm, CM_NUM_TEMPS) = tag_smallint(0);
        OBJ_FIELD(at_bccm, CM_LITERALS) = (uint64_t)at_blits;
        OBJ_FIELD(at_bccm, CM_BYTECODES) = (uint64_t)at_cbc;

        sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
        fp = (uint64_t *)0xCAFE;
        stack_push(&sp, stack, (uint64_t)ba);
        activate_method(&sp, &fp, 0, (uint64_t)at_bccm, 0, 0);
        result = interpret(&sp, &fp,
                           (uint8_t *)&OBJ_FIELD(at_cbc, 0),
                           class_table, om, NULL);
        ASSERT_EQ(ctx, result, tag_smallint(66),
                  "at: bytes 1-based: at:2 → 66 (B)");

        uint64_t *at_fail_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 6);
        uint8_t *afbb = (uint8_t *)&OBJ_FIELD(at_fail_bc, 0);
        afbb[0] = BC_PUSH_LITERAL;
        WRITE_U32(&afbb[1], 0);
        afbb[5] = BC_RETURN;

        uint64_t *at_fail_lits = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 1);
        OBJ_FIELD(at_fail_lits, 0) = tag_smallint(222);

        uint64_t *at_fail_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(at_fail_cm, CM_PRIMITIVE) = tag_smallint(PRIM_AT);
        OBJ_FIELD(at_fail_cm, CM_NUM_ARGS) = tag_smallint(1);
        OBJ_FIELD(at_fail_cm, CM_NUM_TEMPS) = tag_smallint(0);
        OBJ_FIELD(at_fail_cm, CM_LITERALS) = (uint64_t)at_fail_lits;
        OBJ_FIELD(at_fail_cm, CM_BYTECODES) = (uint64_t)at_fail_bc;

        uint64_t *fail_idx_class = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 4);
        OBJ_FIELD(fail_idx_class, CLASS_SUPERCLASS) = tagged_nil();
        OBJ_FIELD(fail_idx_class, CLASS_INST_SIZE) = tag_smallint(0);
        OBJ_FIELD(fail_idx_class, CLASS_INST_FORMAT) = tag_smallint(FORMAT_INDEXABLE);
        uint64_t *fail_idx_md = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 2);
        OBJ_FIELD(fail_idx_md, 0) = sel_at;
        OBJ_FIELD(fail_idx_md, 1) = (uint64_t)at_fail_cm;
        OBJ_FIELD(fail_idx_class, CLASS_METHOD_DICT) = (uint64_t)fail_idx_md;

        uint64_t *fail_arr = om_alloc(om, (uint64_t)fail_idx_class, FORMAT_INDEXABLE, 2);
        OBJ_FIELD(fail_arr, 0) = tag_smallint(10);
        OBJ_FIELD(fail_arr, 1) = tag_smallint(20);

        uint64_t *at_fail_cbc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 20);
        uint8_t *afbc = (uint8_t *)&OBJ_FIELD(at_fail_cbc, 0);
        afbc[0] = BC_PUSH_SELF;
        afbc[1] = BC_PUSH_LITERAL;
        WRITE_U32(&afbc[2], 1);
        afbc[6] = BC_SEND_MESSAGE;
        WRITE_U32(&afbc[7], 0);
        WRITE_U32(&afbc[11], 1);
        afbc[15] = BC_HALT;

        uint64_t *bad_index_lits = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 2);
        OBJ_FIELD(bad_index_lits, 0) = sel_at;
        OBJ_FIELD(bad_index_lits, 1) = tagged_nil();

        uint64_t *bad_index_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(bad_index_cm, CM_PRIMITIVE) = tag_smallint(0);
        OBJ_FIELD(bad_index_cm, CM_NUM_ARGS) = tag_smallint(0);
        OBJ_FIELD(bad_index_cm, CM_NUM_TEMPS) = tag_smallint(0);
        OBJ_FIELD(bad_index_cm, CM_LITERALS) = (uint64_t)bad_index_lits;
        OBJ_FIELD(bad_index_cm, CM_BYTECODES) = (uint64_t)at_fail_cbc;

        sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
        fp = (uint64_t *)0xCAFE;
        stack_push(&sp, stack, (uint64_t)fail_arr);
        activate_method(&sp, &fp, 0, (uint64_t)bad_index_cm, 0, 0);
        result = interpret(&sp, &fp,
                           (uint8_t *)&OBJ_FIELD(at_fail_cbc, 0),
                           class_table, om, NULL);
        ASSERT_EQ(ctx, result, tag_smallint(222),
                  "primitive failure: at: wrong index type falls through to method body");

        uint64_t *oob_lits = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 2);
        OBJ_FIELD(oob_lits, 0) = sel_at;
        OBJ_FIELD(oob_lits, 1) = tag_smallint(9);

        uint64_t *oob_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(oob_cm, CM_PRIMITIVE) = tag_smallint(0);
        OBJ_FIELD(oob_cm, CM_NUM_ARGS) = tag_smallint(0);
        OBJ_FIELD(oob_cm, CM_NUM_TEMPS) = tag_smallint(0);
        OBJ_FIELD(oob_cm, CM_LITERALS) = (uint64_t)oob_lits;
        OBJ_FIELD(oob_cm, CM_BYTECODES) = (uint64_t)at_fail_cbc;

        sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
        fp = (uint64_t *)0xCAFE;
        stack_push(&sp, stack, (uint64_t)fail_arr);
        activate_method(&sp, &fp, 0, (uint64_t)oob_cm, 0, 0);
        result = interpret(&sp, &fp,
                           (uint8_t *)&OBJ_FIELD(at_fail_cbc, 0),
                           class_table, om, NULL);
        ASSERT_EQ(ctx, result, tag_smallint(222),
                  "primitive failure: at: bounds error falls through to method body");
    }

    // --- printChar primitive on Character ---
    {
        uint64_t sel_printChar = tag_smallint(77);

        uint64_t *pc_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 2);
        uint64_t *pc_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(pc_cm, CM_PRIMITIVE) = tag_smallint(PRIM_PRINT_CHAR);
        OBJ_FIELD(pc_cm, CM_NUM_ARGS) = tag_smallint(0);
        OBJ_FIELD(pc_cm, CM_NUM_TEMPS) = tag_smallint(0);
        OBJ_FIELD(pc_cm, CM_LITERALS) = tagged_nil();
        OBJ_FIELD(pc_cm, CM_BYTECODES) = (uint64_t)pc_bc;

        // Add printChar to Character class
        uint64_t *char_cls = ctx->character_class;
        uint64_t old_md_val = OBJ_FIELD(char_cls, CLASS_METHOD_DICT);
        uint64_t *old_md = (old_md_val != tagged_nil() && (old_md_val & 3) == 0)
                               ? (uint64_t *)old_md_val
                               : NULL;
        uint64_t old_size = old_md ? OBJ_SIZE(old_md) : 0;
        uint64_t *new_md = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, old_size + 2);
        for (uint64_t i = 0; i < old_size; i++)
            OBJ_FIELD(new_md, i) = OBJ_FIELD(old_md, i);
        OBJ_FIELD(new_md, old_size) = sel_printChar;
        OBJ_FIELD(new_md, old_size + 1) = (uint64_t)pc_cm;
        OBJ_FIELD(char_cls, CLASS_METHOD_DICT) = (uint64_t)new_md;

        // Caller: PUSH_LITERAL 0 ($. = Character 46), SEND printChar 0, HALT
        uint64_t *pc_cbc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 16);
        uint8_t *pcbc = (uint8_t *)&OBJ_FIELD(pc_cbc, 0);
        pcbc[0] = BC_PUSH_LITERAL;
        WRITE_U32(&pcbc[1], 0);
        pcbc[5] = BC_SEND_MESSAGE;
        WRITE_U32(&pcbc[6], 1);
        WRITE_U32(&pcbc[10], 0);
        pcbc[14] = BC_HALT;

        uint64_t *pc_lits = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 2);
        OBJ_FIELD(pc_lits, 0) = tag_character(46); // $. (period)
        OBJ_FIELD(pc_lits, 1) = sel_printChar;

        uint64_t *pc_ccm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(pc_ccm, CM_PRIMITIVE) = tag_smallint(0);
        OBJ_FIELD(pc_ccm, CM_NUM_ARGS) = tag_smallint(0);
        OBJ_FIELD(pc_ccm, CM_NUM_TEMPS) = tag_smallint(0);
        OBJ_FIELD(pc_ccm, CM_LITERALS) = (uint64_t)pc_lits;
        OBJ_FIELD(pc_ccm, CM_BYTECODES) = (uint64_t)pc_cbc;

        sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
        fp = (uint64_t *)0xCAFE;
        stack_push(&sp, stack, tag_smallint(0)); // dummy receiver

        activate_method(&sp, &fp, 0, (uint64_t)pc_ccm, 0, 0);
        result = interpret(&sp, &fp,
                           (uint8_t *)&OBJ_FIELD(pc_cbc, 0),
                           class_table, om, NULL);
        // printChar returns self (the Character)
        ASSERT_EQ(ctx, result, tag_character(46),
                  "printChar: Character $. returns self");
        ASSERT_EQ(ctx, (uint64_t)run_trap_test(ctx, trap_print_char_wrong_receiver),
                  (uint64_t)SIGTRAP,
                  "primitive trap: Character>>printChar wrong receiver traps");
    }

    // --- value: primitive (1-arg block) ---
    {
        uint64_t sel_valueArg = tag_smallint(78);

        // Block body CM: push literal 0, return
        uint64_t *bv_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 10);
        uint8_t *bvbc = (uint8_t *)&OBJ_FIELD(bv_bc, 0);
        bvbc[0] = BC_PUSH_LITERAL;
        WRITE_U32(&bvbc[1], 0); // literal 0 = 99
        bvbc[5] = BC_RETURN;

        uint64_t *bv_lits = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 1);
        OBJ_FIELD(bv_lits, 0) = tag_smallint(99);
        uint64_t *bv_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(bv_cm, CM_PRIMITIVE) = tag_smallint(0);
        OBJ_FIELD(bv_cm, CM_NUM_ARGS) = tag_smallint(1);
        OBJ_FIELD(bv_cm, CM_NUM_TEMPS) = tag_smallint(0);
        OBJ_FIELD(bv_cm, CM_LITERALS) = (uint64_t)bv_lits;
        OBJ_FIELD(bv_cm, CM_BYTECODES) = (uint64_t)bv_bc;

        // Create block object: field 0 = home context, field 1 = home receiver, field 2 = CM
        uint64_t *block_obj = om_alloc(om, (uint64_t)ctx->block_class, FORMAT_FIELDS, 3);
        OBJ_FIELD(block_obj, BLOCK_HOME_CONTEXT) = tagged_nil();
        OBJ_FIELD(block_obj, BLOCK_HOME_RECEIVER) = tag_smallint(0); // home receiver (dummy)
        OBJ_FIELD(block_obj, BLOCK_CM) = (uint64_t)bv_cm;

        // Add value: prim to block class
        uint64_t *bvp_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 2);
        uint64_t *bvp_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(bvp_cm, CM_PRIMITIVE) = tag_smallint(PRIM_BLOCK_VALUE_ARG);
        OBJ_FIELD(bvp_cm, CM_NUM_ARGS) = tag_smallint(1);
        OBJ_FIELD(bvp_cm, CM_NUM_TEMPS) = tag_smallint(0);
        OBJ_FIELD(bvp_cm, CM_LITERALS) = tagged_nil();
        OBJ_FIELD(bvp_cm, CM_BYTECODES) = (uint64_t)bvp_bc;

        uint64_t *block_class = ctx->block_class;
        uint64_t bmd_val = OBJ_FIELD(block_class, CLASS_METHOD_DICT);
        uint64_t *bmd_old = (bmd_val != tagged_nil() && (bmd_val & 3) == 0)
                                ? (uint64_t *)bmd_val
                                : NULL;
        uint64_t bmd_sz = bmd_old ? OBJ_SIZE(bmd_old) : 0;
        uint64_t *bmd_new = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, bmd_sz + 2);
        for (uint64_t i = 0; i < bmd_sz; i++)
            OBJ_FIELD(bmd_new, i) = OBJ_FIELD(bmd_old, i);
        OBJ_FIELD(bmd_new, bmd_sz) = sel_valueArg;
        OBJ_FIELD(bmd_new, bmd_sz + 1) = (uint64_t)bvp_cm;
        OBJ_FIELD(block_class, CLASS_METHOD_DICT) = (uint64_t)bmd_new;

        // Caller: PUSH_SELF (block), PUSH_LITERAL 1 (arg=99), SEND value: 1, HALT
        uint64_t *bv_cbc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 20);
        uint8_t *bvcbc = (uint8_t *)&OBJ_FIELD(bv_cbc, 0);
        bvcbc[0] = BC_PUSH_SELF; // block
        bvcbc[1] = BC_PUSH_LITERAL;
        WRITE_U32(&bvcbc[2], 1); // lit 1 = arg
        bvcbc[6] = BC_SEND_MESSAGE;
        WRITE_U32(&bvcbc[7], 0);  // lit 0 = sel
        WRITE_U32(&bvcbc[11], 1); // 1 arg
        bvcbc[15] = BC_HALT;

        uint64_t *bv_caller_lits = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 2);
        OBJ_FIELD(bv_caller_lits, 0) = sel_valueArg;
        OBJ_FIELD(bv_caller_lits, 1) = tag_smallint(99);

        uint64_t *bv_caller_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(bv_caller_cm, CM_PRIMITIVE) = tag_smallint(0);
        OBJ_FIELD(bv_caller_cm, CM_NUM_ARGS) = tag_smallint(0);
        OBJ_FIELD(bv_caller_cm, CM_NUM_TEMPS) = tag_smallint(0);
        OBJ_FIELD(bv_caller_cm, CM_LITERALS) = (uint64_t)bv_caller_lits;
        OBJ_FIELD(bv_caller_cm, CM_BYTECODES) = (uint64_t)bv_cbc;

        sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
        fp = (uint64_t *)0xCAFE;
        stack_push(&sp, stack, (uint64_t)block_obj);
        activate_method(&sp, &fp, 0, (uint64_t)bv_caller_cm, 0, 0);
        result = interpret(&sp, &fp,
                           (uint8_t *)&OBJ_FIELD(bv_cbc, 0),
                           class_table, om, NULL);
        ASSERT_EQ(ctx, result, tag_smallint(99),
                  "value: 1-arg block returns the argument");
        ASSERT_EQ(ctx, (uint64_t)run_trap_test(ctx, trap_block_value_wrong_receiver),
                  (uint64_t)SIGTRAP,
                  "primitive trap: Block>>value wrong receiver traps");
    }

    // --- perform: primitive ---
    {
        uint64_t sel_perform = tag_smallint(79);
        uint64_t sel_answer = tag_smallint(80);

        // A method that returns 42
        uint64_t *ans_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 10);
        uint8_t *ansbc = (uint8_t *)&OBJ_FIELD(ans_bc, 0);
        ansbc[0] = BC_PUSH_LITERAL;
        WRITE_U32(&ansbc[1], 0);
        ansbc[5] = BC_RETURN;

        uint64_t *ans_lits = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 1);
        OBJ_FIELD(ans_lits, 0) = tag_smallint(42);

        uint64_t *ans_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(ans_cm, CM_PRIMITIVE) = tag_smallint(0);
        OBJ_FIELD(ans_cm, CM_NUM_ARGS) = tag_smallint(0);
        OBJ_FIELD(ans_cm, CM_NUM_TEMPS) = tag_smallint(0);
        OBJ_FIELD(ans_cm, CM_LITERALS) = (uint64_t)ans_lits;
        OBJ_FIELD(ans_cm, CM_BYTECODES) = (uint64_t)ans_bc;

        // perform: prim method
        uint64_t *perf_pbc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 2);
        uint64_t *perf_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(perf_cm, CM_PRIMITIVE) = tag_smallint(PRIM_PERFORM);
        OBJ_FIELD(perf_cm, CM_NUM_ARGS) = tag_smallint(1);
        OBJ_FIELD(perf_cm, CM_NUM_TEMPS) = tag_smallint(0);
        OBJ_FIELD(perf_cm, CM_LITERALS) = tagged_nil();
        OBJ_FIELD(perf_cm, CM_BYTECODES) = (uint64_t)perf_pbc;

        // A class with both #answer and #perform:
        uint64_t *perf_class = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 4);
        OBJ_FIELD(perf_class, CLASS_SUPERCLASS) = tagged_nil();
        OBJ_FIELD(perf_class, CLASS_INST_SIZE) = tag_smallint(0);
        OBJ_FIELD(perf_class, CLASS_INST_FORMAT) = tag_smallint(FORMAT_FIELDS);
        uint64_t *perf_md = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 4);
        OBJ_FIELD(perf_md, 0) = sel_answer;
        OBJ_FIELD(perf_md, 1) = (uint64_t)ans_cm;
        OBJ_FIELD(perf_md, 2) = sel_perform;
        OBJ_FIELD(perf_md, 3) = (uint64_t)perf_cm;
        OBJ_FIELD(perf_class, CLASS_METHOD_DICT) = (uint64_t)perf_md;

        uint64_t *perf_obj = om_alloc(om, (uint64_t)perf_class, FORMAT_FIELDS, 0);

        // Caller: PUSH_SELF, PUSH_LITERAL 1 (#answer), SEND #perform: 1, HALT
        uint64_t *perf_cbc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 20);
        uint8_t *pcbc2 = (uint8_t *)&OBJ_FIELD(perf_cbc, 0);
        pcbc2[0] = BC_PUSH_SELF;
        pcbc2[1] = BC_PUSH_LITERAL;
        WRITE_U32(&pcbc2[2], 1); // lit 1 = #answer
        pcbc2[6] = BC_SEND_MESSAGE;
        WRITE_U32(&pcbc2[7], 0);  // lit 0 = #perform:
        WRITE_U32(&pcbc2[11], 1); // 1 arg
        pcbc2[15] = BC_HALT;

        uint64_t *perf_lits = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 2);
        OBJ_FIELD(perf_lits, 0) = sel_perform;
        OBJ_FIELD(perf_lits, 1) = sel_answer;

        uint64_t *perf_ccm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(perf_ccm, CM_PRIMITIVE) = tag_smallint(0);
        OBJ_FIELD(perf_ccm, CM_NUM_ARGS) = tag_smallint(0);
        OBJ_FIELD(perf_ccm, CM_NUM_TEMPS) = tag_smallint(0);
        OBJ_FIELD(perf_ccm, CM_LITERALS) = (uint64_t)perf_lits;
        OBJ_FIELD(perf_ccm, CM_BYTECODES) = (uint64_t)perf_cbc;

        sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
        fp = (uint64_t *)0xCAFE;
        stack_push(&sp, stack, (uint64_t)perf_obj);
        activate_method(&sp, &fp, 0, (uint64_t)perf_ccm, 0, 0);
        result = interpret(&sp, &fp,
                           (uint8_t *)&OBJ_FIELD(perf_cbc, 0),
                           class_table, om, NULL);
        ASSERT_EQ(ctx, result, tag_smallint(42),
                  "perform: dynamically sends #answer, returns 42");
    }

    // --- perform: on nil uses UndefinedObject class ---
    {
        uint64_t sel_perform = tag_smallint(79);
        uint64_t sel_basicClass = tag_smallint(74);

        uint64_t *uo_class = ctx->undefined_object_class;
        uint64_t *bc_cm_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 2);
        uint64_t *bc_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(bc_cm, CM_PRIMITIVE) = tag_smallint(PRIM_BASIC_CLASS);
        OBJ_FIELD(bc_cm, CM_NUM_ARGS) = tag_smallint(0);
        OBJ_FIELD(bc_cm, CM_NUM_TEMPS) = tag_smallint(0);
        OBJ_FIELD(bc_cm, CM_LITERALS) = tagged_nil();
        OBJ_FIELD(bc_cm, CM_BYTECODES) = (uint64_t)bc_cm_bc;

        uint64_t *perf_cm_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 2);
        uint64_t *perf_cm_nil = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(perf_cm_nil, CM_PRIMITIVE) = tag_smallint(PRIM_PERFORM);
        OBJ_FIELD(perf_cm_nil, CM_NUM_ARGS) = tag_smallint(1);
        OBJ_FIELD(perf_cm_nil, CM_NUM_TEMPS) = tag_smallint(0);
        OBJ_FIELD(perf_cm_nil, CM_LITERALS) = tagged_nil();
        OBJ_FIELD(perf_cm_nil, CM_BYTECODES) = (uint64_t)perf_cm_bc;

        uint64_t *uo_md = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 4);
        OBJ_FIELD(uo_md, 0) = sel_basicClass;
        OBJ_FIELD(uo_md, 1) = (uint64_t)bc_cm;
        OBJ_FIELD(uo_md, 2) = sel_perform;
        OBJ_FIELD(uo_md, 3) = (uint64_t)perf_cm_nil;
        OBJ_FIELD(uo_class, CLASS_METHOD_DICT) = (uint64_t)uo_md;

        uint64_t *caller_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 20);
        uint8_t *cbb = (uint8_t *)&OBJ_FIELD(caller_bc, 0);
        cbb[0] = BC_PUSH_LITERAL;
        WRITE_U32(&cbb[1], 0); // nil
        cbb[5] = BC_PUSH_LITERAL;
        WRITE_U32(&cbb[6], 1); // #basicClass
        cbb[10] = BC_SEND_MESSAGE;
        WRITE_U32(&cbb[11], 2); // #perform:
        WRITE_U32(&cbb[15], 1);
        cbb[19] = BC_HALT;

        uint64_t *caller_lits = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 3);
        OBJ_FIELD(caller_lits, 0) = tagged_nil();
        OBJ_FIELD(caller_lits, 1) = sel_basicClass;
        OBJ_FIELD(caller_lits, 2) = sel_perform;

        uint64_t *caller_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(caller_cm, CM_PRIMITIVE) = tag_smallint(0);
        OBJ_FIELD(caller_cm, CM_NUM_ARGS) = tag_smallint(0);
        OBJ_FIELD(caller_cm, CM_NUM_TEMPS) = tag_smallint(0);
        OBJ_FIELD(caller_cm, CM_LITERALS) = (uint64_t)caller_lits;
        OBJ_FIELD(caller_cm, CM_BYTECODES) = (uint64_t)caller_bc;

        sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
        fp = (uint64_t *)0xCAFE;
        stack_push(&sp, stack, tag_smallint(0));
        activate_method(&sp, &fp, 0, (uint64_t)caller_cm, 0, 0);
        result = interpret(&sp, &fp,
                           (uint8_t *)&OBJ_FIELD(caller_bc, 0),
                           class_table, om, NULL);
        ASSERT_EQ(ctx, result, (uint64_t)uo_class,
                  "perform: nil dispatches through UndefinedObject");
    }

    // --- perform: primitive target ---
    {
        uint64_t sel_perform = tag_smallint(79);
        uint64_t sel_basicClass = tag_smallint(74);

        uint64_t *klass_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 2);
        uint64_t *klass_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(klass_cm, CM_PRIMITIVE) = tag_smallint(PRIM_BASIC_CLASS);
        OBJ_FIELD(klass_cm, CM_NUM_ARGS) = tag_smallint(0);
        OBJ_FIELD(klass_cm, CM_NUM_TEMPS) = tag_smallint(0);
        OBJ_FIELD(klass_cm, CM_LITERALS) = tagged_nil();
        OBJ_FIELD(klass_cm, CM_BYTECODES) = (uint64_t)klass_bc;

        uint64_t *perform_bc2 = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 2);
        uint64_t *perform_cm2 = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(perform_cm2, CM_PRIMITIVE) = tag_smallint(PRIM_PERFORM);
        OBJ_FIELD(perform_cm2, CM_NUM_ARGS) = tag_smallint(1);
        OBJ_FIELD(perform_cm2, CM_NUM_TEMPS) = tag_smallint(0);
        OBJ_FIELD(perform_cm2, CM_LITERALS) = tagged_nil();
        OBJ_FIELD(perform_cm2, CM_BYTECODES) = (uint64_t)perform_bc2;

        uint64_t *perf_class2 = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 4);
        OBJ_FIELD(perf_class2, CLASS_SUPERCLASS) = tagged_nil();
        OBJ_FIELD(perf_class2, CLASS_INST_SIZE) = tag_smallint(0);
        OBJ_FIELD(perf_class2, CLASS_INST_FORMAT) = tag_smallint(FORMAT_FIELDS);
        uint64_t *perf_md2 = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 4);
        OBJ_FIELD(perf_md2, 0) = sel_basicClass;
        OBJ_FIELD(perf_md2, 1) = (uint64_t)klass_cm;
        OBJ_FIELD(perf_md2, 2) = sel_perform;
        OBJ_FIELD(perf_md2, 3) = (uint64_t)perform_cm2;
        OBJ_FIELD(perf_class2, CLASS_METHOD_DICT) = (uint64_t)perf_md2;

        uint64_t *perf_obj2 = om_alloc(om, (uint64_t)perf_class2, FORMAT_FIELDS, 0);

        uint64_t *perf_bc2 = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 20);
        uint8_t *pcbc3 = (uint8_t *)&OBJ_FIELD(perf_bc2, 0);
        pcbc3[0] = BC_PUSH_SELF;
        pcbc3[1] = BC_PUSH_LITERAL;
        WRITE_U32(&pcbc3[2], 1); // lit 1 = #basicClass
        pcbc3[6] = BC_SEND_MESSAGE;
        WRITE_U32(&pcbc3[7], 0);  // lit 0 = #perform:
        WRITE_U32(&pcbc3[11], 1); // 1 arg
        pcbc3[15] = BC_HALT;

        uint64_t *perf_lits2 = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 2);
        OBJ_FIELD(perf_lits2, 0) = sel_perform;
        OBJ_FIELD(perf_lits2, 1) = sel_basicClass;

        uint64_t *perf_ccm2 = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(perf_ccm2, CM_PRIMITIVE) = tag_smallint(0);
        OBJ_FIELD(perf_ccm2, CM_NUM_ARGS) = tag_smallint(0);
        OBJ_FIELD(perf_ccm2, CM_NUM_TEMPS) = tag_smallint(0);
        OBJ_FIELD(perf_ccm2, CM_LITERALS) = (uint64_t)perf_lits2;
        OBJ_FIELD(perf_ccm2, CM_BYTECODES) = (uint64_t)perf_bc2;

        sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
        fp = (uint64_t *)0xCAFE;
        stack_push(&sp, stack, (uint64_t)perf_obj2);
        activate_method(&sp, &fp, 0, (uint64_t)perf_ccm2, 0, 0);
        result = interpret(&sp, &fp,
                           (uint8_t *)&OBJ_FIELD(perf_bc2, 0),
                           class_table, om, NULL);
        ASSERT_EQ(ctx, result, (uint64_t)perf_class2,
                  "perform: primitive target sees the original receiver stack shape");
    }
}
