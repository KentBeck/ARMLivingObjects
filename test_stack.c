#include "test_defs.h"

void test_stack(TestContext *ctx)
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
    uint64_t ip;
    uint64_t fake_ip=0x1000;
    uint64_t caller_fp_val=0xBEEF;
    uint64_t caller_ip_val=0xDEAD;
    uint64_t result;

    sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));


    // Test: push a value onto a stack and read it back
    stack_push(&sp, stack, 42);
    ASSERT_EQ(ctx, stack_top(&sp), 42, "push a value and read it back");

    // Test: push two values and pop one, reading the remaining top
    stack_push(&sp, stack, 100);
    stack_push(&sp, stack, 200);
    stack_pop(&sp);
    ASSERT_EQ(ctx, stack_top(&sp), 100, "push two values and pop one");


    // --- Method Activation Tests ---
    // Reset stack for activation tests
    sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
    fp = 0;

    stack_push(&sp, stack, receiver); // caller pushes receiver
    activate_method(&sp, &fp, fake_ip, method, 0, 0);

    ASSERT_EQ(ctx, fp[FRAME_SAVED_IP], fake_ip, "activate 0/0: saved IP");
    ASSERT_EQ(ctx, fp[FRAME_SAVED_FP], 0, "activate 0/0: saved caller FP (was null)");
    ASSERT_EQ(ctx, fp[FRAME_METHOD], method, "activate 0/0: method");
    ASSERT_EQ(ctx, fp[FRAME_FLAGS], 0, "activate 0/0: flags (all zero)");
    ASSERT_EQ(ctx, fp[FRAME_CONTEXT], 0, "activate 0/0: context slot (nil)");
    ASSERT_EQ(ctx, fp[FRAME_RECEIVER], receiver, "activate 0/0: receiver");
    ASSERT_EQ(ctx, (uint64_t)sp, (uint64_t)&fp[FRAME_RECEIVER],
              "activate 0/0: SP points at receiver (last pushed)");

    // Test: read receiver from frame via ARM64 function
    ASSERT_EQ(ctx, frame_receiver(fp), receiver, "frame_receiver reads receiver at FP-4*W");

    // Test: activate with 0 temps (exercises cbz early-out)
    // Already tested above as activate 0/0

    // Test: activate with 1 temp (exercises odd path)
    sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
    fp = 0;
    stack_push(&sp, stack, receiver);
    activate_method(&sp, &fp, fake_ip, method, 0, 1);
    ASSERT_EQ(ctx, fp[FRAME_RECEIVER], receiver, "activate 0/1: receiver");
    ASSERT_EQ(ctx, fp[FRAME_TEMP0], 0, "activate 0/1: temp 0 initialized to 0");
    ASSERT_EQ(ctx, (uint64_t)sp, (uint64_t)&fp[FRAME_TEMP0],
              "activate 0/1: SP points at temp 0");

    // Test: activate with 2 temps (exercises pairs path, even count)
    sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
    fp = 0;
    stack_push(&sp, stack, receiver);
    activate_method(&sp, &fp, fake_ip, method, 0, 2);
    ASSERT_EQ(ctx, fp[FRAME_TEMP0], 0, "activate 0/2: temp 0 initialized to 0");
    ASSERT_EQ(ctx, fp[FRAME_TEMP0 - 1], 0, "activate 0/2: temp 1 initialized to 0");
    ASSERT_EQ(ctx, (uint64_t)sp, (uint64_t)&fp[FRAME_TEMP0 - 1],
              "activate 0/2: SP points at temp 1");

    // Test: activate with 3 temps (exercises odd + pairs path)
    sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
    fp = 0;
    stack_push(&sp, stack, receiver);
    activate_method(&sp, &fp, fake_ip, method, 0, 3);
    ASSERT_EQ(ctx, fp[FRAME_TEMP0], 0, "activate 0/3: temp 0 initialized to 0");
    ASSERT_EQ(ctx, fp[FRAME_TEMP0 - 1], 0, "activate 0/3: temp 1 initialized to 0");
    ASSERT_EQ(ctx, fp[FRAME_TEMP0 - 2], 0, "activate 0/3: temp 2 initialized to 0");
    ASSERT_EQ(ctx, (uint64_t)sp, (uint64_t)&fp[FRAME_TEMP0 - 2],
              "activate 0/3: SP points at temp 2");

    // Test: activate with 1 arg, 0 temps: arg accessible above frame
    sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
    fp = 0;
    uint64_t arg0 = 0xAAAA;
    stack_push(&sp, stack, receiver); // caller pushes receiver
    stack_push(&sp, stack, arg0);     // caller pushes arg 0
    activate_method(&sp, &fp, fake_ip, method, 1, 0);
    ASSERT_EQ(ctx, fp[FRAME_RECEIVER], receiver, "activate 1/0: receiver");
    // arg 0 is at FP + 2*W (above saved IP)
    ASSERT_EQ(ctx, fp[2], arg0, "activate 1/0: arg 0 at FP+2*W");
    // flags should encode num_args=1 in byte 1
    ASSERT_EQ(ctx, fp[FRAME_FLAGS] & 0xFF00, 1 << 8, "activate 1/0: flags encode num_args=1");

    // Test: activate with 2 args, 1 temp: verify args and temp layout
    sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
    fp = 0;
    uint64_t arg1 = 0xBBBB;
    stack_push(&sp, stack, receiver); // caller pushes receiver
    stack_push(&sp, stack, arg0);     // caller pushes arg 0
    stack_push(&sp, stack, arg1);     // caller pushes arg 1
    activate_method(&sp, &fp, fake_ip, method, 2, 1);
    ASSERT_EQ(ctx, fp[FRAME_RECEIVER], receiver, "activate 2/1: receiver");
    // Args are in stack order: last pushed (arg1) is closest to frame
    ASSERT_EQ(ctx, fp[2], arg1, "activate 2/1: arg 1 at FP+2*W (last pushed)");
    ASSERT_EQ(ctx, fp[3], arg0, "activate 2/1: arg 0 at FP+3*W (first pushed)");
    ASSERT_EQ(ctx, fp[FRAME_TEMP0], 0, "activate 2/1: temp 0 initialized to 0");
    ASSERT_EQ(ctx, fp[FRAME_FLAGS] & 0xFF00, 2 << 8, "activate 2/1: flags encode num_args=2");

    // Test: read method from frame at FP - 1*W
    ASSERT_EQ(ctx, frame_method(fp), method, "frame_method reads method at FP-1*W");

    // Test: read flags from frame at FP - 2*W
    ASSERT_EQ(ctx, frame_flags(fp), 2 << 8, "frame_flags reads flags (num_args=2)");

    // Test: decode num_args from flags byte 1
    ASSERT_EQ(ctx, frame_num_args(fp), 2, "frame_num_args decodes 2");

    // Test: decode is_block from flags byte 2
    ASSERT_EQ(ctx, frame_is_block(fp), 0, "frame_is_block is 0 for method");

    // Test: decode has_context from flags byte 0
    ASSERT_EQ(ctx, frame_has_context(fp), 0, "frame_has_context is 0 initially");

    // --- Section 4: Temporary Variable Access ---
    // Use the 2-arg, 1-temp frame from above (still in fp)

    // Test: access temp 0 at FP - 5*W
    ASSERT_EQ(ctx, frame_temp(fp, 0), 0, "frame_temp(0) reads temp 0 (was 0)");

    // Test: access arg 0 (last pushed = arg1) at FP + 2*W
    ASSERT_EQ(ctx, frame_arg(fp, 0), arg1, "frame_arg(0) reads arg1 (last pushed)");

    // Test: access arg 1 (first pushed = arg0) at FP + 3*W
    ASSERT_EQ(ctx, frame_arg(fp, 1), arg0, "frame_arg(1) reads arg0 (first pushed)");

    // Test: store into temp 0 and read it back
    frame_store_temp(fp, 0, 0xDEAD);
    ASSERT_EQ(ctx, frame_temp(fp, 0), 0xDEAD, "store_temp(0) then frame_temp(0)");

    // --- Section 5: Return ---

    // Test: return from a 0-arg method
    sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
    fp = 0;
    ip = 0;
    caller_fp_val = 0xCAFE;
    caller_ip_val = 0xF00D;
    // Simulate caller: set FP to a "caller frame" value, then do a send
    fp = (uint64_t *)caller_fp_val;   // fake caller FP
    ip = caller_ip_val;               // fake caller IP
    stack_push(&sp, stack, receiver); // push receiver
    activate_method(&sp, &fp, ip, method, 0, 0);
    // Now return with value 99
    frame_return(&sp, &fp, &ip, 99);
    ASSERT_EQ(ctx, stack_top(&sp), 99, "return 0-arg: result on stack");
    ASSERT_EQ(ctx, (uint64_t)fp, caller_fp_val, "return 0-arg: FP restored");
    ASSERT_EQ(ctx, ip, caller_ip_val, "return 0-arg: IP restored");

    // Test: return from a 1-arg method
    sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
    fp = (uint64_t *)caller_fp_val;
    ip = caller_ip_val;
    stack_push(&sp, stack, receiver);
    stack_push(&sp, stack, arg0);
    uint64_t *sp_before_send = sp;
    activate_method(&sp, &fp, ip, method, 1, 0);
    frame_return(&sp, &fp, &ip, 77);
    ASSERT_EQ(ctx, stack_top(&sp), 77, "return 1-arg: result on stack");
    ASSERT_EQ(ctx, (uint64_t)fp, caller_fp_val, "return 1-arg: FP restored");
    // SP should point where receiver was (arg consumed)
    ASSERT_EQ(ctx, (uint64_t)sp, (uint64_t)(sp_before_send + 1),
              "return 1-arg: SP at receiver slot (arg popped)");

    // Test: return from a 2-arg method
    sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
    fp = (uint64_t *)caller_fp_val;
    ip = caller_ip_val;
    stack_push(&sp, stack, receiver);
    stack_push(&sp, stack, arg0);
    stack_push(&sp, stack, arg1);
    sp_before_send = sp;
    activate_method(&sp, &fp, ip, method, 2, 0);
    frame_return(&sp, &fp, &ip, 55);
    ASSERT_EQ(ctx, stack_top(&sp), 55, "return 2-arg: result on stack");
    ASSERT_EQ(ctx, (uint64_t)fp, caller_fp_val, "return 2-arg: FP restored");
    ASSERT_EQ(ctx, (uint64_t)sp, (uint64_t)(sp_before_send + 2),
              "return 2-arg: SP at receiver slot (both args popped)");

    // --- Section 6: Bytecode Implementations ---

    // Test: PUSH_SELF (bytecode 3)
    sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
    fp = 0;
    stack_push(&sp, stack, receiver);
    activate_method(&sp, &fp, fake_ip, method, 0, 0);
    bc_push_self(&sp, &fp);
    ASSERT_EQ(ctx, stack_top(&sp), receiver, "PUSH_SELF: receiver on stack");

    // Test: PUSH_TEMPORARY_VARIABLE (bytecode 2)
    sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
    fp = 0;
    stack_push(&sp, stack, receiver);
    activate_method(&sp, &fp, fake_ip, method, 0, 1);
    frame_store_temp(fp, 0, 0x1234);
    bc_push_temp(&sp, &fp, 0);
    ASSERT_EQ(ctx, stack_top(&sp), 0x1234, "PUSH_TEMP: temp 0 on stack");

    // Test: PUSH_INSTANCE_VARIABLE (bytecode 1)
    // Create a class with 4 inst vars, and an instance
    uint64_t *iv_class = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 3);
    OBJ_FIELD(iv_class, CLASS_SUPERCLASS) = tagged_nil();
    OBJ_FIELD(iv_class, CLASS_METHOD_DICT) = tagged_nil();
    OBJ_FIELD(iv_class, CLASS_INST_SIZE) = tag_smallint(4);
    uint64_t *iv_obj = om_alloc(om, (uint64_t)iv_class, FORMAT_FIELDS, 4);
    OBJ_FIELD(iv_obj, 0) = tag_smallint(10);
    OBJ_FIELD(iv_obj, 1) = tag_smallint(20);
    OBJ_FIELD(iv_obj, 2) = tag_smallint(30);
    OBJ_FIELD(iv_obj, 3) = tag_smallint(40);
    sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
    fp = 0;
    stack_push(&sp, stack, (uint64_t)iv_obj);
    activate_method(&sp, &fp, fake_ip, method, 0, 0);
    bc_push_inst_var(&sp, &fp, 2);
    ASSERT_EQ(ctx, stack_top(&sp), tag_smallint(30), "PUSH_INST_VAR: field 2 on stack");

    // Test: PUSH_LITERAL (bytecode 0)
    // Create a CompiledMethod with 3 literals
    uint64_t *lit_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 2);
    uint64_t *lit_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
    // fields: num_args=0, num_temps=0, literal_count=3, lit0, lit1, lit2, bytecodes
    OBJ_FIELD(lit_cm, CM_PRIMITIVE) = tag_smallint(0);
    OBJ_FIELD(lit_cm, CM_NUM_ARGS) = tag_smallint(0);
    OBJ_FIELD(lit_cm, CM_NUM_TEMPS) = tag_smallint(0);
    uint64_t *_lits_0 = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 3);
    OBJ_FIELD(_lits_0, 0) = tag_smallint(0xAAA);
    OBJ_FIELD(_lits_0, 1) = tag_smallint(0xBBB);
    OBJ_FIELD(_lits_0, 2) = tag_smallint(0xCCC);
    OBJ_FIELD(lit_cm, CM_LITERALS) = (uint64_t)_lits_0;
    OBJ_FIELD(lit_cm, CM_BYTECODES) = (uint64_t)lit_bc;
    sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
    fp = 0;
    stack_push(&sp, stack, receiver);
    activate_method(&sp, &fp, fake_ip, (uint64_t)lit_cm, 0, 0);
    bc_push_literal(&sp, &fp, 1);
    ASSERT_EQ(ctx, stack_top(&sp), tag_smallint(0xBBB), "PUSH_LITERAL: literal 1 on stack");

    // Test: STORE_TEMPORARY_VARIABLE (bytecode 5)
    sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
    fp = 0;
    stack_push(&sp, stack, receiver);
    activate_method(&sp, &fp, fake_ip, method, 0, 1);
    stack_push(&sp, stack, 0x5678); // push value to store
    bc_store_temp(&sp, &fp, 0);
    ASSERT_EQ(ctx, frame_temp(fp, 0), 0x5678, "STORE_TEMP: value in temp 0");

    // Test: STORE_INSTANCE_VARIABLE (bytecode 4)
    uint64_t *si_obj = om_alloc(om, (uint64_t)iv_class, FORMAT_FIELDS, 4);
    sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
    fp = 0;
    stack_push(&sp, stack, (uint64_t)si_obj);
    activate_method(&sp, &fp, fake_ip, method, 0, 0);
    stack_push(&sp, stack, tag_smallint(0x9999));
    bc_store_inst_var(&sp, &fp, 1);
    ASSERT_EQ(ctx, OBJ_FIELD(si_obj, 1), tag_smallint(0x9999), "STORE_INST_VAR: value in field 1");

    // Test: RETURN_STACK_TOP (bytecode 7)
    sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
    fp = (uint64_t *)caller_fp_val;
    ip = caller_ip_val;
    stack_push(&sp, stack, receiver);
    activate_method(&sp, &fp, ip, method, 0, 0);
    stack_push(&sp, stack, 42); // push return value
    bc_return_stack_top(&sp, &fp, &ip);
    ASSERT_EQ(ctx, stack_top(&sp), 42, "RETURN_STACK_TOP: result on caller stack");
    ASSERT_EQ(ctx, (uint64_t)fp, caller_fp_val, "RETURN_STACK_TOP: FP restored");
    ASSERT_EQ(ctx, ip, caller_ip_val, "RETURN_STACK_TOP: IP restored");

    // Test: DUPLICATE (bytecode 12)
    sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
    stack_push(&sp, stack, 0x777);
    bc_duplicate(&sp);
    ASSERT_EQ(ctx, stack_top(&sp), 0x777, "DUPLICATE: top is copy");
    stack_pop(&sp);
    ASSERT_EQ(ctx, stack_top(&sp), 0x777, "DUPLICATE: original still there");

    // Test: POP (bytecode 11) via bc_pop
    sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
    stack_push(&sp, stack, 0x111);
    stack_push(&sp, stack, 0x222);
    bc_pop(&sp);
    ASSERT_EQ(ctx, stack_top(&sp), 0x111, "POP: top removed, original remains");

    // --- Section 15: Simulated Execution Scenarios ---

    // Scenario 1: call a method, push self, return → receiver on caller's stack
    // Smalltalk: obj foo   where foo is: ^self
    sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
    fp = (uint64_t *)caller_fp_val;
    ip = caller_ip_val;
    stack_push(&sp, stack, receiver);            // caller pushes receiver
    activate_method(&sp, &fp, ip, method, 0, 0); // send #foo
    bc_push_self(&sp, &fp);                      // pushSelf
    bc_return_stack_top(&sp, &fp, &ip);          // returnStackTop
    ASSERT_EQ(ctx, stack_top(&sp), receiver,
              "scenario: ^self returns receiver to caller");
    ASSERT_EQ(ctx, (uint64_t)fp, caller_fp_val,
              "scenario: ^self restores caller FP");

    // Scenario 2: call with 1 arg, push arg, return → arg on caller's stack
    // Smalltalk: obj foo: x   where foo: is: ^x
    sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
    fp = (uint64_t *)caller_fp_val;
    ip = caller_ip_val;
    stack_push(&sp, stack, receiver);
    stack_push(&sp, stack, 0xAAAA); // push arg
    activate_method(&sp, &fp, ip, method, 1, 0);
    // arg 0 is at FP+2*W, which is frame_arg(fp, 0)
    // To push it, we treat it as a temp access (but it's above the frame)
    // For now we use bc_push_temp won't work for args — we need a different approach
    // Actually, in Cog the bytecode indexes args and temps uniformly.
    // But our current bc_push_temp only accesses below-FP temps.
    // Let's just manually push the arg value for now:
    stack_push(&sp, stack, frame_arg(fp, 0));
    bc_return_stack_top(&sp, &fp, &ip);
    ASSERT_EQ(ctx, stack_top(&sp), 0xAAAA,
              "scenario: ^arg returns arg to caller");

    // Scenario 3: call, store into temp, push temp, return
    // Smalltalk: foo   | t | t := 42. ^t
    sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
    fp = (uint64_t *)caller_fp_val;
    ip = caller_ip_val;
    stack_push(&sp, stack, receiver);
    activate_method(&sp, &fp, ip, method, 0, 1);
    stack_push(&sp, stack, 42);         // pushLiteral 42
    bc_store_temp(&sp, &fp, 0);         // storeTemp 0
    bc_push_temp(&sp, &fp, 0);          // pushTemp 0
    bc_return_stack_top(&sp, &fp, &ip); // returnStackTop
    ASSERT_EQ(ctx, stack_top(&sp), 42,
              "scenario: store+push temp returns 42");

    // Scenario 4: push instance variable, return
    // Smalltalk: foo   ^instVar2   (field index 2)
    uint64_t *s4_obj = om_alloc(om, (uint64_t)iv_class, FORMAT_FIELDS, 4);
    OBJ_FIELD(s4_obj, 0) = tag_smallint(100);
    OBJ_FIELD(s4_obj, 1) = tag_smallint(200);
    OBJ_FIELD(s4_obj, 2) = tag_smallint(300);
    OBJ_FIELD(s4_obj, 3) = tag_smallint(400);
    sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
    fp = (uint64_t *)caller_fp_val;
    ip = caller_ip_val;
    stack_push(&sp, stack, (uint64_t)s4_obj);
    activate_method(&sp, &fp, ip, method, 0, 0);
    bc_push_inst_var(&sp, &fp, 2); // pushInstVar 2
    bc_return_stack_top(&sp, &fp, &ip);
    ASSERT_EQ(ctx, stack_top(&sp), tag_smallint(300),
              "scenario: ^instVar2 returns 300");

    // Scenario 5: nested send — A calls B, B returns, A returns
    // Smalltalk: A>>foo  ^self bar.   B>>bar  ^self
    sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
    fp = (uint64_t *)caller_fp_val;
    ip = caller_ip_val;
    // Enter method A (foo)
    stack_push(&sp, stack, receiver);
    activate_method(&sp, &fp, ip, method, 0, 0);
    uint64_t a_ip = 0x3000; // A's IP at point of send
    // A sends #bar: push self as receiver for bar
    bc_push_self(&sp, &fp);
    // Enter method B (bar)
    activate_method(&sp, &fp, a_ip, method, 0, 0);
    // B executes: ^self
    bc_push_self(&sp, &fp);
    bc_return_stack_top(&sp, &fp, &ip);
    // Now back in A; bar's result (receiver) is on stack
    // A returns that result
    bc_return_stack_top(&sp, &fp, &ip);
    ASSERT_EQ(ctx, stack_top(&sp), receiver,
              "scenario: nested A>>foo calls B>>bar, returns receiver");
    ASSERT_EQ(ctx, (uint64_t)fp, caller_fp_val,
              "scenario: nested send restores original caller FP");


    ctx->smallint_class=smallint_class;
    memcpy(ctx->class_table,class_table,sizeof(ctx->class_table));
}
