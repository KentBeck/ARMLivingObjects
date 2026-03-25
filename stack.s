// ARM64 Smalltalk context/frame prototype
// Stack grows downward, as in the Cog VM.

.global _stack_push
.global _stack_pop
.global _stack_top
.global _activate_method

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
// Bytecode 11: POP
_stack_pop:
    ldr     x3, [x0]           // x3 = current SP
    ldr     x1, [x3]           // x1 = value at SP
    add     x3, x3, #8         // SP += 8 (shrink down, stack grows down)
    str     x3, [x0]           // write back updated SP
    mov     x0, x1             // return the popped value
    ret

// activate_method(sp_ptr, fp_ptr, saved_ip, method, num_args, num_temps)
// x0 = pointer to SP variable
// x1 = pointer to FP variable
// x2 = saved caller IP
// x3 = method pointer
// x4 = num_args
// x5 = num_temps (local temps only, not including args)
//
// Caller has already pushed receiver (and args) onto the stack.
// We build the frame: push saved IP, push saved FP, set FP=SP,
// push method, push flags, push 0 (context), push receiver, push 0 per temp.
//
// Frame layout (from FP):
//   FP + 1*W = saved caller IP
//   FP + 0   = saved caller FP
//   FP - 1*W = method
//   FP - 2*W = flags (has_context | num_args<<8 | is_block<<16)
//   FP - 3*W = context slot (0 = nil)
//   FP - 4*W = receiver
//   FP - 5*W = temp 0 ...
_activate_method:
    // Save callee-saved registers we'll use
    stp     x19, x20, [sp, #-16]!
    stp     x21, x22, [sp, #-16]!

    // Load current SP and FP from memory
    ldr     x6, [x0]           // x6 = current smalltalk SP
    ldr     x7, [x1]           // x7 = current smalltalk FP

    // Read receiver from stack: it's at SP + num_args * 8
    // (args are on top, receiver is below them)
    lsl     x8, x4, #3         // x8 = num_args * 8
    add     x9, x6, x8         // x9 = SP + num_args * 8
    ldr     x10, [x9]          // x10 = receiver

    // Push saved caller IP
    sub     x6, x6, #8
    str     x2, [x6]

    // Push saved caller FP
    sub     x6, x6, #8
    str     x7, [x6]

    // Set FP = SP (FP points at saved caller FP slot)
    mov     x7, x6

    // Push method
    sub     x6, x6, #8
    str     x3, [x6]

    // Encode and push flags: (is_block<<16 | num_args<<8 | has_context)
    // For method activation: is_block=0, has_context=0
    lsl     x11, x4, #8        // num_args << 8
    sub     x6, x6, #8
    str     x11, [x6]

    // Push context slot (0 = nil)
    sub     x6, x6, #8
    str     xzr, [x6]

    // Push receiver (again, for fast access at FP - 4*W)
    sub     x6, x6, #8
    str     x10, [x6]

    // Push 0 for each local temp
    mov     x19, x5            // x19 = num_temps counter
    cbz     x19, .Ltemps_done
.Ltemps_loop:
    sub     x6, x6, #8
    str     xzr, [x6]
    sub     x19, x19, #1
    cbnz    x19, .Ltemps_loop
.Ltemps_done:

    // Write back SP and FP
    str     x6, [x0]
    str     x7, [x1]

    // Restore callee-saved registers
    ldp     x21, x22, [sp], #16
    ldp     x19, x20, [sp], #16
    ret

// stack_top(uint64_t **sp_ptr) -> uint64_t
// x0 = pointer to the stack pointer variable
// Returns the value at the top of stack without popping.
_stack_top:
    ldr     x3, [x0]           // x3 = current SP
    ldr     x0, [x3]           // return value at SP
    ret

