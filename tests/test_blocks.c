#include "test_defs.h"
#include "bootstrap_compiler.h"
#include "primitives.h"

void test_blocks(TestContext *ctx)
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
    uint64_t result;

    // --- Section 12b: Blocks ---

    // Install Block>>value (primitive PRIM_BLOCK_VALUE)
    {
        uint64_t sel_value = tag_smallint(60);
        uint64_t sel_cannot_return = cannot_return_selector_oop();
        uint64_t *value_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 2);
        uint64_t *value_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(value_cm, CM_PRIMITIVE) = tag_smallint(PRIM_BLOCK_VALUE);
        OBJ_FIELD(value_cm, CM_NUM_ARGS) = tag_smallint(0);
        OBJ_FIELD(value_cm, CM_NUM_TEMPS) = tag_smallint(0);
        OBJ_FIELD(value_cm, CM_LITERALS) = tagged_nil();
        OBJ_FIELD(value_cm, CM_BYTECODES) = (uint64_t)value_bc;

        uint64_t *cannot_return_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 8);
        uint8_t *crb = (uint8_t *)&OBJ_FIELD(cannot_return_bc, 0);
        crb[0] = BC_PUSH_ARG;
        WRITE_U32(&crb[1], 0);
        crb[5] = BC_RETURN;

        uint64_t *cannot_return_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(cannot_return_cm, CM_PRIMITIVE) = tag_smallint(0);
        OBJ_FIELD(cannot_return_cm, CM_NUM_ARGS) = tag_smallint(1);
        OBJ_FIELD(cannot_return_cm, CM_NUM_TEMPS) = tag_smallint(0);
        OBJ_FIELD(cannot_return_cm, CM_LITERALS) = tagged_nil();
        OBJ_FIELD(cannot_return_cm, CM_BYTECODES) = (uint64_t)cannot_return_bc;

        uint64_t *blk_md = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 4);
        OBJ_FIELD(blk_md, 0) = sel_value;
        OBJ_FIELD(blk_md, 1) = (uint64_t)value_cm;
        OBJ_FIELD(blk_md, 2) = sel_cannot_return;
        OBJ_FIELD(blk_md, 3) = (uint64_t)cannot_return_cm;
        OBJ_FIELD(block_class, CLASS_METHOD_DICT) = (uint64_t)blk_md;

        // Test: PUSH_CLOSURE creates a Block, send #value, block returns 77
        // Block body CM: PUSH_LITERAL 0 (= 77), RETURN
        uint64_t *blk_body_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 16);
        uint8_t *bbb = (uint8_t *)&OBJ_FIELD(blk_body_bc, 0);
        bbb[0] = BC_PUSH_LITERAL;
        WRITE_U32(&bbb[1], 0);
        bbb[5] = BC_RETURN;

        uint64_t *blk_body_lits = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 1);
        OBJ_FIELD(blk_body_lits, 0) = tag_smallint(77);
        uint64_t *blk_body_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(blk_body_cm, CM_PRIMITIVE) = tag_smallint(0);
        OBJ_FIELD(blk_body_cm, CM_NUM_ARGS) = tag_smallint(0);
        OBJ_FIELD(blk_body_cm, CM_NUM_TEMPS) = tag_smallint(0);
        OBJ_FIELD(blk_body_cm, CM_LITERALS) = (uint64_t)blk_body_lits;
        OBJ_FIELD(blk_body_cm, CM_BYTECODES) = (uint64_t)blk_body_bc;

        // Caller: PUSH_CLOSURE 0 (block body CM is literal 0), SEND #value, HALT
        uint64_t *caller_bc2 = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 24);
        uint8_t *cb2 = (uint8_t *)&OBJ_FIELD(caller_bc2, 0);
        cb2[0] = BC_PUSH_CLOSURE;
        WRITE_U32(&cb2[1], 0); // literal 0 = blk_body_cm
        cb2[5] = BC_SEND_MESSAGE;
        WRITE_U32(&cb2[6], 1);  // selector index 1 = sel_value
        WRITE_U32(&cb2[10], 0); // 0 args
        cb2[14] = BC_HALT;

        uint64_t *caller_lits2 = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 2);
        OBJ_FIELD(caller_lits2, 0) = (uint64_t)blk_body_cm;
        OBJ_FIELD(caller_lits2, 1) = sel_value;
        uint64_t *caller_cm2 = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(caller_cm2, CM_PRIMITIVE) = tag_smallint(0);
        OBJ_FIELD(caller_cm2, CM_NUM_ARGS) = tag_smallint(0);
        OBJ_FIELD(caller_cm2, CM_NUM_TEMPS) = tag_smallint(0);
        OBJ_FIELD(caller_cm2, CM_LITERALS) = (uint64_t)caller_lits2;
        OBJ_FIELD(caller_cm2, CM_BYTECODES) = (uint64_t)caller_bc2;

        sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
        fp = (uint64_t *)0xCAFE;
        stack_push(&sp, stack, receiver);
        activate_method(&sp, &fp, 0, (uint64_t)caller_cm2, 0, 0);
        result = interpret(&sp, &fp,
                           (uint8_t *)&OBJ_FIELD(caller_bc2, 0),
                           class_table, om, NULL);
        ASSERT_EQ(ctx, result, tag_smallint(77),
                  "Block: PUSH_CLOSURE + send value returns 77");

        // Test: PUSH_CLOSURE records a home context as well as receiver and CM.
        {
            uint64_t *inspect_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 8);
            uint8_t *ibc = (uint8_t *)&OBJ_FIELD(inspect_bc, 0);
            ibc[0] = BC_PUSH_CLOSURE;
            WRITE_U32(&ibc[1], 0);
            ibc[5] = BC_HALT;

            uint64_t *inspect_lits = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 1);
            OBJ_FIELD(inspect_lits, 0) = (uint64_t)blk_body_cm;
            uint64_t *inspect_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
            OBJ_FIELD(inspect_cm, CM_PRIMITIVE) = tag_smallint(0);
            OBJ_FIELD(inspect_cm, CM_NUM_ARGS) = tag_smallint(0);
            OBJ_FIELD(inspect_cm, CM_NUM_TEMPS) = tag_smallint(0);
            OBJ_FIELD(inspect_cm, CM_LITERALS) = (uint64_t)inspect_lits;
            OBJ_FIELD(inspect_cm, CM_BYTECODES) = (uint64_t)inspect_bc;

            sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
            fp = (uint64_t *)0xCAFE;
            stack_push(&sp, stack, receiver);
            activate_method(&sp, &fp, 0, (uint64_t)inspect_cm, 0, 0);
            uint64_t block_oop = interpret(&sp, &fp,
                                           (uint8_t *)&OBJ_FIELD(inspect_bc, 0),
                                           class_table, om, NULL);
            uint64_t *block_obj = (uint64_t *)block_oop;
            ASSERT_EQ(ctx, is_object_ptr(block_oop), 1,
                      "Block: PUSH_CLOSURE returns block object");
            ASSERT_EQ(ctx, is_object_ptr(OBJ_FIELD(block_obj, BLOCK_HOME_CONTEXT)), 1,
                      "Block: PUSH_CLOSURE stores home context");
            ASSERT_EQ(ctx, OBJ_FIELD(block_obj, BLOCK_HOME_RECEIVER), receiver,
                      "Block: PUSH_CLOSURE stores home receiver");
            ASSERT_EQ(ctx, OBJ_FIELD(block_obj, BLOCK_CM), (uint64_t)blk_body_cm,
                      "Block: PUSH_CLOSURE stores block compiled method");
        }

        // Test: block captures self — block body pushes self, returns
        uint64_t *self_blk_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 8);
        uint8_t *sblk = (uint8_t *)&OBJ_FIELD(self_blk_bc, 0);
        sblk[0] = BC_PUSH_SELF;
        sblk[1] = BC_RETURN;

        uint64_t *self_blk_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(self_blk_cm, CM_PRIMITIVE) = tag_smallint(0);
        OBJ_FIELD(self_blk_cm, CM_NUM_ARGS) = tag_smallint(0);
        OBJ_FIELD(self_blk_cm, CM_NUM_TEMPS) = tag_smallint(0);
        OBJ_FIELD(self_blk_cm, CM_LITERALS) = tagged_nil();
        OBJ_FIELD(self_blk_cm, CM_BYTECODES) = (uint64_t)self_blk_bc;

        // Caller: PUSH_CLOSURE 0, SEND #value, HALT
        uint64_t *caller_bc3 = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 24);
        uint8_t *cb3 = (uint8_t *)&OBJ_FIELD(caller_bc3, 0);
        cb3[0] = BC_PUSH_CLOSURE;
        WRITE_U32(&cb3[1], 0);
        cb3[5] = BC_SEND_MESSAGE;
        WRITE_U32(&cb3[6], 1);
        WRITE_U32(&cb3[10], 0);
        cb3[14] = BC_HALT;

        uint64_t *caller_lits3 = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 2);
        OBJ_FIELD(caller_lits3, 0) = (uint64_t)self_blk_cm;
        OBJ_FIELD(caller_lits3, 1) = sel_value;
        uint64_t *caller_cm3 = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(caller_cm3, CM_PRIMITIVE) = tag_smallint(0);
        OBJ_FIELD(caller_cm3, CM_NUM_ARGS) = tag_smallint(0);
        OBJ_FIELD(caller_cm3, CM_NUM_TEMPS) = tag_smallint(0);
        OBJ_FIELD(caller_cm3, CM_LITERALS) = (uint64_t)caller_lits3;
        OBJ_FIELD(caller_cm3, CM_BYTECODES) = (uint64_t)caller_bc3;

        sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
        fp = (uint64_t *)0xCAFE;
        stack_push(&sp, stack, receiver);
        activate_method(&sp, &fp, 0, (uint64_t)caller_cm3, 0, 0);
        result = interpret(&sp, &fp,
                           (uint8_t *)&OBJ_FIELD(caller_bc3, 0),
                           class_table, om, NULL);
        ASSERT_EQ(ctx, result, receiver,
                  "Block: captures self — [self] value returns receiver");

        // Test: copied temp values are captured at closure creation time.
        {
            uint64_t *copied_blk_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 8);
            uint8_t *cblk = (uint8_t *)&OBJ_FIELD(copied_blk_bc, 0);
            cblk[0] = BC_PUSH_TEMP;
            WRITE_U32(&cblk[1], 0);
            cblk[5] = BC_RETURN;

            uint64_t *copied_blk_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
            OBJ_FIELD(copied_blk_cm, CM_PRIMITIVE) = tag_smallint(0);
            OBJ_FIELD(copied_blk_cm, CM_NUM_ARGS) = tag_smallint(0);
            OBJ_FIELD(copied_blk_cm, CM_NUM_TEMPS) = tag_smallint(0);
            OBJ_FIELD(copied_blk_cm, CM_LITERALS) = tagged_nil();
            OBJ_FIELD(copied_blk_cm, CM_BYTECODES) = (uint64_t)copied_blk_bc;

            uint64_t *copied_caller_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 40);
            uint8_t *ccb = (uint8_t *)&OBJ_FIELD(copied_caller_bc, 0);
            ccb[0] = BC_PUSH_LITERAL;
            WRITE_U32(&ccb[1], 0);   // 41
            ccb[5] = BC_STORE_TEMP;
            WRITE_U32(&ccb[6], 0);
            ccb[10] = BC_PUSH_CLOSURE;
            WRITE_U32(&ccb[11], 1);  // copied block
            ccb[15] = BC_PUSH_LITERAL;
            WRITE_U32(&ccb[16], 2);  // 99
            ccb[20] = BC_STORE_TEMP;
            WRITE_U32(&ccb[21], 0);
            ccb[25] = BC_SEND_MESSAGE;
            WRITE_U32(&ccb[26], 3);  // #value
            WRITE_U32(&ccb[30], 0);
            ccb[34] = BC_HALT;

            uint64_t *copied_caller_lits = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 4);
            OBJ_FIELD(copied_caller_lits, 0) = tag_smallint(41);
            OBJ_FIELD(copied_caller_lits, 1) = (uint64_t)copied_blk_cm;
            OBJ_FIELD(copied_caller_lits, 2) = tag_smallint(99);
            OBJ_FIELD(copied_caller_lits, 3) = sel_value;
            uint64_t *copied_caller_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
            OBJ_FIELD(copied_caller_cm, CM_PRIMITIVE) = tag_smallint(0);
            OBJ_FIELD(copied_caller_cm, CM_NUM_ARGS) = tag_smallint(0);
            OBJ_FIELD(copied_caller_cm, CM_NUM_TEMPS) = tag_smallint(1);
            OBJ_FIELD(copied_caller_cm, CM_LITERALS) = (uint64_t)copied_caller_lits;
            OBJ_FIELD(copied_caller_cm, CM_BYTECODES) = (uint64_t)copied_caller_bc;

            sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
            fp = (uint64_t *)0xCAFE;
            stack_push(&sp, stack, receiver);
            activate_method(&sp, &fp, 0, (uint64_t)copied_caller_cm, 0, 1);
            result = interpret(&sp, &fp,
                               (uint8_t *)&OBJ_FIELD(copied_caller_bc, 0),
                               class_table, om, NULL);
            ASSERT_EQ(ctx, result, tag_smallint(41),
                      "Block: copied temp keeps creation-time value");
        }

        // Direct interpreter repro for the failing visitMessage send path:
        // a fake `false ifTrue:ifFalse:` activates a block that references a
        // copied outer temp (`aNode`) and then grows the send sequence until it fails.
        {
            uint64_t sel_value_exact = intern_cstring_symbol(om, "value");
            uint64_t sel_receiver = intern_cstring_symbol(om, "receiver");
            uint64_t sel_arguments = intern_cstring_symbol(om, "arguments");
            uint64_t sel_selector = intern_cstring_symbol(om, "selector");
            uint64_t sel_size = intern_cstring_symbol(om, "size");
            uint64_t sel_visitNode = intern_cstring_symbol(om, "visitNode:");
            uint64_t sel_visitMessageArgsFrom = intern_cstring_symbol(om, "visitMessageArgs:from:");
            uint64_t sel_addSelectorLiteral = intern_cstring_symbol(om, "addSelectorLiteral:");
            uint64_t sel_emitSendMessageArgc = intern_cstring_symbol(om, "emitSendMessage:argc:");
            uint64_t sel_ifTrueIfFalse = intern_cstring_symbol(om, "ifTrue:ifFalse:");

            uint64_t *ret_self_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 8);
            uint8_t *rsb = (uint8_t *)&OBJ_FIELD(ret_self_bc, 0);
            rsb[0] = BC_PUSH_SELF;
            rsb[1] = BC_RETURN;

            uint64_t *ret_self_1_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
            OBJ_FIELD(ret_self_1_cm, CM_PRIMITIVE) = tag_smallint(0);
            OBJ_FIELD(ret_self_1_cm, CM_NUM_ARGS) = tag_smallint(1);
            OBJ_FIELD(ret_self_1_cm, CM_NUM_TEMPS) = tag_smallint(0);
            OBJ_FIELD(ret_self_1_cm, CM_LITERALS) = tagged_nil();
            OBJ_FIELD(ret_self_1_cm, CM_BYTECODES) = (uint64_t)ret_self_bc;

            uint64_t *ret_self_2_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
            OBJ_FIELD(ret_self_2_cm, CM_PRIMITIVE) = tag_smallint(0);
            OBJ_FIELD(ret_self_2_cm, CM_NUM_ARGS) = tag_smallint(2);
            OBJ_FIELD(ret_self_2_cm, CM_NUM_TEMPS) = tag_smallint(0);
            OBJ_FIELD(ret_self_2_cm, CM_LITERALS) = tagged_nil();
            OBJ_FIELD(ret_self_2_cm, CM_BYTECODES) = (uint64_t)ret_self_bc;

            uint64_t *ret_lit_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 8);
            uint8_t *rlb = (uint8_t *)&OBJ_FIELD(ret_lit_bc, 0);
            rlb[0] = BC_PUSH_LITERAL;
            WRITE_U32(&rlb[1], 0);
            rlb[5] = BC_RETURN;

            uint64_t *ret_one_lits = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 1);
            OBJ_FIELD(ret_one_lits, 0) = tag_smallint(1);
            uint64_t *ret_one_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
            OBJ_FIELD(ret_one_cm, CM_PRIMITIVE) = tag_smallint(0);
            OBJ_FIELD(ret_one_cm, CM_NUM_ARGS) = tag_smallint(1);
            OBJ_FIELD(ret_one_cm, CM_NUM_TEMPS) = tag_smallint(0);
            OBJ_FIELD(ret_one_cm, CM_LITERALS) = (uint64_t)ret_one_lits;
            OBJ_FIELD(ret_one_cm, CM_BYTECODES) = (uint64_t)ret_lit_bc;

            uint64_t *ret_zero_lits = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 1);
            OBJ_FIELD(ret_zero_lits, 0) = tag_smallint(0);
            uint64_t *ret_zero_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
            OBJ_FIELD(ret_zero_cm, CM_PRIMITIVE) = tag_smallint(0);
            OBJ_FIELD(ret_zero_cm, CM_NUM_ARGS) = tag_smallint(0);
            OBJ_FIELD(ret_zero_cm, CM_NUM_TEMPS) = tag_smallint(0);
            OBJ_FIELD(ret_zero_cm, CM_LITERALS) = (uint64_t)ret_zero_lits;
            OBJ_FIELD(ret_zero_cm, CM_BYTECODES) = (uint64_t)ret_lit_bc;

            uint64_t *args_size_lits = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 1);
            OBJ_FIELD(args_size_lits, 0) = tag_smallint(1);
            uint64_t *args_size_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
            OBJ_FIELD(args_size_cm, CM_PRIMITIVE) = tag_smallint(0);
            OBJ_FIELD(args_size_cm, CM_NUM_ARGS) = tag_smallint(0);
            OBJ_FIELD(args_size_cm, CM_NUM_TEMPS) = tag_smallint(0);
            OBJ_FIELD(args_size_cm, CM_LITERALS) = (uint64_t)args_size_lits;
            OBJ_FIELD(args_size_cm, CM_BYTECODES) = (uint64_t)ret_lit_bc;

            uint64_t *args_md = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 2);
            OBJ_FIELD(args_md, 0) = sel_size;
            OBJ_FIELD(args_md, 1) = (uint64_t)args_size_cm;
            uint64_t *args_class = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 4);
            OBJ_FIELD(args_class, CLASS_SUPERCLASS) = tagged_nil();
            OBJ_FIELD(args_class, CLASS_METHOD_DICT) = (uint64_t)args_md;
            OBJ_FIELD(args_class, CLASS_INST_SIZE) = tag_smallint(0);
            OBJ_FIELD(args_class, CLASS_INST_FORMAT) = tag_smallint(FORMAT_FIELDS);
            uint64_t *args_obj = om_alloc(om, (uint64_t)args_class, FORMAT_FIELDS, 0);

            uint64_t *node_receiver_lits = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 1);
            OBJ_FIELD(node_receiver_lits, 0) = tag_smallint(41);
            uint64_t *node_receiver_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
            OBJ_FIELD(node_receiver_cm, CM_PRIMITIVE) = tag_smallint(0);
            OBJ_FIELD(node_receiver_cm, CM_NUM_ARGS) = tag_smallint(0);
            OBJ_FIELD(node_receiver_cm, CM_NUM_TEMPS) = tag_smallint(0);
            OBJ_FIELD(node_receiver_cm, CM_LITERALS) = (uint64_t)node_receiver_lits;
            OBJ_FIELD(node_receiver_cm, CM_BYTECODES) = (uint64_t)ret_lit_bc;

            uint64_t *node_arguments_lits = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 1);
            OBJ_FIELD(node_arguments_lits, 0) = (uint64_t)args_obj;
            uint64_t *node_arguments_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
            OBJ_FIELD(node_arguments_cm, CM_PRIMITIVE) = tag_smallint(0);
            OBJ_FIELD(node_arguments_cm, CM_NUM_ARGS) = tag_smallint(0);
            OBJ_FIELD(node_arguments_cm, CM_NUM_TEMPS) = tag_smallint(0);
            OBJ_FIELD(node_arguments_cm, CM_LITERALS) = (uint64_t)node_arguments_lits;
            OBJ_FIELD(node_arguments_cm, CM_BYTECODES) = (uint64_t)ret_lit_bc;

            uint64_t *node_selector_lits = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 1);
            OBJ_FIELD(node_selector_lits, 0) = tag_smallint(99);
            uint64_t *node_selector_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
            OBJ_FIELD(node_selector_cm, CM_PRIMITIVE) = tag_smallint(0);
            OBJ_FIELD(node_selector_cm, CM_NUM_ARGS) = tag_smallint(0);
            OBJ_FIELD(node_selector_cm, CM_NUM_TEMPS) = tag_smallint(0);
            OBJ_FIELD(node_selector_cm, CM_LITERALS) = (uint64_t)node_selector_lits;
            OBJ_FIELD(node_selector_cm, CM_BYTECODES) = (uint64_t)ret_lit_bc;

            uint64_t *node_md = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 6);
            OBJ_FIELD(node_md, 0) = sel_receiver;
            OBJ_FIELD(node_md, 1) = (uint64_t)node_receiver_cm;
            OBJ_FIELD(node_md, 2) = sel_arguments;
            OBJ_FIELD(node_md, 3) = (uint64_t)node_arguments_cm;
            OBJ_FIELD(node_md, 4) = sel_selector;
            OBJ_FIELD(node_md, 5) = (uint64_t)node_selector_cm;
            uint64_t *node_class = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 4);
            OBJ_FIELD(node_class, CLASS_SUPERCLASS) = tagged_nil();
            OBJ_FIELD(node_class, CLASS_METHOD_DICT) = (uint64_t)node_md;
            OBJ_FIELD(node_class, CLASS_INST_SIZE) = tag_smallint(0);
            OBJ_FIELD(node_class, CLASS_INST_FORMAT) = tag_smallint(FORMAT_FIELDS);
            uint64_t *node_obj = om_alloc(om, (uint64_t)node_class, FORMAT_FIELDS, 0);

            uint64_t *gen_md = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 8);
            OBJ_FIELD(gen_md, 0) = sel_visitNode;
            OBJ_FIELD(gen_md, 1) = (uint64_t)ret_self_1_cm;
            OBJ_FIELD(gen_md, 2) = sel_visitMessageArgsFrom;
            OBJ_FIELD(gen_md, 3) = (uint64_t)ret_self_2_cm;
            OBJ_FIELD(gen_md, 4) = sel_addSelectorLiteral;
            OBJ_FIELD(gen_md, 5) = (uint64_t)ret_one_cm;
            OBJ_FIELD(gen_md, 6) = sel_emitSendMessageArgc;
            OBJ_FIELD(gen_md, 7) = (uint64_t)ret_self_2_cm;
            uint64_t *gen_class = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 4);
            OBJ_FIELD(gen_class, CLASS_SUPERCLASS) = tagged_nil();
            OBJ_FIELD(gen_class, CLASS_METHOD_DICT) = (uint64_t)gen_md;
            OBJ_FIELD(gen_class, CLASS_INST_SIZE) = tag_smallint(0);
            OBJ_FIELD(gen_class, CLASS_INST_FORMAT) = tag_smallint(FORMAT_FIELDS);
            uint64_t *gen_obj = om_alloc(om, (uint64_t)gen_class, FORMAT_FIELDS, 0);

            uint64_t *if_false_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 15);
            uint8_t *ifb = (uint8_t *)&OBJ_FIELD(if_false_bc, 0);
            ifb[0] = BC_PUSH_ARG;
            WRITE_U32(&ifb[1], 1);
            ifb[5] = BC_SEND_MESSAGE;
            WRITE_U32(&ifb[6], 0);
            WRITE_U32(&ifb[10], 0);
            ifb[14] = BC_RETURN;

            uint64_t *if_false_lits = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 1);
            OBJ_FIELD(if_false_lits, 0) = sel_value_exact;
            uint64_t *if_false_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
            OBJ_FIELD(if_false_cm, CM_PRIMITIVE) = tag_smallint(0);
            OBJ_FIELD(if_false_cm, CM_NUM_ARGS) = tag_smallint(2);
            OBJ_FIELD(if_false_cm, CM_NUM_TEMPS) = tag_smallint(0);
            OBJ_FIELD(if_false_cm, CM_LITERALS) = (uint64_t)if_false_lits;
            OBJ_FIELD(if_false_cm, CM_BYTECODES) = (uint64_t)if_false_bc;

            uint64_t *false_md = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 2);
            OBJ_FIELD(false_md, 0) = sel_ifTrueIfFalse;
            OBJ_FIELD(false_md, 1) = (uint64_t)if_false_cm;
            uint64_t *false_class = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 4);
            OBJ_FIELD(false_class, CLASS_SUPERCLASS) = tagged_nil();
            OBJ_FIELD(false_class, CLASS_METHOD_DICT) = (uint64_t)false_md;
            OBJ_FIELD(false_class, CLASS_INST_SIZE) = tag_smallint(0);
            OBJ_FIELD(false_class, CLASS_INST_FORMAT) = tag_smallint(FORMAT_FIELDS);
            uint64_t *false_obj = om_alloc(om, (uint64_t)false_class, FORMAT_FIELDS, 0);

            {
                uint64_t *block_md_exact = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 6);
                OBJ_FIELD(block_md_exact, 0) = sel_value;
                OBJ_FIELD(block_md_exact, 1) = (uint64_t)value_cm;
                OBJ_FIELD(block_md_exact, 2) = sel_value_exact;
                OBJ_FIELD(block_md_exact, 3) = (uint64_t)value_cm;
                OBJ_FIELD(block_md_exact, 4) = sel_cannot_return;
                OBJ_FIELD(block_md_exact, 5) = (uint64_t)cannot_return_cm;
                OBJ_FIELD(block_class, CLASS_METHOD_DICT) = (uint64_t)block_md_exact;
            }

            {
                uint64_t *blk_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 25);
                uint8_t *bb = (uint8_t *)&OBJ_FIELD(blk_bc, 0);
                bb[0] = BC_PUSH_SELF;
                bb[1] = BC_PUSH_TEMP;
                WRITE_U32(&bb[2], 0);
                bb[6] = BC_SEND_MESSAGE;
                WRITE_U32(&bb[7], 0);
                WRITE_U32(&bb[11], 0);
                bb[15] = BC_SEND_MESSAGE;
                WRITE_U32(&bb[16], 1);
                WRITE_U32(&bb[20], 1);
                bb[24] = BC_RETURN;

                uint64_t *blk_lits = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 2);
                OBJ_FIELD(blk_lits, 0) = sel_receiver;
                OBJ_FIELD(blk_lits, 1) = sel_visitNode;
                uint64_t *blk_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
                OBJ_FIELD(blk_cm, CM_PRIMITIVE) = tag_smallint(0);
                OBJ_FIELD(blk_cm, CM_NUM_ARGS) = tag_smallint(0);
                OBJ_FIELD(blk_cm, CM_NUM_TEMPS) = tag_smallint(0);
                OBJ_FIELD(blk_cm, CM_LITERALS) = (uint64_t)blk_lits;
                OBJ_FIELD(blk_cm, CM_BYTECODES) = (uint64_t)blk_bc;

                uint64_t *caller_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 37);
                uint8_t *cb = (uint8_t *)&OBJ_FIELD(caller_bc, 0);
                cb[0] = BC_PUSH_LITERAL;
                WRITE_U32(&cb[1], 0);
                cb[5] = BC_STORE_TEMP;
                WRITE_U32(&cb[6], 0);
                cb[10] = BC_PUSH_LITERAL;
                WRITE_U32(&cb[11], 1);
                cb[15] = BC_PUSH_CLOSURE;
                WRITE_U32(&cb[16], 2);
                cb[20] = BC_PUSH_CLOSURE;
                WRITE_U32(&cb[21], 3);
                cb[25] = BC_SEND_MESSAGE;
                WRITE_U32(&cb[26], 4);
                WRITE_U32(&cb[30], 2);
                cb[34] = BC_POP;
                cb[35] = BC_PUSH_SELF;
                cb[36] = BC_RETURN;

                uint64_t *caller_lits = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 5);
                OBJ_FIELD(caller_lits, 0) = (uint64_t)node_obj;
                OBJ_FIELD(caller_lits, 1) = (uint64_t)false_obj;
                OBJ_FIELD(caller_lits, 2) = (uint64_t)ret_zero_cm;
                OBJ_FIELD(caller_lits, 3) = (uint64_t)blk_cm;
                OBJ_FIELD(caller_lits, 4) = sel_ifTrueIfFalse;
                uint64_t *caller_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
                OBJ_FIELD(caller_cm, CM_PRIMITIVE) = tag_smallint(0);
                OBJ_FIELD(caller_cm, CM_NUM_ARGS) = tag_smallint(0);
                OBJ_FIELD(caller_cm, CM_NUM_TEMPS) = tag_smallint(1);
                OBJ_FIELD(caller_cm, CM_LITERALS) = (uint64_t)caller_lits;
                OBJ_FIELD(caller_cm, CM_BYTECODES) = (uint64_t)caller_bc;

                sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
                fp = (uint64_t *)0xCAFE;
                stack_push(&sp, stack, (uint64_t)gen_obj);
                activate_method(&sp, &fp, 0, (uint64_t)caller_cm, 0, 1);
                result = interpret(&sp, &fp,
                                   (uint8_t *)&OBJ_FIELD(caller_bc, 0),
                                   class_table, om, NULL);
                ASSERT_EQ(ctx, result, (uint64_t)gen_obj,
                          "Dispatch repro: ifFalse block with visit receiver returns self");
            }

            {
                uint64_t *blk_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 94);
                uint8_t *bb = (uint8_t *)&OBJ_FIELD(blk_bc, 0);
                bb[0] = BC_PUSH_SELF;
                bb[1] = BC_PUSH_TEMP;
                WRITE_U32(&bb[2], 0);
                bb[6] = BC_SEND_MESSAGE;
                WRITE_U32(&bb[7], 0);
                WRITE_U32(&bb[11], 0);
                bb[15] = BC_SEND_MESSAGE;
                WRITE_U32(&bb[16], 1);
                WRITE_U32(&bb[20], 1);
                bb[24] = BC_POP;
                bb[25] = BC_PUSH_SELF;
                bb[26] = BC_PUSH_TEMP;
                WRITE_U32(&bb[27], 0);
                bb[31] = BC_SEND_MESSAGE;
                WRITE_U32(&bb[32], 2);
                WRITE_U32(&bb[36], 0);
                bb[40] = BC_PUSH_LITERAL;
                WRITE_U32(&bb[41], 1);
                bb[45] = BC_SEND_MESSAGE;
                WRITE_U32(&bb[46], 3);
                WRITE_U32(&bb[50], 2);
                bb[54] = BC_POP;
                bb[55] = BC_PUSH_SELF;
                bb[56] = BC_PUSH_LITERAL;
                WRITE_U32(&bb[57], 1);
                bb[61] = BC_PUSH_TEMP;
                WRITE_U32(&bb[62], 0);
                bb[66] = BC_SEND_MESSAGE;
                WRITE_U32(&bb[67], 2);
                WRITE_U32(&bb[71], 0);
                bb[75] = BC_SEND_MESSAGE;
                WRITE_U32(&bb[76], 4);
                WRITE_U32(&bb[80], 0);
                bb[84] = BC_SEND_MESSAGE;
                WRITE_U32(&bb[85], 5);
                WRITE_U32(&bb[89], 2);
                bb[93] = BC_RETURN;

                uint64_t *blk_lits = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 6);
                OBJ_FIELD(blk_lits, 0) = sel_receiver;
                OBJ_FIELD(blk_lits, 1) = sel_visitNode;
                OBJ_FIELD(blk_lits, 2) = sel_arguments;
                OBJ_FIELD(blk_lits, 3) = sel_visitMessageArgsFrom;
                OBJ_FIELD(blk_lits, 4) = sel_size;
                OBJ_FIELD(blk_lits, 5) = sel_emitSendMessageArgc;
                uint64_t *blk_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
                OBJ_FIELD(blk_cm, CM_PRIMITIVE) = tag_smallint(0);
                OBJ_FIELD(blk_cm, CM_NUM_ARGS) = tag_smallint(0);
                OBJ_FIELD(blk_cm, CM_NUM_TEMPS) = tag_smallint(0);
                OBJ_FIELD(blk_cm, CM_LITERALS) = (uint64_t)blk_lits;
                OBJ_FIELD(blk_cm, CM_BYTECODES) = (uint64_t)blk_bc;

                uint64_t *caller_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 37);
                uint8_t *cb = (uint8_t *)&OBJ_FIELD(caller_bc, 0);
                cb[0] = BC_PUSH_LITERAL;
                WRITE_U32(&cb[1], 0);
                cb[5] = BC_STORE_TEMP;
                WRITE_U32(&cb[6], 0);
                cb[10] = BC_PUSH_LITERAL;
                WRITE_U32(&cb[11], 1);
                cb[15] = BC_PUSH_CLOSURE;
                WRITE_U32(&cb[16], 2);
                cb[20] = BC_PUSH_CLOSURE;
                WRITE_U32(&cb[21], 3);
                cb[25] = BC_SEND_MESSAGE;
                WRITE_U32(&cb[26], 4);
                WRITE_U32(&cb[30], 2);
                cb[34] = BC_POP;
                cb[35] = BC_PUSH_SELF;
                cb[36] = BC_RETURN;

                uint64_t *caller_lits = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 5);
                OBJ_FIELD(caller_lits, 0) = (uint64_t)node_obj;
                OBJ_FIELD(caller_lits, 1) = (uint64_t)false_obj;
                OBJ_FIELD(caller_lits, 2) = (uint64_t)ret_zero_cm;
                OBJ_FIELD(caller_lits, 3) = (uint64_t)blk_cm;
                OBJ_FIELD(caller_lits, 4) = sel_ifTrueIfFalse;
                uint64_t *caller_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
                OBJ_FIELD(caller_cm, CM_PRIMITIVE) = tag_smallint(0);
                OBJ_FIELD(caller_cm, CM_NUM_ARGS) = tag_smallint(0);
                OBJ_FIELD(caller_cm, CM_NUM_TEMPS) = tag_smallint(1);
                OBJ_FIELD(caller_cm, CM_LITERALS) = (uint64_t)caller_lits;
                OBJ_FIELD(caller_cm, CM_BYTECODES) = (uint64_t)caller_bc;

                sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
                fp = (uint64_t *)0xCAFE;
                stack_push(&sp, stack, (uint64_t)gen_obj);
                activate_method(&sp, &fp, 0, (uint64_t)caller_cm, 0, 1);
                result = interpret(&sp, &fp,
                                   (uint8_t *)&OBJ_FIELD(caller_bc, 0),
                                   class_table, om, NULL);
                ASSERT_EQ(ctx, result, (uint64_t)gen_obj,
                          "Dispatch repro: ifFalse block through emitSendMessage returns self");
            }

            {
                uint64_t *blk_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 123);
                uint8_t *bb = (uint8_t *)&OBJ_FIELD(blk_bc, 0);
                bb[0] = BC_PUSH_SELF;
                bb[1] = BC_PUSH_TEMP;
                WRITE_U32(&bb[2], 0);
                bb[6] = BC_SEND_MESSAGE;
                WRITE_U32(&bb[7], 0);
                WRITE_U32(&bb[11], 0);
                bb[15] = BC_SEND_MESSAGE;
                WRITE_U32(&bb[16], 1);
                WRITE_U32(&bb[20], 1);
                bb[24] = BC_POP;
                bb[25] = BC_PUSH_SELF;
                bb[26] = BC_PUSH_TEMP;
                WRITE_U32(&bb[27], 0);
                bb[31] = BC_SEND_MESSAGE;
                WRITE_U32(&bb[32], 2);
                WRITE_U32(&bb[36], 0);
                bb[40] = BC_PUSH_LITERAL;
                WRITE_U32(&bb[41], 1);
                bb[45] = BC_SEND_MESSAGE;
                WRITE_U32(&bb[46], 3);
                WRITE_U32(&bb[50], 2);
                bb[54] = BC_POP;
                bb[55] = BC_PUSH_SELF;
                bb[56] = BC_PUSH_TEMP;
                WRITE_U32(&bb[57], 0);
                bb[61] = BC_SEND_MESSAGE;
                WRITE_U32(&bb[62], 4);
                WRITE_U32(&bb[66], 0);
                bb[70] = BC_SEND_MESSAGE;
                WRITE_U32(&bb[71], 5);
                WRITE_U32(&bb[75], 1);
                bb[79] = BC_STORE_TEMP;
                WRITE_U32(&bb[80], 1);
                bb[84] = BC_PUSH_SELF;
                bb[85] = BC_PUSH_TEMP;
                WRITE_U32(&bb[86], 1);
                bb[90] = BC_PUSH_TEMP;
                WRITE_U32(&bb[91], 0);
                bb[95] = BC_SEND_MESSAGE;
                WRITE_U32(&bb[96], 2);
                WRITE_U32(&bb[100], 0);
                bb[104] = BC_SEND_MESSAGE;
                WRITE_U32(&bb[105], 6);
                WRITE_U32(&bb[109], 0);
                bb[113] = BC_SEND_MESSAGE;
                WRITE_U32(&bb[114], 7);
                WRITE_U32(&bb[118], 2);
                bb[122] = BC_RETURN;

                uint64_t *blk_lits = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 8);
                OBJ_FIELD(blk_lits, 0) = sel_receiver;
                OBJ_FIELD(blk_lits, 1) = sel_visitNode;
                OBJ_FIELD(blk_lits, 2) = sel_arguments;
                OBJ_FIELD(blk_lits, 3) = sel_visitMessageArgsFrom;
                OBJ_FIELD(blk_lits, 4) = sel_selector;
                OBJ_FIELD(blk_lits, 5) = sel_addSelectorLiteral;
                OBJ_FIELD(blk_lits, 6) = sel_size;
                OBJ_FIELD(blk_lits, 7) = sel_emitSendMessageArgc;
                uint64_t *blk_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
                OBJ_FIELD(blk_cm, CM_PRIMITIVE) = tag_smallint(0);
                OBJ_FIELD(blk_cm, CM_NUM_ARGS) = tag_smallint(0);
                OBJ_FIELD(blk_cm, CM_NUM_TEMPS) = tag_smallint(1);
                OBJ_FIELD(blk_cm, CM_LITERALS) = (uint64_t)blk_lits;
                OBJ_FIELD(blk_cm, CM_BYTECODES) = (uint64_t)blk_bc;

                uint64_t *caller_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 37);
                uint8_t *cb = (uint8_t *)&OBJ_FIELD(caller_bc, 0);
                cb[0] = BC_PUSH_LITERAL;
                WRITE_U32(&cb[1], 0);
                cb[5] = BC_STORE_TEMP;
                WRITE_U32(&cb[6], 0);
                cb[10] = BC_PUSH_LITERAL;
                WRITE_U32(&cb[11], 1);
                cb[15] = BC_PUSH_CLOSURE;
                WRITE_U32(&cb[16], 2);
                cb[20] = BC_PUSH_CLOSURE;
                WRITE_U32(&cb[21], 3);
                cb[25] = BC_SEND_MESSAGE;
                WRITE_U32(&cb[26], 4);
                WRITE_U32(&cb[30], 2);
                cb[34] = BC_POP;
                cb[35] = BC_PUSH_SELF;
                cb[36] = BC_RETURN;

                uint64_t *caller_lits = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 5);
                OBJ_FIELD(caller_lits, 0) = (uint64_t)node_obj;
                OBJ_FIELD(caller_lits, 1) = (uint64_t)false_obj;
                OBJ_FIELD(caller_lits, 2) = (uint64_t)ret_zero_cm;
                OBJ_FIELD(caller_lits, 3) = (uint64_t)blk_cm;
                OBJ_FIELD(caller_lits, 4) = sel_ifTrueIfFalse;
                uint64_t *caller_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
                OBJ_FIELD(caller_cm, CM_PRIMITIVE) = tag_smallint(0);
                OBJ_FIELD(caller_cm, CM_NUM_ARGS) = tag_smallint(0);
                OBJ_FIELD(caller_cm, CM_NUM_TEMPS) = tag_smallint(1);
                OBJ_FIELD(caller_cm, CM_LITERALS) = (uint64_t)caller_lits;
                OBJ_FIELD(caller_cm, CM_BYTECODES) = (uint64_t)caller_bc;

                sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
                fp = (uint64_t *)0xCAFE;
                stack_push(&sp, stack, (uint64_t)gen_obj);
                activate_method(&sp, &fp, 0, (uint64_t)caller_cm, 0, 1);
                result = interpret(&sp, &fp,
                                   (uint8_t *)&OBJ_FIELD(caller_bc, 0),
                                   class_table, om, NULL);
                ASSERT_EQ(ctx, result, (uint64_t)gen_obj,
                          "Dispatch repro: ifFalse block with selector temp returns self");
            }

            {
                const char *probe_source =
                    "!CodeGenerator methodsFor: 'testing'!\n"
                    "probeIfFalseFullVisitMessageThenSelf: aNode\n"
                    "    | selectorIndex |\n"
                    "    false ifTrue: [0] ifFalse: [\n"
                    "        self visitNode: aNode receiver.\n"
                    "        self visitMessageArgs: aNode arguments from: 1.\n"
                    "        selectorIndex := self addSelectorLiteral: aNode selector.\n"
                    "        self emitSendMessage: selectorIndex argc: aNode arguments size\n"
                    "    ].\n"
                    "    ^ self\n"
                    "!\n";
                BClassBinding bindings[] = {
                    {"CodeGenerator", (ObjPtr)gen_class},
                };
                OBJ_FIELD(class_table, CLASS_TABLE_FALSE) = (uint64_t)false_class;

                ASSERT_EQ(ctx,
                          bc_compile_and_install_source_methods(om, class_class, bindings, 1, probe_source),
                          1, "Dispatch repro: install exact bootstrap probe method");

                uint64_t probe_selector =
                    intern_cstring_symbol(om, "probeIfFalseFullVisitMessageThenSelf:");
                uint64_t *probe_cm = (uint64_t *)class_lookup((ObjPtr)gen_class, probe_selector);
                uint64_t *probe_bc = (uint64_t *)OBJ_FIELD(probe_cm, CM_BYTECODES);

                sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
                fp = (uint64_t *)0xCAFE;
                stack_push(&sp, stack, (uint64_t)gen_obj);
                stack_push(&sp, stack, (uint64_t)node_obj);
                activate_method(&sp, &fp, 0, (uint64_t)probe_cm, 1,
                                (uint64_t)untag_smallint(OBJ_FIELD(probe_cm, CM_NUM_TEMPS)));
                result = interpret(&sp, &fp,
                                   (uint8_t *)&OBJ_FIELD(probe_bc, 0),
                                   class_table, om, NULL);
                ASSERT_EQ(ctx, result, (uint64_t)gen_obj,
                          "Dispatch repro: exact bootstrap probe method returns self");
            }
        }

        // Test: a returned block still sees its copied outer value after home returns.
        {
            uint64_t *escaped_blk_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 8);
            uint8_t *eblk = (uint8_t *)&OBJ_FIELD(escaped_blk_bc, 0);
            eblk[0] = BC_PUSH_TEMP;
            WRITE_U32(&eblk[1], 0);
            eblk[5] = BC_RETURN;

            uint64_t *escaped_blk_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
            OBJ_FIELD(escaped_blk_cm, CM_PRIMITIVE) = tag_smallint(0);
            OBJ_FIELD(escaped_blk_cm, CM_NUM_ARGS) = tag_smallint(0);
            OBJ_FIELD(escaped_blk_cm, CM_NUM_TEMPS) = tag_smallint(0);
            OBJ_FIELD(escaped_blk_cm, CM_LITERALS) = tagged_nil();
            OBJ_FIELD(escaped_blk_cm, CM_BYTECODES) = (uint64_t)escaped_blk_bc;

            uint64_t *maker_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 24);
            uint8_t *mb = (uint8_t *)&OBJ_FIELD(maker_bc, 0);
            mb[0] = BC_PUSH_LITERAL;
            WRITE_U32(&mb[1], 0);   // 55
            mb[5] = BC_STORE_TEMP;
            WRITE_U32(&mb[6], 0);
            mb[10] = BC_PUSH_CLOSURE;
            WRITE_U32(&mb[11], 1);  // escaped block
            mb[15] = BC_RETURN;

            uint64_t *maker_lits = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 2);
            OBJ_FIELD(maker_lits, 0) = tag_smallint(55);
            OBJ_FIELD(maker_lits, 1) = (uint64_t)escaped_blk_cm;
            uint64_t *maker_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
            OBJ_FIELD(maker_cm, CM_PRIMITIVE) = tag_smallint(0);
            OBJ_FIELD(maker_cm, CM_NUM_ARGS) = tag_smallint(0);
            OBJ_FIELD(maker_cm, CM_NUM_TEMPS) = tag_smallint(1);
            OBJ_FIELD(maker_cm, CM_LITERALS) = (uint64_t)maker_lits;
            OBJ_FIELD(maker_cm, CM_BYTECODES) = (uint64_t)maker_bc;

            sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
            fp = (uint64_t *)0xCAFE;
            stack_push(&sp, stack, receiver);
            activate_method(&sp, &fp, 0, (uint64_t)maker_cm, 0, 1);
            uint64_t escaped_block = interpret(&sp, &fp,
                                               (uint8_t *)&OBJ_FIELD(maker_bc, 0),
                                               class_table, om, NULL);

            uint64_t *invoke_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 24);
            uint8_t *ib = (uint8_t *)&OBJ_FIELD(invoke_bc, 0);
            ib[0] = BC_PUSH_LITERAL;
            WRITE_U32(&ib[1], 0);   // escaped block object
            ib[5] = BC_SEND_MESSAGE;
            WRITE_U32(&ib[6], 1);   // #value
            WRITE_U32(&ib[10], 0);
            ib[14] = BC_HALT;

            uint64_t *invoke_lits = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 2);
            OBJ_FIELD(invoke_lits, 0) = escaped_block;
            OBJ_FIELD(invoke_lits, 1) = sel_value;
            uint64_t *invoke_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
            OBJ_FIELD(invoke_cm, CM_PRIMITIVE) = tag_smallint(0);
            OBJ_FIELD(invoke_cm, CM_NUM_ARGS) = tag_smallint(0);
            OBJ_FIELD(invoke_cm, CM_NUM_TEMPS) = tag_smallint(0);
            OBJ_FIELD(invoke_cm, CM_LITERALS) = (uint64_t)invoke_lits;
            OBJ_FIELD(invoke_cm, CM_BYTECODES) = (uint64_t)invoke_bc;

            sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
            fp = (uint64_t *)0xCAFE;
            stack_push(&sp, stack, receiver);
            activate_method(&sp, &fp, 0, (uint64_t)invoke_cm, 0, 0);
            result = interpret(&sp, &fp,
                               (uint8_t *)&OBJ_FIELD(invoke_bc, 0),
                               class_table, om, NULL);
            ASSERT_EQ(ctx, result, tag_smallint(55),
                      "Block: escaped closure keeps copied temp after home returns");
        }

        // Test: escaped non-local return sends cannotReturn: to the closure.
        {
            uint64_t *escaped_nlr_blk_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 16);
            uint8_t *enrb = (uint8_t *)&OBJ_FIELD(escaped_nlr_blk_bc, 0);
            enrb[0] = BC_PUSH_LITERAL;
            WRITE_U32(&enrb[1], 0);
            enrb[5] = BC_RETURN_NON_LOCAL;

            uint64_t *escaped_nlr_blk_lits = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 1);
            OBJ_FIELD(escaped_nlr_blk_lits, 0) = tag_smallint(123);
            uint64_t *escaped_nlr_blk_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
            OBJ_FIELD(escaped_nlr_blk_cm, CM_PRIMITIVE) = tag_smallint(0);
            OBJ_FIELD(escaped_nlr_blk_cm, CM_NUM_ARGS) = tag_smallint(0);
            OBJ_FIELD(escaped_nlr_blk_cm, CM_NUM_TEMPS) = tag_smallint(0);
            OBJ_FIELD(escaped_nlr_blk_cm, CM_LITERALS) = (uint64_t)escaped_nlr_blk_lits;
            OBJ_FIELD(escaped_nlr_blk_cm, CM_BYTECODES) = (uint64_t)escaped_nlr_blk_bc;

            uint64_t *escaped_maker_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 8);
            uint8_t *emb = (uint8_t *)&OBJ_FIELD(escaped_maker_bc, 0);
            emb[0] = BC_PUSH_CLOSURE;
            WRITE_U32(&emb[1], 0);
            emb[5] = BC_RETURN;

            uint64_t *escaped_maker_lits = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 1);
            OBJ_FIELD(escaped_maker_lits, 0) = (uint64_t)escaped_nlr_blk_cm;
            uint64_t *escaped_maker_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
            OBJ_FIELD(escaped_maker_cm, CM_PRIMITIVE) = tag_smallint(0);
            OBJ_FIELD(escaped_maker_cm, CM_NUM_ARGS) = tag_smallint(0);
            OBJ_FIELD(escaped_maker_cm, CM_NUM_TEMPS) = tag_smallint(0);
            OBJ_FIELD(escaped_maker_cm, CM_LITERALS) = (uint64_t)escaped_maker_lits;
            OBJ_FIELD(escaped_maker_cm, CM_BYTECODES) = (uint64_t)escaped_maker_bc;

            sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
            fp = (uint64_t *)0xCAFE;
            stack_push(&sp, stack, receiver);
            activate_method(&sp, &fp, 0, (uint64_t)escaped_maker_cm, 0, 0);
            uint64_t escaped_nlr_block = interpret(&sp, &fp,
                                                   (uint8_t *)&OBJ_FIELD(escaped_maker_bc, 0),
                                                   class_table, om, NULL);

            uint64_t *escaped_invoke_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 24);
            uint8_t *eib = (uint8_t *)&OBJ_FIELD(escaped_invoke_bc, 0);
            eib[0] = BC_PUSH_LITERAL;
            WRITE_U32(&eib[1], 0);
            eib[5] = BC_SEND_MESSAGE;
            WRITE_U32(&eib[6], 1);
            WRITE_U32(&eib[10], 0);
            eib[14] = BC_HALT;

            uint64_t *escaped_invoke_lits = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 2);
            OBJ_FIELD(escaped_invoke_lits, 0) = escaped_nlr_block;
            OBJ_FIELD(escaped_invoke_lits, 1) = sel_value;
            uint64_t *escaped_invoke_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
            OBJ_FIELD(escaped_invoke_cm, CM_PRIMITIVE) = tag_smallint(0);
            OBJ_FIELD(escaped_invoke_cm, CM_NUM_ARGS) = tag_smallint(0);
            OBJ_FIELD(escaped_invoke_cm, CM_NUM_TEMPS) = tag_smallint(0);
            OBJ_FIELD(escaped_invoke_cm, CM_LITERALS) = (uint64_t)escaped_invoke_lits;
            OBJ_FIELD(escaped_invoke_cm, CM_BYTECODES) = (uint64_t)escaped_invoke_bc;

            sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
            fp = (uint64_t *)0xCAFE;
            stack_push(&sp, stack, receiver);
            activate_method(&sp, &fp, 0, (uint64_t)escaped_invoke_cm, 0, 0);
            result = interpret(&sp, &fp,
                               (uint8_t *)&OBJ_FIELD(escaped_invoke_bc, 0),
                               class_table, om, NULL);
            ASSERT_EQ(ctx, result, tag_smallint(123),
                      "Block: escaped non-local return sends cannotReturn:");
        }

        // Test: copied arguments are visible inside the block body.
        {
            uint64_t *arg_blk_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 8);
            uint8_t *ablk = (uint8_t *)&OBJ_FIELD(arg_blk_bc, 0);
            ablk[0] = BC_PUSH_TEMP;
            WRITE_U32(&ablk[1], 0);
            ablk[5] = BC_RETURN;

            uint64_t *arg_blk_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
            OBJ_FIELD(arg_blk_cm, CM_PRIMITIVE) = tag_smallint(0);
            OBJ_FIELD(arg_blk_cm, CM_NUM_ARGS) = tag_smallint(0);
            OBJ_FIELD(arg_blk_cm, CM_NUM_TEMPS) = tag_smallint(0);
            OBJ_FIELD(arg_blk_cm, CM_LITERALS) = tagged_nil();
            OBJ_FIELD(arg_blk_cm, CM_BYTECODES) = (uint64_t)arg_blk_bc;

            uint64_t *arg_caller_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 24);
            uint8_t *acb = (uint8_t *)&OBJ_FIELD(arg_caller_bc, 0);
            acb[0] = BC_PUSH_CLOSURE;
            WRITE_U32(&acb[1], 0);
            acb[5] = BC_SEND_MESSAGE;
            WRITE_U32(&acb[6], 1);  // #value
            WRITE_U32(&acb[10], 0);
            acb[14] = BC_HALT;

            uint64_t *arg_caller_lits = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 2);
            OBJ_FIELD(arg_caller_lits, 0) = (uint64_t)arg_blk_cm;
            OBJ_FIELD(arg_caller_lits, 1) = sel_value;
            uint64_t *arg_caller_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
            OBJ_FIELD(arg_caller_cm, CM_PRIMITIVE) = tag_smallint(0);
            OBJ_FIELD(arg_caller_cm, CM_NUM_ARGS) = tag_smallint(1);
            OBJ_FIELD(arg_caller_cm, CM_NUM_TEMPS) = tag_smallint(0);
            OBJ_FIELD(arg_caller_cm, CM_LITERALS) = (uint64_t)arg_caller_lits;
            OBJ_FIELD(arg_caller_cm, CM_BYTECODES) = (uint64_t)arg_caller_bc;

            sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
            fp = (uint64_t *)0xCAFE;
            stack_push(&sp, stack, receiver);
            stack_push(&sp, stack, tag_smallint(77));
            activate_method(&sp, &fp, 0, (uint64_t)arg_caller_cm, 1, 0);
            result = interpret(&sp, &fp,
                               (uint8_t *)&OBJ_FIELD(arg_caller_bc, 0),
                               class_table, om, NULL);
            ASSERT_EQ(ctx, result, tag_smallint(77),
                      "Block: copied outer argument is available in block body");
        }

        // Test: a nested block created inside an active block still captures
        // copied outer values after the outer block frame materializes a
        // context for closure creation.
        {
            uint64_t *inner_blk_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 8);
            uint8_t *iblk = (uint8_t *)&OBJ_FIELD(inner_blk_bc, 0);
            iblk[0] = BC_PUSH_TEMP;
            WRITE_U32(&iblk[1], 0);
            iblk[5] = BC_RETURN;

            uint64_t *inner_blk_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
            OBJ_FIELD(inner_blk_cm, CM_PRIMITIVE) = tag_smallint(0);
            OBJ_FIELD(inner_blk_cm, CM_NUM_ARGS) = tag_smallint(0);
            OBJ_FIELD(inner_blk_cm, CM_NUM_TEMPS) = tag_smallint(0);
            OBJ_FIELD(inner_blk_cm, CM_LITERALS) = tagged_nil();
            OBJ_FIELD(inner_blk_cm, CM_BYTECODES) = (uint64_t)inner_blk_bc;

            uint64_t *outer_blk_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 24);
            uint8_t *oblk = (uint8_t *)&OBJ_FIELD(outer_blk_bc, 0);
            oblk[0] = BC_PUSH_CLOSURE;
            WRITE_U32(&oblk[1], 0);
            oblk[5] = BC_SEND_MESSAGE;
            WRITE_U32(&oblk[6], 1);
            WRITE_U32(&oblk[10], 0);
            oblk[14] = BC_RETURN;

            uint64_t *outer_blk_lits = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 2);
            OBJ_FIELD(outer_blk_lits, 0) = (uint64_t)inner_blk_cm;
            OBJ_FIELD(outer_blk_lits, 1) = sel_value;
            uint64_t *outer_blk_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
            OBJ_FIELD(outer_blk_cm, CM_PRIMITIVE) = tag_smallint(0);
            OBJ_FIELD(outer_blk_cm, CM_NUM_ARGS) = tag_smallint(0);
            OBJ_FIELD(outer_blk_cm, CM_NUM_TEMPS) = tag_smallint(0);
            OBJ_FIELD(outer_blk_cm, CM_LITERALS) = (uint64_t)outer_blk_lits;
            OBJ_FIELD(outer_blk_cm, CM_BYTECODES) = (uint64_t)outer_blk_bc;

            uint64_t *nested_caller_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 24);
            uint8_t *ncb = (uint8_t *)&OBJ_FIELD(nested_caller_bc, 0);
            ncb[0] = BC_PUSH_CLOSURE;
            WRITE_U32(&ncb[1], 0);
            ncb[5] = BC_SEND_MESSAGE;
            WRITE_U32(&ncb[6], 1);
            WRITE_U32(&ncb[10], 0);
            ncb[14] = BC_HALT;

            uint64_t *nested_caller_lits = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 2);
            OBJ_FIELD(nested_caller_lits, 0) = (uint64_t)outer_blk_cm;
            OBJ_FIELD(nested_caller_lits, 1) = sel_value;
            uint64_t *nested_caller_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
            OBJ_FIELD(nested_caller_cm, CM_PRIMITIVE) = tag_smallint(0);
            OBJ_FIELD(nested_caller_cm, CM_NUM_ARGS) = tag_smallint(1);
            OBJ_FIELD(nested_caller_cm, CM_NUM_TEMPS) = tag_smallint(0);
            OBJ_FIELD(nested_caller_cm, CM_LITERALS) = (uint64_t)nested_caller_lits;
            OBJ_FIELD(nested_caller_cm, CM_BYTECODES) = (uint64_t)nested_caller_bc;

            sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
            fp = (uint64_t *)0xCAFE;
            stack_push(&sp, stack, receiver);
            stack_push(&sp, stack, tag_smallint(77));
            activate_method(&sp, &fp, 0, (uint64_t)nested_caller_cm, 1, 0);
            result = interpret(&sp, &fp,
                               (uint8_t *)&OBJ_FIELD(nested_caller_bc, 0),
                               class_table, om, NULL);
            ASSERT_EQ(ctx, result, tag_smallint(77),
                      "Block: nested block keeps copied outer argument");
        }

        // Test: explicit ^ inside a block performs a non-local return to the home method.
        {
            uint64_t *nlr_blk_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 16);
            uint8_t *nrb = (uint8_t *)&OBJ_FIELD(nlr_blk_bc, 0);
            nrb[0] = BC_PUSH_LITERAL;
            WRITE_U32(&nrb[1], 0);
            nrb[5] = BC_RETURN_NON_LOCAL;

            uint64_t *nlr_blk_lits = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 1);
            OBJ_FIELD(nlr_blk_lits, 0) = tag_smallint(88);
            uint64_t *nlr_blk_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
            OBJ_FIELD(nlr_blk_cm, CM_PRIMITIVE) = tag_smallint(0);
            OBJ_FIELD(nlr_blk_cm, CM_NUM_ARGS) = tag_smallint(0);
            OBJ_FIELD(nlr_blk_cm, CM_NUM_TEMPS) = tag_smallint(0);
            OBJ_FIELD(nlr_blk_cm, CM_LITERALS) = (uint64_t)nlr_blk_lits;
            OBJ_FIELD(nlr_blk_cm, CM_BYTECODES) = (uint64_t)nlr_blk_bc;

            uint64_t *nlr_caller_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 32);
            uint8_t *ncb = (uint8_t *)&OBJ_FIELD(nlr_caller_bc, 0);
            ncb[0] = BC_PUSH_CLOSURE;
            WRITE_U32(&ncb[1], 0);
            ncb[5] = BC_SEND_MESSAGE;
            WRITE_U32(&ncb[6], 1);   // #value
            WRITE_U32(&ncb[10], 0);
            ncb[14] = BC_PUSH_LITERAL;
            WRITE_U32(&ncb[15], 2);  // unreachable 99
            ncb[19] = BC_RETURN;

            uint64_t *nlr_caller_lits = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 3);
            OBJ_FIELD(nlr_caller_lits, 0) = (uint64_t)nlr_blk_cm;
            OBJ_FIELD(nlr_caller_lits, 1) = sel_value;
            OBJ_FIELD(nlr_caller_lits, 2) = tag_smallint(99);
            uint64_t *nlr_caller_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
            OBJ_FIELD(nlr_caller_cm, CM_PRIMITIVE) = tag_smallint(0);
            OBJ_FIELD(nlr_caller_cm, CM_NUM_ARGS) = tag_smallint(0);
            OBJ_FIELD(nlr_caller_cm, CM_NUM_TEMPS) = tag_smallint(0);
            OBJ_FIELD(nlr_caller_cm, CM_LITERALS) = (uint64_t)nlr_caller_lits;
            OBJ_FIELD(nlr_caller_cm, CM_BYTECODES) = (uint64_t)nlr_caller_bc;

            sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
            fp = (uint64_t *)0xCAFE;
            stack_push(&sp, stack, receiver);
            activate_method(&sp, &fp, 0, (uint64_t)nlr_caller_cm, 0, 0);
            result = interpret(&sp, &fp,
                               (uint8_t *)&OBJ_FIELD(nlr_caller_bc, 0),
                               class_table, om, NULL);
            ASSERT_EQ(ctx, result, tag_smallint(88),
                      "Block: explicit ^ returns from the home method");
        }
    }

    // --- True and False classes with ifTrue:ifFalse: ---
    {
        uint64_t sel_value = tag_smallint(60);
        uint64_t sel_ifTF = tag_smallint(70); // #ifTrue:ifFalse:

        // True >> ifTrue: aBlock ifFalse: anotherBlock  ^ aBlock value

        // True>>ifTrue:ifFalse: bytecodes:
        //   PUSH_ARG 0    (aBlock = first source argument)
        //   SEND #value 0
        //   RETURN
        uint64_t *true_itf_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 24);
        uint8_t *titf = (uint8_t *)&OBJ_FIELD(true_itf_bc, 0);
        titf[0] = 15;           // BC_PUSH_ARG
        WRITE_U32(&titf[1], 0); // arg index 0 = aBlock
        titf[5] = BC_SEND_MESSAGE;
        WRITE_U32(&titf[6], 0);  // selector index 0 = sel_value
        WRITE_U32(&titf[10], 0); // 0 args to value
        titf[14] = BC_RETURN;

        uint64_t *true_itf_lits = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 1);
        OBJ_FIELD(true_itf_lits, 0) = sel_value;
        uint64_t *true_itf_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(true_itf_cm, CM_PRIMITIVE) = tag_smallint(0);
        OBJ_FIELD(true_itf_cm, CM_NUM_ARGS) = tag_smallint(2);
        OBJ_FIELD(true_itf_cm, CM_NUM_TEMPS) = tag_smallint(0);
        OBJ_FIELD(true_itf_cm, CM_LITERALS) = (uint64_t)true_itf_lits;
        OBJ_FIELD(true_itf_cm, CM_BYTECODES) = (uint64_t)true_itf_bc;

        // False>>ifTrue:ifFalse: bytecodes:
        //   PUSH_ARG 1    (anotherBlock = second source argument)
        //   SEND #value 0
        //   RETURN
        uint64_t *false_itf_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 24);
        uint8_t *fitf = (uint8_t *)&OBJ_FIELD(false_itf_bc, 0);
        fitf[0] = 15;           // BC_PUSH_ARG
        WRITE_U32(&fitf[1], 1); // arg index 1 = anotherBlock
        fitf[5] = BC_SEND_MESSAGE;
        WRITE_U32(&fitf[6], 0);
        WRITE_U32(&fitf[10], 0);
        fitf[14] = BC_RETURN;

        uint64_t *false_itf_lits = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 1);
        OBJ_FIELD(false_itf_lits, 0) = sel_value;
        uint64_t *false_itf_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(false_itf_cm, CM_PRIMITIVE) = tag_smallint(0);
        OBJ_FIELD(false_itf_cm, CM_NUM_ARGS) = tag_smallint(2);
        OBJ_FIELD(false_itf_cm, CM_NUM_TEMPS) = tag_smallint(0);
        OBJ_FIELD(false_itf_cm, CM_LITERALS) = (uint64_t)false_itf_lits;
        OBJ_FIELD(false_itf_cm, CM_BYTECODES) = (uint64_t)false_itf_bc;

        // True class
        uint64_t *true_md = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 2);
        OBJ_FIELD(true_md, 0) = sel_ifTF;
        OBJ_FIELD(true_md, 1) = (uint64_t)true_itf_cm;
        uint64_t *true_class = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 4);
        OBJ_FIELD(true_class, CLASS_SUPERCLASS) = tagged_nil();
        OBJ_FIELD(true_class, CLASS_METHOD_DICT) = (uint64_t)true_md;
        OBJ_FIELD(true_class, CLASS_INST_SIZE) = tag_smallint(0);
        OBJ_FIELD(true_class, CLASS_INST_FORMAT) = tag_smallint(FORMAT_FIELDS);

        // False class
        uint64_t *false_md = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 2);
        OBJ_FIELD(false_md, 0) = sel_ifTF;
        OBJ_FIELD(false_md, 1) = (uint64_t)false_itf_cm;
        uint64_t *false_class = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 4);
        OBJ_FIELD(false_class, CLASS_SUPERCLASS) = tagged_nil();
        OBJ_FIELD(false_class, CLASS_METHOD_DICT) = (uint64_t)false_md;
        OBJ_FIELD(false_class, CLASS_INST_SIZE) = tag_smallint(0);
        OBJ_FIELD(false_class, CLASS_INST_FORMAT) = tag_smallint(FORMAT_FIELDS);

        // Register in class table
        OBJ_FIELD(class_table, 2) = (uint64_t)true_class;
        OBJ_FIELD(class_table, 3) = (uint64_t)false_class;

        // Test: true ifTrue: [77] ifFalse: [99] → 77
        // Block bodies
        uint64_t *blk77_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 16);
        uint8_t *b77 = (uint8_t *)&OBJ_FIELD(blk77_bc, 0);
        b77[0] = BC_PUSH_LITERAL;
        WRITE_U32(&b77[1], 0);
        b77[5] = BC_RETURN;
        uint64_t *blk77_lits = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 1);
        OBJ_FIELD(blk77_lits, 0) = tag_smallint(77);
        uint64_t *blk77_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(blk77_cm, CM_PRIMITIVE) = tag_smallint(0);
        OBJ_FIELD(blk77_cm, CM_NUM_ARGS) = tag_smallint(0);
        OBJ_FIELD(blk77_cm, CM_NUM_TEMPS) = tag_smallint(0);
        OBJ_FIELD(blk77_cm, CM_LITERALS) = (uint64_t)blk77_lits;
        OBJ_FIELD(blk77_cm, CM_BYTECODES) = (uint64_t)blk77_bc;

        uint64_t *blk99_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 16);
        uint8_t *b99 = (uint8_t *)&OBJ_FIELD(blk99_bc, 0);
        b99[0] = BC_PUSH_LITERAL;
        WRITE_U32(&b99[1], 0);
        b99[5] = BC_RETURN;
        uint64_t *blk99_lits = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 1);
        OBJ_FIELD(blk99_lits, 0) = tag_smallint(99);
        uint64_t *blk99_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(blk99_cm, CM_PRIMITIVE) = tag_smallint(0);
        OBJ_FIELD(blk99_cm, CM_NUM_ARGS) = tag_smallint(0);
        OBJ_FIELD(blk99_cm, CM_NUM_TEMPS) = tag_smallint(0);
        OBJ_FIELD(blk99_cm, CM_LITERALS) = (uint64_t)blk99_lits;
        OBJ_FIELD(blk99_cm, CM_BYTECODES) = (uint64_t)blk99_bc;

        // Caller: PUSH_LITERAL 0 (=true), PUSH_CLOSURE 1 (blk77), PUSH_CLOSURE 2 (blk99),
        //         SEND #ifTrue:ifFalse: 2 args, HALT
        uint64_t *itf_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 32);
        uint8_t *itfb = (uint8_t *)&OBJ_FIELD(itf_bc, 0);
        itfb[0] = BC_PUSH_LITERAL;
        WRITE_U32(&itfb[1], 0); // literal 0 = tagged true
        itfb[5] = BC_PUSH_CLOSURE;
        WRITE_U32(&itfb[6], 1); // literal 1 = blk77_cm
        itfb[10] = BC_PUSH_CLOSURE;
        WRITE_U32(&itfb[11], 2); // literal 2 = blk99_cm
        itfb[15] = BC_SEND_MESSAGE;
        WRITE_U32(&itfb[16], 3); // selector index 3 = sel_ifTF
        WRITE_U32(&itfb[20], 2); // 2 args
        itfb[24] = BC_HALT;

        uint64_t *itf_lits = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 4);
        OBJ_FIELD(itf_lits, 0) = tagged_true();
        OBJ_FIELD(itf_lits, 1) = (uint64_t)blk77_cm;
        OBJ_FIELD(itf_lits, 2) = (uint64_t)blk99_cm;
        OBJ_FIELD(itf_lits, 3) = sel_ifTF;
        uint64_t *itf_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(itf_cm, CM_PRIMITIVE) = tag_smallint(0);
        OBJ_FIELD(itf_cm, CM_NUM_ARGS) = tag_smallint(0);
        OBJ_FIELD(itf_cm, CM_NUM_TEMPS) = tag_smallint(0);
        OBJ_FIELD(itf_cm, CM_LITERALS) = (uint64_t)itf_lits;
        OBJ_FIELD(itf_cm, CM_BYTECODES) = (uint64_t)itf_bc;

        sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
        fp = (uint64_t *)0xCAFE;
        stack_push(&sp, stack, receiver);
        activate_method(&sp, &fp, 0, (uint64_t)itf_cm, 0, 0);
        result = interpret(&sp, &fp,
                           (uint8_t *)&OBJ_FIELD(itf_bc, 0),
                           class_table, om, NULL);
        ASSERT_EQ(ctx, result, tag_smallint(77),
                  "True ifTrue: [77] ifFalse: [99] → 77");

        // Test: false ifTrue: [77] ifFalse: [99] → 99
        // Same bytecodes but literal 0 = tagged false
        uint64_t *itf_lits2 = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 4);
        OBJ_FIELD(itf_lits2, 0) = tagged_false();
        OBJ_FIELD(itf_lits2, 1) = (uint64_t)blk77_cm;
        OBJ_FIELD(itf_lits2, 2) = (uint64_t)blk99_cm;
        OBJ_FIELD(itf_lits2, 3) = sel_ifTF;
        uint64_t *itf_cm2 = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(itf_cm2, CM_PRIMITIVE) = tag_smallint(0);
        OBJ_FIELD(itf_cm2, CM_NUM_ARGS) = tag_smallint(0);
        OBJ_FIELD(itf_cm2, CM_NUM_TEMPS) = tag_smallint(0);
        OBJ_FIELD(itf_cm2, CM_LITERALS) = (uint64_t)itf_lits2;
        OBJ_FIELD(itf_cm2, CM_BYTECODES) = (uint64_t)itf_bc;

        sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
        fp = (uint64_t *)0xCAFE;
        stack_push(&sp, stack, receiver);
        activate_method(&sp, &fp, 0, (uint64_t)itf_cm2, 0, 0);
        result = interpret(&sp, &fp,
                           (uint8_t *)&OBJ_FIELD(itf_bc, 0),
                           class_table, om, NULL);
        ASSERT_EQ(ctx, result, tag_smallint(99),
                  "False ifTrue: [77] ifFalse: [99] → 99");
    }

    ctx->smallint_class = smallint_class;
}
