// stack_ops.s — Stack push/pop/top primitives
// Stack grows downward, as in the Cog VM.

.global _stack_push
.global _stack_pop
.global _stack_top

.align 2

// stack_push(uint64_t *sp_ptr, uint64_t *stack_base, uint64_t value)
// x0 = pointer to the stack pointer variable (so we can update it)
// x1 = (unused for now, stack base for bounds checking later)
// x2 = value to push
// Decrements SP by 8, stores value at new SP.
_stack_push:
    ldr     x3, [x0]           // x3 = current SP
    sub     x3, x3, #8         // SP -= 8 (grow down)
    str     x2, [x3]           // store value at new SP
    str     x3, [x0]           // write back updated SP
    ret

// stack_pop(uint64_t **sp_ptr) -> uint64_t
// x0 = pointer to the stack pointer variable
// Returns the value at SP, then increments SP by 8.
_stack_pop:
    ldr     x3, [x0]           // x3 = current SP
    ldr     x1, [x3]           // x1 = value at SP
    add     x3, x3, #8         // SP += 8 (shrink down, stack grows down)
    str     x3, [x0]           // write back updated SP
    mov     x0, x1             // return the popped value
    ret

// stack_top(uint64_t **sp_ptr) -> uint64_t
// x0 = pointer to the stack pointer variable
// Returns the value at the top of stack without popping.
_stack_top:
    ldr     x3, [x0]           // x3 = current SP
    ldr     x0, [x3]           // return value at SP
    ret

