#ifndef TEST_DEFS_H
#define TEST_DEFS_H
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

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
extern uint64_t tag_character(uint64_t code_point);
extern uint64_t untag_character(uint64_t tagged);
extern uint64_t is_character(uint64_t value);
extern uint64_t smallint_add(uint64_t a, uint64_t b);
extern uint64_t smallint_sub(uint64_t a, uint64_t b);
extern uint64_t smallint_less_than(uint64_t a, uint64_t b);
extern uint64_t smallint_equal(uint64_t a, uint64_t b);
extern uint64_t prim_string_eq(uint64_t receiver, uint64_t arg);
extern uint64_t prim_string_hash_fnv(uint64_t receiver);
extern uint64_t prim_string_as_symbol(uint64_t receiver);
extern uint64_t prim_symbol_eq(uint64_t receiver, uint64_t arg);
extern uint64_t *ensure_frame_context(uint64_t *fp, uint64_t *om, uint64_t context_class);
extern uint64_t *ensure_frame_context_global(uint64_t *fp, uint64_t *om);
extern uint64_t cannot_return_selector_oop(void);
extern uint64_t *global_symbol_table; // Declare global symbol table
extern uint64_t *global_context_class;
extern void om_init(void *buffer, uint64_t size_bytes, uint64_t *free_ptr_var);
extern uint64_t *om_alloc(uint64_t *free_ptr_var, uint64_t class_ptr, uint64_t format, uint64_t size);

// GC context for the interpreter: two semi-spaces
// Layout (all uint64_t):
//   [0] from_free_ptr    — current allocation pointer
//   [1] from_end_ptr     — end of from-space
//   [2] to_free_ptr      — start of to-space (reset before each GC)
//   [3] to_end_ptr       — end of to-space
//   [4] from_buf_start   — start address of from-space buffer
//   [5] to_buf_start     — start address of to-space buffer
//   [6] space_size       — size of each space in bytes
//   [7] tenured_start    — start of tenured space (0 if none)
//   [8] tenured_end      — end of tenured space
//   [9] remembered_set   — pointer to remembered set log (or 0)
#define GC_FROM_FREE 0
#define GC_FROM_END 1
#define GC_TO_FREE 2
#define GC_TO_END 3
#define GC_FROM_START 4
#define GC_TO_START 5
#define GC_SPACE_SIZE 6
#define GC_TENURED_START 7
#define GC_TENURED_END 8
#define GC_REMEMBERED 9

static inline void gc_ctx_init(uint64_t *gc_ctx, uint8_t *buf_a, uint8_t *buf_b, uint64_t size)
{
    gc_ctx[GC_FROM_FREE] = (uint64_t)buf_a;
    gc_ctx[GC_FROM_END] = (uint64_t)(buf_a + size);
    gc_ctx[GC_TO_FREE] = (uint64_t)buf_b;
    gc_ctx[GC_TO_END] = (uint64_t)(buf_b + size);
    gc_ctx[GC_FROM_START] = (uint64_t)buf_a;
    gc_ctx[GC_TO_START] = (uint64_t)buf_b;
    gc_ctx[GC_SPACE_SIZE] = size;
    gc_ctx[GC_TENURED_START] = 0;
    gc_ctx[GC_TENURED_END] = 0;
    gc_ctx[GC_REMEMBERED] = 0;
}

// Swap from/to spaces in the GC context
static inline void gc_ctx_swap(uint64_t *gc_ctx)
{
    uint64_t tmp;
    tmp = gc_ctx[GC_FROM_FREE];
    gc_ctx[GC_FROM_FREE] = gc_ctx[GC_TO_FREE];
    gc_ctx[GC_TO_FREE] = tmp;
    tmp = gc_ctx[GC_FROM_END];
    gc_ctx[GC_FROM_END] = gc_ctx[GC_TO_END];
    gc_ctx[GC_TO_END] = tmp;
    tmp = gc_ctx[GC_FROM_START];
    gc_ctx[GC_FROM_START] = gc_ctx[GC_TO_START];
    gc_ctx[GC_TO_START] = tmp;
}
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

// GC functions
// gc_copy_object(obj, to_space) -> new_obj
//   Copy obj to to_space[0] (bump pointer), leave forwarding ptr in old obj.
//   Returns pointer to the new copy. Advances to_space[0].
extern uint64_t *gc_copy_object(uint64_t *obj, uint64_t *to_space);
// gc_is_forwarded(obj) -> 1 if obj[0] has forwarding tag, 0 otherwise
extern uint64_t gc_is_forwarded(uint64_t *obj);
// gc_forwarding_ptr(obj) -> the forwarding address (clears tag)
extern uint64_t *gc_forwarding_ptr(uint64_t *obj);
// gc_collect(roots, num_roots, from_space, to_space, from_start, from_end)
//   Cheney's algorithm: copy roots, then scan to-space updating pointers.
//   roots = array of tagged values (only object ptrs with tag 00 are followed)
//   from_start/from_end = address range of from-space (to identify pointers into it)
extern void gc_collect(uint64_t *roots, uint64_t num_roots,
                       uint64_t *from_space, uint64_t *to_space,
                       uint64_t from_start, uint64_t from_end);
