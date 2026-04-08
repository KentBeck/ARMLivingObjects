#include "test_defs.h"

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
        uint64_t *value_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 2);
        uint64_t *value_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(value_cm, CM_PRIMITIVE) = tag_smallint(PRIM_BLOCK_VALUE);
        OBJ_FIELD(value_cm, CM_NUM_ARGS) = tag_smallint(0);
        OBJ_FIELD(value_cm, CM_NUM_TEMPS) = tag_smallint(0);
        OBJ_FIELD(value_cm, CM_LITERALS) = tagged_nil();
        OBJ_FIELD(value_cm, CM_BYTECODES) = (uint64_t)value_bc;

        uint64_t *blk_md = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 2);
        OBJ_FIELD(blk_md, 0) = sel_value;
        OBJ_FIELD(blk_md, 1) = (uint64_t)value_cm;
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
    }

    // --- True and False classes with ifTrue:ifFalse: ---
    {
        uint64_t sel_value = tag_smallint(60);
        uint64_t sel_ifTF = tag_smallint(70); // #ifTrue:ifFalse:

        // True >> ifTrue: aBlock ifFalse: anotherBlock  ^ aBlock value
        // In this calling convention, the first pushed arg is frame_arg(fp, 1)
        // and the second pushed arg is frame_arg(fp, 0), so the methods use
        // BC_PUSH_ARG with explicit indices.

        // True>>ifTrue:ifFalse: bytecodes:
        //   PUSH_ARG 1    (aBlock = first arg pushed = frame_arg(1))
        //   SEND #value 0
        //   RETURN
        uint64_t *true_itf_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 24);
        uint8_t *titf = (uint8_t *)&OBJ_FIELD(true_itf_bc, 0);
        titf[0] = 15;           // BC_PUSH_ARG
        WRITE_U32(&titf[1], 1); // arg index 1 = aBlock
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
        //   PUSH_ARG 0    (anotherBlock = second arg pushed = frame_arg(0))
        //   SEND #value 0
        //   RETURN
        uint64_t *false_itf_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 24);
        uint8_t *fitf = (uint8_t *)&OBJ_FIELD(false_itf_bc, 0);
        fitf[0] = 15;           // BC_PUSH_ARG
        WRITE_U32(&fitf[1], 0); // arg index 0 = anotherBlock
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
