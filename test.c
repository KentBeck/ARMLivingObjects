#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

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

// Class field indices
#define CLASS_SUPERCLASS 0
#define CLASS_METHOD_DICT 1
#define CLASS_INST_SIZE 2

// CompiledMethod field indices (always 5 fields)
#define CM_PRIMITIVE 0
#define CM_NUM_ARGS 1
#define CM_NUM_TEMPS 2
#define CM_LITERALS 3  // pointer to Array object (or tagged nil if none)
#define CM_BYTECODES 4 // pointer to ByteArray object

// Primitive indices
#define PRIM_NONE 0
#define PRIM_SMALLINT_ADD 1
#define PRIM_SMALLINT_SUB 2
#define PRIM_SMALLINT_LT 3
#define PRIM_SMALLINT_EQ 4
#define PRIM_SMALLINT_MUL 5
#define PRIM_AT 6
#define PRIM_AT_PUT 7
#define PRIM_NEW 8
#define PRIM_BLOCK_VALUE 9

// Class resolution: given any OOP + class table, returns the class pointer
extern uint64_t *oop_class(uint64_t oop, uint64_t *class_table);

// Method dictionary lookup (ARM64)
extern uint64_t md_lookup(uint64_t *method_dict, uint64_t selector);
// Class-based lookup: walks superclass chain
extern uint64_t class_lookup(uint64_t *klass, uint64_t selector);

// Bytecode dispatch loop
// class_table: [0]=SmallInteger, [1]=Block, [2]=True, [3]=False
extern uint64_t interpret(uint64_t **sp_ptr, uint64_t **fp_ptr, uint8_t *ip,
                          uint64_t *class_table, uint64_t *om);

// Block field indices
#define BLOCK_HOME_RECEIVER 0
#define BLOCK_CM 1

// Bytecode opcodes
#define BC_PUSH_LITERAL 0
#define BC_PUSH_INST_VAR 1
#define BC_PUSH_TEMP 2
#define BC_PUSH_SELF 3
#define BC_STORE_INST_VAR 4
#define BC_STORE_TEMP 5
#define BC_SEND_MESSAGE 6
#define BC_RETURN 7
#define BC_JUMP 8
#define BC_JUMP_IF_TRUE 9
#define BC_JUMP_IF_FALSE 10
#define BC_POP 11
#define BC_DUPLICATE 12
#define BC_HALT 13
#define BC_PUSH_CLOSURE 14
#define BC_PUSH_ARG 15

// Frame layout offsets from FP (in words, multiply by 8 for bytes)
#define FRAME_SAVED_IP 1  // FP + 1*W
#define FRAME_SAVED_FP 0  // FP + 0
#define FRAME_METHOD -1   // FP - 1*W
#define FRAME_FLAGS -2    // FP - 2*W
#define FRAME_CONTEXT -3  // FP - 3*W
#define FRAME_RECEIVER -4 // FP - 4*W
#define FRAME_TEMP0 -5    // FP - 5*W

#define STACK_WORDS 4096
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

void debug_mnu(uint64_t selector)
{
    fprintf(stderr, "MNU: selector=%lld (untagged=%lld)\n", selector, selector >> 2);
}

void debug_oom(void)
{
    fprintf(stderr, "OOM: out of object memory\n");
}

void debug_unknown_prim(uint64_t prim_index)
{
    fprintf(stderr, "UNKNOWN PRIM: %lld\n", prim_index);
}

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

    // --- Initialize Object Memory early so all tests can use real objects ---
