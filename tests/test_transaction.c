#include "test_defs.h"
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

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

    // Durable commit currently installs writes through its own entry point.
    txn_log_write(log, (uint64_t)obj, 1, tag_smallint(123));
    ASSERT_EQ(ctx, log[0], 1, "txn_log_write: durable log count is 1");
    txn_commit_durable(log);
    ASSERT_EQ(ctx, OBJ_FIELD(obj, 0), tag_smallint(99), "after durable commit: field 0 unchanged");
    ASSERT_EQ(ctx, OBJ_FIELD(obj, 1), tag_smallint(123), "after durable commit: field 1 is 123");
    ASSERT_EQ(ctx, log[0], (uint64_t)0, "after durable commit: log is empty");

    // Durable append+fsync writes triples to the transaction log file.
    unlink(txn_durable_log_path());
    txn_log_write(log, (uint64_t)obj, 0, tag_smallint(321));
    ASSERT_EQ(ctx, txn_log_append_fsync(log, (uint64_t)om_registered_start(om), om[0]), 1,
              "txn_log_append_fsync succeeds");
    {
        FILE *f = fopen(txn_durable_log_path(), "rb");
        uint64_t record[6];
        ASSERT_EQ(ctx, f != NULL, 1, "durable log file created");
        ASSERT_EQ(ctx, fread(record, sizeof(uint64_t), 6, f), (size_t)6,
                  "durable log file has framed one-entry commit");
        fclose(f);
        ASSERT_EQ(ctx, record[0], UINT64_C(0x41524c4f54584e31),
                  "durable log stores frame magic");
        ASSERT_EQ(ctx, record[1], (uint64_t)1,
                  "durable log stores commit entry count");
        ASSERT_EQ(ctx, record[3], (uint64_t)((uint8_t *)obj - (uint8_t *)om_registered_start(om)),
                  "durable log stores object offset");
        ASSERT_EQ(ctx, record[4], (uint64_t)0,
                  "durable log stores field index");
        ASSERT_EQ(ctx, record[5], tag_smallint(321),
                  "durable log stores tagged value");
    }

    // Durable replay applies the logged write to a loaded heap image.
    {
        uint64_t *replay_obj = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 1);
        uint64_t heap_start = om_registered_start(om);
        ASSERT_EQ(ctx, replay_obj != NULL, 1, "replay object allocated");
        OBJ_FIELD(replay_obj, 0) = tag_smallint(5);
        om_clear_dirty_pages(om);

        txn_log_write(log, (uint64_t)replay_obj, 0, tag_smallint(444));
        ASSERT_EQ(ctx, txn_log_append_fsync(log, heap_start, om[0]), 1,
                  "txn_log_append_fsync succeeds for replay");
        ASSERT_EQ(ctx, OBJ_FIELD(replay_obj, 0), tag_smallint(5),
                  "replay source object unchanged before replay");
        ASSERT_EQ(ctx, txn_log_replay(heap_start, om[0] - heap_start), 1,
                  "txn_log_replay succeeds");
        ASSERT_EQ(ctx, OBJ_FIELD(replay_obj, 0), tag_smallint(444),
                  "txn_log_replay applies durable write");
        ASSERT_EQ(ctx, om_dirty_page_count(om), (uint64_t)1,
                  "txn_log_replay marks one dirty page");
    }

    // Replay of a later field in a multi-page object dirties only the touched page.
    {
        uint64_t *multi_obj = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 1200);
        uint64_t heap_start = om_registered_start(om);
        uint64_t touched_page;
        uint64_t first_page;
        ASSERT_EQ(ctx, multi_obj != NULL, 1, "replay multipage object allocated");
        ASSERT_EQ(ctx, om_object_spans_pages(om, multi_obj), (uint64_t)1,
                  "replay multipage object spans pages");
        OBJ_FIELD(multi_obj, 1199) = tag_smallint(12);
        om_clear_dirty_pages(om);
        txn_durable_log_clear();
        log[0] = 0;

        touched_page = om_page_id_for_address(om, (uint64_t)&OBJ_FIELD(multi_obj, 1199));
        first_page = om_page_id_for_address(om, (uint64_t)multi_obj);
        om_mark_field_dirty(om, multi_obj, 1199);
        ASSERT_EQ(ctx, om_page_is_dirty(om, touched_page), (uint64_t)1,
                  "om_mark_field_dirty marks touched multipage page");
        ASSERT_EQ(ctx, om_page_is_dirty(om, first_page), touched_page == first_page ? (uint64_t)1 : (uint64_t)0,
                  "om_mark_field_dirty does not dirty untouched earlier multipage page");
        om_clear_dirty_pages(om);

        txn_log_write(log, (uint64_t)multi_obj, 1199, tag_smallint(888));
        ASSERT_EQ(ctx, txn_log_append_fsync(log, heap_start, om[0]), 1,
                  "txn_log_append_fsync succeeds for multipage replay");
        ASSERT_EQ(ctx, txn_log_replay(heap_start, om[0] - heap_start), 1,
                  "txn_log_replay succeeds for multipage object");
        ASSERT_EQ(ctx, OBJ_FIELD(multi_obj, 1199), tag_smallint(888),
                  "txn_log_replay updates later multipage field");
        ASSERT_EQ(ctx, om_page_is_dirty(om, touched_page), (uint64_t)1,
                  "txn_log_replay marks touched multipage page dirty");
        ASSERT_EQ(ctx, om_page_is_dirty(om, first_page), touched_page == first_page ? (uint64_t)1 : (uint64_t)0,
                  "txn_log_replay does not dirty an untouched earlier multipage page");
    }

    // Replay ignores a torn tail and keeps the last complete durable commit.
    {
        uint64_t *torn_obj = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 1);
        int fd;
        uint64_t partial = tag_smallint(777);
        uint64_t heap_start = om_registered_start(om);
        uint64_t object_offset;

        ASSERT_EQ(ctx, torn_obj != NULL, 1, "torn-tail object allocated");
        OBJ_FIELD(torn_obj, 0) = tag_smallint(9);
        txn_durable_log_clear();
        txn_log_write(log, (uint64_t)torn_obj, 0, tag_smallint(555));
        ASSERT_EQ(ctx, txn_log_append_fsync(log, heap_start, om[0]), 1,
                  "txn_log_append_fsync succeeds for torn-tail baseline");

        fd = open(txn_durable_log_path(), O_WRONLY | O_APPEND);
        ASSERT_EQ(ctx, fd >= 0, 1, "durable log opened for torn-tail append");
        object_offset = (uint64_t)((uint8_t *)torn_obj - (uint8_t *)heap_start);
        ASSERT_EQ(ctx, write(fd, &object_offset, sizeof(uint64_t)), (ssize_t)sizeof(uint64_t),
                  "torn-tail append writes partial object offset");
        ASSERT_EQ(ctx, write(fd, &partial, sizeof(uint64_t)), (ssize_t)sizeof(uint64_t),
                  "torn-tail append writes second partial word");
        close(fd);

        ASSERT_EQ(ctx, txn_log_replay(heap_start, om[0] - heap_start), 1,
                  "txn_log_replay ignores torn tail");
        ASSERT_EQ(ctx, OBJ_FIELD(torn_obj, 0), tag_smallint(555),
                  "torn-tail replay preserves last complete commit only");
    }

    ASSERT_EQ(ctx, txn_durable_log_clear(), 1, "txn_durable_log_clear succeeds");
    ASSERT_EQ(ctx, access(txn_durable_log_path(), F_OK), -1,
              "txn_durable_log_clear removes journal file");
    unlink(txn_durable_log_path());

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
        om_clear_dirty_pages(om);

        activate_method(&sp, &fp, 0, (uint64_t)caller_cm2, 0, 0);
        interpret(&sp, &fp,
                  (uint8_t *)&OBJ_FIELD(caller_bc2, 0),
                  class_table, om, NULL);

        ASSERT_EQ(ctx, OBJ_FIELD(arr2, 1), tag_smallint(99),
                  "at:put: array[1] = 99");
        ASSERT_EQ(ctx, OBJ_FIELD(arr2, 0), tag_smallint(10),
                  "at:put: array[0] unchanged");
        ASSERT_EQ(ctx, om_dirty_page_count(om), (uint64_t)1,
                  "at:put: direct write marks one dirty page");
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

    // --- byte at:/at:put: with transaction ---
    {
        uint64_t *om = ctx->om;
        uint64_t *class_class = ctx->class_class;
        uint64_t *class_table = ctx->class_table;
        uint64_t *stack = ctx->stack;
        uint64_t *sp, *fp;

        uint64_t *byte_class = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 4);
        OBJ_FIELD(byte_class, CLASS_SUPERCLASS) = tagged_nil();
        OBJ_FIELD(byte_class, CLASS_INST_SIZE) = tag_smallint(0);
        OBJ_FIELD(byte_class, CLASS_INST_FORMAT) = tag_smallint(FORMAT_BYTES);

        uint64_t sel_at = tag_smallint(90);
        uint64_t sel_atput = tag_smallint(91);

        uint64_t *at_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        uint64_t *at_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 2);
        ((uint8_t *)&OBJ_FIELD(at_bc, 0))[0] = BC_HALT;
        OBJ_FIELD(at_cm, CM_PRIMITIVE) = tag_smallint(PRIM_AT);
        OBJ_FIELD(at_cm, CM_NUM_ARGS) = tag_smallint(1);
        OBJ_FIELD(at_cm, CM_NUM_TEMPS) = tag_smallint(0);
        OBJ_FIELD(at_cm, CM_LITERALS) = tagged_nil();
        OBJ_FIELD(at_cm, CM_BYTECODES) = (uint64_t)at_bc;

        uint64_t *atput_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        uint64_t *atput_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 2);
        ((uint8_t *)&OBJ_FIELD(atput_bc, 0))[0] = BC_HALT;
        OBJ_FIELD(atput_cm, CM_PRIMITIVE) = tag_smallint(PRIM_AT_PUT);
        OBJ_FIELD(atput_cm, CM_NUM_ARGS) = tag_smallint(2);
        OBJ_FIELD(atput_cm, CM_NUM_TEMPS) = tag_smallint(0);
        OBJ_FIELD(atput_cm, CM_LITERALS) = tagged_nil();
        OBJ_FIELD(atput_cm, CM_BYTECODES) = (uint64_t)atput_bc;

        uint64_t *md = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 4);
        OBJ_FIELD(md, 0) = sel_at;
        OBJ_FIELD(md, 1) = (uint64_t)at_cm;
        OBJ_FIELD(md, 2) = sel_atput;
        OBJ_FIELD(md, 3) = (uint64_t)atput_cm;
        OBJ_FIELD(byte_class, CLASS_METHOD_DICT) = (uint64_t)md;

        uint64_t *bytes_obj = om_alloc(om, (uint64_t)byte_class, FORMAT_BYTES, 3);
        uint8_t *raw = (uint8_t *)&OBJ_FIELD(bytes_obj, 0);
        raw[0] = 10;
        raw[1] = 20;
        raw[2] = 30;

        // Write in transaction: bytes_obj at: 2 put: 77
        uint64_t *put_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 30);
        uint8_t *pbc = (uint8_t *)&OBJ_FIELD(put_bc, 0);
        pbc[0] = BC_PUSH_SELF;
        pbc[1] = BC_PUSH_LITERAL;
        WRITE_U32(pbc + 2, 0);
        pbc[6] = BC_PUSH_LITERAL;
        WRITE_U32(pbc + 7, 1);
        pbc[11] = BC_SEND_MESSAGE;
        WRITE_U32(pbc + 12, 2);
        WRITE_U32(pbc + 16, 2);
        pbc[20] = BC_HALT;

        uint64_t *put_lits = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 3);
        OBJ_FIELD(put_lits, 0) = tag_smallint(2);
        OBJ_FIELD(put_lits, 1) = tag_smallint(77);
        OBJ_FIELD(put_lits, 2) = sel_atput;

        uint64_t *put_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(put_cm, CM_PRIMITIVE) = tag_smallint(0);
        OBJ_FIELD(put_cm, CM_NUM_ARGS) = tag_smallint(0);
        OBJ_FIELD(put_cm, CM_NUM_TEMPS) = tag_smallint(0);
        OBJ_FIELD(put_cm, CM_LITERALS) = (uint64_t)put_lits;
        OBJ_FIELD(put_cm, CM_BYTECODES) = (uint64_t)put_bc;

        uint64_t txn[1 + 64 * 3];
        txn[0] = 0;

        sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
        fp = (uint64_t *)0xCAFE;
        stack_push(&sp, stack, (uint64_t)bytes_obj);
        activate_method(&sp, &fp, 0, (uint64_t)put_cm, 0, 0);
        interpret(&sp, &fp, (uint8_t *)&OBJ_FIELD(put_bc, 0), class_table, om, txn);

        ASSERT_EQ(ctx, raw[1], (uint64_t)20,
                  "txn byte at:put: leaves object unchanged before commit");
        ASSERT_EQ(ctx, txn[0], 1,
                  "txn byte at:put: writes to log");

        // Read in same transaction: bytes_obj at: 2 => 77 from txn log
        uint64_t *get_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 30);
        uint8_t *gbc = (uint8_t *)&OBJ_FIELD(get_bc, 0);
        gbc[0] = BC_PUSH_SELF;
        gbc[1] = BC_PUSH_LITERAL;
        WRITE_U32(gbc + 2, 0);
        gbc[6] = BC_SEND_MESSAGE;
        WRITE_U32(gbc + 7, 1);
        WRITE_U32(gbc + 11, 1);
        gbc[15] = BC_HALT;

        uint64_t *get_lits = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 2);
        OBJ_FIELD(get_lits, 0) = tag_smallint(2);
        OBJ_FIELD(get_lits, 1) = sel_at;

        uint64_t *get_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(get_cm, CM_PRIMITIVE) = tag_smallint(0);
        OBJ_FIELD(get_cm, CM_NUM_ARGS) = tag_smallint(0);
        OBJ_FIELD(get_cm, CM_NUM_TEMPS) = tag_smallint(0);
        OBJ_FIELD(get_cm, CM_LITERALS) = (uint64_t)get_lits;
        OBJ_FIELD(get_cm, CM_BYTECODES) = (uint64_t)get_bc;

        sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
        fp = (uint64_t *)0xCAFE;
        stack_push(&sp, stack, (uint64_t)bytes_obj);
        activate_method(&sp, &fp, 0, (uint64_t)get_cm, 0, 0);
        uint64_t result = interpret(&sp, &fp,
                                    (uint8_t *)&OBJ_FIELD(get_bc, 0),
                                    class_table, om, txn);

        ASSERT_EQ(ctx, result, tag_smallint(77),
                  "txn byte at: reads pending value from log");

        txn_commit(txn);
        ASSERT_EQ(ctx, raw[1], (uint64_t)77,
                  "txn byte at:put: writes byte on commit");
    }
}

