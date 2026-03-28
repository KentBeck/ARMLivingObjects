#ifndef TEST_DEFS_H
#define TEST_DEFS_H
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern void stack_push(uint64_t **sp_ptr, uint64_t *stack_base, uint64_t value);
extern uint64_t stack_pop(uint64_t **sp_ptr);
extern uint64_t stack_top(uint64_t **sp_ptr);
extern void activate_method(uint64_t **sp_ptr, uint64_t **fp_ptr, uint64_t saved_ip, uint64_t method, uint64_t num_args, uint64_t num_temps);
extern uint64_t frame_receiver(uint64_t *fp);
extern uint64_t frame_method(uint64_t *fp);
extern uint64_t frame_flags(uint64_t *fp);
extern uint64_t frame_num_args(uint64_t *fp);
extern uint64_t frame_is_block(uint64_t *fp);
extern uint64_t frame_has_context(uint64_t *fp);
extern uint64_t frame_temp(uint64_t *fp, uint64_t index);
extern uint64_t frame_arg(uint64_t *fp, uint64_t index);
extern void frame_store_temp(uint64_t *fp, uint64_t index, uint64_t value);
extern void frame_return(uint64_t **sp_ptr, uint64_t **fp_ptr, uint64_t *ip_ptr, uint64_t return_value);
extern void bc_push_self(uint64_t **sp_ptr, uint64_t **fp_ptr);
extern void bc_push_temp(uint64_t **sp_ptr, uint64_t **fp_ptr, uint64_t index);
extern void bc_push_inst_var(uint64_t **sp_ptr, uint64_t **fp_ptr, uint64_t field_index);
extern void bc_push_literal(uint64_t **sp_ptr, uint64_t **fp_ptr, uint64_t literal_index);
extern void bc_store_temp(uint64_t **sp_ptr, uint64_t **fp_ptr, uint64_t index);
extern void bc_store_inst_var(uint64_t **sp_ptr, uint64_t **fp_ptr, uint64_t field_index);
extern void bc_return_stack_top(uint64_t **sp_ptr, uint64_t **fp_ptr, uint64_t *ip_ptr);
extern void bc_duplicate(uint64_t **sp_ptr);
extern void bc_pop(uint64_t **sp_ptr);
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
extern void om_init(void *buffer, uint64_t size_bytes, uint64_t *free_ptr_var);
extern uint64_t *om_alloc(uint64_t *free_ptr_var, uint64_t class_ptr, uint64_t format, uint64_t size);
extern uint64_t *oop_class(uint64_t oop, uint64_t *class_table);
extern uint64_t md_lookup(uint64_t *method_dict, uint64_t selector);
extern uint64_t class_lookup(uint64_t *klass, uint64_t selector);
extern uint64_t interpret(uint64_t **sp_ptr, uint64_t **fp_ptr, uint8_t *ip, uint64_t *class_table, uint64_t *om, uint64_t *txn_log);

// Transaction log functions
// Log layout: [0] = count, then triples at [1+i*3], [2+i*3], [3+i*3]
//   triple = (object_ptr, field_index, new_value)
extern void txn_log_write(uint64_t *log, uint64_t obj, uint64_t field_index, uint64_t value);
extern uint64_t txn_log_read(uint64_t *log, uint64_t obj, uint64_t field_index, uint64_t *found);
extern void txn_commit(uint64_t *log);
extern void txn_abort(uint64_t *log);

#define OBJ_CLASS(obj) ((obj)[0])
#define OBJ_FORMAT(obj) ((obj)[1])
#define OBJ_SIZE(obj) ((obj)[2])
#define OBJ_FIELD(obj, n) ((obj)[3 + (n)])
#define FORMAT_FIELDS 0
#define FORMAT_INDEXABLE 1
#define FORMAT_BYTES 2
#define CLASS_SUPERCLASS 0
#define CLASS_METHOD_DICT 1
#define CLASS_INST_SIZE 2
#define CM_PRIMITIVE 0
#define CM_NUM_ARGS 1
#define CM_NUM_TEMPS 2
#define CM_LITERALS 3  // pointer to Array object (or tagged nil if none)
#define CM_BYTECODES 4 // pointer to ByteArray object
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
#define BLOCK_HOME_RECEIVER 0
#define BLOCK_CM 1
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
#define FRAME_SAVED_IP 1  // FP + 1*W
#define FRAME_SAVED_FP 0  // FP + 0
#define FRAME_METHOD -1   // FP - 1*W
#define FRAME_FLAGS -2    // FP - 2*W
#define FRAME_CONTEXT -3  // FP - 3*W
#define FRAME_RECEIVER -4 // FP - 4*W
#define FRAME_TEMP0 -5    // FP - 5*W
#define STACK_WORDS 4096

#define OM_SIZE (1024 * 1024)

static inline void WRITE_U32(uint8_t *p, uint32_t v)
{
    p[0] = v & 0xFF;
    p[1] = (v >> 8) & 0xFF;
    p[2] = (v >> 16) & 0xFF;
    p[3] = (v >> 24) & 0xFF;
}

typedef struct
{
    uint64_t *om;
    uint64_t *class_class;
    uint64_t *smallint_class;
    uint64_t *block_class;
    uint64_t *test_class;
    uint64_t receiver;
    uint64_t method;
    uint64_t *class_table; // heap-allocated indexable object
    uint64_t stack[STACK_WORDS];
    int passes;
    int failures;
} TestContext;

#define ASSERT_EQ(ctx, a, b, msg)                                        \
    do                                                                   \
    {                                                                    \
        uint64_t _a = (a), _b = (b);                                     \
        if (_a != _b)                                                    \
        {                                                                \
            printf("FAIL: %s (expected %llu, got %llu)\n", msg, _b, _a); \
            (ctx)->failures++;                                           \
        }                                                                \
        else                                                             \
        {                                                                \
            printf("PASS: %s\n", msg);                                   \
            (ctx)->passes++;                                             \
        }                                                                \
    } while (0)

void debug_mnu(uint64_t selector);
void debug_oom(void);
void debug_unknown_prim(uint64_t prim_index);

void test_stack(TestContext *ctx);
void test_tagged(TestContext *ctx);
void test_object(TestContext *ctx);
void test_dispatch(TestContext *ctx);
void test_blocks(TestContext *ctx);
void test_factorial(TestContext *ctx);
void test_transaction(TestContext *ctx);

#endif
