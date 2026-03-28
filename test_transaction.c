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
}
