#include "test_defs.h"

void test_transaction(TestContext *ctx)
{
    uint64_t *om = ctx->om;
    uint64_t *class_class = ctx->class_class;
    (void)om;
    (void)class_class;

    // --- Transaction log: write, read, commit ---

    // Create a simple object with 2 fields
    uint64_t *obj = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 2);
    OBJ_FIELD(obj, 0) = tag_smallint(10);
    OBJ_FIELD(obj, 1) = tag_smallint(20);

    // Transaction log: slot 0 = count, then (obj, field, value) triples
    uint64_t log[1 + 64 * 3];
    log[0] = 0;

    // Write to the log: obj field 0 = 99
    txn_log_write(log, (uint64_t)obj, 0, tag_smallint(99));
    ASSERT_EQ(ctx, log[0], 1, "txn_log_write: count is 1");

    // Read from the log: should find the entry
    uint64_t found = 0;
    uint64_t value = txn_log_read(log, (uint64_t)obj, 0, &found);
    ASSERT_EQ(ctx, found, 1, "txn_log_read: found entry");
    ASSERT_EQ(ctx, value, tag_smallint(99), "txn_log_read: value is 99");

    // Object is unchanged
    ASSERT_EQ(ctx, OBJ_FIELD(obj, 0), tag_smallint(10), "object field 0 still 10");

    // Read a field NOT in the log: miss
    found = 0;
    value = txn_log_read(log, (uint64_t)obj, 1, &found);
    ASSERT_EQ(ctx, found, 0, "txn_log_read: field 1 not found");

    // Commit: apply log to objects
    txn_commit(log);
    ASSERT_EQ(ctx, OBJ_FIELD(obj, 0), tag_smallint(99), "after commit: field 0 is 99");
    ASSERT_EQ(ctx, OBJ_FIELD(obj, 1), tag_smallint(20), "after commit: field 1 unchanged");
    ASSERT_EQ(ctx, log[0], (uint64_t)0, "after commit: log is empty");

    // --- STORE_INST_VAR through transaction log ---
    {
        uint64_t *om = ctx->om;
        uint64_t *class_class = ctx->class_class;
        uint64_t *class_table = ctx->class_table;
        uint64_t *stack = ctx->stack;
        uint64_t *sp, *fp;

        // Object with 2 fields
        uint64_t *tobj = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 2);
        OBJ_FIELD(tobj, 0) = tag_smallint(10);
        OBJ_FIELD(tobj, 1) = tag_smallint(20);

        // Bytecodes: PUSH_LITERAL 0, STORE_INST_VAR 0, HALT
        uint64_t *tbc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 20);
        uint8_t *bc = (uint8_t *)&OBJ_FIELD(tbc, 0);
        bc[0] = BC_PUSH_LITERAL;
        WRITE_U32(bc + 1, 0);
        bc[5] = BC_STORE_INST_VAR;
        WRITE_U32(bc + 6, 0);
        bc[10] = BC_HALT;

        // Literals: [0] = 99
        uint64_t *tlits = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 1);
        OBJ_FIELD(tlits, 0) = tag_smallint(99);

        // Method
        uint64_t *tcm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(tcm, CM_PRIMITIVE) = tag_smallint(0);
        OBJ_FIELD(tcm, CM_NUM_ARGS) = tag_smallint(0);
        OBJ_FIELD(tcm, CM_NUM_TEMPS) = tag_smallint(0);
        OBJ_FIELD(tcm, CM_LITERALS) = (uint64_t)tlits;
        OBJ_FIELD(tcm, CM_BYTECODES) = (uint64_t)tbc;

        // Transaction log
        uint64_t txn[1 + 64 * 3];
        txn[0] = 0;

        sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
        fp = (uint64_t *)0xCAFE;
        stack_push(&sp, stack, (uint64_t)tobj); // receiver

        activate_method(&sp, &fp, 0, (uint64_t)tcm, 0, 0);
        interpret(&sp, &fp, (uint8_t *)&OBJ_FIELD(tbc, 0), class_table, om, txn);

        // Object unchanged — write went to log
        ASSERT_EQ(ctx, OBJ_FIELD(tobj, 0), tag_smallint(10),
                  "txn STORE_INST_VAR: object unchanged");
        ASSERT_EQ(ctx, txn[0], 1,
                  "txn STORE_INST_VAR: log has 1 entry");

        // Commit and verify
        txn_commit(txn);
        ASSERT_EQ(ctx, OBJ_FIELD(tobj, 0), tag_smallint(99),
                  "txn STORE_INST_VAR: after commit field 0 is 99");
    }

    // --- PUSH_INST_VAR reads from transaction log ---
    {
        uint64_t *om = ctx->om;
        uint64_t *class_class = ctx->class_class;
        uint64_t *class_table = ctx->class_table;
        uint64_t *stack = ctx->stack;
        uint64_t *sp, *fp;

        // Object with field 0 = 10
        uint64_t *tobj = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 1);
        OBJ_FIELD(tobj, 0) = tag_smallint(10);

        // Bytecodes: PUSH_INST_VAR 0, HALT
        uint64_t *tbc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 10);
        uint8_t *bc = (uint8_t *)&OBJ_FIELD(tbc, 0);
        bc[0] = BC_PUSH_INST_VAR;
        WRITE_U32(bc + 1, 0);
        bc[5] = BC_HALT;

        // Method (no literals needed)
        uint64_t *tcm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(tcm, CM_PRIMITIVE) = tag_smallint(0);
        OBJ_FIELD(tcm, CM_NUM_ARGS) = tag_smallint(0);
        OBJ_FIELD(tcm, CM_NUM_TEMPS) = tag_smallint(0);
        OBJ_FIELD(tcm, CM_LITERALS) = tagged_nil();
        OBJ_FIELD(tcm, CM_BYTECODES) = (uint64_t)tbc;

        // Pre-populate log: obj field 0 = 77
        uint64_t txn[1 + 64 * 3];
        txn[0] = 0;
        txn_log_write(txn, (uint64_t)tobj, 0, tag_smallint(77));

        sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
        fp = (uint64_t *)0xCAFE;
        stack_push(&sp, stack, (uint64_t)tobj); // receiver

        activate_method(&sp, &fp, 0, (uint64_t)tcm, 0, 0);
        uint64_t result = interpret(&sp, &fp,
                                    (uint8_t *)&OBJ_FIELD(tbc, 0),
                                    class_table, om, txn);

        // Should read 77 from log, not 10 from object
        ASSERT_EQ(ctx, result, tag_smallint(77),
                  "txn PUSH_INST_VAR: reads 77 from log, not 10 from object");
    }

    // --- Transaction abort: discard log, object unchanged ---
    {
        uint64_t *om = ctx->om;
        uint64_t *class_class = ctx->class_class;
        uint64_t *class_table = ctx->class_table;
        uint64_t *stack = ctx->stack;
        uint64_t *sp, *fp;

        // Object with field 0 = 42
        uint64_t *tobj = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 1);
        OBJ_FIELD(tobj, 0) = tag_smallint(42);

        // Bytecodes: PUSH_LITERAL 0, STORE_INST_VAR 0, HALT
        uint64_t *tbc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 20);
        uint8_t *bc = (uint8_t *)&OBJ_FIELD(tbc, 0);
        bc[0] = BC_PUSH_LITERAL;
        WRITE_U32(bc + 1, 0);
        bc[5] = BC_STORE_INST_VAR;
        WRITE_U32(bc + 6, 0);
        bc[10] = BC_HALT;

        uint64_t *tlits = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 1);
        OBJ_FIELD(tlits, 0) = tag_smallint(999);

        uint64_t *tcm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(tcm, CM_PRIMITIVE) = tag_smallint(0);
        OBJ_FIELD(tcm, CM_NUM_ARGS) = tag_smallint(0);
        OBJ_FIELD(tcm, CM_NUM_TEMPS) = tag_smallint(0);
        OBJ_FIELD(tcm, CM_LITERALS) = (uint64_t)tlits;
        OBJ_FIELD(tcm, CM_BYTECODES) = (uint64_t)tbc;

        uint64_t txn[1 + 64 * 3];
        txn[0] = 0;

        sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
        fp = (uint64_t *)0xCAFE;
        stack_push(&sp, stack, (uint64_t)tobj);

        activate_method(&sp, &fp, 0, (uint64_t)tcm, 0, 0);
        interpret(&sp, &fp, (uint8_t *)&OBJ_FIELD(tbc, 0), class_table, om, txn);

        // Log has the write, object unchanged
        ASSERT_EQ(ctx, txn[0], 1,
                  "txn abort: log has 1 entry before abort");
        ASSERT_EQ(ctx, OBJ_FIELD(tobj, 0), tag_smallint(42),
                  "txn abort: object still 42 before abort");

        // Abort: discard log
        txn_abort(txn);
        ASSERT_EQ(ctx, txn[0], (uint64_t)0,
                  "txn abort: log empty after abort");
        ASSERT_EQ(ctx, OBJ_FIELD(tobj, 0), tag_smallint(42),
                  "txn abort: object still 42 after abort");
    }

    // --- at: primitive (no transaction) ---
    {
        uint64_t *om = ctx->om;
        uint64_t *class_class = ctx->class_class;
        uint64_t *smallint_class = ctx->smallint_class;
        uint64_t *class_table = ctx->class_table;
        uint64_t *stack = ctx->stack;
        uint64_t *sp, *fp;

        // Create an Array class with at: method (PRIM_AT)
        uint64_t *array_class = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 4);
        OBJ_FIELD(array_class, CLASS_SUPERCLASS) = tagged_nil();
        OBJ_FIELD(array_class, CLASS_INST_SIZE) = tag_smallint(0);
        OBJ_FIELD(array_class, CLASS_INST_FORMAT) = tag_smallint(FORMAT_INDEXABLE);

        uint64_t sel_at = tag_smallint(90);
        uint64_t *at_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        uint64_t *at_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 2);
        uint8_t *abc = (uint8_t *)&OBJ_FIELD(at_bc, 0);
        abc[0] = BC_HALT;
        OBJ_FIELD(at_cm, CM_PRIMITIVE) = tag_smallint(PRIM_AT);
        OBJ_FIELD(at_cm, CM_NUM_ARGS) = tag_smallint(1);
        OBJ_FIELD(at_cm, CM_NUM_TEMPS) = tag_smallint(0);
        OBJ_FIELD(at_cm, CM_LITERALS) = tagged_nil();
        OBJ_FIELD(at_cm, CM_BYTECODES) = (uint64_t)at_bc;

        uint64_t *arr_md = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 2);
        OBJ_FIELD(arr_md, 0) = sel_at;
        OBJ_FIELD(arr_md, 1) = (uint64_t)at_cm;
        OBJ_FIELD(array_class, CLASS_METHOD_DICT) = (uint64_t)arr_md;

        // Caller method: PUSH_SELF, PUSH_LITERAL 0, SEND at: 1, HALT
        uint64_t *caller_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 30);
        uint8_t *cbc = (uint8_t *)&OBJ_FIELD(caller_bc, 0);
        cbc[0] = BC_PUSH_SELF;
        cbc[1] = BC_PUSH_LITERAL;
        WRITE_U32(cbc + 2, 0); // literal 0 = index
        cbc[6] = BC_SEND_MESSAGE;
        WRITE_U32(cbc + 7, 1);  // selector index 1 = sel_at
        WRITE_U32(cbc + 11, 1); // 1 arg
        cbc[15] = BC_HALT;

        uint64_t *caller_lits = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 2);
        OBJ_FIELD(caller_lits, 0) = tag_smallint(3); // index: at: 3 (1-based, gets third element)
        OBJ_FIELD(caller_lits, 1) = sel_at;          // selector

        uint64_t *caller_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(caller_cm, CM_PRIMITIVE) = tag_smallint(0);
        OBJ_FIELD(caller_cm, CM_NUM_ARGS) = tag_smallint(0);
        OBJ_FIELD(caller_cm, CM_NUM_TEMPS) = tag_smallint(0);
        OBJ_FIELD(caller_cm, CM_LITERALS) = (uint64_t)caller_lits;
        OBJ_FIELD(caller_cm, CM_BYTECODES) = (uint64_t)caller_bc;

        // Create array [10, 20, 30] with array_class
        uint64_t *arr = om_alloc(om, (uint64_t)array_class, FORMAT_INDEXABLE, 3);
        OBJ_FIELD(arr, 0) = tag_smallint(10);
        OBJ_FIELD(arr, 1) = tag_smallint(20);
        OBJ_FIELD(arr, 2) = tag_smallint(30);

        sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
        fp = (uint64_t *)0xCAFE;
        stack_push(&sp, stack, (uint64_t)arr);

        activate_method(&sp, &fp, 0, (uint64_t)caller_cm, 0, 0);
        uint64_t result = interpret(&sp, &fp,
                                    (uint8_t *)&OBJ_FIELD(caller_bc, 0),
                                    class_table, om, NULL);

        ASSERT_EQ(ctx, result, tag_smallint(30),
                  "at: array[2] = 30");
    }

    // --- at:put: primitive (no transaction) ---
    {
        uint64_t *om = ctx->om;
        uint64_t *class_class = ctx->class_class;
        uint64_t *class_table = ctx->class_table;
        uint64_t *stack = ctx->stack;
        uint64_t *sp, *fp;

        // Reuse array_class concept: create class with at:put: method
        uint64_t *array_class2 = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 3);
        OBJ_FIELD(array_class2, CLASS_SUPERCLASS) = tagged_nil();
        OBJ_FIELD(array_class2, CLASS_INST_SIZE) = tag_smallint(0);

        uint64_t sel_atput = tag_smallint(91);
        uint64_t *atput_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        uint64_t *atput_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 2);
        uint8_t *apbc = (uint8_t *)&OBJ_FIELD(atput_bc, 0);
        apbc[0] = BC_HALT;
        OBJ_FIELD(atput_cm, CM_PRIMITIVE) = tag_smallint(PRIM_AT_PUT);
        OBJ_FIELD(atput_cm, CM_NUM_ARGS) = tag_smallint(2);
        OBJ_FIELD(atput_cm, CM_NUM_TEMPS) = tag_smallint(0);
        OBJ_FIELD(atput_cm, CM_LITERALS) = tagged_nil();
        OBJ_FIELD(atput_cm, CM_BYTECODES) = (uint64_t)atput_bc;

        uint64_t *arr_md2 = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 2);
        OBJ_FIELD(arr_md2, 0) = sel_atput;
        OBJ_FIELD(arr_md2, 1) = (uint64_t)atput_cm;
        OBJ_FIELD(array_class2, CLASS_METHOD_DICT) = (uint64_t)arr_md2;

        // Caller: PUSH_SELF, PUSH_LIT 0 (index), PUSH_LIT 1 (value), SEND at:put: 2, HALT
        uint64_t *caller_bc2 = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 30);
        uint8_t *cbc2 = (uint8_t *)&OBJ_FIELD(caller_bc2, 0);
        cbc2[0] = BC_PUSH_SELF;
        cbc2[1] = BC_PUSH_LITERAL;
        WRITE_U32(cbc2 + 2, 0); // literal 0 = index
        cbc2[6] = BC_PUSH_LITERAL;
        WRITE_U32(cbc2 + 7, 1); // literal 1 = value
        cbc2[11] = BC_SEND_MESSAGE;
        WRITE_U32(cbc2 + 12, 2); // selector index 2 = sel_atput
        WRITE_U32(cbc2 + 16, 2); // 2 args
        cbc2[20] = BC_HALT;

        uint64_t *caller_lits2 = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 3);
        OBJ_FIELD(caller_lits2, 0) = tag_smallint(2);  // index (1-based, second element)
        OBJ_FIELD(caller_lits2, 1) = tag_smallint(99); // value
        OBJ_FIELD(caller_lits2, 2) = sel_atput;        // selector

        uint64_t *caller_cm2 = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(caller_cm2, CM_PRIMITIVE) = tag_smallint(0);
        OBJ_FIELD(caller_cm2, CM_NUM_ARGS) = tag_smallint(0);
        OBJ_FIELD(caller_cm2, CM_NUM_TEMPS) = tag_smallint(0);
        OBJ_FIELD(caller_cm2, CM_LITERALS) = (uint64_t)caller_lits2;
        OBJ_FIELD(caller_cm2, CM_BYTECODES) = (uint64_t)caller_bc2;

        // Array [10, 20, 30]
        uint64_t *arr2 = om_alloc(om, (uint64_t)array_class2, FORMAT_INDEXABLE, 3);
        OBJ_FIELD(arr2, 0) = tag_smallint(10);
        OBJ_FIELD(arr2, 1) = tag_smallint(20);
        OBJ_FIELD(arr2, 2) = tag_smallint(30);

        sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
        fp = (uint64_t *)0xCAFE;
        stack_push(&sp, stack, (uint64_t)arr2);

        activate_method(&sp, &fp, 0, (uint64_t)caller_cm2, 0, 0);
        interpret(&sp, &fp,
                  (uint8_t *)&OBJ_FIELD(caller_bc2, 0),
                  class_table, om, NULL);

        ASSERT_EQ(ctx, OBJ_FIELD(arr2, 1), tag_smallint(99),
                  "at:put: array[1] = 99");
        ASSERT_EQ(ctx, OBJ_FIELD(arr2, 0), tag_smallint(10),
                  "at:put: array[0] unchanged");
    }

    // --- at:put: with transaction (write goes to log) ---
    {
        uint64_t *om = ctx->om;
        uint64_t *class_class = ctx->class_class;
        uint64_t *class_table = ctx->class_table;
        uint64_t *stack = ctx->stack;
        uint64_t *sp, *fp;

        uint64_t *array_class3 = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 4);
        OBJ_FIELD(array_class3, CLASS_SUPERCLASS) = tagged_nil();
        OBJ_FIELD(array_class3, CLASS_INST_SIZE) = tag_smallint(0);
        OBJ_FIELD(array_class3, CLASS_INST_FORMAT) = tag_smallint(FORMAT_INDEXABLE);

        uint64_t sel_atput = tag_smallint(91);
        uint64_t *atput_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        uint64_t *atput_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 2);
        ((uint8_t *)&OBJ_FIELD(atput_bc, 0))[0] = BC_HALT;
        OBJ_FIELD(atput_cm, CM_PRIMITIVE) = tag_smallint(PRIM_AT_PUT);
        OBJ_FIELD(atput_cm, CM_NUM_ARGS) = tag_smallint(2);
        OBJ_FIELD(atput_cm, CM_NUM_TEMPS) = tag_smallint(0);
        OBJ_FIELD(atput_cm, CM_LITERALS) = tagged_nil();
        OBJ_FIELD(atput_cm, CM_BYTECODES) = (uint64_t)atput_bc;

        uint64_t *arr_md3 = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 2);
        OBJ_FIELD(arr_md3, 0) = sel_atput;
        OBJ_FIELD(arr_md3, 1) = (uint64_t)atput_cm;
        OBJ_FIELD(array_class3, CLASS_METHOD_DICT) = (uint64_t)arr_md3;

        // Caller: PUSH_SELF, PUSH_LIT 0, PUSH_LIT 1, SEND at:put: 2, HALT
        uint64_t *cbc3 = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 30);
        uint8_t *b3 = (uint8_t *)&OBJ_FIELD(cbc3, 0);
        b3[0] = BC_PUSH_SELF;
        b3[1] = BC_PUSH_LITERAL;
        WRITE_U32(b3 + 2, 0);
        b3[6] = BC_PUSH_LITERAL;
        WRITE_U32(b3 + 7, 1);
        b3[11] = BC_SEND_MESSAGE;
        WRITE_U32(b3 + 12, 2);
        WRITE_U32(b3 + 16, 2);
        b3[20] = BC_HALT;

        uint64_t *clits3 = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 3);
        OBJ_FIELD(clits3, 0) = tag_smallint(1); // 1-based index
        OBJ_FIELD(clits3, 1) = tag_smallint(77);
        OBJ_FIELD(clits3, 2) = sel_atput;

        uint64_t *ccm3 = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(ccm3, CM_PRIMITIVE) = tag_smallint(0);
        OBJ_FIELD(ccm3, CM_NUM_ARGS) = tag_smallint(0);
        OBJ_FIELD(ccm3, CM_NUM_TEMPS) = tag_smallint(0);
        OBJ_FIELD(ccm3, CM_LITERALS) = (uint64_t)clits3;
        OBJ_FIELD(ccm3, CM_BYTECODES) = (uint64_t)cbc3;

        uint64_t *arr3 = om_alloc(om, (uint64_t)array_class3, FORMAT_INDEXABLE, 3);
        OBJ_FIELD(arr3, 0) = tag_smallint(10);
        OBJ_FIELD(arr3, 1) = tag_smallint(20);
        OBJ_FIELD(arr3, 2) = tag_smallint(30);

        uint64_t txn[1 + 64 * 3];
        txn[0] = 0;

        sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
        fp = (uint64_t *)0xCAFE;
        stack_push(&sp, stack, (uint64_t)arr3);

        activate_method(&sp, &fp, 0, (uint64_t)ccm3, 0, 0);
        interpret(&sp, &fp,
                  (uint8_t *)&OBJ_FIELD(cbc3, 0),
                  class_table, om, txn);

        ASSERT_EQ(ctx, OBJ_FIELD(arr3, 0), tag_smallint(10),
                  "txn at:put: array[0] still 10");
        ASSERT_EQ(ctx, txn[0], 1,
                  "txn at:put: log has 1 entry");

        txn_commit(txn);
        ASSERT_EQ(ctx, OBJ_FIELD(arr3, 0), tag_smallint(77),
                  "txn at:put: after commit array[0] = 77");
    }
}
