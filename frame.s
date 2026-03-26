// frame.s — Frame activation, return, and field accessors

.global _activate_method
.global _frame_receiver
.global _frame_method
.global _frame_flags
.global _frame_num_args
.global _frame_is_block
.global _frame_has_context
.global _frame_temp
.global _frame_arg
.global _frame_store_temp
.global _frame_return

.align 2

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
    stp     x19, x20, [sp, #-16]!
    stp     x21, x22, [sp, #-16]!

    ldr     x6, [x0]           // x6 = current smalltalk SP
    ldr     x7, [x1]           // x7 = current smalltalk FP

    // Read receiver from stack: it's at SP + num_args * 8
    lsl     x8, x4, #3
    add     x9, x6, x8
    ldr     x10, [x9]          // x10 = receiver

    // Push saved caller IP
    sub     x6, x6, #8
    str     x2, [x6]

    // Push saved caller FP
    sub     x6, x6, #8
    str     x7, [x6]

    // Set FP = SP
    mov     x7, x6

    // Push method
    sub     x6, x6, #8
    str     x3, [x6]

    // Encode and push flags: (is_block<<16 | num_args<<8 | has_context)
    lsl     x11, x4, #8
    sub     x6, x6, #8
    str     x11, [x6]

    // Push context slot (0 = nil)
    sub     x6, x6, #8
    str     xzr, [x6]

    // Push receiver (again, for fast access at FP - 4*W)
    sub     x6, x6, #8
    str     x10, [x6]

    // Zero-initialize local temps using stp to clear 2 words per instruction
    mov     x19, x5
    cbz     x19, .Ltemps_done
    tbnz    x19, #0, .Ltemps_odd
    b       .Ltemps_pairs
.Ltemps_odd:
    sub     x6, x6, #8
    str     xzr, [x6]
    sub     x19, x19, #1
    cbz     x19, .Ltemps_done
.Ltemps_pairs:
    stp     xzr, xzr, [x6, #-16]!
    sub     x19, x19, #2
    cbnz    x19, .Ltemps_pairs
.Ltemps_done:

    str     x6, [x0]
    str     x7, [x1]

    ldp     x21, x22, [sp], #16
    ldp     x19, x20, [sp], #16
    ret

// frame_receiver(fp) -> uint64_t
_frame_receiver:
    ldr     x0, [x0, #-32]     // FP - 4*8
    ret

// frame_method(fp) -> uint64_t
_frame_method:
    ldr     x0, [x0, #-8]      // FP - 1*8
    ret

// frame_flags(fp) -> uint64_t
_frame_flags:
    ldr     x0, [x0, #-16]     // FP - 2*8
    ret

// frame_num_args(fp) -> uint64_t
_frame_num_args:
    ldr     x1, [x0, #-16]
    ubfx    x0, x1, #8, #8
    ret

// frame_is_block(fp) -> uint64_t
_frame_is_block:
    ldr     x1, [x0, #-16]
    ubfx    x0, x1, #16, #8
    ret

// frame_has_context(fp) -> uint64_t
_frame_has_context:
    ldr     x1, [x0, #-16]
    and     x0, x1, #0xFF
    ret

// frame_temp(fp, index) -> uint64_t
_frame_temp:
    add     x1, x1, #5
    lsl     x1, x1, #3
    sub     x2, x0, x1
    ldr     x0, [x2]
    ret

// frame_arg(fp, arg_index) -> uint64_t
_frame_arg:
    add     x1, x1, #2
    lsl     x1, x1, #3
    add     x2, x0, x1
    ldr     x0, [x2]
    ret

// frame_store_temp(fp, index, value)
_frame_store_temp:
    add     x1, x1, #5
    lsl     x1, x1, #3
    sub     x3, x0, x1
    str     x2, [x3]
    ret

// frame_return(sp_ptr, fp_ptr, ip_ptr, return_value)
// Dismantles the current frame.
_frame_return:
    ldr     x4, [x1]           // x4 = current FP
    ldr     x5, [x4, #-16]     // flags word
    ubfx    x5, x5, #8, #8     // num_args
    add     x5, x5, #2
    lsl     x5, x5, #3
    add     x6, x4, x5         // new SP
    str     x3, [x6]           // store return value
    ldr     x7, [x4]           // saved caller FP
    str     x7, [x1]
    ldr     x8, [x4, #8]       // saved caller IP
    str     x8, [x2]
    str     x6, [x0]
    ret

