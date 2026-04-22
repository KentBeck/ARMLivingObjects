#include "vm_defs.h"

#include <stdint.h>

static void push(uint64_t **sp_ptr, uint64_t value)
{
    uint64_t *sp = *sp_ptr - 1;
    *sp = value;
    *sp_ptr = sp;
}

static uint64_t pop(uint64_t **sp_ptr)
{
    uint64_t *sp = *sp_ptr;
    uint64_t value = *sp;
    *sp_ptr = sp + 1;
    return value;
}

void bc_push_self(uint64_t **sp_ptr, uint64_t **fp_ptr)
{
    push(sp_ptr, (*fp_ptr)[FRAME_RECEIVER]);
}

void bc_push_temp(uint64_t **sp_ptr, uint64_t **fp_ptr, uint64_t index)
{
    push(sp_ptr, (*fp_ptr)[-(int64_t)(FP_TEMP_BASE_WORDS + index)]);
}

void bc_push_inst_var(uint64_t **sp_ptr, uint64_t **fp_ptr, uint64_t field_index)
{
    uint64_t *receiver = (uint64_t *)(*fp_ptr)[FRAME_RECEIVER];
    push(sp_ptr, OBJ_FIELD(receiver, field_index));
}

void bc_push_literal(uint64_t **sp_ptr, uint64_t **fp_ptr, uint64_t literal_index)
{
    uint64_t *method = (uint64_t *)(*fp_ptr)[FRAME_METHOD];
    uint64_t *literals = (uint64_t *)OBJ_FIELD(method, CM_LITERALS);
    push(sp_ptr, OBJ_FIELD(literals, literal_index));
}

void bc_store_temp(uint64_t **sp_ptr, uint64_t **fp_ptr, uint64_t index)
{
    (*fp_ptr)[-(int64_t)(FP_TEMP_BASE_WORDS + index)] = pop(sp_ptr);
}

void bc_store_inst_var(uint64_t **sp_ptr, uint64_t **fp_ptr, uint64_t field_index)
{
    uint64_t value = pop(sp_ptr);
    uint64_t *receiver = (uint64_t *)(*fp_ptr)[FRAME_RECEIVER];
    OBJ_FIELD(receiver, field_index) = value;
}

void bc_return_stack_top(uint64_t **sp_ptr, uint64_t **fp_ptr, uint64_t *ip_ptr)
{
    uint64_t return_value = **sp_ptr;
    uint64_t *fp = *fp_ptr;
    uint64_t flags = fp[FRAME_FLAGS];
    uint64_t num_args = (flags >> FRAME_FLAGS_NUM_ARGS_SHIFT) & 0xFF;
    uint64_t *new_sp = fp + FP_ARG_BASE_WORDS + num_args;

    *new_sp = return_value;
    *fp_ptr = (uint64_t *)fp[FRAME_SAVED_FP];
    *ip_ptr = fp[FRAME_SAVED_IP];
    *sp_ptr = new_sp;
}

void bc_duplicate(uint64_t **sp_ptr)
{
    push(sp_ptr, **sp_ptr);
}

void bc_pop(uint64_t **sp_ptr)
{
    *sp_ptr = *sp_ptr + 1;
}
