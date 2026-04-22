#include "vm_defs.h"

#include <signal.h>
#include <stdint.h>

uint64_t *smalltalk_stack_limit_low = 0;

static void trap_stack_overflow(void)
{
    raise(SIGTRAP);
}

void stack_push(uint64_t **sp_ptr, uint64_t *stack_base, uint64_t value)
{
    smalltalk_stack_limit_low = stack_base;

    uint64_t *sp = *sp_ptr - 1;
    if (sp < stack_base)
    {
        trap_stack_overflow();
    }

    *sp = value;
    *sp_ptr = sp;
}

uint64_t stack_pop(uint64_t **sp_ptr)
{
    uint64_t *sp = *sp_ptr;
    uint64_t value = *sp;
    *sp_ptr = sp + 1;
    return value;
}

uint64_t stack_top(uint64_t **sp_ptr)
{
    return **sp_ptr;
}
