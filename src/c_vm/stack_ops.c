#include "vm_defs.h"

#include <signal.h>
#include <stdint.h>

Oop *smalltalk_stack_limit_low = 0;

static void trap_stack_overflow(void)
{
    raise(SIGTRAP);
}

void stack_push(Oop **sp_ptr, Oop *stack_base, Oop value)
{
    smalltalk_stack_limit_low = stack_base;

    Oop *sp = *sp_ptr - 1;
    if (sp < stack_base)
    {
        trap_stack_overflow();
    }

    *sp = value;
    *sp_ptr = sp;
}

Oop stack_pop(Oop **sp_ptr)
{
    Oop *sp = *sp_ptr;
    Oop value = *sp;
    *sp_ptr = sp + 1;
    return value;
}

Oop stack_top(Oop **sp_ptr)
{
    return **sp_ptr;
}
