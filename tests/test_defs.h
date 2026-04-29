#ifndef TEST_DEFS_H
#define TEST_DEFS_H
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include "vm_defs.h"

extern void stack_push(Oop **sp_ptr, Oop *stack_base, Oop value);
extern Oop stack_pop(Oop **sp_ptr);
extern Oop stack_top(Oop **sp_ptr);
extern void activate_method(Oop **sp_ptr, ObjPtr *fp_ptr, uint64_t saved_ip,
                            Oop method, uint64_t num_args, uint64_t num_temps);
extern Oop frame_receiver(ObjPtr fp);
extern Oop frame_method(ObjPtr fp);
extern uint64_t frame_flags(ObjPtr fp);
extern uint64_t frame_num_args(ObjPtr fp);
extern uint64_t frame_is_block(ObjPtr fp);
extern uint64_t frame_has_context(ObjPtr fp);
extern Oop frame_temp(ObjPtr fp, uint64_t index);
extern Oop frame_arg(ObjPtr fp, uint64_t index);
extern void frame_store_temp(ObjPtr fp, uint64_t index, Oop value);
extern void frame_return(Oop **sp_ptr, ObjPtr *fp_ptr, uint64_t *ip_ptr, Oop return_value);
extern void bc_push_self(Oop **sp_ptr, ObjPtr *fp_ptr);
extern void bc_push_temp(Oop **sp_ptr, ObjPtr *fp_ptr, uint64_t index);
extern void bc_push_inst_var(Oop **sp_ptr, ObjPtr *fp_ptr, uint64_t field_index);
extern void bc_push_literal(Oop **sp_ptr, ObjPtr *fp_ptr, uint64_t literal_index);
extern void bc_store_temp(Oop **sp_ptr, ObjPtr *fp_ptr, uint64_t index);
extern void bc_store_inst_var(Oop **sp_ptr, ObjPtr *fp_ptr, uint64_t field_index);
extern void bc_return_stack_top(Oop **sp_ptr, ObjPtr *fp_ptr, uint64_t *ip_ptr);
extern void bc_duplicate(Oop **sp_ptr);
extern void bc_pop(Oop **sp_ptr);
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
extern Oop prim_string_eq(Oop receiver, Oop arg) LO_NO_ALLOC;
extern Oop prim_string_hash_fnv(Oop receiver) LO_NO_ALLOC;
extern Oop prim_string_as_symbol(Oop receiver) LO_NO_ALLOC;
extern Oop prim_symbol_eq(Oop receiver, Oop arg) LO_NO_ALLOC;
extern ObjPtr ensure_frame_context(ObjPtr fp, Om om, Oop context_class) LO_ALLOCATES;
extern ObjPtr ensure_frame_context_with_sp(ObjPtr fp, Oop *sp, Om om, Oop context_class) LO_ALLOCATES;
extern ObjPtr ensure_frame_context_global(ObjPtr fp, Om om) LO_ALLOCATES;
extern Oop cannot_return_selector_oop(void) LO_NO_ALLOC;
extern void activate_context(Oop **sp_ptr, ObjPtr *fp_ptr, uint64_t saved_ip,
                             ObjPtr context);
extern uint64_t *global_symbol_table; // Declare global symbol table
extern uint64_t *global_symbol_class;
extern uint64_t *global_context_class;
extern uint64_t *global_smalltalk_dictionary;
extern void om_init(void *buffer, uint64_t size_bytes, Om free_ptr_var) LO_NO_ALLOC;
extern ObjPtr om_alloc(Om free_ptr_var, Oop class_ptr, uint64_t format, uint64_t size) LO_ALLOCATES;
extern void gc_register_context(Om gc_ctx) LO_NO_ALLOC;
extern Oop gc_is_registered_context(Om om) LO_NO_ALLOC;

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
    gc_register_context(gc_ctx);
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

typedef struct
{
    Oop roots[16];
    uint64_t count;
} OopRootSet;

static inline uint64_t oop_roots_add(OopRootSet *set, Oop oop)
{
    uint64_t index = set->count;
    if (index < (uint64_t)(sizeof(set->roots) / sizeof(set->roots[0])))
    {
        set->roots[index] = oop;
        set->count++;
    }
    return index;
}

static inline Oop oop_roots_get(const OopRootSet *set, uint64_t index)
{
    return set->roots[index];
}

static inline ObjPtr oop_roots_ptr(const OopRootSet *set, uint64_t index)
{
    return (ObjPtr)set->roots[index];
}

