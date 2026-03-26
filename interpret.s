// interpret.s — Bytecode dispatch loop
//
// interpret(sp_ptr, fp_ptr, ip) -> result (tagged value)
// x0 = pointer to SP variable
// x1 = pointer to FP variable
// x2 = IP: pointer to first bytecode (in a ByteArray's data area)
//
// Runs bytecodes until RETURN_STACK_TOP (7) or HALT (13).
// Returns the result value (top of stack at exit).
//
// Bytecode format (from LivingObjects):
//   0  PUSH_LITERAL            + 4-byte index
//   1  PUSH_INSTANCE_VARIABLE  + 4-byte index
//   2  PUSH_TEMPORARY_VARIABLE + 4-byte index
//   3  PUSH_SELF
//   4  STORE_INSTANCE_VARIABLE + 4-byte index
//   5  STORE_TEMPORARY_VARIABLE+ 4-byte index
//   6  SEND_MESSAGE            + 4-byte selector + 4-byte arg_count
//   7  RETURN_STACK_TOP
//   8  JUMP                    + 4-byte offset
//   9  JUMP_IF_TRUE            + 4-byte offset
//  10  JUMP_IF_FALSE           + 4-byte offset
//  11  POP
//  12  DUPLICATE
//  13  HALT (stop interpreter, return top of stack)
//
// Register conventions during dispatch:
//   x19 = sp_ptr (preserved)
//   x20 = fp_ptr (preserved)
//   x21 = IP (bytecode pointer, advances as we go)

.global _interpret

.align 2