extern uint64_t gc_collect_stack_slots(uint64_t *sp, uint64_t *fp,
                                       uint64_t **slot_buf, uint64_t max_slots);

// gc_update_stack(fp, from_start, from_end)
//   Walk stack frames, update any pointer in from-space range to its forwarding address.
extern void gc_update_stack(uint64_t *fp, uint64_t from_start, uint64_t from_end);

// gc_scan_stack(fp, root_buf, max_roots) -> num_roots_found
//   Walk stack frames from fp, collecting object pointers (receiver, method, temps)
//   into root_buf. Stops at sentinel FP (0xCAFE or 0).
extern uint64_t gc_scan_stack(uint64_t *fp, uint64_t *root_buf, uint64_t max_roots);

// Persistence: pointer <-> offset conversion
// image_pointers_to_offsets(buf, size, heap_base)
//   Walk all objects in buf, convert any pointer in [heap_base, heap_base+size) to offset.
extern void image_pointers_to_offsets(uint8_t *buf, uint64_t size, uint64_t heap_base);
// image_offsets_to_pointers(buf, size, new_base)
//   Walk all objects in buf, convert offsets back to pointers by adding new_base.
extern void image_offsets_to_pointers(uint8_t *buf, uint64_t size, uint64_t new_base);

#define GC_FORWARD_TAG 1 // bit 0 set on a forwarding pointer (real class ptrs are aligned)

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
#define CLASS_INST_FORMAT 3 // tagged SmallInt: 0=fields, 1=indexable, 2=bytes
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
#define PRIM_BASIC_NEW 8
#define PRIM_BLOCK_VALUE 9
#define PRIM_BASIC_NEW_SIZE 10
#define PRIM_SIZE 11
#define PRIM_IDENTITY_EQ 12
#define PRIM_BASIC_CLASS 13
#define PRIM_HASH 14
#define PRIM_PRINT_CHAR 15
#define PRIM_BLOCK_VALUE_ARG 16
#define PRIM_PERFORM 17
#define PRIM_HALT 18
#define PRIM_CHAR_VALUE 19
#define PRIM_AS_CHARACTER 20
#define PRIM_CHAR_IS_LETTER 21
#define PRIM_CHAR_IS_DIGIT 22
#define PRIM_CHAR_UPPERCASE 23
#define PRIM_CHAR_LOWERCASE 24
#define PRIM_STRING_EQ 25
#define PRIM_STRING_HASH_FNV 26
#define PRIM_STRING_AS_SYMBOL 27
#define PRIM_SYMBOL_EQ 28 // Identity equality for symbols
#define PRIM_ERROR 29
#define BLOCK_HOME_CONTEXT 0
#define BLOCK_HOME_RECEIVER 1
#define BLOCK_CM 2
#define BLOCK_COPIED_BASE 3
#define CONTEXT_SENDER 0
#define CONTEXT_IP 1
#define CONTEXT_METHOD 2
#define CONTEXT_RECEIVER 3
#define CONTEXT_HOME 4
#define CONTEXT_CLOSURE 5
#define CONTEXT_FLAGS 6
#define CONTEXT_NUM_ARGS 7
#define CONTEXT_NUM_TEMPS 8
#define CONTEXT_VAR_BASE 9
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
#define BC_RETURN_NON_LOCAL 16
#define FRAME_SAVED_IP 1  // FP + 1*W
#define FRAME_SAVED_FP 0  // FP + 0
#define FRAME_METHOD -1   // FP - 1*W
#define FRAME_FLAGS -2    // FP - 2*W
#define FRAME_CONTEXT -3  // FP - 3*W
#define FRAME_RECEIVER -4 // FP - 4*W
#define FRAME_TEMP0 -5    // FP - 5*W
#define FRAME_FLAGS_HAS_CONTEXT_MASK 0x1
#define FRAME_FLAGS_BLOCK_CLOSURE_MASK 0x2
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
    uint64_t *undefined_object_class;
    uint64_t *character_class;
    uint64_t *string_class;
    uint64_t *symbol_class;
    uint64_t *context_class;
    uint64_t *symbol_table;
    uint64_t *test_class;
    uint64_t receiver;
    uint64_t method;
    uint64_t *class_table; // heap-allocated indexable object
    uint64_t stack[STACK_WORDS];
    int passes;
    int failures;
} TestContext;

typedef void (*TrapTestFn)(TestContext *ctx);

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
void debug_error(uint64_t message, uint64_t *fp, uint64_t *class_table);
int run_trap_test(TestContext *ctx, TrapTestFn fn);

void test_stack(TestContext *ctx);
void test_tagged(TestContext *ctx);
void test_object(TestContext *ctx);
void test_dispatch(TestContext *ctx);
void test_blocks(TestContext *ctx);
void test_factorial(TestContext *ctx);
void test_transaction(TestContext *ctx);
void test_gc(TestContext *ctx);
void test_persist(TestContext *ctx);
void test_primitives(TestContext *ctx);
void test_smalltalk_sources(TestContext *ctx);
void test_string_dispatch(TestContext *ctx);
void test_array_dispatch(TestContext *ctx);
void test_symbol_dispatch(TestContext *ctx);
void test_bootstrap_compiler(TestContext *ctx);
void test_smalltalk_expressions(TestContext *ctx);

#endif