extern Oop oop_class(Oop oop, ObjPtr class_table) LO_NO_ALLOC;
extern Oop md_lookup(ObjPtr method_dict, Oop selector) LO_NO_ALLOC;
extern Oop class_lookup(ObjPtr klass, Oop selector) LO_NO_ALLOC;
extern Oop interpret(Oop **sp_ptr, Oop **fp_ptr, uint8_t *ip, ObjPtr class_table, Om om, Oop *txn_log) LO_MAY_GC;

// Transaction log functions
// Log layout: [0] = count, then triples at [1+i*3], [2+i*3], [3+i*3]
//   triple = (object_ptr, field_index, new_value)
extern void txn_log_write(uint64_t *log, uint64_t obj, uint64_t field_index, uint64_t value);
extern uint64_t txn_log_read(uint64_t *log, uint64_t obj, uint64_t field_index, uint64_t *found);
extern void txn_commit(uint64_t *log);
extern void txn_commit_durable(uint64_t *log);
extern void txn_abort(uint64_t *log);
extern int txn_log_append_fsync(const uint64_t *log, uint64_t heap_start, uint64_t heap_limit);
extern int txn_log_replay(uint64_t heap_start, uint64_t heap_used);
extern int txn_durable_log_clear(void);
extern const char *txn_durable_log_path(void);
extern uint64_t om_registered_start(uint64_t *om);

// GC functions
// gc_copy_object(obj, to_space) -> new_obj
//   Copy obj to to_space[0] (bump pointer), leave forwarding ptr in old obj.
//   Returns pointer to the new copy. Advances to_space[0].
extern ObjPtr gc_copy_object(ObjPtr obj, Om to_space) LO_MAY_GC;
// gc_is_forwarded(obj) -> 1 if obj[0] has forwarding tag, 0 otherwise
extern uint64_t gc_is_forwarded(ObjPtr obj) LO_NO_ALLOC;
// gc_forwarding_ptr(obj) -> the forwarding address (clears tag)
extern ObjPtr gc_forwarding_ptr(ObjPtr obj) LO_NO_ALLOC;
// gc_collect(roots, num_roots, from_space, to_space, from_start, from_end)
//   Cheney's algorithm: copy roots, then scan to-space updating pointers.
//   roots = array of tagged values (only object ptrs with tag 00 are followed)
//   from_start/from_end = address range of from-space (to identify pointers into it)
extern void gc_collect(Oop *roots, uint64_t num_roots,
                       Om from_space, Om to_space,
                       uint64_t from_start, uint64_t from_end) LO_MAY_GC;
extern uint64_t gc_collect_stack_slots(Oop *sp, ObjPtr fp,
                                       Oop **slot_buf, uint64_t max_slots) LO_NO_ALLOC;

// gc_update_stack(fp, from_start, from_end)
//   Walk stack frames, update any pointer in from-space range to its forwarding address.
extern void gc_update_stack(ObjPtr fp, uint64_t from_start, uint64_t from_end) LO_MAY_GC;

// gc_scan_stack(fp, root_buf, max_roots) -> num_roots_found
//   Walk stack frames from fp, collecting object pointers (receiver, method, temps)
//   into root_buf. Stops at sentinel FP (0xCAFE or 0).
extern uint64_t gc_scan_stack(ObjPtr fp, Oop *root_buf, uint64_t max_roots) LO_NO_ALLOC;

// Persistence: pointer <-> offset conversion
// image_pointers_to_offsets(buf, size, heap_base)
//   Walk all objects in buf, convert any pointer in [heap_base, heap_base+size) to offset.
extern void image_pointers_to_offsets(uint8_t *buf, uint64_t size, uint64_t heap_base);
// image_offsets_to_pointers(buf, size, new_base)
//   Walk all objects in buf, convert offsets back to pointers by adding new_base.
extern void image_offsets_to_pointers(uint8_t *buf, uint64_t size, uint64_t new_base);

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
            (void)(msg);                                                 \
            (ctx)->passes++;                                             \
        }                                                                \
    } while (0)

void debug_mnu(uint64_t selector);
void debug_mnu_context(uint64_t selector, uint64_t *current_cm, uint64_t selector_index);
void debug_oom(void);
void debug_unknown_prim(uint64_t prim_index);
void debug_error(uint64_t message, uint64_t *fp, uint64_t *class_table);
int run_trap_test(TestContext *ctx, TrapTestFn fn);

void test_stack(TestContext *ctx);
void test_tagged(TestContext *ctx);
void test_object(TestContext *ctx);
void test_dispatch(TestContext *ctx);
void test_blocks(TestContext *ctx);
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
void test_expression_fixtures(TestContext *ctx);
void test_smalltalk_runtime(TestContext *ctx);

#endif
