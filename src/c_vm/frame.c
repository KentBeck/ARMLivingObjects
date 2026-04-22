#include "vm_defs.h"

#include <signal.h>
#include <stdint.h>

extern uint64_t *smalltalk_stack_limit_low;

static void trap_stack_overflow(void)
{
    raise(SIGTRAP);
}

void activate_method(uint64_t **sp_ptr, uint64_t **fp_ptr, uint64_t saved_ip,
                     uint64_t method, uint64_t num_args, uint64_t num_temps)
{
    uint64_t *sp = *sp_ptr;
    uint64_t *caller_fp = *fp_ptr;
    uint64_t required_words = num_temps + 6;

    if (sp - required_words < smalltalk_stack_limit_low)
    {
        trap_stack_overflow();
    }

    uint64_t receiver = sp[num_args];

    *--sp = saved_ip;
    *--sp = (uint64_t)caller_fp;
    uint64_t *fp = sp;

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

uint64_t frame_receiver(uint64_t *fp)
{
    return fp[FRAME_RECEIVER];
}

uint64_t frame_method(uint64_t *fp)
{
    return fp[FRAME_METHOD];
}

uint64_t frame_flags(uint64_t *fp)
{
    return fp[FRAME_FLAGS];
}

uint64_t frame_num_args(uint64_t *fp)
{
    return (fp[FRAME_FLAGS] >> FRAME_FLAGS_NUM_ARGS_SHIFT) & 0xFF;
}

uint64_t frame_is_block(uint64_t *fp)
{
    return (fp[FRAME_FLAGS] >> FRAME_FLAGS_IS_BLOCK_SHIFT) & 0xFF;
}

uint64_t frame_has_context(uint64_t *fp)
{
    return fp[FRAME_FLAGS] & FRAME_FLAGS_HAS_CONTEXT_MASK;
}

uint64_t frame_temp(uint64_t *fp, uint64_t index)
{
    return fp[-(int64_t)(FP_TEMP_BASE_WORDS + index)];
}

uint64_t frame_arg(uint64_t *fp, uint64_t index)
{
    uint64_t num_args = frame_num_args(fp);
    return fp[FP_ARG_BASE_WORDS + (num_args - 1 - index)];
}

void frame_store_temp(uint64_t *fp, uint64_t index, uint64_t value)
{
    fp[-(int64_t)(FP_TEMP_BASE_WORDS + index)] = value;
}

void frame_return(uint64_t **sp_ptr, uint64_t **fp_ptr, uint64_t *ip_ptr, uint64_t return_value)
{
    uint64_t *fp = *fp_ptr;
    uint64_t num_args = frame_num_args(fp);
    uint64_t *new_sp = fp + FP_ARG_BASE_WORDS + num_args;

    *new_sp = return_value;
    *fp_ptr = (uint64_t *)fp[FRAME_SAVED_FP];
    *ip_ptr = fp[FRAME_SAVED_IP];
    *sp_ptr = new_sp;
}