// Additional durability hardening regressions.
// Intentionally not wired into test_main yet; these are for focused manual runs
// while recovery policy is still being decided.
void test_transaction_reliability_extra(TestContext *ctx)
{
    uint64_t *om = ctx->om;
    uint64_t *class_class = ctx->class_class;
    uint64_t log[1 + 64 * 3];
    uint64_t heap_start;

    txn_durable_log_clear();

    // Multiple durable commits replay in append order, with later commits
    // overwriting earlier values for the same slot.
    {
        uint64_t *obj = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 1);
        ASSERT_EQ(ctx, obj != NULL, 1, "extra durability: multi-commit object allocated");
        OBJ_FIELD(obj, 0) = tag_smallint(1);
        heap_start = om_registered_start(om);

        log[0] = 0;
        txn_log_write(log, (uint64_t)obj, 0, tag_smallint(100));
        ASSERT_EQ(ctx, txn_log_append_fsync(log, heap_start, om[0]), 1,
                  "extra durability: first framed commit appended");

        log[0] = 0;
        txn_log_write(log, (uint64_t)obj, 0, tag_smallint(200));
        ASSERT_EQ(ctx, txn_log_append_fsync(log, heap_start, om[0]), 1,
                  "extra durability: second framed commit appended");

        OBJ_FIELD(obj, 0) = tag_smallint(1);
        ASSERT_EQ(ctx, txn_log_replay(heap_start, om[0] - heap_start), 1,
                  "extra durability: multi-commit replay succeeds");
        ASSERT_EQ(ctx, OBJ_FIELD(obj, 0), tag_smallint(200),
                  "extra durability: later commit wins during replay");
    }

    txn_durable_log_clear();

    // A checksum-corrupted but complete later frame is ignored; replay keeps the
    // last valid earlier commit and reports success under the current policy.
    {
        uint64_t *obj = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 1);
        int fd;
        uint64_t bad_value = tag_smallint(333);
        off_t bad_value_offset_words = (off_t)11;

        ASSERT_EQ(ctx, obj != NULL, 1, "extra durability: corrupted-frame object allocated");
        OBJ_FIELD(obj, 0) = tag_smallint(7);
        heap_start = om_registered_start(om);

        log[0] = 0;
        txn_log_write(log, (uint64_t)obj, 0, tag_smallint(111));
        ASSERT_EQ(ctx, txn_log_append_fsync(log, heap_start, om[0]), 1,
                  "extra durability: baseline valid commit appended");

        log[0] = 0;
        txn_log_write(log, (uint64_t)obj, 0, tag_smallint(222));
        ASSERT_EQ(ctx, txn_log_append_fsync(log, heap_start, om[0]), 1,
                  "extra durability: later valid commit appended before corruption");

        fd = open(txn_durable_log_path(), O_WRONLY);
        ASSERT_EQ(ctx, fd >= 0, 1, "extra durability: durable log opened for corruption");
        ASSERT_EQ(ctx, lseek(fd, (off_t)(bad_value_offset_words * (off_t)sizeof(uint64_t)), SEEK_SET),
                  (off_t)(bad_value_offset_words * (off_t)sizeof(uint64_t)),
                  "extra durability: seek to second frame value");
        ASSERT_EQ(ctx, write(fd, &bad_value, sizeof(uint64_t)), (ssize_t)sizeof(uint64_t),
                  "extra durability: overwrite second frame value without checksum update");
        close(fd);

        OBJ_FIELD(obj, 0) = tag_smallint(7);
        ASSERT_EQ(ctx, txn_log_replay(heap_start, om[0] - heap_start), 1,
                  "extra durability: replay succeeds despite corrupted later frame");
        ASSERT_EQ(ctx, OBJ_FIELD(obj, 0), tag_smallint(111),
                  "extra durability: corrupted later frame is ignored, prior commit remains");
    }

    ASSERT_EQ(ctx, txn_durable_log_clear(), 1,
              "extra durability: durable log cleared after manual regression checks");
}
