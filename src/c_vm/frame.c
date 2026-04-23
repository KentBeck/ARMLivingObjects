#include "vm_defs.h"

#include <signal.h>
#include <stdint.h>

extern Oop *smalltalk_stack_limit_low;

static void trap_stack_overflow(void)
{
    raise(SIGTRAP);
}

void activate_method(Oop **sp_ptr, ObjPtr *fp_ptr, uint64_t saved_ip,
                     Oop method, uint64_t num_args, uint64_t num_temps)
{
    Oop *sp = *sp_ptr;
    ObjPtr caller_fp = *fp_ptr;
    uint64_t required_words = num_temps + 6;

    if (sp - required_words < smalltalk_stack_limit_low)
    {
        trap_stack_overflow();
    }

    Oop receiver = sp[num_args];

    *--sp = saved_ip;
    *--sp = (Oop)caller_fp;
    ObjPtr fp = sp;

    *--sp = method;
    *--sp = num_args << FRAME_FLAGS_NUM_ARGS_SHIFT;
    *--sp = 0;
    *--sp = receiver;

    for (uint64_t index = 0; index < num_temps; index++)
    {
        *--sp = 0;
    }

    *sp_ptr = sp;
    *fp_ptr = fp;
}

Oop frame_receiver(ObjPtr fp)
{
    return fp[FRAME_RECEIVER];
}

Oop frame_method(ObjPtr fp)
{
    return fp[FRAME_METHOD];
}

uint64_t frame_flags(ObjPtr fp)
{
    return fp[FRAME_FLAGS];
}

uint64_t frame_num_args(ObjPtr fp)
{
    return (fp[FRAME_FLAGS] >> FRAME_FLAGS_NUM_ARGS_SHIFT) & 0xFF;
}

uint64_t frame_is_block(ObjPtr fp)
{
    return (fp[FRAME_FLAGS] >> FRAME_FLAGS_IS_BLOCK_SHIFT) & 0xFF;
}

uint64_t frame_has_context(ObjPtr fp)
{
    return fp[FRAME_FLAGS] & FRAME_FLAGS_HAS_CONTEXT_MASK;
}

Oop frame_temp(ObjPtr fp, uint64_t index)
{
    return fp[-(int64_t)(FP_TEMP_BASE_WORDS + index)];
}

Oop frame_arg(ObjPtr fp, uint64_t index)
{
    uint64_t num_args = frame_num_args(fp);
    return fp[FP_ARG_BASE_WORDS + (num_args - 1 - index)];
}

void frame_store_temp(ObjPtr fp, uint64_t index, Oop value)
{
    fp[-(int64_t)(FP_TEMP_BASE_WORDS + index)] = value;
}

void frame_return(Oop **sp_ptr, ObjPtr *fp_ptr, uint64_t *ip_ptr, Oop return_value)
{
    ObjPtr fp = *fp_ptr;
    uint64_t num_args = frame_num_args(fp);
    Oop *new_sp = fp + FP_ARG_BASE_WORDS + num_args;

    *new_sp = return_value;
    *fp_ptr = (ObjPtr)fp[FRAME_SAVED_FP];
    *ip_ptr = fp[FRAME_SAVED_IP];
    *sp_ptr = new_sp;
}
