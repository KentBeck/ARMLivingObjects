// ARM64 Smalltalk context/frame prototype
// Stack grows downward, as in the Cog VM.

.global _stack_push
.global _stack_pop
.global _stack_top
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
.global _bc_push_self
.global _bc_push_temp
.global _bc_push_inst_var
.global _bc_push_literal
.global _bc_store_temp
.global _bc_store_inst_var
.global _bc_return_stack_top
.global _bc_duplicate
.global _bc_pop
.global _tag_smallint
.global _untag_smallint
.global _get_tag
.global _is_smallint
.global _is_object_ptr
.global _is_immediate_float
.global _is_special
.global _tagged_nil
.global _tagged_true
.global _tagged_false
.global _is_nil
.global _is_boolean
.global _smallint_add
.global _smallint_sub
.global _smallint_less_than
.global _smallint_equal
.global _om_init
.global _om_alloc

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

    // Zero-initialize local temps using stp to clear 2 words per instruction
    mov     x19, x5            // x19 = num_temps counter
    cbz     x19, .Ltemps_done
    // Handle odd temp: if count is odd, zero one slot first
    tbnz    x19, #0, .Ltemps_odd
    b       .Ltemps_pairs
.Ltemps_odd:
    sub     x6, x6, #8
    str     xzr, [x6]
    sub     x19, x19, #1
    cbz     x19, .Ltemps_done
