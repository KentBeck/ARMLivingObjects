#include "test_defs.h"

void test_transaction(TestContext *ctx)
{
    uint64_t *om = ctx->om;
    uint64_t *class_class = ctx->class_class;
    (void)om; (void)class_class;

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
}

