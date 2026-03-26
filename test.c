#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

// ARM64 assembly functions
extern void stack_push(uint64_t **sp_ptr, uint64_t *stack_base, uint64_t value);
extern uint64_t stack_pop(uint64_t **sp_ptr);
extern uint64_t stack_top(uint64_t **sp_ptr);

// activate_method(sp_ptr, fp_ptr, saved_ip, method, num_args, num_temps)
// Builds a frame on the stack. Caller must have already pushed receiver (and args).
extern void activate_method(uint64_t **sp_ptr, uint64_t **fp_ptr,
                            uint64_t saved_ip, uint64_t method,
                            uint64_t num_args, uint64_t num_temps);

// frame_receiver(fp) -> uint64_t
// Read the receiver from a frame at FP - 4*W.
extern uint64_t frame_receiver(uint64_t *fp);

// frame_method(fp) -> uint64_t
extern uint64_t frame_method(uint64_t *fp);
extern uint64_t frame_flags(uint64_t *fp);
extern uint64_t frame_num_args(uint64_t *fp);
extern uint64_t frame_is_block(uint64_t *fp);
extern uint64_t frame_has_context(uint64_t *fp);
extern uint64_t frame_temp(uint64_t *fp, uint64_t index);
extern uint64_t frame_arg(uint64_t *fp, uint64_t index);
extern void frame_store_temp(uint64_t *fp, uint64_t index, uint64_t value);
extern void frame_return(uint64_t **sp_ptr, uint64_t **fp_ptr,
                         uint64_t *ip_ptr, uint64_t return_value);

// Bytecode functions
extern void bc_push_self(uint64_t **sp_ptr, uint64_t **fp_ptr);
extern void bc_push_temp(uint64_t **sp_ptr, uint64_t **fp_ptr, uint64_t index);
extern void bc_push_inst_var(uint64_t **sp_ptr, uint64_t **fp_ptr, uint64_t field_index);
extern void bc_push_literal(uint64_t **sp_ptr, uint64_t **fp_ptr, uint64_t literal_index);
extern void bc_store_temp(uint64_t **sp_ptr, uint64_t **fp_ptr, uint64_t index);
extern void bc_store_inst_var(uint64_t **sp_ptr, uint64_t **fp_ptr, uint64_t field_index);
extern void bc_return_stack_top(uint64_t **sp_ptr, uint64_t **fp_ptr, uint64_t *ip_ptr);
extern void bc_duplicate(uint64_t **sp_ptr);
extern void bc_pop(uint64_t **sp_ptr);

// Tagged pointer functions
extern uint64_t tag_smallint(int64_t value);
extern int64_t untag_smallint(uint64_t tagged);
extern uint64_t get_tag(uint64_t value);
extern uint64_t is_smallint(uint64_t value);
extern uint64_t is_object_ptr(uint64_t value);
extern uint64_t is_immediate_float(uint64_t value);
extern uint64_t is_special(uint64_t value);
extern uint64_t tagged_nil(void);
extern uint64_t tagged_true(void);
extern uint64_t tagged_false(void);
extern uint64_t is_nil(uint64_t value);
extern uint64_t is_boolean(uint64_t value);
extern uint64_t smallint_add(uint64_t a, uint64_t b);
extern uint64_t smallint_sub(uint64_t a, uint64_t b);
extern uint64_t smallint_less_than(uint64_t a, uint64_t b);
extern uint64_t smallint_equal(uint64_t a, uint64_t b);

// Object memory
extern void om_init(void *buffer, uint64_t size_bytes, uint64_t *free_ptr_var);
extern uint64_t *om_alloc(uint64_t *free_ptr_var, uint64_t class_ptr,
                          uint64_t format, uint64_t size);

#define OBJ_CLASS(obj) ((obj)[0])
#define OBJ_FORMAT(obj) ((obj)[1])
#define OBJ_SIZE(obj) ((obj)[2])
#define OBJ_FIELD(obj, n) ((obj)[3 + (n)])
#define FORMAT_FIELDS 0
#define FORMAT_INDEXABLE 1
#define FORMAT_BYTES 2

