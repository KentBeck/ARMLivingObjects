// interpret.s — Bytecode dispatch loop
//
// interpret(sp_ptr, fp_ptr, ip, class_table, om) -> result (tagged value)
// x0 = pointer to SP variable
// x1 = pointer to FP variable
// x2 = IP: pointer to first bytecode (in a ByteArray's data area)
// x3 = pointer to class table: [0]=SmallInteger, [1]=Block, [2]=True, [3]=False
// x4 = pointer to om {free_ptr, end_ptr} pair (for allocation)
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
    stp     x25, x26, [sp, #-16]!

    mov     x19, x0             // x19 = sp_ptr
    mov     x20, x1             // x20 = fp_ptr
    mov     x21, x2             // x21 = IP
    mov     x23, x2             // x23 = bytecodes base
    ldr     x24, [x20]          // x24 = entry frame FP (for detecting top-level return)
    mov     x25, x3             // x25 = class table pointer
    mov     x26, x4             // x26 = om pointer

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
    .long   .Lbc_push_closure    - .Ldispatch_table  // 14
    .long   .Lbc_push_arg        - .Ldispatch_table  // 15

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
    ldr     x7, [x7, #48]      // CM_LITERALS → Array pointer
    add     x7, x7, #24        // skip Array 3-word header
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
    // Read selector_index (4 bytes) and arg_count (4 bytes)
    READ_U32                    // w5 = selector_index
    mov     x9, x5             // x9 = selector_index (save before next READ_U32 clobbers w5)
    READ_U32                    // w5 = arg_count
    mov     x10, x5             // x10 = arg_count

    // Get selector from current method's literals Array
    ldr     x11, [x20]          // FP
    ldr     x12, [x11, #-8]    // current method (CompiledMethod)
    ldr     x13, [x12, #48]    // CM_LITERALS → Array pointer
    add     x13, x13, #24      // skip Array header
    ldr     x14, [x13, x9, lsl #3]  // selector (tagged SmallInt)

    // Get receiver from stack: at SP + arg_count * 8
    ldr     x15, [x19]         // SP
    ldr     x0, [x15, x10, lsl #3]  // receiver

    // Look up receiver's class via oop_class(oop, class_table)
    mov     x1, x25            // class_table
    stp     x9, x10, [sp, #-16]!
    str     x14, [sp, #-16]!
    bl      _oop_class
    ldr     x14, [sp], #16
    ldp     x9, x10, [sp], #16
    mov     x1, x14            // selector
    // Save volatile state before function call
    stp     x9, x10, [sp, #-16]!
    str     x14, [sp, #-16]!   // save selector
    bl      _class_lookup       // x0 = found method or 0
    ldr     x14, [sp], #16
    ldp     x9, x10, [sp], #16

    // Check if method was found
    cbz     x0, .Lsend_not_found

    // Check for primitive: CM_PRIMITIVE at method + header(24) + field0(0) = offset 24
    ldr     x3, [x0, #24]      // primitive index (tagged)
    cmp     x3, #1              // tag_smallint(0) = 1 means no primitive
    b.ne    .Lsend_primitive

    // x0 = found CompiledMethod, x10 = arg_count, x21 = IP (past SEND operands)
    // Read num_temps from found method
    ldr     x3, [x0, #40]      // num_temps (tagged) at method + header(24) + field2(16)
    asr     x3, x3, #2         // untag num_temps

    // Activate: call _activate_method(sp_ptr, fp_ptr, saved_ip, method, num_args, num_temps)
    mov     x5, x3             // num_temps
    mov     x4, x10            // num_args
    mov     x3, x0             // method (CompiledMethod pointer)
    mov     x2, x21            // saved IP = current IP (will be restored on return)
    mov     x1, x20            // fp_ptr
    mov     x0, x19            // sp_ptr
    bl      _activate_method

    // Set IP to new method's bytecodes (CM_BYTECODES = field 4, offset 56)
    ldr     x6, [x20]          // new FP
    ldr     x7, [x6, #-8]      // new method
    ldr     x10, [x7, #56]     // CM_BYTECODES → ByteArray pointer
    add     x23, x10, #24      // skip ByteArray 3-word header → data
    mov     x21, x23            // IP = start of new method's bytecodes
    b       .Ldispatch

.Lsend_not_found:
    brk     #3                  // message not understood (trap for now)

.Lsend_primitive:
    // x0 = CompiledMethod, x3 = primitive index (tagged), x10 = arg_count
    // Decode primitive index
    asr     x3, x3, #2         // untag

    // Dispatch to primitive handler
    // Primitives operate on tagged values already on the stack.
    // Stack has: ... receiver arg0 arg1 ... (top = last arg, receiver below args)
    // After primitive: pop args and receiver, push result.

    cmp     x3, #1              // PRIM_SMALLINT_ADD
    b.eq    .Lprim_add
    cmp     x3, #2              // PRIM_SMALLINT_SUB
    b.eq    .Lprim_sub
    cmp     x3, #3              // PRIM_SMALLINT_LT
    b.eq    .Lprim_lt
    cmp     x3, #4              // PRIM_SMALLINT_EQ
    b.eq    .Lprim_eq
    cmp     x3, #5              // PRIM_SMALLINT_MUL
    b.eq    .Lprim_mul
    cmp     x3, #9              // PRIM_BLOCK_VALUE
    b.eq    .Lprim_block_value
    brk     #4                  // unknown primitive

.Lprim_add:
    // receiver + arg0  (both tagged SmallInts)
    ldr     x5, [x19]          // SP
    ldr     x6, [x5]           // arg0
    ldr     x7, [x5, #8]       // receiver
    add     x8, x7, x6
    sub     x8, x8, #1          // tag correction
    add     x5, x5, #16         // pop both
    sub     x5, x5, #8          // push result
    str     x8, [x5]
    str     x5, [x19]
    b       .Ldispatch

.Lprim_sub:
    ldr     x5, [x19]
    ldr     x6, [x5]           // arg0
    ldr     x7, [x5, #8]       // receiver
    sub     x8, x7, x6
    add     x8, x8, #1
    add     x5, x5, #8         // pop arg, result replaces receiver
    str     x8, [x5]
    str     x5, [x19]
    b       .Ldispatch

.Lprim_lt:
    ldr     x5, [x19]
    ldr     x6, [x5]           // arg0
    ldr     x7, [x5, #8]       // receiver
    cmp     x7, x6              // signed compare (works on tagged)
    mov     x8, #7              // tagged true
    mov     x9, #11             // tagged false
    csel    x8, x8, x9, lt
    add     x5, x5, #8
    str     x8, [x5]
    str     x5, [x19]
    b       .Ldispatch

.Lprim_eq:
    ldr     x5, [x19]
    ldr     x6, [x5]           // arg0
    ldr     x7, [x5, #8]       // receiver
    cmp     x7, x6
    mov     x8, #7
    mov     x9, #11
    csel    x8, x8, x9, eq
    add     x5, x5, #8
    str     x8, [x5]
    str     x5, [x19]
    b       .Ldispatch

.Lprim_mul:
    // receiver * arg0 (both tagged SmallInts)
    // tagged: (a<<2|1) * (b<<2|1) doesn't work directly.
    // Untag both, multiply, retag.
    ldr     x5, [x19]
    ldr     x6, [x5]           // arg0 (tagged)
    ldr     x7, [x5, #8]       // receiver (tagged)
    asr     x6, x6, #2         // untag arg0
    asr     x7, x7, #2         // untag receiver
    mul     x8, x7, x6         // result = receiver * arg0
    lsl     x8, x8, #2         // retag
    orr     x8, x8, #1
    add     x5, x5, #8         // pop arg, result replaces receiver
    str     x8, [x5]
    str     x5, [x19]
    b       .Ldispatch

.Lprim_block_value:
    // Block>>value: receiver is a Block object on stack.
    // Block field 0 (offset 24) = home receiver
    // Block field 1 (offset 32) = CompiledMethod
    // Pop block from stack, activate its CM with the home receiver, continue dispatch.
    ldr     x5, [x19]          // SP
    ldr     x6, [x5]           // block object (the receiver of #value)
    add     x5, x5, #8         // pop block
    str     x5, [x19]

    ldr     x7, [x6, #24]      // home receiver (block field 0)
    ldr     x8, [x6, #32]      // CM (block field 1)

    // Read num_temps from CM
    ldr     x3, [x8, #40]      // num_temps (tagged)
    asr     x3, x3, #2         // untag

    // Push home receiver as the "receiver" for activation
    ldr     x5, [x19]
    sub     x5, x5, #8
    str     x7, [x5]
    str     x5, [x19]

    // Activate: call _activate_method(sp_ptr, fp_ptr, saved_ip, method, num_args, num_temps)
    mov     x5, x3             // num_temps
    mov     x4, #0             // 0 args (no-arg block)
    mov     x3, x8             // method = block's CM
    mov     x2, x21            // saved IP = current IP
    mov     x1, x20            // fp_ptr
    mov     x0, x19            // sp_ptr
    bl      _activate_method

    // Set IP to block CM's bytecodes
    ldr     x6, [x20]          // new FP
    ldr     x7, [x6, #-8]      // method (block's CM)
    ldr     x10, [x7, #56]     // CM_BYTECODES → ByteArray
    add     x23, x10, #24      // skip header → data
    mov     x21, x23
    b       .Ldispatch

.Lbc_return_stack_top:
    // Pop return value, dismantle frame
    ldr     x6, [x19]          // SP
    ldr     x0, [x6]           // return value
    ldr     x7, [x20]          // FP (frame being dismantled)
    ldr     x9, [x7, #-16]     // flags
    ubfx    x9, x9, #8, #8     // num_args
    add     x9, x9, #2
    lsl     x9, x9, #3
    add     x10, x7, x9        // new SP = FP + (2+num_args)*8
    str     x0, [x10]          // store result at new SP (replaces receiver)
    ldr     x11, [x7]          // saved caller FP
    ldr     x12, [x7, #8]      // saved caller IP
    str     x10, [x19]         // write back SP
    str     x11, [x20]         // write back FP

    // Is this the entry frame? (FP we're dismantling == entry FP)
    cmp     x7, x24
    b.eq    .Lexit              // yes → exit interpreter with result in x0

    // Continue in caller: restore IP and recompute bytecodes base
    mov     x21, x12            // restore IP from saved caller IP

    // Recompute x23 = bytecodes base from caller's method
    ldr     x13, [x11, #-8]    // caller method at new FP - 1*W
    ldr     x15, [x13, #56]    // CM_BYTECODES (field 4) → ByteArray pointer
    add     x23, x15, #24      // skip ByteArray 3-word header → data
    b       .Ldispatch

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

.Lbc_push_arg:
    // PUSH_ARG: read 4-byte arg index, push arg from above frame
    // arg at FP + (2 + index) * 8
    READ_U32                    // w5 = arg index
    ldr     x6, [x20]          // FP
    add     x7, x5, #2
    ldr     x8, [x6, x7, lsl #3]
    ldr     x9, [x19]          // SP
    sub     x9, x9, #8
    str     x8, [x9]
    str     x9, [x19]
    b       .Ldispatch

.Lbc_push_closure:
    // PUSH_CLOSURE: read literal_index, get CM from literals,
    // allocate Block(2 fields: home_receiver, cm), push Block.
    READ_U32                    // w5 = literal index
    ldr     x6, [x20]          // FP
    ldr     x7, [x6, #-8]      // current method
    ldr     x7, [x7, #48]      // CM_LITERALS → Array
    add     x7, x7, #24        // skip Array header
    ldr     x9, [x7, x5, lsl #3]   // x9 = block CM (from literals)
    ldr     x10, [x6, #-32]    // x10 = current receiver (home receiver)
    // Allocate Block: om_alloc(om, block_class, FORMAT_FIELDS, 2)
    mov     x0, x26             // om
    ldr     x1, [x25, #8]      // class_table[1] = Block class
    mov     x2, #0              // FORMAT_FIELDS
    mov     x3, #2              // 2 fields
    // Save volatile state
    stp     x9, x10, [sp, #-16]!
    bl      _om_alloc
    ldp     x9, x10, [sp], #16
    // x0 = new Block object
    str     x10, [x0, #24]     // field 0 = home receiver (offset 24 = header)
    str     x9, [x0, #32]      // field 1 = CM (offset 32)
    // Push Block onto stack
    ldr     x6, [x19]          // SP
    sub     x6, x6, #8
    str     x0, [x6]
    str     x6, [x19]
    b       .Ldispatch

.Lbc_halt:
    // Return top of stack without dismantling frame
    ldr     x6, [x19]
    ldr     x0, [x6]
    b       .Lexit

.Lexit:
    ldp     x25, x26, [sp], #16
    ldp     x23, x24, [sp], #16
    ldp     x21, x22, [sp], #16
    ldp     x19, x20, [sp], #16
    ldp     x29, x30, [sp], #16
    ret
