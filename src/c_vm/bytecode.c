#include "vm_defs.h"

#include <stdint.h>

static void push(Oop **sp_ptr, Oop value)
{
    Oop *sp = *sp_ptr - 1;
    *sp = value;
    *sp_ptr = sp;
}

static Oop pop(Oop **sp_ptr)
{
    Oop *sp = *sp_ptr;
    Oop value = *sp;
    *sp_ptr = sp + 1;
    return value;
}

void bc_push_self(Oop **sp_ptr, ObjPtr *fp_ptr)
{
    push(sp_ptr, (*fp_ptr)[FRAME_RECEIVER]);
}

void bc_push_temp(Oop **sp_ptr, ObjPtr *fp_ptr, uint64_t index)
{
    push(sp_ptr, (*fp_ptr)[-(int64_t)(FP_TEMP_BASE_WORDS + index)]);
}

void bc_push_inst_var(Oop **sp_ptr, ObjPtr *fp_ptr, uint64_t field_index)
{
    ObjPtr receiver = (ObjPtr)(*fp_ptr)[FRAME_RECEIVER];
    push(sp_ptr, OBJ_FIELD(receiver, field_index));
}

void bc_push_literal(Oop **sp_ptr, ObjPtr *fp_ptr, uint64_t literal_index)
{
    ObjPtr method = (ObjPtr)(*fp_ptr)[FRAME_METHOD];
    ObjPtr literals = (ObjPtr)OBJ_FIELD(method, CM_LITERALS);
    push(sp_ptr, OBJ_FIELD(literals, literal_index));
}

void bc_store_temp(Oop **sp_ptr, ObjPtr *fp_ptr, uint64_t index)
{
    (*fp_ptr)[-(int64_t)(FP_TEMP_BASE_WORDS + index)] = pop(sp_ptr);
}

void bc_store_inst_var(Oop **sp_ptr, ObjPtr *fp_ptr, uint64_t field_index)
{
    Oop value = pop(sp_ptr);
    ObjPtr receiver = (ObjPtr)(*fp_ptr)[FRAME_RECEIVER];
    OBJ_FIELD(receiver, field_index) = value;
}

void bc_return_stack_top(Oop **sp_ptr, ObjPtr *fp_ptr, uint64_t *ip_ptr)
{
    Oop return_value = **sp_ptr;
    ObjPtr fp = *fp_ptr;
    uint64_t flags = fp[FRAME_FLAGS];
    uint64_t num_args = (flags >> FRAME_FLAGS_NUM_ARGS_SHIFT) & 0xFF;
    Oop *new_sp = fp + FP_ARG_BASE_WORDS + num_args;

    *new_sp = return_value;
    *fp_ptr = (ObjPtr)fp[FRAME_SAVED_FP];
    *ip_ptr = fp[FRAME_SAVED_IP];
    *sp_ptr = new_sp;
}

void bc_duplicate(Oop **sp_ptr)
{
    push(sp_ptr, **sp_ptr);
}

void bc_pop(Oop **sp_ptr)
{
    *sp_ptr = *sp_ptr + 1;
}