.Ltemps_pairs:
    // Zero two words at a time: stp xzr, xzr, [x6, #-16]!
    stp     xzr, xzr, [x6, #-16]!
    sub     x19, x19, #2
    cbnz    x19, .Ltemps_pairs
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

// frame_receiver(fp) -> uint64_t
// x0 = frame pointer
// Returns the receiver at FP - 4*W (offset -32).
_frame_receiver:
    ldr     x0, [x0, #-32]     // receiver is at FP - 4*8
    ret

// frame_method(fp) -> uint64_t
// x0 = frame pointer
// Returns the method at FP - 1*W (offset -8).
_frame_method:
    ldr     x0, [x0, #-8]      // method is at FP - 1*8
    ret

// frame_flags(fp) -> uint64_t
// Returns the full flags word at FP - 2*W.
_frame_flags:
    ldr     x0, [x0, #-16]     // flags at FP - 2*8
    ret

// frame_num_args(fp) -> uint64_t
// Returns num_args from flags byte 1.
_frame_num_args:
    ldr     x1, [x0, #-16]     // flags word
    ubfx    x0, x1, #8, #8     // extract byte 1 (bits 15:8)
    ret

// frame_is_block(fp) -> uint64_t
// Returns is_block from flags byte 2.
_frame_is_block:
    ldr     x1, [x0, #-16]
    ubfx    x0, x1, #16, #8    // extract byte 2 (bits 23:16)
    ret

// frame_has_context(fp) -> uint64_t
// Returns has_context from flags byte 0.
_frame_has_context:
    ldr     x1, [x0, #-16]
    and     x0, x1, #0xFF      // extract byte 0 (bits 7:0)
    ret

// frame_temp(fp, index) -> uint64_t
// x0 = frame pointer, x1 = temp index (0-based)
// Temp N is at FP - (5+N)*W
_frame_temp:
    add     x1, x1, #5          // offset = 5 + index
    lsl     x1, x1, #3          // offset * 8
    sub     x2, x0, x1          // FP - offset
    ldr     x0, [x2]
    ret

// frame_arg(fp, arg_index) -> uint64_t
// x0 = frame pointer, x1 = arg index (0-based, where 0 = last pushed arg)
// Arg N is at FP + (2+N)*W
_frame_arg:
    add     x1, x1, #2          // offset = 2 + index
    lsl     x1, x1, #3          // offset * 8
    add     x2, x0, x1          // FP + offset
    ldr     x0, [x2]
    ret

// frame_store_temp(fp, index, value)
// x0 = frame pointer, x1 = temp index, x2 = value
// Stores value at FP - (5+index)*W
_frame_store_temp:
    add     x1, x1, #5
    lsl     x1, x1, #3
    sub     x3, x0, x1
    str     x2, [x3]
    ret

// frame_return(sp_ptr, fp_ptr, ip_ptr, return_value)
// x0 = pointer to SP variable
// x1 = pointer to FP variable
// x2 = pointer to IP variable (receives restored caller IP)
// x3 = return value
//
// Dismantles the current frame:
//   1. Read num_args from flags at FP - 2*W
//   2. Set SP = FP + (2 + num_args) * W  (pop frame + args, point at receiver slot)
//   3. Store return_value at SP (replaces receiver)
//   4. Restore caller FP from FP + 0
//   5. Restore caller IP from FP + 1*W
_frame_return:
    ldr     x4, [x1]           // x4 = current FP

    // Read num_args from flags (byte 1 of flags word at FP - 2*W)
    ldr     x5, [x4, #-16]     // flags word
    ubfx    x5, x5, #8, #8     // num_args

    // SP = FP + (2 + num_args) * 8  (points at receiver slot in caller)
    add     x5, x5, #2
    lsl     x5, x5, #3
    add     x6, x4, x5         // new SP

    // Store return value at new SP (replaces receiver)
    str     x3, [x6]

    // Restore caller FP
    ldr     x7, [x4]           // saved caller FP at FP + 0
    str     x7, [x1]           // write back FP

    // Restore caller IP
    ldr     x8, [x4, #8]       // saved caller IP at FP + 1*W
    str     x8, [x2]           // write back IP

    // Write back SP
    str     x6, [x0]

    ret

// === Bytecode Implementations ===
// All bytecodes take (sp_ptr, fp_ptr) as first two args.
// Additional args vary per bytecode.

// bc_push_self(sp_ptr, fp_ptr)
// Bytecode 3: PUSH_SELF — push receiver from FP - 4*W onto stack
_bc_push_self:
    ldr     x2, [x1]           // x2 = FP
    ldr     x3, [x2, #-32]     // receiver at FP - 4*8
    ldr     x4, [x0]           // x4 = SP
    sub     x4, x4, #8
    str     x3, [x4]           // push receiver
    str     x4, [x0]           // write back SP
    ret

// bc_push_temp(sp_ptr, fp_ptr, index)
// Bytecode 2: PUSH_TEMPORARY_VARIABLE — push temp N onto stack
// x2 = temp index
_bc_push_temp:
    ldr     x3, [x1]           // x3 = FP
    add     x4, x2, #5         // offset = 5 + index
    lsl     x4, x4, #3         // * 8
    sub     x5, x3, x4         // FP - offset
    ldr     x6, [x5]           // temp value
    ldr     x7, [x0]           // SP
    sub     x7, x7, #8
    str     x6, [x7]           // push
    str     x7, [x0]           // write back SP
    ret

// bc_push_inst_var(sp_ptr, fp_ptr, field_index)
// Bytecode 1: PUSH_INSTANCE_VARIABLE — push field N of receiver
// x2 = field index
// Receiver is treated as a pointer to an array of uint64_t fields.
_bc_push_inst_var:
    ldr     x3, [x1]           // FP
    ldr     x4, [x3, #-32]     // receiver pointer
    lsl     x5, x2, #3         // field_index * 8
    add     x6, x4, x5
    ldr     x7, [x6]           // load field
    ldr     x8, [x0]           // SP
    sub     x8, x8, #8
    str     x7, [x8]
    str     x8, [x0]
    ret

// bc_push_literal(sp_ptr, fp_ptr, literal_index)
// Bytecode 0: PUSH_LITERAL — push literal N from method's literal area
// x2 = literal index
// The method pointer (at FP - 1*W) is treated as a pointer to an array of
// uint64_t values, where entry 0 is the literal at index 0.
_bc_push_literal:
    ldr     x3, [x1]           // FP
    ldr     x4, [x3, #-8]      // method pointer
    lsl     x5, x2, #3         // literal_index * 8
    add     x6, x4, x5
    ldr     x7, [x6]           // load literal
    ldr     x8, [x0]           // SP
    sub     x8, x8, #8
    str     x7, [x8]
    str     x8, [x0]
    ret

// bc_store_temp(sp_ptr, fp_ptr, index)
// Bytecode 5: STORE_TEMPORARY_VARIABLE — pop and store into temp N
// x2 = temp index
_bc_store_temp:
    ldr     x3, [x0]           // SP
    ldr     x4, [x3]           // pop value
    add     x3, x3, #8
    str     x3, [x0]           // write back SP
    ldr     x5, [x1]           // FP
    add     x6, x2, #5
    lsl     x6, x6, #3
    sub     x7, x5, x6         // FP - (5+index)*8
    str     x4, [x7]           // store into temp
    ret

// bc_store_inst_var(sp_ptr, fp_ptr, field_index)
// Bytecode 4: STORE_INSTANCE_VARIABLE — pop and store into receiver field N
// x2 = field index
_bc_store_inst_var:
    ldr     x3, [x0]           // SP
    ldr     x4, [x3]           // pop value
    add     x3, x3, #8
    str     x3, [x0]           // write back SP
    ldr     x5, [x1]           // FP
    ldr     x6, [x5, #-32]     // receiver pointer
    lsl     x7, x2, #3
    add     x8, x6, x7
    str     x4, [x8]           // store into field
    ret

// bc_return_stack_top(sp_ptr, fp_ptr, ip_ptr)
// Bytecode 7: RETURN_STACK_TOP — pop top of stack and return to caller
// x2 = pointer to IP variable
_bc_return_stack_top:
    // Pop return value
    ldr     x3, [x0]           // SP
    ldr     x4, [x3]           // return value
    // Now do frame_return logic inline
    ldr     x5, [x1]           // FP
    ldr     x6, [x5, #-16]     // flags
    ubfx    x6, x6, #8, #8     // num_args
    add     x6, x6, #2
    lsl     x6, x6, #3
    add     x7, x5, x6         // new SP = FP + (2+num_args)*8
    str     x4, [x7]           // store return value
    ldr     x8, [x5]           // saved caller FP
    str     x8, [x1]           // restore FP
    ldr     x9, [x5, #8]       // saved caller IP
    str     x9, [x2]           // restore IP
    str     x7, [x0]           // write back SP
    ret

// bc_duplicate(sp_ptr)
// Bytecode 12: DUPLICATE — push a copy of top of stack
// x0 = pointer to SP
_bc_duplicate:
    ldr     x1, [x0]           // SP
    ldr     x2, [x1]           // top value
    sub     x1, x1, #8
    str     x2, [x1]           // push copy
    str     x1, [x0]           // write back SP
    ret

// bc_pop(sp_ptr)
// Bytecode 11: POP — discard top of stack
// x0 = pointer to SP
_bc_pop:
    ldr     x1, [x0]           // SP
    add     x1, x1, #8         // pop
    str     x1, [x0]           // write back SP
    ret

// === Tagged Pointers ===
// Tag bits (1:0): 00=object pointer, 01=SmallInteger, 10=float, 11=special

// tag_smallint(int64_t value) -> uint64_t
// Encode: (value << 2) | 0b01
_tag_smallint:
    lsl     x0, x0, #2
    orr     x0, x0, #1
    ret

// untag_smallint(uint64_t tagged) -> int64_t
// Decode: arithmetic right shift 2
_untag_smallint:
    asr     x0, x0, #2
    ret

// get_tag(uint64_t value) -> uint64_t
// Returns bits 1:0 of value.
_get_tag:
    and     x0, x0, #3
    ret

// is_smallint(uint64_t value) -> uint64_t (1 or 0)
_is_smallint:
    and     x0, x0, #3
    cmp     x0, #1
    cset    x0, eq
    ret

// is_object_ptr(uint64_t value) -> uint64_t (1 or 0)
_is_object_ptr:
    and     x0, x0, #3
    cmp     x0, #0
    cset    x0, eq
    ret

// is_immediate_float(uint64_t value) -> uint64_t (1 or 0)
_is_immediate_float:
    and     x0, x0, #3
    cmp     x0, #2
    cset    x0, eq
    ret

// is_special(uint64_t value) -> uint64_t (1 or 0)
_is_special:
    and     x0, x0, #3
    cmp     x0, #3
    cset    x0, eq
    ret

// tagged_nil() -> uint64_t   = 0x03
_tagged_nil:
    mov     x0, #3
    ret

// tagged_true() -> uint64_t  = 0x07
_tagged_true:
    mov     x0, #7
    ret

// tagged_false() -> uint64_t = 0x0B
_tagged_false:
    mov     x0, #11
    ret

// is_nil(uint64_t value) -> uint64_t (1 or 0)
_is_nil:
    cmp     x0, #3
    cset    x0, eq
    ret

// is_boolean(uint64_t value) -> uint64_t (1 or 0)
// true=0x07, false=0x0B. Check: value == 7 || value == 11
_is_boolean:
    cmp     x0, #7
    cset    x1, eq
    cmp     x0, #11
    cset    x2, eq
    orr     x0, x1, x2
    ret

// smallint_add(uint64_t a, uint64_t b) -> uint64_t
// Both a and b are tagged SmallIntegers. Result is tagged.
// Shortcut: (a - 1) + b  (subtract one tag bit, add keeps the other)
// Actually: a + b - 1 works because tag(a)+tag(b) = 01+01 = 10, minus 1 = 01
_smallint_add:
    add     x0, x0, x1
    sub     x0, x0, #1
    ret

// smallint_sub(uint64_t a, uint64_t b) -> uint64_t
// a - b + 1 (tag correction: 01 - 01 = 00, + 1 = 01)
_smallint_sub:
    sub     x0, x0, x1
    add     x0, x0, #1
    ret

// smallint_less_than(uint64_t a, uint64_t b) -> uint64_t
// Returns tagged true (0x07) or tagged false (0x0B).
// Since both are tagged the same way, we can compare directly.
_smallint_less_than:
    cmp     x0, x1
    mov     x0, #7              // true
    mov     x1, #11             // false
    csel    x0, x0, x1, lt      // signed less-than
    ret

// smallint_equal(uint64_t a, uint64_t b) -> uint64_t
// Returns tagged true or false. Direct comparison works on tagged values.
_smallint_equal:
    cmp     x0, x1
    mov     x0, #7
    mov     x1, #11
    csel    x0, x0, x1, eq
    ret

// === Object Memory ===
// Fixed-size bump allocator. Crash (brk #1) on OOM.
//
// Object layout (3-word header + fields):
//   word 0 = class pointer (tagged object pointer to a Class)
//   word 1 = format (0=fields, 1=indexable, 2=bytes)
//   word 2 = size (slot count or byte count)
//   word 3..N = slots (tagged values or raw bytes)

// om_init(buffer, buffer_size_bytes, free_ptr_out)
// x0 = pointer to buffer
// x1 = buffer size in bytes
// x2 = pointer to free_ptr variable (uint64_t*)
// Sets free_ptr to start of buffer. Stores end in free_ptr[1].
_om_init:
    str     x0, [x2]           // free_ptr = buffer start
    add     x1, x0, x1         // end = buffer + size
    str     x1, [x2, #8]       // store end after free_ptr
    ret

// om_alloc(free_ptr_var, class_ptr, format, size) -> object pointer
// x0 = pointer to {free_ptr, end_ptr} pair
// x1 = class pointer (tagged)
// x2 = format (0=fields, 1=indexable, 2=bytes)
// x3 = size (slot count for format 0/1, byte count for format 2)
// Returns: object pointer (untagged, aligned)
_om_alloc:
    ldr     x4, [x0]           // x4 = free_ptr
    ldr     x5, [x0, #8]       // x5 = end_ptr

    // Calculate total words: 3 (header) + slot_words
    // For format 2 (bytes): slot_words = (size + 7) / 8
    cmp     x2, #2
    b.eq    .Lalloc_bytes
    mov     x6, x3             // format 0/1: slot_words = size
    b       .Lalloc_calc
.Lalloc_bytes:
    add     x6, x3, #7         // (size + 7)
    lsr     x6, x6, #3         // / 8
.Lalloc_calc:
    add     x6, x6, #3         // + 3 header words
    lsl     x7, x6, #3         // total bytes = total_words * 8

    // Check OOM
    add     x8, x4, x7         // new_free = free_ptr + total_bytes
    cmp     x8, x5             // new_free > end?
    b.hi    .Loom

    // Write header
    str     x1, [x4]           // word 0: class pointer
    str     x2, [x4, #8]       // word 1: format
    str     x3, [x4, #16]      // word 2: size

    // Zero-initialize slots
    mov     x9, x4
    add     x9, x9, #24        // skip header (3 * 8)
    mov     x10, x6
    sub     x10, x10, #3       // slot_words (total - header)
    cbz     x10, .Lalloc_done
.Lalloc_zero:
    str     xzr, [x9], #8
    sub     x10, x10, #1
    cbnz    x10, .Lalloc_zero

.Lalloc_done:
    // Update free_ptr
    str     x8, [x0]

    // Return object pointer (x4 = start of object)
    mov     x0, x4
    ret

.Loom:
    brk     #1                  // crash on OOM