#define OM_SIZE (1024 * 1024)
    static uint8_t om_buffer[OM_SIZE] __attribute__((aligned(8)));
    uint64_t om[2]; // om[0] = free_ptr, om[1] = end_ptr
    om_init(om_buffer, OM_SIZE, om);

    // Bootstrap Class class (self-referential)
    uint64_t *class_class = om_alloc(om, 0, FORMAT_FIELDS, 3);
    OBJ_CLASS(class_class) = (uint64_t)class_class;
    OBJ_FIELD(class_class, CLASS_SUPERCLASS) = tagged_nil();
    OBJ_FIELD(class_class, CLASS_METHOD_DICT) = tagged_nil();
    OBJ_FIELD(class_class, CLASS_INST_SIZE) = tag_smallint(3);

    // SmallInteger class (methods added later in Section 12)
    uint64_t *smallint_class = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 3);
    OBJ_FIELD(smallint_class, CLASS_SUPERCLASS) = tagged_nil();
    OBJ_FIELD(smallint_class, CLASS_METHOD_DICT) = tagged_nil();
    OBJ_FIELD(smallint_class, CLASS_INST_SIZE) = tag_smallint(0);

    // Block class
    uint64_t *block_class = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 3);
    OBJ_FIELD(block_class, CLASS_SUPERCLASS) = tagged_nil();
    OBJ_FIELD(block_class, CLASS_METHOD_DICT) = tagged_nil(); // value method added later
    OBJ_FIELD(block_class, CLASS_INST_SIZE) = tag_smallint(2);

    // Class table for dispatch loop
    uint64_t class_table[4];
    class_table[0] = (uint64_t)smallint_class;
    class_table[1] = (uint64_t)block_class;

    // Create a simple class for test objects (0 inst vars)
    uint64_t *test_class = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 3);
    OBJ_FIELD(test_class, CLASS_SUPERCLASS) = tagged_nil();
    OBJ_FIELD(test_class, CLASS_METHOD_DICT) = tagged_nil();
    OBJ_FIELD(test_class, CLASS_INST_SIZE) = tag_smallint(0);

    // Create a real receiver object (0 fields for now)
    uint64_t *recv_obj = om_alloc(om, (uint64_t)test_class, FORMAT_FIELDS, 0);
    uint64_t receiver = (uint64_t)recv_obj;

    // Create a real CompiledMethod for tests (0 args, 0 temps, 0 literals)
    uint64_t *test_bytecodes = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 2);
    uint8_t *test_bc = (uint8_t *)&OBJ_FIELD(test_bytecodes, 0);
    test_bc[0] = 3; // PUSH_SELF
    test_bc[1] = 7; // RETURN_STACK_TOP
    uint64_t *test_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
    OBJ_FIELD(test_cm, CM_PRIMITIVE) = tag_smallint(0);
    OBJ_FIELD(test_cm, CM_NUM_ARGS) = tag_smallint(0);
    OBJ_FIELD(test_cm, CM_NUM_TEMPS) = tag_smallint(0);
    OBJ_FIELD(test_cm, CM_LITERALS) = tagged_nil();
    OBJ_FIELD(test_cm, CM_BYTECODES) = (uint64_t)test_bytecodes;
    uint64_t method = (uint64_t)test_cm;
    uint64_t fake_ip = 0x1000;

    // --- Method Activation Tests ---
    // Reset stack for activation tests
    sp = (uint64_t *)((uint8_t *)stack + sizeof(stack));
    uint64_t *fp = 0;

    stack_push(&sp, stack, receiver); // caller pushes receiver
    activate_method(&sp, &fp, fake_ip, method, 0, 0);

    ASSERT_EQ(fp[FRAME_SAVED_IP], fake_ip, "activate 0/0: saved IP");
    ASSERT_EQ(fp[FRAME_SAVED_FP], 0, "activate 0/0: saved caller FP (was null)");
    ASSERT_EQ(fp[FRAME_METHOD], method, "activate 0/0: method");
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
    activate_method(&sp, &fp, fake_ip, method, 0, 1);
    ASSERT_EQ(fp[FRAME_RECEIVER], receiver, "activate 0/1: receiver");
    ASSERT_EQ(fp[FRAME_TEMP0], 0, "activate 0/1: temp 0 initialized to 0");
    ASSERT_EQ((uint64_t)sp, (uint64_t)&fp[FRAME_TEMP0],
              "activate 0/1: SP points at temp 0");

    // Test: activate with 2 temps (exercises pairs path, even count)
    sp = (uint64_t *)((uint8_t *)stack + sizeof(stack));
    fp = 0;
    stack_push(&sp, stack, receiver);
    activate_method(&sp, &fp, fake_ip, method, 0, 2);
    ASSERT_EQ(fp[FRAME_TEMP0], 0, "activate 0/2: temp 0 initialized to 0");
    ASSERT_EQ(fp[FRAME_TEMP0 - 1], 0, "activate 0/2: temp 1 initialized to 0");
    ASSERT_EQ((uint64_t)sp, (uint64_t)&fp[FRAME_TEMP0 - 1],
              "activate 0/2: SP points at temp 1");

    // Test: activate with 3 temps (exercises odd + pairs path)
    sp = (uint64_t *)((uint8_t *)stack + sizeof(stack));
    fp = 0;
    stack_push(&sp, stack, receiver);
    activate_method(&sp, &fp, fake_ip, method, 0, 3);
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
    activate_method(&sp, &fp, fake_ip, method, 1, 0);
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
    activate_method(&sp, &fp, fake_ip, method, 2, 1);
    ASSERT_EQ(fp[FRAME_RECEIVER], receiver, "activate 2/1: receiver");
    // Args are in stack order: last pushed (arg1) is closest to frame
    ASSERT_EQ(fp[2], arg1, "activate 2/1: arg 1 at FP+2*W (last pushed)");
    ASSERT_EQ(fp[3], arg0, "activate 2/1: arg 0 at FP+3*W (first pushed)");
    ASSERT_EQ(fp[FRAME_TEMP0], 0, "activate 2/1: temp 0 initialized to 0");
    ASSERT_EQ(fp[FRAME_FLAGS] & 0xFF00, 2 << 8, "activate 2/1: flags encode num_args=2");

    // Test: read method from frame at FP - 1*W
    ASSERT_EQ(frame_method(fp), method, "frame_method reads method at FP-1*W");

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
    activate_method(&sp, &fp, ip, method, 0, 0);
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
    activate_method(&sp, &fp, ip, method, 1, 0);
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
    activate_method(&sp, &fp, ip, method, 2, 0);
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
    activate_method(&sp, &fp, fake_ip, method, 0, 0);
    bc_push_self(&sp, &fp);
    ASSERT_EQ(stack_top(&sp), receiver, "PUSH_SELF: receiver on stack");

    // Test: PUSH_TEMPORARY_VARIABLE (bytecode 2)
    sp = (uint64_t *)((uint8_t *)stack + sizeof(stack));
    fp = 0;
    stack_push(&sp, stack, receiver);
    activate_method(&sp, &fp, fake_ip, method, 0, 1);
    frame_store_temp(fp, 0, 0x1234);
    bc_push_temp(&sp, &fp, 0);
    ASSERT_EQ(stack_top(&sp), 0x1234, "PUSH_TEMP: temp 0 on stack");

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
    sp = (uint64_t *)((uint8_t *)stack + sizeof(stack));
    fp = 0;
    stack_push(&sp, stack, (uint64_t)iv_obj);
    activate_method(&sp, &fp, fake_ip, method, 0, 0);
    bc_push_inst_var(&sp, &fp, 2);
    ASSERT_EQ(stack_top(&sp), tag_smallint(30), "PUSH_INST_VAR: field 2 on stack");

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
    sp = (uint64_t *)((uint8_t *)stack + sizeof(stack));
    fp = 0;
    stack_push(&sp, stack, receiver);
    activate_method(&sp, &fp, fake_ip, (uint64_t)lit_cm, 0, 0);
    bc_push_literal(&sp, &fp, 1);
    ASSERT_EQ(stack_top(&sp), tag_smallint(0xBBB), "PUSH_LITERAL: literal 1 on stack");

    // Test: STORE_TEMPORARY_VARIABLE (bytecode 5)
    sp = (uint64_t *)((uint8_t *)stack + sizeof(stack));
    fp = 0;
    stack_push(&sp, stack, receiver);
    activate_method(&sp, &fp, fake_ip, method, 0, 1);
    stack_push(&sp, stack, 0x5678); // push value to store
    bc_store_temp(&sp, &fp, 0);
    ASSERT_EQ(frame_temp(fp, 0), 0x5678, "STORE_TEMP: value in temp 0");

    // Test: STORE_INSTANCE_VARIABLE (bytecode 4)
    uint64_t *si_obj = om_alloc(om, (uint64_t)iv_class, FORMAT_FIELDS, 4);
    sp = (uint64_t *)((uint8_t *)stack + sizeof(stack));
    fp = 0;
    stack_push(&sp, stack, (uint64_t)si_obj);
    activate_method(&sp, &fp, fake_ip, method, 0, 0);
    stack_push(&sp, stack, tag_smallint(0x9999));
    bc_store_inst_var(&sp, &fp, 1);
    ASSERT_EQ(OBJ_FIELD(si_obj, 1), tag_smallint(0x9999), "STORE_INST_VAR: value in field 1");

    // Test: RETURN_STACK_TOP (bytecode 7)
    sp = (uint64_t *)((uint8_t *)stack + sizeof(stack));
    fp = (uint64_t *)caller_fp_val;
    ip = caller_ip_val;
    stack_push(&sp, stack, receiver);
    activate_method(&sp, &fp, ip, method, 0, 0);
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
    stack_push(&sp, stack, receiver);            // caller pushes receiver
    activate_method(&sp, &fp, ip, method, 0, 0); // send #foo
    bc_push_self(&sp, &fp);                      // pushSelf
    bc_return_stack_top(&sp, &fp, &ip);          // returnStackTop
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
    activate_method(&sp, &fp, ip, method, 1, 0);
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
    activate_method(&sp, &fp, ip, method, 0, 1);
    stack_push(&sp, stack, 42);         // pushLiteral 42
    bc_store_temp(&sp, &fp, 0);         // storeTemp 0
    bc_push_temp(&sp, &fp, 0);          // pushTemp 0
    bc_return_stack_top(&sp, &fp, &ip); // returnStackTop
    ASSERT_EQ(stack_top(&sp), 42,
              "scenario: store+push temp returns 42");

    // Scenario 4: push instance variable, return
    // Smalltalk: foo   ^instVar2   (field index 2)
    uint64_t *s4_obj = om_alloc(om, (uint64_t)iv_class, FORMAT_FIELDS, 4);
    OBJ_FIELD(s4_obj, 0) = tag_smallint(100);
    OBJ_FIELD(s4_obj, 1) = tag_smallint(200);
    OBJ_FIELD(s4_obj, 2) = tag_smallint(300);
    OBJ_FIELD(s4_obj, 3) = tag_smallint(400);
    sp = (uint64_t *)((uint8_t *)stack + sizeof(stack));
    fp = (uint64_t *)caller_fp_val;
    ip = caller_ip_val;
    stack_push(&sp, stack, (uint64_t)s4_obj);
    activate_method(&sp, &fp, ip, method, 0, 0);
    bc_push_inst_var(&sp, &fp, 2); // pushInstVar 2
    bc_return_stack_top(&sp, &fp, &ip);
    ASSERT_EQ(stack_top(&sp), tag_smallint(300),
              "scenario: ^instVar2 returns 300");

    // Scenario 5: nested send — A calls B, B returns, A returns
    // Smalltalk: A>>foo  ^self bar.   B>>bar  ^self
    sp = (uint64_t *)((uint8_t *)stack + sizeof(stack));
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

    // --- Section 8: Object Memory (uses om + class_class from bootstrap above) ---

    // Allocate an object with 2 fields: size is correct
    uint64_t *obj2 = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 2);
    ASSERT_EQ(OBJ_SIZE(obj2), 2, "alloc 2 fields: size is 2");

    // Read class pointer from object (word 0)
    ASSERT_EQ(OBJ_CLASS(obj2), (uint64_t)class_class, "read class pointer from object");

    // Read format from object (word 1)
    ASSERT_EQ(OBJ_FORMAT(obj2), FORMAT_FIELDS, "read format from object");

    // Read size from object (word 2)
    ASSERT_EQ(OBJ_SIZE(obj2), 2, "read size from object");

    // Read field 0 from an object (at header + 3*W) — initialized to 0
    ASSERT_EQ(OBJ_FIELD(obj2, 0), 0, "field 0 initialized to 0");

    // Write field 1 of an object
    OBJ_FIELD(obj2, 1) = tag_smallint(0xBEEF);
    ASSERT_EQ(OBJ_FIELD(obj2, 1), tag_smallint(0xBEEF), "write and read field 1");

    // Object pointer has tag 00 (aligned)
    ASSERT_EQ(is_object_ptr((uint64_t)obj2), 1, "object pointer has tag 00");

    // Fields store tagged values
    OBJ_FIELD(obj2, 0) = tag_smallint(42);
    ASSERT_EQ(is_smallint(OBJ_FIELD(obj2, 0)), 1, "field stores tagged SmallInt");
    ASSERT_EQ(untag_smallint(OBJ_FIELD(obj2, 0)), 42, "field SmallInt value is 42");

    // Allocate a fields object (format 0)
    uint64_t *obj_f = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 3);
    ASSERT_EQ(OBJ_FORMAT(obj_f), FORMAT_FIELDS, "fields object: format 0");
    ASSERT_EQ(OBJ_SIZE(obj_f), 3, "fields object: size 3");

    // Allocate an indexable object (format 1)
    uint64_t *obj_i = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 5);
    ASSERT_EQ(OBJ_FORMAT(obj_i), FORMAT_INDEXABLE, "indexable object: format 1");
    ASSERT_EQ(OBJ_SIZE(obj_i), 5, "indexable object: size 5");
    OBJ_FIELD(obj_i, 3) = tag_smallint(99);
    ASSERT_EQ(untag_smallint(OBJ_FIELD(obj_i, 3)), 99, "indexable: store/read slot 3");

    // Allocate a bytes object (format 2)
    uint64_t *obj_b = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 10);
    ASSERT_EQ(OBJ_FORMAT(obj_b), FORMAT_BYTES, "bytes object: format 2");
    ASSERT_EQ(OBJ_SIZE(obj_b), 10, "bytes object: size 10 bytes");
    uint8_t *bytes = (uint8_t *)&OBJ_FIELD(obj_b, 0);
    bytes[0] = 0xAB;
    bytes[9] = 0xCD;
    ASSERT_EQ(bytes[0], 0xAB, "bytes object: write/read byte 0");
    ASSERT_EQ(bytes[9], 0xCD, "bytes object: write/read byte 9");

    // bc_push_inst_var with real object
    uint64_t *obj_recv = om_alloc(om, (uint64_t)iv_class, FORMAT_FIELDS, 4);
    OBJ_FIELD(obj_recv, 0) = tag_smallint(10);
    OBJ_FIELD(obj_recv, 1) = tag_smallint(20);
    OBJ_FIELD(obj_recv, 2) = tag_smallint(30);
    sp = (uint64_t *)((uint8_t *)stack + sizeof(stack));
    fp = 0;
    stack_push(&sp, stack, (uint64_t)obj_recv);
    activate_method(&sp, &fp, fake_ip, method, 0, 0);
    bc_push_inst_var(&sp, &fp, 1);
    ASSERT_EQ(stack_top(&sp), tag_smallint(20),
              "bc_push_inst_var with real object: field 1");

    // bc_store_inst_var with real object
    stack_push(&sp, stack, tag_smallint(99));
    bc_store_inst_var(&sp, &fp, 2);
    ASSERT_EQ(OBJ_FIELD(obj_recv, 2), tag_smallint(99),
              "bc_store_inst_var with real object: field 2");

    // --- Section 9: Class and Method Dictionary ---
    // (class_class already bootstrapped above)

    ASSERT_EQ(OBJ_CLASS(class_class), (uint64_t)class_class,
              "bootstrap: Class class is self-referential");

    // Create a method dictionary (Array) with one (selector, method) pair
    // Selector = tagged SmallInt 1 (symbol index for e.g. #foo)
    uint64_t sel_foo = tag_smallint(1);
    uint64_t *fake_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 1);
    // method dict: 2 slots (1 pair)
    uint64_t *md = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 2);
    OBJ_FIELD(md, 0) = sel_foo;
    OBJ_FIELD(md, 1) = (uint64_t)fake_cm;
    ASSERT_EQ(OBJ_FIELD(md, 0), sel_foo, "method dict: selector stored");
    ASSERT_EQ(OBJ_FIELD(md, 1), (uint64_t)fake_cm, "method dict: method stored");

    // Look up a selector in a method dictionary: found
    uint64_t found = md_lookup(md, sel_foo);
    ASSERT_EQ(found, (uint64_t)fake_cm, "md_lookup: found method for selector");

    // Look up a selector in a method dictionary: not found
    uint64_t sel_bar = tag_smallint(2);
    uint64_t not_found = md_lookup(md, sel_bar);
    ASSERT_EQ(not_found, 0, "md_lookup: not found returns 0");

    // Look up with superclass chain: found in superclass
    // Create a parent class with the method dict
    uint64_t *parent_class = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 3);
    OBJ_FIELD(parent_class, CLASS_SUPERCLASS) = tagged_nil();
    OBJ_FIELD(parent_class, CLASS_METHOD_DICT) = (uint64_t)md;
    OBJ_FIELD(parent_class, CLASS_INST_SIZE) = tag_smallint(0);
    // Create a child class with empty method dict, parent as superclass
    uint64_t *child_class = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 3);
    OBJ_FIELD(child_class, CLASS_SUPERCLASS) = (uint64_t)parent_class;
    OBJ_FIELD(child_class, CLASS_METHOD_DICT) = tagged_nil();
    OBJ_FIELD(child_class, CLASS_INST_SIZE) = tag_smallint(0);
    found = class_lookup(child_class, sel_foo);
    ASSERT_EQ(found, (uint64_t)fake_cm,
              "class_lookup: found in superclass");
    not_found = class_lookup(child_class, sel_bar);
    ASSERT_EQ(not_found, 0,
              "class_lookup: not found in chain returns 0");

    // Create a CompiledMethod with bytecodes and literals
    // CM: 4 fields (num_args, num_temps, literal_count, bytecodes_ptr)
    // + literal_count literal fields
    // For a method with 1 arg, 2 temps, 1 literal:
    // fields: num_args=1, num_temps=2, literal_count=1, literal0, bytecodes
    uint64_t *bytecodes = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 4);
    uint8_t *bc = (uint8_t *)&OBJ_FIELD(bytecodes, 0);
    bc[0] = 3; // PUSH_SELF
    bc[1] = 7; // RETURN_STACK_TOP
    bc[2] = 0; // padding
    bc[3] = 0;

    uint64_t *cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
    OBJ_FIELD(cm, CM_PRIMITIVE) = tag_smallint(0);
    OBJ_FIELD(cm, CM_NUM_ARGS) = tag_smallint(1);
    OBJ_FIELD(cm, CM_NUM_TEMPS) = tag_smallint(2);
    uint64_t *_lits_1 = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 1);
    OBJ_FIELD(_lits_1, 0) = tag_smallint(42);
    OBJ_FIELD(cm, CM_LITERALS) = (uint64_t)_lits_1;
    OBJ_FIELD(cm, CM_BYTECODES) = (uint64_t)bytecodes;

    // Read num_args and num_temps from a CompiledMethod
    ASSERT_EQ(untag_smallint(OBJ_FIELD(cm, CM_NUM_ARGS)), 1,
              "CompiledMethod: num_args = 1");
    ASSERT_EQ(untag_smallint(OBJ_FIELD(cm, CM_NUM_TEMPS)), 2,
              "CompiledMethod: num_temps = 2");
    ASSERT_EQ(OBJ_FIELD(_lits_1, 0), tag_smallint(42),
              "CompiledMethod: literal 0 = 42");
    ASSERT_EQ(OBJ_FIELD(cm, CM_BYTECODES), (uint64_t)bytecodes,
              "CompiledMethod: bytecodes pointer");

    // Look up class from receiver's header, then find method
    // Create an instance of parent_class, look up sel_foo
    uint64_t *instance = om_alloc(om, (uint64_t)parent_class, FORMAT_FIELDS, 0);
    uint64_t *recv_class = (uint64_t *)OBJ_CLASS(instance);
    found = class_lookup(recv_class, sel_foo);
    ASSERT_EQ(found, (uint64_t)fake_cm,
              "lookup from receiver: class -> method dict -> method");

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

        sp = (uint64_t *)((uint8_t *)stack + sizeof(stack));
        fp = 0;
        stack_push(&sp, stack, receiver);
        activate_method(&sp, &fp, 0, (uint64_t)d_cm, 0, 0);
        uint64_t result = interpret(&sp, &fp, dbc, class_table, om);
        ASSERT_EQ(result, tag_smallint(42), "dispatch: PUSH_LITERAL 0 + HALT");
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

        sp = (uint64_t *)((uint8_t *)stack + sizeof(stack));
        fp = (uint64_t *)caller_fp_val;
        stack_push(&sp, stack, receiver);
        activate_method(&sp, &fp, caller_ip_val, (uint64_t)d_cm, 0, 0);
        uint64_t result = interpret(&sp, &fp, dbc, class_table, om);
        ASSERT_EQ(result, tag_smallint(99),
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

        sp = (uint64_t *)((uint8_t *)stack + sizeof(stack));
        fp = (uint64_t *)caller_fp_val;
        stack_push(&sp, stack, receiver);
        activate_method(&sp, &fp, caller_ip_val, (uint64_t)d_cm, 0, 0);
        uint64_t result = interpret(&sp, &fp, dbc, class_table, om);
        ASSERT_EQ(result, receiver, "dispatch: PUSH_SELF + RETURN");
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

        sp = (uint64_t *)((uint8_t *)stack + sizeof(stack));
        fp = 0;
        stack_push(&sp, stack, receiver);
        activate_method(&sp, &fp, 0, (uint64_t)d_cm, 0, 2);
        // Store values into temps manually
        frame_store_temp(fp, 0, tag_smallint(10));
        frame_store_temp(fp, 1, tag_smallint(20));
        uint64_t result = interpret(&sp, &fp, dbc, class_table, om);
        ASSERT_EQ(result, tag_smallint(20), "dispatch: PUSH_TEMP 0, PUSH_TEMP 1 + HALT");
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

        sp = (uint64_t *)((uint8_t *)stack + sizeof(stack));
        fp = (uint64_t *)caller_fp_val;
        stack_push(&sp, stack, receiver);
        activate_method(&sp, &fp, caller_ip_val, (uint64_t)d_cm, 0, 1);
        uint64_t result = interpret(&sp, &fp, dbc, class_table, om);
        ASSERT_EQ(result, tag_smallint(77),
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

        sp = (uint64_t *)((uint8_t *)stack + sizeof(stack));
        fp = 0;
        stack_push(&sp, stack, receiver);
        activate_method(&sp, &fp, 0, (uint64_t)d_cm, 0, 0);
        uint64_t result = interpret(&sp, &fp, dbc, class_table, om);
        ASSERT_EQ(result, tag_smallint(222), "dispatch: JUMP skips to literal 1");
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

        sp = (uint64_t *)((uint8_t *)stack + sizeof(stack));
        fp = 0;
        stack_push(&sp, stack, receiver);
        activate_method(&sp, &fp, 0, (uint64_t)d_cm, 0, 0);
        uint64_t result = interpret(&sp, &fp, dbc, class_table, om);
        ASSERT_EQ(result, tag_smallint(222), "dispatch: JUMP_IF_TRUE with true jumps");
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

        sp = (uint64_t *)((uint8_t *)stack + sizeof(stack));
        fp = 0;
        stack_push(&sp, stack, receiver);
        activate_method(&sp, &fp, 0, (uint64_t)d_cm, 0, 0);
        uint64_t result = interpret(&sp, &fp, dbc, class_table, om);
        ASSERT_EQ(result, tag_smallint(111),
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

        sp = (uint64_t *)((uint8_t *)stack + sizeof(stack));
        fp = 0;
        stack_push(&sp, stack, receiver);
        activate_method(&sp, &fp, 0, (uint64_t)d_cm, 0, 0);
        uint64_t result = interpret(&sp, &fp, dbc, class_table, om);
        ASSERT_EQ(result, tag_smallint(333), "dispatch: JUMP_IF_FALSE with false jumps");
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

        sp = (uint64_t *)((uint8_t *)stack + sizeof(stack));
        fp = 0;
        stack_push(&sp, stack, receiver);
        activate_method(&sp, &fp, 0, (uint64_t)d_cm, 0, 0);
        uint64_t result = interpret(&sp, &fp, dbc, class_table, om);
        ASSERT_EQ(result, tag_smallint(444),
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
        uint64_t *send_class = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 3);
        OBJ_FIELD(send_class, CLASS_SUPERCLASS) = tagged_nil();
        OBJ_FIELD(send_class, CLASS_METHOD_DICT) = (uint64_t)send_md;
        OBJ_FIELD(send_class, CLASS_INST_SIZE) = tag_smallint(0);

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

        sp = (uint64_t *)((uint8_t *)stack + sizeof(stack));
        fp = (uint64_t *)0xCAFE; // caller FP sentinel
        stack_push(&sp, stack, (uint64_t)send_obj);
        activate_method(&sp, &fp, 0, (uint64_t)caller_cm, 0, 0);
        uint64_t result = interpret(&sp, &fp, (uint8_t *)&OBJ_FIELD(caller_bc, 0), class_table, om);
        ASSERT_EQ(result, (uint64_t)send_obj,
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
        uint64_t *pt_class = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 3);
        OBJ_FIELD(pt_class, CLASS_SUPERCLASS) = tagged_nil();
        OBJ_FIELD(pt_class, CLASS_INST_SIZE) = tag_smallint(1);

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

        sp = (uint64_t *)((uint8_t *)stack + sizeof(stack));
        fp = (uint64_t *)0xCAFE;
        stack_push(&sp, stack, (uint64_t)pt_obj);
        activate_method(&sp, &fp, 0, (uint64_t)c2_cm, 0, 0);
        uint64_t result = interpret(&sp, &fp, (uint8_t *)&OBJ_FIELD(c2_bc, 0), class_table, om);
        ASSERT_EQ(result, tag_smallint(42),
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

        uint64_t *wa_class = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 3);
        OBJ_FIELD(wa_class, CLASS_SUPERCLASS) = tagged_nil();
        OBJ_FIELD(wa_class, CLASS_METHOD_DICT) = (uint64_t)wa_md;
        OBJ_FIELD(wa_class, CLASS_INST_SIZE) = tag_smallint(0);

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

        sp = (uint64_t *)((uint8_t *)stack + sizeof(stack));
        fp = (uint64_t *)0xCAFE;
        stack_push(&sp, stack, (uint64_t)wa_obj);
        activate_method(&sp, &fp, 0, (uint64_t)c3_cm, 0, 0);
        uint64_t result = interpret(&sp, &fp,
                                    (uint8_t *)&OBJ_FIELD(c3_bc, 0), class_table, om);
        ASSERT_EQ(result, (uint64_t)wa_obj,
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

        uint64_t *par_class = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 3);
        OBJ_FIELD(par_class, CLASS_SUPERCLASS) = tagged_nil();
        OBJ_FIELD(par_class, CLASS_METHOD_DICT) = (uint64_t)par_md;
        OBJ_FIELD(par_class, CLASS_INST_SIZE) = tag_smallint(0);

        // Child class: empty method dict, superclass = par_class
        uint64_t *child_cls = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 3);
        OBJ_FIELD(child_cls, CLASS_SUPERCLASS) = (uint64_t)par_class;
        OBJ_FIELD(child_cls, CLASS_METHOD_DICT) = tagged_nil();
        OBJ_FIELD(child_cls, CLASS_INST_SIZE) = tag_smallint(0);

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

        sp = (uint64_t *)((uint8_t *)stack + sizeof(stack));
        fp = (uint64_t *)0xCAFE;
        stack_push(&sp, stack, (uint64_t)child_obj);
        activate_method(&sp, &fp, 0, (uint64_t)sc_cm, 0, 0);
        uint64_t result = interpret(&sp, &fp, (uint8_t *)&OBJ_FIELD(sc_bc, 0), class_table, om);
        ASSERT_EQ(result, tag_smallint(999),
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

        sp = (uint64_t *)((uint8_t *)stack + sizeof(stack));
        fp = (uint64_t *)0xCAFE;
        stack_push(&sp, stack, receiver);
        activate_method(&sp, &fp, 0, (uint64_t)add_cm, 0, 0);
        uint64_t result = interpret(&sp, &fp,
                                    (uint8_t *)&OBJ_FIELD(add_bc, 0), class_table, om);
        ASSERT_EQ(result, tag_smallint(7), "primitive: 3 + 4 = 7 via dispatch");

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

        sp = (uint64_t *)((uint8_t *)stack + sizeof(stack));
        fp = (uint64_t *)0xCAFE;
        stack_push(&sp, stack, receiver);
        activate_method(&sp, &fp, 0, (uint64_t)sub_cm2, 0, 0);
        result = interpret(&sp, &fp,
                           (uint8_t *)&OBJ_FIELD(sub_bc2, 0), class_table, om);
        ASSERT_EQ(result, tag_smallint(7), "primitive: 10 - 3 = 7 via dispatch");

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

        sp = (uint64_t *)((uint8_t *)stack + sizeof(stack));
        fp = (uint64_t *)0xCAFE;
        stack_push(&sp, stack, receiver);
        activate_method(&sp, &fp, 0, (uint64_t)lt_cm2, 0, 0);
        result = interpret(&sp, &fp,
                           (uint8_t *)&OBJ_FIELD(lt_bc2, 0), class_table, om);
        ASSERT_EQ(result, tagged_true(), "primitive: 3 < 5 = true via dispatch");

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

        sp = (uint64_t *)((uint8_t *)stack + sizeof(stack));
        fp = (uint64_t *)0xCAFE;
        stack_push(&sp, stack, receiver);
        activate_method(&sp, &fp, 0, (uint64_t)eq_cm2, 0, 0);
        result = interpret(&sp, &fp,
                           (uint8_t *)&OBJ_FIELD(eq_bc2, 0), class_table, om);
        ASSERT_EQ(result, tagged_true(), "primitive: 42 = 42 = true via dispatch");

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

        sp = (uint64_t *)((uint8_t *)stack + sizeof(stack));
        fp = (uint64_t *)0xCAFE;
        stack_push(&sp, stack, receiver);
        activate_method(&sp, &fp, 0, (uint64_t)mul_cm2, 0, 0);
        result = interpret(&sp, &fp,
                           (uint8_t *)&OBJ_FIELD(mul_bc2, 0), class_table, om);
        ASSERT_EQ(result, tag_smallint(42), "primitive: 6 * 7 = 42 via dispatch");
    }

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

        sp = (uint64_t *)((uint8_t *)stack + sizeof(stack));
        fp = (uint64_t *)0xCAFE;
        stack_push(&sp, stack, receiver);
        activate_method(&sp, &fp, 0, (uint64_t)caller_cm2, 0, 0);
        uint64_t result = interpret(&sp, &fp,
                                    (uint8_t *)&OBJ_FIELD(caller_bc2, 0),
                                    class_table, om);
        ASSERT_EQ(result, tag_smallint(77),
                  "Block: PUSH_CLOSURE + send value returns 77");

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

        sp = (uint64_t *)((uint8_t *)stack + sizeof(stack));
        fp = (uint64_t *)0xCAFE;
        stack_push(&sp, stack, receiver);
        activate_method(&sp, &fp, 0, (uint64_t)caller_cm3, 0, 0);
        result = interpret(&sp, &fp,
                           (uint8_t *)&OBJ_FIELD(caller_bc3, 0),
                           class_table, om);
        ASSERT_EQ(result, receiver,
                  "Block: captures self — [self] value returns receiver");
    }

    // --- True and False classes with ifTrue:ifFalse: ---
    {
        uint64_t sel_value = tag_smallint(60);
        uint64_t sel_ifTF = tag_smallint(70); // #ifTrue:ifFalse:

        // True >> ifTrue: aBlock ifFalse: anotherBlock  ^ aBlock value
        // Bytecodes: PUSH_ARG? No — we don't have PUSH_ARG yet.
        // Args are above the frame at FP + (2+i)*8.
        // For a 2-arg method, arg0 (aBlock) is at FP+3*W, arg1 (anotherBlock) at FP+2*W.
        // We need frame_arg(fp, 0) = FP+2*W (most recently pushed), but we want aBlock
        // which was pushed first = FP+3*W = frame_arg(fp, 1).
        //
        // Actually in our calling convention:
        //   stack_push(receiver), stack_push(arg0=aBlock), stack_push(arg1=anotherBlock)
        //   arg0 at FP + 3*W = frame_arg(fp, 1)
        //   arg1 at FP + 2*W = frame_arg(fp, 0)
        //
        // We don't have a PUSH_ARG bytecode. Let's add one: BC_PUSH_ARG (15).
        // For now, use PUSH_TEMP with a negative offset hack? No.
        // Let me just add BC_PUSH_ARG.

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
        uint64_t *true_class = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 3);
        OBJ_FIELD(true_class, CLASS_SUPERCLASS) = tagged_nil();
        OBJ_FIELD(true_class, CLASS_METHOD_DICT) = (uint64_t)true_md;
        OBJ_FIELD(true_class, CLASS_INST_SIZE) = tag_smallint(0);

        // False class
        uint64_t *false_md = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 2);
        OBJ_FIELD(false_md, 0) = sel_ifTF;
        OBJ_FIELD(false_md, 1) = (uint64_t)false_itf_cm;
        uint64_t *false_class = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 3);
        OBJ_FIELD(false_class, CLASS_SUPERCLASS) = tagged_nil();
        OBJ_FIELD(false_class, CLASS_METHOD_DICT) = (uint64_t)false_md;
        OBJ_FIELD(false_class, CLASS_INST_SIZE) = tag_smallint(0);

        // Register in class table
        class_table[2] = (uint64_t)true_class;
        class_table[3] = (uint64_t)false_class;

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

        sp = (uint64_t *)((uint8_t *)stack + sizeof(stack));
        fp = (uint64_t *)0xCAFE;
        stack_push(&sp, stack, receiver);
        activate_method(&sp, &fp, 0, (uint64_t)itf_cm, 0, 0);
        uint64_t result = interpret(&sp, &fp,
                                    (uint8_t *)&OBJ_FIELD(itf_bc, 0),
                                    class_table, om);
        ASSERT_EQ(result, tag_smallint(77),
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

        sp = (uint64_t *)((uint8_t *)stack + sizeof(stack));
        fp = (uint64_t *)0xCAFE;
        stack_push(&sp, stack, receiver);
        activate_method(&sp, &fp, 0, (uint64_t)itf_cm2, 0, 0);
        result = interpret(&sp, &fp,
                           (uint8_t *)&OBJ_FIELD(itf_bc, 0),
                           class_table, om);
        ASSERT_EQ(result, tag_smallint(99),
                  "False ifTrue: [77] ifFalse: [99] → 99");
    }

    // --- Section 13: Factorial ---
    // SmallInteger >> factorial
    //     ^ (self = 1) ifTrue: [1] ifFalse: [self * (self - 1) factorial]
    {
        uint64_t sel_eq2 = tag_smallint(53);
        uint64_t sel_minus2 = tag_smallint(51);
        uint64_t sel_mul2 = tag_smallint(54);
        uint64_t sel_value2 = tag_smallint(60);
        uint64_t sel_ifTF2 = tag_smallint(70);
        uint64_t sel_fact = tag_smallint(80);

        // --- True block: [1] ---
        uint64_t *tb_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 16);
        uint8_t *tb = (uint8_t *)&OBJ_FIELD(tb_bc, 0);
        tb[0] = BC_PUSH_LITERAL;
        WRITE_U32(&tb[1], 0);
        tb[5] = BC_RETURN;

        uint64_t *tb_lits = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 1);
        OBJ_FIELD(tb_lits, 0) = tag_smallint(1);
        uint64_t *tb_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(tb_cm, CM_PRIMITIVE) = tag_smallint(0);
        OBJ_FIELD(tb_cm, CM_NUM_ARGS) = tag_smallint(0);
        OBJ_FIELD(tb_cm, CM_NUM_TEMPS) = tag_smallint(0);
        OBJ_FIELD(tb_cm, CM_LITERALS) = (uint64_t)tb_lits;
        OBJ_FIELD(tb_cm, CM_BYTECODES) = (uint64_t)tb_bc;

        // --- False block: [self * (self - 1) factorial] ---
        // PUSH_SELF, PUSH_SELF, PUSH_LITERAL 0 (=1), SEND #- 1,
        // SEND #factorial 0, SEND #* 1, RETURN
        uint64_t *fb_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 40);
        uint8_t *fb = (uint8_t *)&OBJ_FIELD(fb_bc, 0);
        fb[0] = BC_PUSH_SELF;
        fb[1] = BC_PUSH_SELF;
        fb[2] = BC_PUSH_LITERAL;
        WRITE_U32(&fb[3], 0); // literal 0 = 1
        fb[7] = BC_SEND_MESSAGE;
        WRITE_U32(&fb[8], 1);  // selector 1 = sel_minus
        WRITE_U32(&fb[12], 1); // 1 arg
        fb[16] = BC_SEND_MESSAGE;
        WRITE_U32(&fb[17], 2); // selector 2 = sel_fact
        WRITE_U32(&fb[21], 0); // 0 args
        fb[25] = BC_SEND_MESSAGE;
        WRITE_U32(&fb[26], 3); // selector 3 = sel_mul
        WRITE_U32(&fb[30], 1); // 1 arg
        fb[34] = BC_RETURN;

        uint64_t *fb_lits = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 4);
        OBJ_FIELD(fb_lits, 0) = tag_smallint(1);
        OBJ_FIELD(fb_lits, 1) = sel_minus2;
        OBJ_FIELD(fb_lits, 2) = sel_fact;
        OBJ_FIELD(fb_lits, 3) = sel_mul2;
        uint64_t *fb_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(fb_cm, CM_PRIMITIVE) = tag_smallint(0);
        OBJ_FIELD(fb_cm, CM_NUM_ARGS) = tag_smallint(0);
        OBJ_FIELD(fb_cm, CM_NUM_TEMPS) = tag_smallint(0);
        OBJ_FIELD(fb_cm, CM_LITERALS) = (uint64_t)fb_lits;
        OBJ_FIELD(fb_cm, CM_BYTECODES) = (uint64_t)fb_bc;

        // --- factorial method ---
        // PUSH_SELF, PUSH_LITERAL 0 (=1), SEND #= 1,
        // PUSH_CLOSURE 1 (true block), PUSH_CLOSURE 2 (false block),
        // SEND #ifTrue:ifFalse: 2, RETURN
        uint64_t *fact_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 40);
        uint8_t *fbc = (uint8_t *)&OBJ_FIELD(fact_bc, 0);
        fbc[0] = BC_PUSH_SELF;
        fbc[1] = BC_PUSH_LITERAL;
        WRITE_U32(&fbc[2], 0); // literal 0 = 1
        fbc[6] = BC_SEND_MESSAGE;
        WRITE_U32(&fbc[7], 1);  // selector 1 = sel_eq
        WRITE_U32(&fbc[11], 1); // 1 arg
        fbc[15] = BC_PUSH_CLOSURE;
        WRITE_U32(&fbc[16], 2); // literal 2 = tb_cm
        fbc[20] = BC_PUSH_CLOSURE;
        WRITE_U32(&fbc[21], 3); // literal 3 = fb_cm
        fbc[25] = BC_SEND_MESSAGE;
        WRITE_U32(&fbc[26], 4); // selector 4 = sel_ifTF
        WRITE_U32(&fbc[30], 2); // 2 args
        fbc[34] = BC_RETURN;

        uint64_t *fact_lits = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 5);
        OBJ_FIELD(fact_lits, 0) = tag_smallint(1);
        OBJ_FIELD(fact_lits, 1) = sel_eq2;
        OBJ_FIELD(fact_lits, 2) = (uint64_t)tb_cm;
        OBJ_FIELD(fact_lits, 3) = (uint64_t)fb_cm;
        OBJ_FIELD(fact_lits, 4) = sel_ifTF2;
        uint64_t *fact_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(fact_cm, CM_PRIMITIVE) = tag_smallint(0);
        OBJ_FIELD(fact_cm, CM_NUM_ARGS) = tag_smallint(0);
        OBJ_FIELD(fact_cm, CM_NUM_TEMPS) = tag_smallint(0);
        OBJ_FIELD(fact_cm, CM_LITERALS) = (uint64_t)fact_lits;
        OBJ_FIELD(fact_cm, CM_BYTECODES) = (uint64_t)fact_bc;

        // Add #factorial to SmallInteger's method dict (rebuild with 12 entries)
        uint64_t *old_md = (uint64_t *)OBJ_FIELD(smallint_class, CLASS_METHOD_DICT);
        uint64_t *new_md = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 12);
        for (int i = 0; i < 10; i++)
            OBJ_FIELD(new_md, i) = OBJ_FIELD(old_md, i);
        OBJ_FIELD(new_md, 10) = sel_fact;
        OBJ_FIELD(new_md, 11) = (uint64_t)fact_cm;
        OBJ_FIELD(smallint_class, CLASS_METHOD_DICT) = (uint64_t)new_md;

        // Caller: PUSH_LITERAL 0 (= N), SEND #factorial 0, HALT
        uint64_t *run_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 16);
        uint8_t *rb = (uint8_t *)&OBJ_FIELD(run_bc, 0);
        rb[0] = BC_PUSH_LITERAL;
        WRITE_U32(&rb[1], 0);
        rb[5] = BC_SEND_MESSAGE;
        WRITE_U32(&rb[6], 1);  // selector 1 = sel_fact
        WRITE_U32(&rb[10], 0); // 0 args
        rb[14] = BC_HALT;

        // 1 factorial = 1
        uint64_t *run_lits = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 2);
        OBJ_FIELD(run_lits, 0) = tag_smallint(1);
        OBJ_FIELD(run_lits, 1) = sel_fact;
        uint64_t *run_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(run_cm, CM_PRIMITIVE) = tag_smallint(0);
        OBJ_FIELD(run_cm, CM_NUM_ARGS) = tag_smallint(0);
        OBJ_FIELD(run_cm, CM_NUM_TEMPS) = tag_smallint(0);
        OBJ_FIELD(run_cm, CM_LITERALS) = (uint64_t)run_lits;
        OBJ_FIELD(run_cm, CM_BYTECODES) = (uint64_t)run_bc;

        sp = (uint64_t *)((uint8_t *)stack + sizeof(stack));
        fp = (uint64_t *)0xCAFE;
        stack_push(&sp, stack, receiver);
        activate_method(&sp, &fp, 0, (uint64_t)run_cm, 0, 0);
        uint64_t result = interpret(&sp, &fp,
                                    (uint8_t *)&OBJ_FIELD(run_bc, 0),
                                    class_table, om);
        ASSERT_EQ(result, tag_smallint(1), "1 factorial = 1");

        // 2 factorial = 2
        uint64_t *run_lits2 = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 2);
        OBJ_FIELD(run_lits2, 0) = tag_smallint(2);
        OBJ_FIELD(run_lits2, 1) = sel_fact;
        uint64_t *run_cm2 = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(run_cm2, CM_PRIMITIVE) = tag_smallint(0);
        OBJ_FIELD(run_cm2, CM_NUM_ARGS) = tag_smallint(0);
        OBJ_FIELD(run_cm2, CM_NUM_TEMPS) = tag_smallint(0);
        OBJ_FIELD(run_cm2, CM_LITERALS) = (uint64_t)run_lits2;
        OBJ_FIELD(run_cm2, CM_BYTECODES) = (uint64_t)run_bc;

        sp = (uint64_t *)((uint8_t *)stack + sizeof(stack));
        fp = (uint64_t *)0xCAFE;
        stack_push(&sp, stack, receiver);
        activate_method(&sp, &fp, 0, (uint64_t)run_cm2, 0, 0);
        result = interpret(&sp, &fp,
                           (uint8_t *)&OBJ_FIELD(run_bc, 0),
                           class_table, om);
        ASSERT_EQ(result, tag_smallint(2), "2 factorial = 2");

        // 5 factorial = 120
        uint64_t *run_lits5 = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 2);
        OBJ_FIELD(run_lits5, 0) = tag_smallint(5);
        OBJ_FIELD(run_lits5, 1) = sel_fact;
        uint64_t *run_cm5 = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(run_cm5, CM_PRIMITIVE) = tag_smallint(0);
        OBJ_FIELD(run_cm5, CM_NUM_ARGS) = tag_smallint(0);
        OBJ_FIELD(run_cm5, CM_NUM_TEMPS) = tag_smallint(0);
        OBJ_FIELD(run_cm5, CM_LITERALS) = (uint64_t)run_lits5;
        OBJ_FIELD(run_cm5, CM_BYTECODES) = (uint64_t)run_bc;

        sp = (uint64_t *)((uint8_t *)stack + sizeof(stack));
        fp = (uint64_t *)0xCAFE;
        stack_push(&sp, stack, receiver);
        activate_method(&sp, &fp, 0, (uint64_t)run_cm5, 0, 0);
        result = interpret(&sp, &fp,
                           (uint8_t *)&OBJ_FIELD(run_bc, 0),
                           class_table, om);
        ASSERT_EQ(result, tag_smallint(120), "5 factorial = 120");

        // 10 factorial = 3628800
        uint64_t *run_lits10 = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 2);
        OBJ_FIELD(run_lits10, 0) = tag_smallint(10);
        OBJ_FIELD(run_lits10, 1) = sel_fact;
        uint64_t *run_cm10 = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(run_cm10, CM_PRIMITIVE) = tag_smallint(0);
        OBJ_FIELD(run_cm10, CM_NUM_ARGS) = tag_smallint(0);
        OBJ_FIELD(run_cm10, CM_NUM_TEMPS) = tag_smallint(0);
        OBJ_FIELD(run_cm10, CM_LITERALS) = (uint64_t)run_lits10;
        OBJ_FIELD(run_cm10, CM_BYTECODES) = (uint64_t)run_bc;

        sp = (uint64_t *)((uint8_t *)stack + sizeof(stack));
        fp = (uint64_t *)0xCAFE;
        stack_push(&sp, stack, receiver);
        activate_method(&sp, &fp, 0, (uint64_t)run_cm10, 0, 0);
        result = interpret(&sp, &fp,
                           (uint8_t *)&OBJ_FIELD(run_bc, 0),
                           class_table, om);
        ASSERT_EQ(result, tag_smallint(3628800), "10 factorial = 3628800");
    }

    printf("\n%d passed, %d failed\n", passes, failures);
    return failures > 0 ? 1 : 0;
}