_interpret:
    // Save callee-saved registers and link register
    stp     x29, x30, [sp, #-16]!
    mov     x29, sp
    stp     x19, x20, [sp, #-16]!
    stp     x21, x22, [sp, #-16]!
    stp     x23, x24, [sp, #-16]!

    mov     x19, x0             // x19 = sp_ptr
    mov     x20, x1             // x20 = fp_ptr
    mov     x21, x2             // x21 = IP
    mov     x23, x2             // x23 = bytecodes base (constant)

.Ldispatch:
    ldrb    w22, [x21], #1      // fetch opcode, advance IP

    // Dispatch via offset table (PIC-safe: store offsets from table base, not absolute addrs)
    adr     x3, .Ldispatch_table
    ldrsw   x4, [x3, x22, lsl #2]   // load signed 32-bit offset
    add     x4, x3, x4              // table_base + offset = handler address
    br      x4

.align 2
.Ldispatch_table:
    .long   .Lbc_push_literal    - .Ldispatch_table  // 0
    .long   .Lbc_push_inst_var   - .Ldispatch_table  // 1
    .long   .Lbc_push_temp       - .Ldispatch_table  // 2
    .long   .Lbc_push_self       - .Ldispatch_table  // 3
    .long   .Lbc_store_inst_var  - .Ldispatch_table  // 4
    .long   .Lbc_store_temp      - .Ldispatch_table  // 5
    .long   .Lbc_send_message    - .Ldispatch_table  // 6
    .long   .Lbc_return_stack_top- .Ldispatch_table  // 7
    .long   .Lbc_jump            - .Ldispatch_table  // 8
    .long   .Lbc_jump_if_true    - .Ldispatch_table  // 9
    .long   .Lbc_jump_if_false   - .Ldispatch_table  // 10
    .long   .Lbc_pop             - .Ldispatch_table  // 11
    .long   .Lbc_duplicate       - .Ldispatch_table  // 12
    .long   .Lbc_halt            - .Ldispatch_table  // 13

// --- Bytecode handlers ---
// Each reads operands from [x21] (IP), advances IP, operates on stack via x19/x20.

// Helper macro: read little-endian uint32 from [x21] into w5, advance x21 by 4
.macro READ_U32
    ldrb    w5, [x21]
    ldrb    w6, [x21, #1]
    ldrb    w7, [x21, #2]
    ldrb    w8, [x21, #3]
    orr     w5, w5, w6, lsl #8
    orr     w5, w5, w7, lsl #16
    orr     w5, w5, w8, lsl #24
    add     x21, x21, #4
.endm

.Lbc_push_literal:
    READ_U32                    // w5 = literal index
    ldr     x6, [x20]          // FP
    ldr     x7, [x6, #-8]      // method (CompiledMethod obj ptr)
    add     x7, x7, #48        // CM_FIRST_LITERAL offset
    ldr     x8, [x7, x5, lsl #3]  // literal value
    ldr     x9, [x19]          // SP
    sub     x9, x9, #8
    str     x8, [x9]
    str     x9, [x19]
    b       .Ldispatch

.Lbc_push_inst_var:
    READ_U32
    ldr     x6, [x20]          // FP
    ldr     x7, [x6, #-32]     // receiver
    add     x7, x7, #24        // skip 3-word header
    ldr     x8, [x7, x5, lsl #3]
    ldr     x9, [x19]
    sub     x9, x9, #8
    str     x8, [x9]
    str     x9, [x19]
    b       .Ldispatch

.Lbc_push_temp:
    READ_U32
    ldr     x6, [x20]          // FP
    add     x7, x5, #5
    lsl     x7, x7, #3
    sub     x8, x6, x7
    ldr     x9, [x8]
    ldr     x10, [x19]
    sub     x10, x10, #8
    str     x9, [x10]
    str     x10, [x19]
    b       .Ldispatch

.Lbc_push_self:
    ldr     x6, [x20]          // FP
    ldr     x7, [x6, #-32]     // receiver
    ldr     x8, [x19]          // SP
    sub     x8, x8, #8
    str     x7, [x8]
    str     x8, [x19]
    b       .Ldispatch

.Lbc_store_inst_var:
    READ_U32
    ldr     x6, [x19]          // SP
    ldr     x7, [x6]           // pop value
    add     x6, x6, #8
    str     x6, [x19]
    ldr     x8, [x20]          // FP
    ldr     x9, [x8, #-32]     // receiver
    add     x9, x9, #24        // skip header
    str     x7, [x9, x5, lsl #3]
    b       .Ldispatch

.Lbc_store_temp:
    READ_U32
    ldr     x6, [x19]          // SP
    ldr     x7, [x6]           // pop value
    add     x6, x6, #8
    str     x6, [x19]
    ldr     x8, [x20]          // FP
    add     x9, x5, #5
    lsl     x9, x9, #3
    sub     x10, x8, x9
    str     x7, [x10]
    b       .Ldispatch

.Lbc_send_message:
    // TODO: implement in Section 11
    brk     #2                  // not yet implemented

.Lbc_return_stack_top:
    // Pop return value, dismantle frame, exit interpreter
    ldr     x6, [x19]          // SP
    ldr     x0, [x6]           // return value (will be our result)
    // Dismantle frame
    ldr     x7, [x20]          // FP
    ldr     x8, [x7, #-16]     // flags
    ubfx    x8, x8, #8, #8     // num_args
    add     x8, x8, #2
    lsl     x8, x8, #3
    add     x9, x7, x8         // new SP
    str     x0, [x9]           // store result at new SP
    ldr     x10, [x7]          // saved caller FP
    str     x10, [x20]
    // Don't restore caller IP — we're exiting the interpreter
    str     x9, [x19]
    b       .Lexit

.Lbc_jump:
    READ_U32                    // x5 = absolute offset from bytecodes base
    add     x21, x23, x5       // new IP = base + offset
    b       .Ldispatch

.Lbc_jump_if_true:
    READ_U32                    // x5 = offset
    ldr     x10, [x19]         // SP
    ldr     x11, [x10]         // pop value
    add     x10, x10, #8
    str     x10, [x19]
    cmp     x11, #7             // tagged true?
    b.ne    .Ldispatch          // not true → fall through
    add     x21, x23, x5       // jump: IP = base + offset
    b       .Ldispatch

.Lbc_jump_if_false:
    READ_U32
    ldr     x10, [x19]
    ldr     x11, [x10]
    add     x10, x10, #8
    str     x10, [x19]
    cmp     x11, #11            // tagged false?
    b.ne    .Ldispatch
    add     x21, x23, x5
    b       .Ldispatch

.Lbc_pop:
    ldr     x6, [x19]
    add     x6, x6, #8
    str     x6, [x19]
    b       .Ldispatch

.Lbc_duplicate:
    ldr     x6, [x19]
    ldr     x7, [x6]
    sub     x6, x6, #8
    str     x7, [x6]
    str     x6, [x19]
    b       .Ldispatch

.Lbc_halt:
    // Return top of stack without dismantling frame
    ldr     x6, [x19]
    ldr     x0, [x6]
    b       .Lexit

.Lexit:
    ldp     x23, x24, [sp], #16
    ldp     x21, x22, [sp], #16
    ldp     x19, x20, [sp], #16
    ldp     x29, x30, [sp], #16
    ret