// Frame layout offsets from FP (in words, multiply by 8 for bytes)
#define FRAME_SAVED_IP 1  // FP + 1*W
#define FRAME_SAVED_FP 0  // FP + 0
#define FRAME_METHOD -1   // FP - 1*W
#define FRAME_FLAGS -2    // FP - 2*W
#define FRAME_CONTEXT -3  // FP - 3*W
#define FRAME_RECEIVER -4 // FP - 4*W
#define FRAME_TEMP0 -5    // FP - 5*W

#define STACK_WORDS 64
#define ASSERT_EQ(a, b, msg)                                             \
    do                                                                   \
    {                                                                    \
        uint64_t _a = (a), _b = (b);                                     \
        if (_a != _b)                                                    \
        {                                                                \
            printf("FAIL: %s (expected %llu, got %llu)\n", msg, _b, _a); \
            failures++;                                                  \
        }                                                                \
        else                                                             \
        {                                                                \
            printf("PASS: %s\n", msg);                                   \
            passes++;                                                    \
        }                                                                \
    } while (0)

int main()
{
    int passes = 0, failures = 0;

    // Allocate stack memory
    uint64_t stack[STACK_WORDS];
    // SP starts at top (one past end), grows down
    uint64_t *sp = (uint64_t *)((uint8_t *)stack + sizeof(stack));

    // Test: push a value onto a stack and read it back
    stack_push(&sp, stack, 42);
    ASSERT_EQ(stack_top(&sp), 42, "push a value and read it back");

    // Test: push two values and pop one, reading the remaining top
    stack_push(&sp, stack, 100);
    stack_push(&sp, stack, 200);
    stack_pop(&sp);
    ASSERT_EQ(stack_top(&sp), 100, "push two values and pop one");

    // --- Method Activation Tests ---
    // Reset stack for activation tests
    sp = (uint64_t *)((uint8_t *)stack + sizeof(stack));
    uint64_t *fp = 0;

    // Test: activate a method with 0 args, 0 temps
    uint64_t receiver = 0xBEEF;
    uint64_t fake_ip = 0x1000;
    uint64_t fake_method = 0x2000;

    stack_push(&sp, stack, receiver); // caller pushes receiver
    activate_method(&sp, &fp, fake_ip, fake_method, 0, 0);

    ASSERT_EQ(fp[FRAME_SAVED_IP], fake_ip, "activate 0/0: saved IP");
    ASSERT_EQ(fp[FRAME_SAVED_FP], 0, "activate 0/0: saved caller FP (was null)");
    ASSERT_EQ(fp[FRAME_METHOD], fake_method, "activate 0/0: method");
    ASSERT_EQ(fp[FRAME_FLAGS], 0, "activate 0/0: flags (all zero)");
    ASSERT_EQ(fp[FRAME_CONTEXT], 0, "activate 0/0: context slot (nil)");
    ASSERT_EQ(fp[FRAME_RECEIVER], receiver, "activate 0/0: receiver");
    ASSERT_EQ((uint64_t)sp, (uint64_t)&fp[FRAME_RECEIVER],
              "activate 0/0: SP points at receiver (last pushed)");

    // Test: read receiver from frame via ARM64 function
    ASSERT_EQ(frame_receiver(fp), receiver, "frame_receiver reads receiver at FP-4*W");

    // Test: activate with 0 temps (exercises cbz early-out)
    // Already tested above as activate 0/0

    // Test: activate with 1 temp (exercises odd path)
    sp = (uint64_t *)((uint8_t *)stack + sizeof(stack));
    fp = 0;
    stack_push(&sp, stack, receiver);
    activate_method(&sp, &fp, fake_ip, fake_method, 0, 1);
    ASSERT_EQ(fp[FRAME_RECEIVER], receiver, "activate 0/1: receiver");
    ASSERT_EQ(fp[FRAME_TEMP0], 0, "activate 0/1: temp 0 initialized to 0");
    ASSERT_EQ((uint64_t)sp, (uint64_t)&fp[FRAME_TEMP0],
              "activate 0/1: SP points at temp 0");

    // Test: activate with 2 temps (exercises pairs path, even count)
    sp = (uint64_t *)((uint8_t *)stack + sizeof(stack));
    fp = 0;
    stack_push(&sp, stack, receiver);
    activate_method(&sp, &fp, fake_ip, fake_method, 0, 2);
    ASSERT_EQ(fp[FRAME_TEMP0], 0, "activate 0/2: temp 0 initialized to 0");
    ASSERT_EQ(fp[FRAME_TEMP0 - 1], 0, "activate 0/2: temp 1 initialized to 0");
    ASSERT_EQ((uint64_t)sp, (uint64_t)&fp[FRAME_TEMP0 - 1],
              "activate 0/2: SP points at temp 1");

    // Test: activate with 3 temps (exercises odd + pairs path)
    sp = (uint64_t *)((uint8_t *)stack + sizeof(stack));
    fp = 0;
    stack_push(&sp, stack, receiver);
    activate_method(&sp, &fp, fake_ip, fake_method, 0, 3);
    ASSERT_EQ(fp[FRAME_TEMP0], 0, "activate 0/3: temp 0 initialized to 0");
    ASSERT_EQ(fp[FRAME_TEMP0 - 1], 0, "activate 0/3: temp 1 initialized to 0");
    ASSERT_EQ(fp[FRAME_TEMP0 - 2], 0, "activate 0/3: temp 2 initialized to 0");
    ASSERT_EQ((uint64_t)sp, (uint64_t)&fp[FRAME_TEMP0 - 2],
              "activate 0/3: SP points at temp 2");

    // Test: activate with 1 arg, 0 temps: arg accessible above frame
    sp = (uint64_t *)((uint8_t *)stack + sizeof(stack));
    fp = 0;
    uint64_t arg0 = 0xAAAA;
    stack_push(&sp, stack, receiver); // caller pushes receiver
    stack_push(&sp, stack, arg0);     // caller pushes arg 0
    activate_method(&sp, &fp, fake_ip, fake_method, 1, 0);
    ASSERT_EQ(fp[FRAME_RECEIVER], receiver, "activate 1/0: receiver");
    // arg 0 is at FP + 2*W (above saved IP)
    ASSERT_EQ(fp[2], arg0, "activate 1/0: arg 0 at FP+2*W");
    // flags should encode num_args=1 in byte 1
    ASSERT_EQ(fp[FRAME_FLAGS] & 0xFF00, 1 << 8, "activate 1/0: flags encode num_args=1");

    // Test: activate with 2 args, 1 temp: verify args and temp layout
    sp = (uint64_t *)((uint8_t *)stack + sizeof(stack));
    fp = 0;
    uint64_t arg1 = 0xBBBB;
    stack_push(&sp, stack, receiver); // caller pushes receiver
    stack_push(&sp, stack, arg0);     // caller pushes arg 0
    stack_push(&sp, stack, arg1);     // caller pushes arg 1
    activate_method(&sp, &fp, fake_ip, fake_method, 2, 1);
    ASSERT_EQ(fp[FRAME_RECEIVER], receiver, "activate 2/1: receiver");
    // Args are in stack order: last pushed (arg1) is closest to frame
    ASSERT_EQ(fp[2], arg1, "activate 2/1: arg 1 at FP+2*W (last pushed)");
    ASSERT_EQ(fp[3], arg0, "activate 2/1: arg 0 at FP+3*W (first pushed)");
    ASSERT_EQ(fp[FRAME_TEMP0], 0, "activate 2/1: temp 0 initialized to 0");
    ASSERT_EQ(fp[FRAME_FLAGS] & 0xFF00, 2 << 8, "activate 2/1: flags encode num_args=2");

    // Test: read method from frame at FP - 1*W
    ASSERT_EQ(frame_method(fp), fake_method, "frame_method reads method at FP-1*W");

    // Test: read flags from frame at FP - 2*W
    ASSERT_EQ(frame_flags(fp), 2 << 8, "frame_flags reads flags (num_args=2)");

    // Test: decode num_args from flags byte 1
    ASSERT_EQ(frame_num_args(fp), 2, "frame_num_args decodes 2");

    // Test: decode is_block from flags byte 2
    ASSERT_EQ(frame_is_block(fp), 0, "frame_is_block is 0 for method");

    // Test: decode has_context from flags byte 0
    ASSERT_EQ(frame_has_context(fp), 0, "frame_has_context is 0 initially");

    // --- Section 4: Temporary Variable Access ---
    // Use the 2-arg, 1-temp frame from above (still in fp)

    // Test: access temp 0 at FP - 5*W
    ASSERT_EQ(frame_temp(fp, 0), 0, "frame_temp(0) reads temp 0 (was 0)");

    // Test: access arg 0 (last pushed = arg1) at FP + 2*W
    ASSERT_EQ(frame_arg(fp, 0), arg1, "frame_arg(0) reads arg1 (last pushed)");

    // Test: access arg 1 (first pushed = arg0) at FP + 3*W
    ASSERT_EQ(frame_arg(fp, 1), arg0, "frame_arg(1) reads arg0 (first pushed)");

    // Test: store into temp 0 and read it back
    frame_store_temp(fp, 0, 0xDEAD);
    ASSERT_EQ(frame_temp(fp, 0), 0xDEAD, "store_temp(0) then frame_temp(0)");

    // --- Section 5: Return ---

    // Test: return from a 0-arg method
    sp = (uint64_t *)((uint8_t *)stack + sizeof(stack));
    fp = 0;
    uint64_t ip = 0;
    uint64_t caller_fp_val = 0xCAFE;
    uint64_t caller_ip_val = 0xF00D;
    // Simulate caller: set FP to a "caller frame" value, then do a send
    fp = (uint64_t *)caller_fp_val;   // fake caller FP
    ip = caller_ip_val;               // fake caller IP
    stack_push(&sp, stack, receiver); // push receiver
    activate_method(&sp, &fp, ip, fake_method, 0, 0);
    // Now return with value 99
    frame_return(&sp, &fp, &ip, 99);
    ASSERT_EQ(stack_top(&sp), 99, "return 0-arg: result on stack");
    ASSERT_EQ((uint64_t)fp, caller_fp_val, "return 0-arg: FP restored");
    ASSERT_EQ(ip, caller_ip_val, "return 0-arg: IP restored");

    // Test: return from a 1-arg method
    sp = (uint64_t *)((uint8_t *)stack + sizeof(stack));
    fp = (uint64_t *)caller_fp_val;
    ip = caller_ip_val;
    stack_push(&sp, stack, receiver);
    stack_push(&sp, stack, arg0);
    uint64_t *sp_before_send = sp;
    activate_method(&sp, &fp, ip, fake_method, 1, 0);
    frame_return(&sp, &fp, &ip, 77);
    ASSERT_EQ(stack_top(&sp), 77, "return 1-arg: result on stack");
    ASSERT_EQ((uint64_t)fp, caller_fp_val, "return 1-arg: FP restored");
    // SP should point where receiver was (arg consumed)
    ASSERT_EQ((uint64_t)sp, (uint64_t)(sp_before_send + 1),
              "return 1-arg: SP at receiver slot (arg popped)");

    // Test: return from a 2-arg method
    sp = (uint64_t *)((uint8_t *)stack + sizeof(stack));
    fp = (uint64_t *)caller_fp_val;
    ip = caller_ip_val;
    stack_push(&sp, stack, receiver);
    stack_push(&sp, stack, arg0);
    stack_push(&sp, stack, arg1);
    sp_before_send = sp;
    activate_method(&sp, &fp, ip, fake_method, 2, 0);
    frame_return(&sp, &fp, &ip, 55);
    ASSERT_EQ(stack_top(&sp), 55, "return 2-arg: result on stack");
    ASSERT_EQ((uint64_t)fp, caller_fp_val, "return 2-arg: FP restored");
    ASSERT_EQ((uint64_t)sp, (uint64_t)(sp_before_send + 2),
              "return 2-arg: SP at receiver slot (both args popped)");

    // --- Section 6: Bytecode Implementations ---

    // Test: PUSH_SELF (bytecode 3)
    sp = (uint64_t *)((uint8_t *)stack + sizeof(stack));
    fp = 0;
    stack_push(&sp, stack, receiver);
    activate_method(&sp, &fp, fake_ip, fake_method, 0, 0);
    bc_push_self(&sp, &fp);
    ASSERT_EQ(stack_top(&sp), receiver, "PUSH_SELF: receiver on stack");

    // Test: PUSH_TEMPORARY_VARIABLE (bytecode 2)
    sp = (uint64_t *)((uint8_t *)stack + sizeof(stack));
    fp = 0;
    stack_push(&sp, stack, receiver);
    activate_method(&sp, &fp, fake_ip, fake_method, 0, 1);
    frame_store_temp(fp, 0, 0x1234);
    bc_push_temp(&sp, &fp, 0);
    ASSERT_EQ(stack_top(&sp), 0x1234, "PUSH_TEMP: temp 0 on stack");

    // Test: PUSH_INSTANCE_VARIABLE (bytecode 1)
    // Receiver is a pointer to an array of fields
    uint64_t obj_fields[4] = {0x10, 0x20, 0x30, 0x40};
    sp = (uint64_t *)((uint8_t *)stack + sizeof(stack));
    fp = 0;
    stack_push(&sp, stack, (uint64_t)obj_fields);
    activate_method(&sp, &fp, fake_ip, fake_method, 0, 0);
    bc_push_inst_var(&sp, &fp, 2);
    ASSERT_EQ(stack_top(&sp), 0x30, "PUSH_INST_VAR: field 2 on stack");

    // Test: PUSH_LITERAL (bytecode 0)
    // Method pointer is treated as array of literals
    uint64_t literals[3] = {0xAAA, 0xBBB, 0xCCC};
    sp = (uint64_t *)((uint8_t *)stack + sizeof(stack));
    fp = 0;
    stack_push(&sp, stack, receiver);
    activate_method(&sp, &fp, fake_ip, (uint64_t)literals, 0, 0);
    bc_push_literal(&sp, &fp, 1);
    ASSERT_EQ(stack_top(&sp), 0xBBB, "PUSH_LITERAL: literal 1 on stack");

    // Test: STORE_TEMPORARY_VARIABLE (bytecode 5)
    sp = (uint64_t *)((uint8_t *)stack + sizeof(stack));
    fp = 0;
    stack_push(&sp, stack, receiver);
    activate_method(&sp, &fp, fake_ip, fake_method, 0, 1);
    stack_push(&sp, stack, 0x5678); // push value to store
    bc_store_temp(&sp, &fp, 0);
    ASSERT_EQ(frame_temp(fp, 0), 0x5678, "STORE_TEMP: value in temp 0");

    // Test: STORE_INSTANCE_VARIABLE (bytecode 4)
    uint64_t obj2_fields[3] = {0, 0, 0};
    sp = (uint64_t *)((uint8_t *)stack + sizeof(stack));
    fp = 0;
    stack_push(&sp, stack, (uint64_t)obj2_fields);
    activate_method(&sp, &fp, fake_ip, fake_method, 0, 0);
    stack_push(&sp, stack, 0x9999);
    bc_store_inst_var(&sp, &fp, 1);
    ASSERT_EQ(obj2_fields[1], 0x9999, "STORE_INST_VAR: value in field 1");

    // Test: RETURN_STACK_TOP (bytecode 7)
    sp = (uint64_t *)((uint8_t *)stack + sizeof(stack));
    fp = (uint64_t *)caller_fp_val;
    ip = caller_ip_val;
    stack_push(&sp, stack, receiver);
    activate_method(&sp, &fp, ip, fake_method, 0, 0);
    stack_push(&sp, stack, 42); // push return value
    bc_return_stack_top(&sp, &fp, &ip);
    ASSERT_EQ(stack_top(&sp), 42, "RETURN_STACK_TOP: result on caller stack");
    ASSERT_EQ((uint64_t)fp, caller_fp_val, "RETURN_STACK_TOP: FP restored");
    ASSERT_EQ(ip, caller_ip_val, "RETURN_STACK_TOP: IP restored");

    // Test: DUPLICATE (bytecode 12)
    sp = (uint64_t *)((uint8_t *)stack + sizeof(stack));
    stack_push(&sp, stack, 0x777);
    bc_duplicate(&sp);
    ASSERT_EQ(stack_top(&sp), 0x777, "DUPLICATE: top is copy");
    stack_pop(&sp);
    ASSERT_EQ(stack_top(&sp), 0x777, "DUPLICATE: original still there");

    // Test: POP (bytecode 11) via bc_pop
    sp = (uint64_t *)((uint8_t *)stack + sizeof(stack));
    stack_push(&sp, stack, 0x111);
    stack_push(&sp, stack, 0x222);
    bc_pop(&sp);
    ASSERT_EQ(stack_top(&sp), 0x111, "POP: top removed, original remains");

    // --- Section 15: Simulated Execution Scenarios ---

    // Scenario 1: call a method, push self, return → receiver on caller's stack
    // Smalltalk: obj foo   where foo is: ^self
    sp = (uint64_t *)((uint8_t *)stack + sizeof(stack));
    fp = (uint64_t *)caller_fp_val;
    ip = caller_ip_val;
    stack_push(&sp, stack, receiver);                 // caller pushes receiver
    activate_method(&sp, &fp, ip, fake_method, 0, 0); // send #foo
    bc_push_self(&sp, &fp);                           // pushSelf
    bc_return_stack_top(&sp, &fp, &ip);               // returnStackTop
    ASSERT_EQ(stack_top(&sp), receiver,
              "scenario: ^self returns receiver to caller");
    ASSERT_EQ((uint64_t)fp, caller_fp_val,
              "scenario: ^self restores caller FP");

    // Scenario 2: call with 1 arg, push arg, return → arg on caller's stack
    // Smalltalk: obj foo: x   where foo: is: ^x
    sp = (uint64_t *)((uint8_t *)stack + sizeof(stack));
    fp = (uint64_t *)caller_fp_val;
    ip = caller_ip_val;
    stack_push(&sp, stack, receiver);
    stack_push(&sp, stack, 0xAAAA); // push arg
    activate_method(&sp, &fp, ip, fake_method, 1, 0);
    // arg 0 is at FP+2*W, which is frame_arg(fp, 0)
    // To push it, we treat it as a temp access (but it's above the frame)
    // For now we use bc_push_temp won't work for args — we need a different approach
    // Actually, in Cog the bytecode indexes args and temps uniformly.
    // But our current bc_push_temp only accesses below-FP temps.
    // Let's just manually push the arg value for now:
    stack_push(&sp, stack, frame_arg(fp, 0));
    bc_return_stack_top(&sp, &fp, &ip);
    ASSERT_EQ(stack_top(&sp), 0xAAAA,
              "scenario: ^arg returns arg to caller");

    // Scenario 3: call, store into temp, push temp, return
    // Smalltalk: foo   | t | t := 42. ^t
    sp = (uint64_t *)((uint8_t *)stack + sizeof(stack));
    fp = (uint64_t *)caller_fp_val;
    ip = caller_ip_val;
    stack_push(&sp, stack, receiver);
    activate_method(&sp, &fp, ip, fake_method, 0, 1);
    stack_push(&sp, stack, 42);         // pushLiteral 42
    bc_store_temp(&sp, &fp, 0);         // storeTemp 0
    bc_push_temp(&sp, &fp, 0);          // pushTemp 0
    bc_return_stack_top(&sp, &fp, &ip); // returnStackTop
    ASSERT_EQ(stack_top(&sp), 42,
              "scenario: store+push temp returns 42");

    // Scenario 4: push instance variable, return
    // Smalltalk: foo   ^instVar2   (field index 2)
    uint64_t obj3_fields[4] = {100, 200, 300, 400};
    sp = (uint64_t *)((uint8_t *)stack + sizeof(stack));
    fp = (uint64_t *)caller_fp_val;
    ip = caller_ip_val;
    stack_push(&sp, stack, (uint64_t)obj3_fields);
    activate_method(&sp, &fp, ip, fake_method, 0, 0);
    bc_push_inst_var(&sp, &fp, 2); // pushInstVar 2
    bc_return_stack_top(&sp, &fp, &ip);
    ASSERT_EQ(stack_top(&sp), 300,
              "scenario: ^instVar2 returns 300");

    // Scenario 5: nested send — A calls B, B returns, A returns
    // Smalltalk: A>>foo  ^self bar.   B>>bar  ^self
    sp = (uint64_t *)((uint8_t *)stack + sizeof(stack));
    fp = (uint64_t *)caller_fp_val;
    ip = caller_ip_val;
    // Enter method A (foo)
    stack_push(&sp, stack, receiver);
    activate_method(&sp, &fp, ip, fake_method, 0, 0);
    uint64_t a_ip = 0x3000; // A's IP at point of send
    // A sends #bar: push self as receiver for bar
    bc_push_self(&sp, &fp);
    // Enter method B (bar)
    activate_method(&sp, &fp, a_ip, fake_method, 0, 0);
    // B executes: ^self
    bc_push_self(&sp, &fp);
    bc_return_stack_top(&sp, &fp, &ip);
    // Now back in A; bar's result (receiver) is on stack
    // A returns that result
    bc_return_stack_top(&sp, &fp, &ip);
    ASSERT_EQ(stack_top(&sp), receiver,
              "scenario: nested A>>foo calls B>>bar, returns receiver");
    ASSERT_EQ((uint64_t)fp, caller_fp_val,
              "scenario: nested send restores original caller FP");

    // --- Section 7: Tagged Pointers ---

    // Test: encode SmallInteger 0 and decode it back
    uint64_t tagged = tag_smallint(0);
    ASSERT_EQ(untag_smallint(tagged), 0, "SmallInt 0: encode/decode roundtrip");

    // Test: encode SmallInteger 42 and decode it back
    tagged = tag_smallint(42);
    ASSERT_EQ(untag_smallint(tagged), 42, "SmallInt 42: encode/decode roundtrip");
    ASSERT_EQ(tagged, (42ULL << 2) | 1, "SmallInt 42: raw tagged value is 169");

    // Test: encode SmallInteger -1 and decode it back
    tagged = tag_smallint(-1);
    ASSERT_EQ((int64_t)untag_smallint(tagged), -1, "SmallInt -1: encode/decode roundtrip");

    // Test: detect tag: SmallInteger has bits 1:0 == 01
    ASSERT_EQ(get_tag(tag_smallint(42)), 1, "tag of SmallInt is 01");
    ASSERT_EQ(is_smallint(tag_smallint(42)), 1, "is_smallint(SmallInt 42)");

    // Test: detect tag: object pointer has bits 1:0 == 00
    uint64_t aligned_ptr = 0x1000; // 8-byte aligned, tag 00
    ASSERT_EQ(get_tag(aligned_ptr), 0, "tag of object pointer is 00");
    ASSERT_EQ(is_object_ptr(aligned_ptr), 1, "is_object_ptr(0x1000)");
    ASSERT_EQ(is_smallint(aligned_ptr), 0, "is_smallint(obj ptr) is 0");

    // Test: detect tag: immediate float has bits 1:0 == 10
    uint64_t fake_float = 0x42 | 2; // tag 10
    ASSERT_EQ(get_tag(fake_float), 2, "tag of immediate float is 10");
    ASSERT_EQ(is_immediate_float(fake_float), 1, "is_immediate_float");

    // Test: detect tag: special object has bits 1:0 == 11
    ASSERT_EQ(get_tag(tagged_nil()), 3, "tag of nil is 11");
    ASSERT_EQ(is_special(tagged_nil()), 1, "is_special(nil)");

    // Test: nil is the tagged value 0x03
    ASSERT_EQ(tagged_nil(), 0x03, "nil == 0x03");

    // Test: true is the tagged value 0x07
    ASSERT_EQ(tagged_true(), 0x07, "true == 0x07");

    // Test: false is the tagged value 0x0B
    ASSERT_EQ(tagged_false(), 0x0B, "false == 0x0B");

    // Test: is_nil check
    ASSERT_EQ(is_nil(tagged_nil()), 1, "is_nil(nil) == 1");
    ASSERT_EQ(is_nil(tagged_true()), 0, "is_nil(true) == 0");
    ASSERT_EQ(is_nil(tag_smallint(0)), 0, "is_nil(SmallInt 0) == 0");

    // Test: is_boolean check
    ASSERT_EQ(is_boolean(tagged_true()), 1, "is_boolean(true) == 1");
    ASSERT_EQ(is_boolean(tagged_false()), 1, "is_boolean(false) == 1");
    ASSERT_EQ(is_boolean(tagged_nil()), 0, "is_boolean(nil) == 0");
    ASSERT_EQ(is_boolean(tag_smallint(7)), 0, "is_boolean(SmallInt 7) == 0");

    // Test: SmallInteger addition
    uint64_t a = tag_smallint(3);
    uint64_t b = tag_smallint(4);
    uint64_t sum = smallint_add(a, b);
    ASSERT_EQ(untag_smallint(sum), 7, "3 + 4 = 7");
    ASSERT_EQ(is_smallint(sum), 1, "sum is tagged SmallInt");

    // Test: SmallInteger subtraction
    uint64_t diff = smallint_sub(tag_smallint(10), tag_smallint(3));
    ASSERT_EQ(untag_smallint(diff), 7, "10 - 3 = 7");
    ASSERT_EQ(is_smallint(diff), 1, "diff is tagged SmallInt");

    // Test: SmallInteger less-than
    ASSERT_EQ(smallint_less_than(tag_smallint(3), tag_smallint(5)),
              tagged_true(), "3 < 5 is true");
    ASSERT_EQ(smallint_less_than(tag_smallint(5), tag_smallint(3)),
              tagged_false(), "5 < 3 is false");
    ASSERT_EQ(smallint_less_than(tag_smallint(3), tag_smallint(3)),
              tagged_false(), "3 < 3 is false");

    // Test: SmallInteger equality
    ASSERT_EQ(smallint_equal(tag_smallint(42), tag_smallint(42)),
              tagged_true(), "42 = 42 is true");
    ASSERT_EQ(smallint_equal(tag_smallint(42), tag_smallint(43)),
              tagged_false(), "42 = 43 is false");

// --- Section 8: Object Memory ---

// Initialize object memory
#define OM_SIZE 4096
    uint8_t om_buffer[OM_SIZE] __attribute__((aligned(8)));
    uint64_t om[2]; // om[0] = free_ptr, om[1] = end_ptr
    om_init(om_buffer, OM_SIZE, om);
    ASSERT_EQ(om[0], (uint64_t)om_buffer, "om_init: free ptr at buffer start");
    ASSERT_EQ(om[1], (uint64_t)(om_buffer + OM_SIZE), "om_init: end ptr correct");

    // Allocate an object with 0 fields
    uint64_t fake_class = 0x100; // placeholder class pointer
    uint64_t *obj0 = om_alloc(om, fake_class, FORMAT_FIELDS, 0);
    ASSERT_EQ((uint64_t)obj0, (uint64_t)om_buffer, "alloc 0 fields: ptr at buffer start");
    ASSERT_EQ((uint64_t)obj0 % 8, 0, "alloc 0 fields: aligned");
    // free ptr advanced by 3 header words = 24 bytes
    ASSERT_EQ(om[0], (uint64_t)om_buffer + 24, "alloc 0 fields: free ptr advanced by 24");

    printf("\n%d passed, %d failed\n", passes, failures);
    return failures > 0 ? 1 : 0;
}
