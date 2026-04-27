#include "vm_defs.h"

#include <signal.h>
#include <stdint.h>

extern Oop *smalltalk_stack_limit_low;

static void trap_stack_overflow(void)
{
    raise(SIGTRAP);
}

static uint64_t decode_smallint_field(Oop value)
{
    return (value & TAG_MASK) == TAG_SMALLINT
               ? (uint64_t)(((int64_t)value) >> SMALLINT_SHIFT)
               : 0;
}

static uint64_t context_num_args(ObjPtr context)
{
    return decode_smallint_field(OBJ_FIELD(context, CONTEXT_NUM_ARGS));
}

static uint64_t context_num_temps(ObjPtr context)
{
    return decode_smallint_field(OBJ_FIELD(context, CONTEXT_NUM_TEMPS));
}

static uint64_t context_stack_size(ObjPtr context)
{
    return decode_smallint_field(OBJ_FIELD(context, CONTEXT_STACK_SIZE));
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

void activate_context(Oop **sp_ptr, ObjPtr *fp_ptr, uint64_t saved_ip,
                      ObjPtr context)
{
    uint64_t num_args;
    uint64_t num_temps;
    uint64_t stack_size;
    Oop *sp = *sp_ptr;

    if (context == 0 || (((Oop)context) & TAG_MASK) != TAG_OBJECT)
    {
        raise(SIGTRAP);
        return;
    }

    num_args = context_num_args(context);
    num_temps = context_num_temps(context);
    stack_size = context_stack_size(context);

    if (sp - (num_args + 1 + stack_size) < smalltalk_stack_limit_low)
    {
        trap_stack_overflow();
    }

    *--sp = OBJ_FIELD(context, CONTEXT_RECEIVER);
    for (uint64_t index = 0; index < num_args; index++)
    {
        *--sp = OBJ_FIELD(context, CONTEXT_VAR_BASE + index);
    }

    *sp_ptr = sp;
    activate_method(sp_ptr, fp_ptr, saved_ip, OBJ_FIELD(context, CONTEXT_METHOD), num_args, num_temps);

    ObjPtr fp = *fp_ptr;
    uint64_t saved_flags = decode_smallint_field(OBJ_FIELD(context, CONTEXT_FLAGS));
    fp[FRAME_FLAGS] |= FRAME_FLAGS_HAS_CONTEXT_MASK |
                       (saved_flags & (UINT64_C(0xFF) << FRAME_FLAGS_IS_BLOCK_SHIFT));
    fp[FRAME_FLAGS] &= ~FRAME_FLAGS_BLOCK_CLOSURE_MASK;
    fp[FRAME_CONTEXT] = (Oop)context;

    for (uint64_t index = 0; index < num_temps; index++)
    {
        fp[-(int64_t)(FP_TEMP_BASE_WORDS + index)] =
            OBJ_FIELD(context, CONTEXT_VAR_BASE + num_args + index);
    }

    if (stack_size != 0)
    {
        Oop *base_sp = *sp_ptr;
        Oop *new_sp = base_sp - stack_size;
        if (new_sp < smalltalk_stack_limit_low)
        {
            trap_stack_overflow();
        }
        for (uint64_t index = 0; index < stack_size; index++)
        {
            new_sp[index] = OBJ_FIELD(context, CONTEXT_VAR_BASE + num_args + num_temps + index);
        }
        *sp_ptr = new_sp;
    }
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
    ObjPtr caller_fp = (ObjPtr)fp[FRAME_SAVED_FP];

    *new_sp = return_value;
    if (caller_fp != 0)
    {
        *fp_ptr = caller_fp;
        *ip_ptr = fp[FRAME_SAVED_IP];
        *sp_ptr = new_sp;
        return;
    }

    if ((fp[FRAME_FLAGS] & FRAME_FLAGS_HAS_CONTEXT_MASK) != 0)
    {
        ObjPtr context = (ObjPtr)fp[FRAME_CONTEXT];
        Oop sender_oop = OBJ_FIELD(context, CONTEXT_SENDER);
        if ((sender_oop & TAG_MASK) == TAG_OBJECT && sender_oop != 0)
        {
            *sp_ptr = new_sp;
            *fp_ptr = 0;
            *ip_ptr = 0;
            activate_context(sp_ptr, fp_ptr, 0, (ObjPtr)sender_oop);
            Oop *sender_sp = *sp_ptr - 1;
            *sender_sp = return_value;
            *sp_ptr = sender_sp;
            *ip_ptr = (uint64_t)OBJ_FIELD((ObjPtr)sender_oop, CONTEXT_IP);
            return;
        }
    }

    *fp_ptr = 0;
    *ip_ptr = 0;
    *sp_ptr = new_sp;
}
