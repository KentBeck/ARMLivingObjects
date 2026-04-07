// interpret.s — Bytecode dispatch loop

.include "macros.s"
.include "asm_constants_shared.s"

// Primitive IDs (interpreter-local)
.equ PRIM_SMALLINT_ADD, 1
.equ PRIM_SMALLINT_SUB, 2
.equ PRIM_SMALLINT_LT, 3
.equ PRIM_SMALLINT_EQ, 4
.equ PRIM_SMALLINT_MUL, 5
.equ PRIM_AT, 6
.equ PRIM_AT_PUT, 7
.equ PRIM_BASIC_NEW, 8
.equ PRIM_BLOCK_VALUE, 9
.equ PRIM_BASIC_NEW_SIZE, 10
.equ PRIM_SIZE, 11
.equ PRIM_IDENTITY_EQ, 12
.equ PRIM_BASIC_CLASS, 13
.equ PRIM_HASH, 14
.equ PRIM_PRINT_CHAR, 15
.equ PRIM_BLOCK_VALUE_ARG, 16
.equ PRIM_PERFORM, 17
.equ PRIM_HALT, 18
.equ PRIM_CHAR_VALUE, 19
.equ PRIM_AS_CHARACTER, 20
.equ PRIM_CHAR_IS_LETTER, 21
.equ PRIM_CHAR_IS_DIGIT, 22
.equ PRIM_CHAR_UPPERCASE, 23
.equ PRIM_CHAR_LOWERCASE, 24
.equ PRIM_STRING_EQ, 25
.equ PRIM_STRING_HASH_FNV, 26
.equ PRIM_STRING_AS_SYMBOL, 27
.equ PRIM_SYMBOL_EQ, 28
.equ PRIM_ERROR, 29

// Bytecodes (interpreter-local)
.equ BC_PUSH_LITERAL, 0
.equ BC_PUSH_INST_VAR, 1
.equ BC_PUSH_TEMP, 2
.equ BC_PUSH_SELF, 3
.equ BC_STORE_INST_VAR, 4
.equ BC_STORE_TEMP, 5
.equ BC_SEND_MESSAGE, 6
.equ BC_RETURN_STACK_TOP, 7
.equ BC_JUMP, 8
.equ BC_JUMP_IF_TRUE, 9
.equ BC_JUMP_IF_FALSE, 10
.equ BC_POP, 11
.equ BC_DUPLICATE, 12
.equ BC_HALT, 13
.equ BC_PUSH_CLOSURE, 14
.equ BC_PUSH_ARG, 15

// Other interpreter-local constants
.equ ASCII_A_UPPER, 65
.equ ASCII_Z_UPPER, 90
.equ ASCII_A_LOWER, 97
.equ ASCII_Z_LOWER, 122
.equ ASCII_0, 48
.equ ASCII_9, 57
.equ ASCII_CASE_DELTA, 32

// GC context offsets (interpreter-local)
.equ GCCTX_FROM_FREE_OFS, 0
.equ GCCTX_FROM_END_OFS, 8
.equ GCCTX_TO_FREE_OFS, 16
.equ GCCTX_TO_END_OFS, 24
.equ GCCTX_FROM_START_OFS, 32
.equ GCCTX_TO_START_OFS, 40
.equ GCCTX_SPACE_SIZE_OFS, 48
.equ GCCTX_TENURED_START_OFS, 56
.equ GCCTX_TENURED_END_OFS, 64
.equ GCCTX_REMEMBERED_SET_OFS, 72

// interpret(sp_ptr, fp_ptr, ip, class_table, om, txn_log) -> result (tagged value)
// x0 = pointer to SP variable
// x1 = pointer to FP variable
// x2 = IP: pointer to first bytecode (in a ByteArray's data area)
// x3 = pointer to class table:
//      [0]=SmallInteger, [1]=Block, [2]=True, [3]=False, [4]=Character, [5]=UndefinedObject
// x4 = pointer to om {free_ptr, end_ptr} pair (for allocation)
// x5 = pointer to transaction log (NULL if no active transaction)
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
    PROLOGUE

    mov     x19, x0             // x19 = sp_ptr
    mov     x20, x1             // x20 = fp_ptr
    mov     x21, x2             // x21 = IP
    mov     x23, x2             // x23 = bytecodes base
    ldr     x24, [x20]          // x24 = entry frame FP (for detecting top-level return)
    mov     x25, x3             // x25 = class table pointer
    mov     x26, x4             // x26 = om pointer
    mov     x27, x5             // x27 = txn_log (NULL if no transaction)

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
    ldr     x7, [x6, #FP_METHOD_OFS]      // method (CompiledMethod obj ptr)
    ldr     x7, [x7, #CM_LITERALS_OFS]      // CM_LITERALS → Array pointer
    add     x7, x7, #OBJ_FIELDS_OFS        // skip Array header
    ldr     x8, [x7, x5, lsl #3]  // literal value
    ldr     x9, [x19]          // SP
    sub     x9, x9, #8
    str     x8, [x9]
    str     x9, [x19]
    b       .Ldispatch

.Lbc_push_inst_var:
    READ_U32                    // x5 = field_index
    ldr     x6, [x20]          // FP
    ldr     x7, [x6, #FP_RECEIVER_OFS]     // receiver (object pointer)
    // If transaction active, check log first
    cbz     x27, .Lpiv_no_txn
    // txn_log_read(log, obj, field_index, &found)
    stp     x5, x7, [sp, #-16]!    // save field_index, receiver
    sub     sp, sp, #16             // space for found flag
    mov     x0, x27             // log
    mov     x1, x7              // obj
    mov     x2, x5              // field_index
    mov     x3, sp              // &found (on stack)
    bl      _txn_log_read
    ldr     x8, [sp]            // found flag
    add     sp, sp, #16         // pop found space
    ldp     x5, x7, [sp], #16  // restore field_index, receiver
    cbnz    x8, .Lpiv_found     // found in log -> x0 = value
    // Not in log, fall through to read from object
.Lpiv_no_txn:
    add     x7, x7, #OBJ_FIELDS_OFS        // skip header
    ldr     x0, [x7, x5, lsl #3]   // read from object
.Lpiv_found:
    // x0 = value (from log or object)
    ldr     x9, [x19]
    sub     x9, x9, #8
    str     x0, [x9]
    str     x9, [x19]
    b       .Ldispatch

.Lbc_push_temp:
    READ_U32
    ldr     x6, [x20]          // FP
    add     x7, x5, #FP_TEMP_BASE_WORDS
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
    ldr     x7, [x6, #FP_RECEIVER_OFS]     // receiver
    ldr     x8, [x19]          // SP
    sub     x8, x8, #8
    str     x7, [x8]
    str     x8, [x19]
    b       .Ldispatch

.Lbc_store_inst_var:
    READ_U32                    // x5 = field_index
    ldr     x6, [x19]          // SP
    ldr     x7, [x6]           // pop value
    add     x6, x6, #8
    str     x6, [x19]
    ldr     x8, [x20]          // FP
    ldr     x9, [x8, #FP_RECEIVER_OFS]     // receiver
    // If transaction active, write to log instead of object
    cbz     x27, .Lsiv_no_txn
    // txn_log_write(log, obj, field_index, value)
    stp     x5, x7, [sp, #-16]!
    mov     x0, x27             // log
    mov     x1, x9              // obj
    mov     x2, x5              // field_index
    mov     x3, x7              // value
    bl      _txn_log_write
    ldp     x5, x7, [sp], #16
    b       .Ldispatch
.Lsiv_no_txn:
    // Write the field
    add     x10, x9, #OBJ_FIELDS_OFS       // skip header (keep x9 = original receiver)
    str     x7, [x10, x5, lsl #3]
    // Write barrier: check if receiver is tenured and value is young
    ldr     x10, [x26, #GCCTX_TENURED_START_OFS]     // gc_ctx[7] = tenured_start
    cbz     x10, .Ldispatch     // no tenured space, skip
    cmp     x9, x10             // receiver >= tenured_start?
    b.lo    .Ldispatch
    ldr     x10, [x26, #GCCTX_TENURED_END_OFS]     // gc_ctx[8] = tenured_end
    cmp     x9, x10             // receiver < tenured_end?
    b.hs    .Ldispatch
    // Receiver is tenured. Check if value is young (in from-space)
    tst     x7, #3              // tagged value? skip
    b.ne    .Ldispatch
    cbz     x7, .Ldispatch
    ldr     x10, [x26, #GCCTX_FROM_START_OFS]     // gc_ctx[4] = from_start
    cmp     x7, x10
    b.lo    .Ldispatch
    ldr     x10, [x26, #GCCTX_FROM_END_OFS]      // gc_ctx[1] = from_end
    cmp     x7, x10
    b.hs    .Ldispatch
    // Old-to-young store: record in remembered set
    ldr     x10, [x26, #GCCTX_REMEMBERED_SET_OFS]     // gc_ctx[9] = remembered_set ptr
    cbz     x10, .Ldispatch     // no remembered set
    stp     x5, x7, [sp, #-16]!
    mov     x0, x10             // remembered set (same format as txn log)
    mov     x1, x9              // obj
    mov     x2, x5              // field_index
    mov     x3, x7              // value
    bl      _txn_log_write
    ldp     x5, x7, [sp], #16
    b       .Ldispatch

.Lbc_store_temp:
    READ_U32
    ldr     x6, [x19]          // SP
    ldr     x7, [x6]           // pop value
    add     x6, x6, #8
    str     x6, [x19]
    ldr     x8, [x20]          // FP
    add     x9, x5, #FP_TEMP_BASE_WORDS
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
    ldr     x12, [x11, #FP_METHOD_OFS]    // current method (CompiledMethod)
    ldr     x13, [x12, #CM_LITERALS_OFS]    // CM_LITERALS → Array pointer
    add     x13, x13, #OBJ_FIELDS_OFS      // skip Array header
    ldr     x14, [x13, x9, lsl #3]  // selector (tagged SmallInt)

    // Get receiver from stack: at SP + arg_count * 8
    ldr     x15, [x19]         // SP
    ldr     x0, [x15, x10, lsl #3]  // receiver
    b       .Llookup_method_for_receiver

.Lsend_not_found:
    // Debug: print "MNU" before trapping
    stp     x0, x14, [sp, #-16]!
    mov     x0, x14             // selector
    bl      _debug_mnu
    ldp     x0, x14, [sp], #16
    brk     #3                  // message not understood (trap for now)

.Llookup_method_for_receiver:
    // x0 = receiver, x14 = selector, x10 = arg_count
    // Returns x0 = found method or branches to .Lsend_not_found.
    mov     x1, x25            // class_table
    stp     x10, x14, [sp, #-16]!
    bl      _oop_class
    ldp     x10, x14, [sp], #16
    mov     x1, x14            // selector
    stp     x10, x14, [sp, #-16]!
    bl      _class_lookup       // x0 = found method or 0
    ldp     x10, x14, [sp], #16
    cbz     x0, .Lsend_not_found
    b       .Ldispatch_found_method

.Ldispatch_found_method:
    // x0 = found CompiledMethod, x10 = arg_count, x21 = saved caller IP
    // Shared by normal send and perform: dispatch primitive or activate frame.
    mov     x28, x0            // preserve found method across primitive fast path
    mov     x22, x10           // preserve arg_count for fallback activation
    ldr     x3, [x28, #CM_PRIMITIVE_OFS]      // primitive index (tagged)
    cmp     x3, #1              // tag_smallint(0) = 1 means no primitive
    b.ne    .Lsend_primitive

.Lactivate_found_method:
    // Read num_temps from found method
    ldr     x3, [x28, #CM_NUM_TEMPS_OFS]      // num_temps (tagged)
    asr     x3, x3, #SMALLINT_SHIFT         // untag num_temps

    // Activate: call _activate_method(sp_ptr, fp_ptr, saved_ip, method, num_args, num_temps)
    mov     x5, x3             // num_temps
    mov     x4, x22            // num_args
    mov     x3, x28            // method (CompiledMethod pointer)
    mov     x2, x21            // saved IP = current IP (will be restored on return)
    mov     x1, x20            // fp_ptr
    mov     x0, x19            // sp_ptr
    bl      _activate_method

    // Set IP to new method's bytecodes (CM_BYTECODES = field 4, offset 56)
    ldr     x6, [x20]          // new FP
    ldr     x7, [x6, #FP_METHOD_OFS]      // new method
    ldr     x10, [x7, #CM_BYTECODES_OFS]     // CM_BYTECODES → ByteArray pointer
    add     x23, x10, #OBJ_FIELDS_OFS      // skip ByteArray header → data
    mov     x21, x23            // IP = start of new method's bytecodes
    b       .Ldispatch

.Lsend_primitive:
    // x28 = CompiledMethod, x3 = primitive index (tagged), x22 = arg_count
    // Decode primitive index
    asr     x3, x3, #SMALLINT_SHIFT         // untag

    // Dispatch to primitive handler
    // Primitives operate on tagged values already on the stack.
    // Stack has: ... receiver arg0 arg1 ... (top = last arg, receiver below args)
    // After primitive: pop args and receiver, push result.

.Lcheck_prim_dispatch:
    cmp     x3, #PRIM_SMALLINT_ADD
    b.eq    .Lprim_add
    cmp     x3, #PRIM_SMALLINT_SUB
    b.eq    .Lprim_sub
    cmp     x3, #PRIM_SMALLINT_LT
    b.eq    .Lprim_lt
    cmp     x3, #PRIM_SMALLINT_EQ
    b.eq    .Lprim_eq
    cmp     x3, #PRIM_SMALLINT_MUL
    b.eq    .Lprim_mul
    cmp     x3, #PRIM_AT
    b.eq    .Lprim_at
    cmp     x3, #PRIM_AT_PUT
    b.eq    .Lprim_at_put
    cmp     x3, #PRIM_BASIC_NEW
    b.eq    .Lprim_basic_new
    cmp     x3, #PRIM_BLOCK_VALUE
    b.eq    .Lprim_block_value
    cmp     x3, #PRIM_BASIC_NEW_SIZE
    b.eq    .Lprim_basic_new_size
    cmp     x3, #PRIM_SIZE
    b.eq    .Lprim_size
    cmp     x3, #PRIM_IDENTITY_EQ
    b.eq    .Lprim_identity_eq
    cmp     x3, #PRIM_BASIC_CLASS
    b.eq    .Lprim_basic_class
    cmp     x3, #PRIM_HASH
    b.eq    .Lprim_hash
    cmp     x3, #PRIM_PRINT_CHAR
    b.eq    .Lprim_print_char
    cmp     x3, #PRIM_BLOCK_VALUE_ARG
    b.eq    .Lprim_block_value_arg
    cmp     x3, #PRIM_PERFORM
    b.eq    .Lprim_perform
    cmp     x3, #PRIM_HALT
    b.eq    .Lprim_halt
    cmp     x3, #PRIM_CHAR_VALUE
    b.eq    .Lprim_char_value
    cmp     x3, #PRIM_AS_CHARACTER
    b.eq    .Lprim_as_character
    cmp     x3, #PRIM_CHAR_IS_LETTER
    b.eq    .Lprim_char_is_letter
    cmp     x3, #PRIM_CHAR_IS_DIGIT
    b.eq    .Lprim_char_is_digit
    cmp     x3, #PRIM_CHAR_UPPERCASE
    b.eq    .Lprim_char_uppercase
    cmp     x3, #PRIM_CHAR_LOWERCASE
    b.eq    .Lprim_char_lowercase
    cmp     x3, #PRIM_STRING_EQ
    b.eq    .Lprim_string_eq
    cmp     x3, #PRIM_STRING_HASH_FNV
    b.eq    .Lprim_string_hash_fnv
    cmp     x3, #PRIM_STRING_AS_SYMBOL
    b.eq    .Lprim_string_as_symbol
    cmp     x3, #PRIM_SYMBOL_EQ
    b.eq    .Lprim_symbol_eq
    cmp     x3, #PRIM_ERROR
    b.eq    .Lprim_error
    // Debug: print unknown primitive
    stp     x0, x3, [sp, #-16]!
    mov     x0, x3
    bl      _debug_unknown_prim
    ldp     x0, x3, [sp], #16
    brk     #4                  // unknown primitive

.Lprim_add:
    // receiver + arg0  (both tagged SmallInts)
    ldr     x5, [x19]          // SP
    ldr     x6, [x5]           // arg0
    ldr     x7, [x5, #8]       // receiver
    and     x9, x7, #TAG_MASK
    cmp     x9, #TAG_SMALLINT
    b.ne    .Lprim_receiver_type_error
    and     x9, x6, #TAG_MASK
    cmp     x9, #TAG_SMALLINT
    b.ne    .Lprimitive_failed
    asr     x6, x6, #SMALLINT_SHIFT
    asr     x7, x7, #SMALLINT_SHIFT
    adds    x8, x7, x6
    b.vs    .Lprimitive_failed
    lsl     x9, x8, #SMALLINT_SHIFT
    asr     x10, x9, #SMALLINT_SHIFT
    cmp     x10, x8
    b.ne    .Lprimitive_failed
    lsl     x8, x8, #SMALLINT_SHIFT
    orr     x8, x8, #TAG_SMALLINT
    add     x5, x5, #16         // pop both
    sub     x5, x5, #8          // push result
    str     x8, [x5]
    str     x5, [x19]
    b       .Ldispatch

.Lprim_sub:
    ldr     x5, [x19]
    ldr     x6, [x5]           // arg0
    ldr     x7, [x5, #8]       // receiver
    and     x9, x7, #TAG_MASK
    cmp     x9, #TAG_SMALLINT
    b.ne    .Lprim_receiver_type_error
    and     x9, x6, #TAG_MASK
    cmp     x9, #TAG_SMALLINT
    b.ne    .Lprimitive_failed
    asr     x6, x6, #SMALLINT_SHIFT
    asr     x7, x7, #SMALLINT_SHIFT
    subs    x8, x7, x6
    b.vs    .Lprimitive_failed
    lsl     x9, x8, #SMALLINT_SHIFT
    asr     x10, x9, #SMALLINT_SHIFT
    cmp     x10, x8
    b.ne    .Lprimitive_failed
    lsl     x8, x8, #SMALLINT_SHIFT
    orr     x8, x8, #TAG_SMALLINT
    add     x5, x5, #8         // pop arg, result replaces receiver
    str     x8, [x5]
    str     x5, [x19]
    b       .Ldispatch

.Lprim_lt:
    ldr     x5, [x19]
    ldr     x6, [x5]           // arg0
    ldr     x7, [x5, #8]       // receiver
    and     x8, x7, #TAG_MASK
    cmp     x8, #TAG_SMALLINT
    b.ne    .Lprim_receiver_type_error
    and     x8, x6, #TAG_MASK
    cmp     x8, #TAG_SMALLINT
    b.ne    .Lprimitive_failed
    cmp     x7, x6
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
    and     x8, x7, #TAG_MASK
    cmp     x8, #TAG_SMALLINT
    b.ne    .Lprim_receiver_type_error
    and     x8, x6, #TAG_MASK
    cmp     x8, #TAG_SMALLINT
    b.ne    .Lprimitive_failed
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
    and     x9, x7, #TAG_MASK
    cmp     x9, #TAG_SMALLINT
    b.ne    .Lprim_receiver_type_error
    and     x9, x6, #TAG_MASK
    cmp     x9, #TAG_SMALLINT
    b.ne    .Lprimitive_failed
    asr     x6, x6, #SMALLINT_SHIFT         // untag arg0
    asr     x7, x7, #SMALLINT_SHIFT         // untag receiver
    mul     x8, x7, x6         // result = receiver * arg0
    smulh   x9, x7, x6
    asr     x11, x8, #63
    cmp     x9, x11
    b.ne    .Lprimitive_failed
    lsl     x9, x8, #SMALLINT_SHIFT
    asr     x10, x9, #SMALLINT_SHIFT
    cmp     x10, x8
    b.ne    .Lprimitive_failed
    lsl     x8, x8, #SMALLINT_SHIFT
    orr     x8, x8, #TAG_SMALLINT
    add     x5, x5, #8         // pop arg, result replaces receiver
    str     x8, [x5]
    str     x5, [x19]
    b       .Ldispatch

.Lprim_at:
    // receiver at: index — dispatch on format
    // Stack: [SP]=index (tagged SmallInt), [SP+8]=receiver (object ptr)
    ldr     x5, [x19]          // SP
    ldr     x6, [x5]           // index (tagged)
    ldr     x7, [x5, #8]       // receiver (object)
    asr     x6, x6, #SMALLINT_SHIFT         // untag index -> 1-based
    sub     x6, x6, #1         // convert to 0-based

    // Check format
    ldr     x8, [x7, #8]       // obj[1] = format
    cbz     x8, .Lprim_at_fields_err  // FORMAT_FIELDS → error

    // Bounds check: 0 <= x6 < size
    ldr     x9, [x7, #OBJ_SIZE_OFS]      // obj[2] = size
    cmp     x6, x9
    b.hs    .Lprim_at_bounds_err

    cmp     x8, #2
    b.eq    .Lprim_at_bytes

    // FORMAT_INDEXABLE — word access
    // If transaction active, check log first
    cbz     x27, .Lprim_at_idx_no_txn
    stp     x5, x6, [sp, #-16]!
    stp     x7, xzr, [sp, #-16]!
    sub     sp, sp, #16         // space for found flag
    mov     x0, x27             // log
    mov     x1, x7              // obj
    mov     x2, x6              // field_index (0-based)
    mov     x3, sp              // &found
    bl      _txn_log_read
    ldr     x8, [sp]            // found flag
    add     sp, sp, #16
    ldp     x7, xzr, [sp], #16
    ldp     x5, x6, [sp], #16
    cbnz    x8, .Lprim_at_got_value  // x0 = value from log
.Lprim_at_idx_no_txn:
    add     x7, x7, #OBJ_FIELDS_OFS        // skip header
    ldr     x0, [x7, x6, lsl #3]   // obj[3 + index]
    b       .Lprim_at_got_value

.Lprim_at_bytes:
    // FORMAT_BYTES — byte access, return tagged SmallInt
    // If transaction active, check txn log first using byte-index marker.
    cbz     x27, .Lprim_at_bytes_no_txn
    stp     x5, x6, [sp, #-16]!
    stp     x7, xzr, [sp, #-16]!
    sub     sp, sp, #16         // space for found flag
    mov     x0, x27             // log
    mov     x1, x7              // obj
    mov     x2, #1
    lsl     x2, x2, #63         // byte marker bit
    orr     x2, x2, x6          // encoded field index
    mov     x3, sp              // &found
    bl      _txn_log_read
    ldr     x8, [sp]            // found flag
    add     sp, sp, #16
    ldp     x7, xzr, [sp], #16
    ldp     x5, x6, [sp], #16
    cbnz    x8, .Lprim_at_got_value  // x0 = tagged value from log

.Lprim_at_bytes_no_txn:
    add     x7, x7, #OBJ_FIELDS_OFS        // skip header
    ldrb    w0, [x7, x6]       // load single byte
    lsl     x0, x0, #SMALLINT_SHIFT
    orr     x0, x0, #TAG_SMALLINT         // tag as SmallInt

.Lprim_at_got_value:
    // x0 = result value
    add     x5, x5, #8         // pop index, result replaces receiver
    str     x0, [x5]
    str     x5, [x19]
    b       .Ldispatch

.Lprim_at_fields_err:
    brk     #6                  // at: on FORMAT_FIELDS object
.Lprim_at_bounds_err:
    brk     #7                  // at: index out of bounds

.Lprim_at_put:
    // receiver at: index put: value — dispatch on format
    // Stack: [SP]=value, [SP+8]=index (tagged SmallInt), [SP+16]=receiver
    ldr     x5, [x19]          // SP
    ldr     x6, [x5]           // value
    ldr     x7, [x5, #8]       // index (tagged)
    ldr     x8, [x5, #16]      // receiver (object)
    asr     x7, x7, #SMALLINT_SHIFT         // untag index -> 1-based
    sub     x7, x7, #1         // convert to 0-based

    // Check format
    ldr     x9, [x8, #OBJ_FORMAT_OFS]       // obj[1] = format
    cbz     x9, .Lprim_at_fields_err  // FORMAT_FIELDS → error

    // Bounds check
    ldr     x10, [x8, #OBJ_SIZE_OFS]     // obj[2] = size
    cmp     x7, x10
    b.hs    .Lprim_at_bounds_err

    cmp     x9, #FORMAT_BYTES
    b.eq    .Lprim_atput_bytes

    // FORMAT_INDEXABLE — word store
    // If transaction active, write to log
    cbz     x27, .Lprim_atput_no_txn
    stp     x5, x6, [sp, #-16]!
    stp     x7, x8, [sp, #-16]!
    mov     x0, x27             // log
    mov     x1, x8              // obj
    mov     x2, x7              // field_index (0-based)
    mov     x3, x6              // value
    bl      _txn_log_write
    ldp     x7, x8, [sp], #16
    ldp     x5, x6, [sp], #16
    b       .Lprim_atput_done
.Lprim_atput_no_txn:
    add     x9, x8, #OBJ_FIELDS_OFS        // skip header
    str     x6, [x9, x7, lsl #3]   // obj[3 + index] = value
    b       .Lprim_atput_done

.Lprim_atput_bytes:
    // FORMAT_BYTES — byte store, value is tagged SmallInt.
    // If transaction active, write encoded byte entry to txn log.
    cbz     x27, .Lprim_atput_bytes_no_txn
    stp     x5, x6, [sp, #-16]!
    stp     x7, x8, [sp, #-16]!
    mov     x0, x27             // log
    mov     x1, x8              // obj
    mov     x2, #1
    lsl     x2, x2, #63         // byte marker bit
    orr     x2, x2, x7          // encoded index
    mov     x3, x6              // tagged SmallInt value
    bl      _txn_log_write
    ldp     x7, x8, [sp], #16
    ldp     x5, x6, [sp], #16
    b       .Lprim_atput_done

.Lprim_atput_bytes_no_txn:
    // No transaction: write directly to byte storage.
    asr     x6, x6, #SMALLINT_SHIFT         // untag value → byte
    add     x9, x8, #OBJ_FIELDS_OFS        // skip header
    strb    w6, [x9, x7]       // store single byte

.Lprim_atput_done:
    // Return the receiver
    add     x5, x5, #16        // pop value and index
    str     x8, [x5]           // push original receiver
    str     x5, [x19]
    b       .Ldispatch

.Lprim_basic_new:
    // basicNew: receiver is a Class object on the stack.
    // Read instSize from class (field 2 = CLASS_INST_SIZE, offset 40)
    // Allocate a new instance with FORMAT_FIELDS and that size.
    // Initialize all fields to nil.
    ldr     x5, [x19]          // SP
    ldr     x5, [x5]           // receiver = the class
    ldr     x6, [x5, #CLASS_INST_SIZE_OFS]      // CLASS_INST_SIZE
    asr     x6, x6, #SMALLINT_SHIFT         // untag SmallInt -> raw size

    // om_alloc(om, class_ptr, format, size)
    mov     x0, x26             // om
    mov     x1, x5              // class ptr = the receiver class
    mov     x2, #FORMAT_FIELDS
    mov     x3, x6              // size
    stp     x5, x6, [sp, #-16]!
    bl      _om_alloc
    ldp     x5, x6, [sp], #16
    // x0 = new object (or NULL)
    cbz     x0, .Lbasicnew_gc

    // Initialize fields to nil (0x03)
.Lbasicnew_init_setup:
    mov     x7, #0              // field index
    mov     x8, #TAGGED_NIL
.Lbasicnew_init:
    cmp     x7, x6
    b.ge    .Lbasicnew_done
    add     x9, x7, #OBJ_HEADER_WORDS          // slot = header + field_index
    str     x8, [x0, x9, lsl #3]
    add     x7, x7, #1
    b       .Lbasicnew_init

.Lbasicnew_done:
    // Replace receiver on stack with new object
    ldr     x5, [x19]          // SP
    str     x0, [x5]           // overwrite receiver with new obj
    b       .Ldispatch

.Lbasicnew_gc:
    sub     x28, x21, x23       // preserve current bytecode offset

    ldr     x0, [x19]
    ldr     x1, [x20]
    mov     x2, #0
    mov     x3, #0
    bl      _gc_collect_stack_slots
    mov     x22, x0             // slot root count

    lsl     x16, x22, #3        // slot_buf bytes
    add     x17, x22, #1
    lsl     x17, x17, #3        // root_buf bytes (slot roots + class table)
    add     x15, x16, x17
    add     x15, x15, #15
    and     x15, x15, #-16
    sub     sp, sp, x15

    ldr     x0, [x19]
    ldr     x1, [x20]
    mov     x2, sp
    mov     x3, x22
    bl      _gc_collect_stack_slots

    add     x11, sp, #0
    add     x12, sp, x22, lsl #3
    mov     x13, #0
.Lbasicnew_slot_roots_to_values:
    cmp     x13, x22
    b.ge    .Lbasicnew_slot_roots_done
    ldr     x14, [x11, x13, lsl #3]
    ldr     x15, [x14]
    str     x15, [x12, x13, lsl #3]
    add     x13, x13, #1
    b       .Lbasicnew_slot_roots_to_values
.Lbasicnew_slot_roots_done:
    str     x25, [x12, x22, lsl #3]

    mov     x0, x12
    add     x1, x22, #1
    mov     x2, x26
    add     x3, x26, #16
    ldr     x4, [x26, #GCCTX_FROM_START_OFS]
    ldr     x5, [x26, #GCCTX_FROM_END_OFS]
    bl      _gc_collect

    add     x11, sp, #0
    add     x12, sp, x22, lsl #3
    mov     x13, #0
.Lbasicnew_values_to_slot_roots:
    cmp     x13, x22
    b.ge    .Lbasicnew_values_done
    ldr     x15, [x11, x13, lsl #3]
    ldr     x6, [x12, x13, lsl #3]
    str     x6, [x15]
    add     x13, x13, #1
    b       .Lbasicnew_values_to_slot_roots
.Lbasicnew_values_done:

    ldr     x25, [x12, x22, lsl #3]

    lsl     x16, x22, #3
    add     x17, x22, #1
    lsl     x17, x17, #3
    add     x15, x16, x17
    add     x15, x15, #15
    and     x15, x15, #-16
    add     sp, sp, x15

    ldp     x0, x1, [x26]
    ldp     x2, x3, [x26, #GCCTX_TO_FREE_OFS]
    stp     x2, x3, [x26]
    stp     x0, x1, [x26, #GCCTX_TO_FREE_OFS]
    ldr     x0, [x26, #GCCTX_FROM_START_OFS]
    ldr     x1, [x26, #GCCTX_TO_START_OFS]
    str     x1, [x26, #GCCTX_FROM_START_OFS]
    str     x0, [x26, #GCCTX_TO_START_OFS]

    ldr     x0, [x26, #GCCTX_TO_START_OFS]
    ldr     x1, [x26, #GCCTX_SPACE_SIZE_OFS]
    str     x0, [x26, #GCCTX_TO_FREE_OFS]
    add     x0, x0, x1
    str     x0, [x26, #GCCTX_TO_END_OFS]

    ldr     x0, [x20]
    ldr     x0, [x0, #FP_METHOD_OFS]
    ldr     x1, [x0, #CM_BYTECODES_OFS]
    add     x23, x1, #OBJ_FIELDS_OFS
    add     x21, x23, x28

    ldr     x5, [x19]
    ldr     x5, [x5]
    ldr     x6, [x5, #CLASS_INST_SIZE_OFS]
    asr     x6, x6, #SMALLINT_SHIFT
    mov     x0, x26
    mov     x1, x5
    mov     x2, #FORMAT_FIELDS
    mov     x3, x6
    stp     x5, x6, [sp, #-16]!
    bl      _om_alloc
    ldp     x5, x6, [sp], #16
    cbz     x0, .Lbasicnew_oom
    b       .Lbasicnew_init_setup

.Lbasicnew_oom:
    brk     #4                  // OOM in basicNew

.Lprim_basic_new_size:
    // basicNew: — receiver is a Class, arg is size (SmallInt)
    // Stack: [arg, receiver, ...]
    // Read inst_format from receiver (CLASS_INST_FORMAT = field 3, offset 48)
    // FORMAT_FIELDS (0) → error
    // FORMAT_INDEXABLE (1) → alloc indexable with arg size
    // FORMAT_BYTES (2) → alloc bytes with arg size
    ldr     x5, [x19]          // SP
    ldr     x8, [x5, #8]       // receiver (below arg) — the class
    ldr     x6, [x5]           // arg = size (tagged SmallInt)
    asr     x6, x6, #SMALLINT_SHIFT         // untag size

    // Read inst_format from the class
    ldr     x7, [x8, #CLASS_INST_FORMAT_OFS]      // CLASS_INST_FORMAT
    asr     x7, x7, #SMALLINT_SHIFT         // untag format

    // Check format
    cbz     x7, .Lbasicnewsize_err  // FORMAT_FIELDS → error

    // om_alloc(om, class_ptr, format, size)
    mov     x0, x26             // om
    mov     x1, x8              // class = receiver
    mov     x2, x7              // format from class
    mov     x3, x6              // size from arg
    stp     x5, x6, [sp, #-16]!
    stp     x7, x8, [sp, #-16]!
    bl      _om_alloc
    ldp     x7, x8, [sp], #16
    ldp     x5, x6, [sp], #16
    cbz     x0, .Lbasicnewsize_gc

    // Initialize fields to nil for FORMAT_INDEXABLE
    cmp     x7, #FORMAT_BYTES
    b.eq    .Lbasicnewsize_done // bytes don't need nil init

    mov     x9, #0
    mov     x10, #TAGGED_NIL
.Lbasicnewsize_init:
    cmp     x9, x6
    b.ge    .Lbasicnewsize_done
    add     x11, x9, #OBJ_HEADER_WORDS
    str     x10, [x0, x11, lsl #3]
    add     x9, x9, #1
    b       .Lbasicnewsize_init

.Lbasicnewsize_done:
    // Pop arg, replace receiver with new object
    // SP currently points to arg. Pop arg, then overwrite receiver.
    add     x5, x5, #8         // pop arg
    str     x0, [x5]           // overwrite receiver with new obj
    str     x5, [x19]          // update SP
    b       .Ldispatch

.Lbasicnewsize_gc:
    sub     x28, x21, x23       // preserve current bytecode offset

    ldr     x0, [x19]
    ldr     x1, [x20]
    mov     x2, #0
    mov     x3, #0
    bl      _gc_collect_stack_slots
    mov     x22, x0             // slot root count

    lsl     x16, x22, #3
    add     x17, x22, #1
    lsl     x17, x17, #3
    add     x15, x16, x17
    add     x15, x15, #15
    and     x15, x15, #-16
    sub     sp, sp, x15

    ldr     x0, [x19]
    ldr     x1, [x20]
    mov     x2, sp
    mov     x3, x22
    bl      _gc_collect_stack_slots

    add     x11, sp, #0
    add     x12, sp, x22, lsl #3
    mov     x13, #0
.Lbasicnewsize_slot_roots_to_values:
    cmp     x13, x22
    b.ge    .Lbasicnewsize_slot_roots_done
    ldr     x14, [x11, x13, lsl #3]
    ldr     x15, [x14]
    str     x15, [x12, x13, lsl #3]
    add     x13, x13, #1
    b       .Lbasicnewsize_slot_roots_to_values
.Lbasicnewsize_slot_roots_done:
    str     x25, [x12, x22, lsl #3]

    mov     x0, x12
    add     x1, x22, #1
    mov     x2, x26
    add     x3, x26, #16
    ldr     x4, [x26, #GCCTX_FROM_START_OFS]
    ldr     x5, [x26, #GCCTX_FROM_END_OFS]
    bl      _gc_collect

    add     x11, sp, #0
    add     x12, sp, x22, lsl #3
    mov     x13, #0
.Lbasicnewsize_values_to_slot_roots:
    cmp     x13, x22
    b.ge    .Lbasicnewsize_values_done
    ldr     x15, [x11, x13, lsl #3]
    ldr     x6, [x12, x13, lsl #3]
    str     x6, [x15]
    add     x13, x13, #1
    b       .Lbasicnewsize_values_to_slot_roots
.Lbasicnewsize_values_done:

    ldr     x25, [x12, x22, lsl #3]

    lsl     x16, x22, #3
    add     x17, x22, #1
    lsl     x17, x17, #3
    add     x15, x16, x17
    add     x15, x15, #15
    and     x15, x15, #-16
    add     sp, sp, x15

    ldp     x0, x1, [x26]
    ldp     x2, x3, [x26, #GCCTX_TO_FREE_OFS]
    stp     x2, x3, [x26]
    stp     x0, x1, [x26, #GCCTX_TO_FREE_OFS]
    ldr     x0, [x26, #GCCTX_FROM_START_OFS]
    ldr     x1, [x26, #GCCTX_TO_START_OFS]
    str     x1, [x26, #GCCTX_FROM_START_OFS]
    str     x0, [x26, #GCCTX_TO_START_OFS]

    ldr     x0, [x26, #GCCTX_TO_START_OFS]
    ldr     x1, [x26, #GCCTX_SPACE_SIZE_OFS]
    str     x0, [x26, #GCCTX_TO_FREE_OFS]
    add     x0, x0, x1
    str     x0, [x26, #GCCTX_TO_END_OFS]

    ldr     x0, [x20]
    ldr     x0, [x0, #FP_METHOD_OFS]
    ldr     x1, [x0, #CM_BYTECODES_OFS]
    add     x23, x1, #OBJ_FIELDS_OFS
    add     x21, x23, x28

    ldr     x5, [x19]
    ldr     x8, [x5, #8]
    ldr     x6, [x5]
    asr     x6, x6, #SMALLINT_SHIFT
    ldr     x7, [x8, #CLASS_INST_FORMAT_OFS]
    asr     x7, x7, #SMALLINT_SHIFT
    cbz     x7, .Lbasicnewsize_err
    mov     x0, x26
    mov     x1, x8
    mov     x2, x7
    mov     x3, x6
    stp     x5, x6, [sp, #-16]!
    stp     x7, x8, [sp, #-16]!
    bl      _om_alloc
    ldp     x7, x8, [sp], #16
    ldp     x5, x6, [sp], #16
    cbz     x0, .Lbasicnew_oom
    cmp     x7, #FORMAT_BYTES
    b.eq    .Lbasicnewsize_done
    mov     x9, #0
    mov     x10, #TAGGED_NIL
    b       .Lbasicnewsize_init

.Lbasicnewsize_err:
    brk     #5                  // basicNew: on non-indexable class

.Lprim_basic_class:
    // basicClass: return the class of the receiver
    // Uses oop_class logic: SmallInt → class_table[0], heap obj → obj[0]
    ldr     x5, [x19]          // SP
    ldr     x6, [x5]           // receiver
    tst     x6, #1             // SmallInt? (bit 0)
    b.ne    .Lbasicclass_smallint
    // Heap object — read class from obj[0]
    ldr     x7, [x6]           // obj[0] = class ptr
    str     x7, [x5]           // replace receiver with class
    b       .Ldispatch
.Lbasicclass_smallint:
    // Check if it's SmallInt (tag 01) or special (tag 11)
    tst     x6, #2             // bit 1 set? → special (true/false/nil)
    b.ne    .Lbasicclass_special
    // SmallInt: class_table[0]
    ldr     x7, [x25, #CLASS_TABLE_SMALLINT_OFS]    // class_table field 0
    str     x7, [x5]
    b       .Ldispatch
.Lbasicclass_special:
    // Check Character first (low 4 bits = 0x0F)
    and     x7, x6, #CHAR_TAG_MASK
    cmp     x7, #CHAR_TAG_VALUE
    b.eq    .Lbasicclass_character
    cmp     x6, #TAGGED_NIL
    b.eq    .Lbasicclass_nil
    // true (0x07) → class_table[2], false (0x0B) → class_table[3]
    cmp     x6, #TAGGED_TRUE
    b.eq    .Lbasicclass_true
    cmp     x6, #TAGGED_FALSE
    b.eq    .Lbasicclass_false
    // Unknown special immediate.
    str     xzr, [x5]
    b       .Ldispatch
.Lbasicclass_nil:
    ldr     x7, [x25, #CLASS_TABLE_UNDEFINED_OBJECT_OFS]
    str     x7, [x5]
    b       .Ldispatch
.Lbasicclass_true:
    ldr     x7, [x25, #CLASS_TABLE_TRUE_OFS]    // class_table field 2 (true class)
    str     x7, [x5]
    b       .Ldispatch
.Lbasicclass_false:
    ldr     x7, [x25, #CLASS_TABLE_FALSE_OFS]    // class_table field 3 (false class)
    str     x7, [x5]
    b       .Ldispatch
.Lbasicclass_character:
    ldr     x7, [x25, #CLASS_TABLE_CHARACTER_OFS]    // class_table field 4 (Character class)
    str     x7, [x5]
    b       .Ldispatch

.Lprim_print_char:
    // printChar: receiver is Character immediate, write byte to stdout, return self
    ldr     x5, [x19]          // SP
    ldr     x6, [x5]           // receiver (Character immediate)
    and     x7, x6, #CHAR_TAG_MASK
    cmp     x7, #CHAR_TAG_VALUE
    b.ne    .Lprimitive_failed
    lsr     x6, x6, #4         // untag Character → code point
    // write(1, &byte, 1) via C library
    sub     sp, sp, #16
    strb    w6, [sp]            // store byte on machine stack
    mov     x0, #1              // fd = stdout
    mov     x1, sp              // buf
    mov     x2, #1              // count
    bl      _write
    add     sp, sp, #16
    // receiver stays on stack (return self)
    b       .Ldispatch

.Lprim_char_value:
    // Character>>value: return code point as tagged SmallInt
    // Receiver is Character immediate: (codePoint << 4) | 0x0F
    ldr     x5, [x19]          // SP
    ldr     x6, [x5]           // receiver (Character immediate)
    and     x7, x6, #CHAR_TAG_MASK
    cmp     x7, #CHAR_TAG_VALUE
    b.ne    .Lprimitive_failed
    lsr     x6, x6, #4         // extract code point
    lsl     x6, x6, #2
    orr     x6, x6, #1         // tag as SmallInt
    str     x6, [x5]           // replace receiver with result
    b       .Ldispatch

.Lprim_as_character:
    // SmallInteger>>asCharacter: convert to Character immediate
    // Receiver is tagged SmallInt, result is (value << 4) | 0x0F
    ldr     x5, [x19]          // SP
    ldr     x6, [x5]           // receiver (tagged SmallInt)
    and     x7, x6, #TAG_MASK
    cmp     x7, #TAG_SMALLINT
    b.ne    .Lprimitive_failed
    asr     x6, x6, #SMALLINT_SHIFT         // untag SmallInt → code point
    lsl     x6, x6, #4
    orr     x6, x6, #CHAR_TAG_VALUE      // tag as Character
    str     x6, [x5]           // replace receiver with result
    b       .Ldispatch

.Lprim_char_is_letter:
    // Character>>isLetter: A-Z or a-z → true, else false
    ldr     x5, [x19]
    ldr     x6, [x5]           // Character immediate
    and     x7, x6, #CHAR_TAG_MASK
    cmp     x7, #CHAR_TAG_VALUE
    b.ne    .Lprimitive_failed
    lsr     x6, x6, #4         // code point
    // Check A-Z (65-90)
    sub     x7, x6, #65
    cmp     x7, #25
    b.ls    .Lchar_true
    // Check a-z (97-122)
    sub     x7, x6, #97
    cmp     x7, #25
    b.ls    .Lchar_true
    b       .Lchar_false

.Lprim_char_is_digit:
    // Character>>isDigit: 0-9 → true, else false
    ldr     x5, [x19]
    ldr     x6, [x5]
    and     x7, x6, #CHAR_TAG_MASK
    cmp     x7, #CHAR_TAG_VALUE
    b.ne    .Lprimitive_failed
    lsr     x6, x6, #4
    sub     x7, x6, #48        // '0' = 48
    cmp     x7, #9
    b.ls    .Lchar_true
    b       .Lchar_false

.Lchar_true:
    mov     x6, #7              // tagged true
    str     x6, [x5]
    b       .Ldispatch
.Lchar_false:
    mov     x6, #11             // tagged false
    str     x6, [x5]
    b       .Ldispatch

.Lprim_char_uppercase:
    // Character>>asUppercase: a-z → A-Z, else self
    ldr     x5, [x19]
    ldr     x6, [x5]           // Character immediate
    and     x8, x6, #CHAR_TAG_MASK
    cmp     x8, #CHAR_TAG_VALUE
    b.ne    .Lprimitive_failed
    lsr     x7, x6, #4         // code point
    sub     x8, x7, #97        // 'a'
    cmp     x8, #25
    b.hi    .Ldispatch          // not lowercase → return self
    sub     x7, x7, #ASCII_CASE_DELTA        // to uppercase
    lsl     x7, x7, #4
    orr     x7, x7, #CHAR_TAG_VALUE
    str     x7, [x5]
    b       .Ldispatch

.Lprim_char_lowercase:
    // Character>>asLowercase: A-Z → a-z, else self
    ldr     x5, [x19]           // SP
    ldr     x6, [x5]            // Character immediate
    and     x8, x6, #CHAR_TAG_MASK
    cmp     x8, #CHAR_TAG_VALUE
    b.ne    .Lprimitive_failed
    lsr     x7, x6, #4          // code point
    sub     x8, x7, #ASCII_A_UPPER
    cmp     x8, #25
    b.hi    .Ldispatch          // not uppercase → return self
    add     x7, x7, #ASCII_CASE_DELTA         // to lowercase
    lsl     x7, x7, #4
    orr     x7, x7, #CHAR_TAG_VALUE
    str     x7, [x5]
    b       .Ldispatch

.Lprim_hash:
    // hash: return identity hash as tagged SmallInt
    // SmallInt receiver → value is its own hash
    // Heap object → address-based hash (shift right 3 to remove alignment, mask)
    ldr     x5, [x19]          // SP
    ldr     x6, [x5]           // receiver
    tst     x6, #1             // SmallInt?
    b.ne    .Lhash_done         // SmallInt is already tagged — it IS its hash
    // Heap object: use address >> 3, mask to fit SmallInt range
    lsr     x6, x6, #3
    and     x6, x6, #0x3FFFFFFF // 30 bits
    lsl     x6, x6, #2
    orr     x6, x6, #TAG_SMALLINT         // tag as SmallInt
.Lhash_done:
    str     x6, [x5]           // replace receiver with hash
    b       .Ldispatch

.Lprim_string_eq:
    ldr     x5, [x19]          // SP
    ldr     x1, [x5]           // arg
    ldr     x0, [x5, #8]       // receiver
    bl      _prim_string_eq    // Call C function
    add     x5, x5, #16        // Pop arg and receiver
    str     x0, [x5]           // Push result
    str     x5, [x19]          // Update SP
    b       .Ldispatch

.Lprim_string_hash_fnv:
    ldr     x5, [x19]          // SP
    ldr     x0, [x5]           // receiver
    bl      _prim_string_hash_fnv  // Call C function
    add     x5, x5, #8         // Pop receiver
    str     x0, [x5]           // Push result
    str     x5, [x19]          // Update SP
    b       .Ldispatch

.Lprim_string_as_symbol:
    ldr     x5, [x19]          // SP
    ldr     x0, [x5]           // receiver
    bl      _prim_string_as_symbol // Call C function
    add     x5, x5, #8         // Pop receiver
    str     x0, [x5]           // Push result
    str     x5, [x19]          // Update SP
    b       .Ldispatch

.Lprim_symbol_eq:
    ldr     x5, [x19]          // SP
    ldr     x1, [x5]           // arg
    ldr     x0, [x5, #8]       // receiver
    bl      _prim_symbol_eq    // Call C function
    add     x5, x5, #16        // Pop arg and receiver
    str     x0, [x5]           // Push result
    str     x5, [x19]          // Update SP
    b       .Ldispatch

.Lprim_error:
    // error: aString -- print the message and stack trace, then trap.
    ldr     x5, [x19]          // SP
    ldr     x0, [x5]           // arg/message
    ldr     x1, [x20]          // current FP
    mov     x2, x25            // class table
    bl      _debug_error
    brk     #13

.Lprim_identity_eq:
    // ==: receiver arg → true if same value, false otherwise
    // Stack: [arg, receiver, ...]
    ldr     x5, [x19]          // SP
    ldr     x6, [x5]           // arg
    ldr     x7, [x5, #8]       // receiver
    cmp     x6, x7
    mov     x8, #TAGGED_TRUE
    mov     x9, #TAGGED_FALSE
    csel    x8, x8, x9, eq
    add     x5, x5, #8         // pop arg
    str     x8, [x5]           // replace receiver with result
    str     x5, [x19]          // update SP
    b       .Ldispatch

.Lprim_size:
    // size: return the object's size field as a tagged SmallInt
    ldr     x5, [x19]          // SP
    ldr     x5, [x5]           // receiver
    ldr     x6, [x5, #OBJ_SIZE_OFS]      // obj[2] = size
    lsl     x6, x6, #SMALLINT_SHIFT
    orr     x6, x6, #TAG_SMALLINT         // tag as SmallInt
    ldr     x5, [x19]          // SP
    str     x6, [x5]           // replace receiver with result
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

    ldr     x7, [x6, #BLOCK_HOME_RECEIVER_OFS]      // home receiver (block field 0)
    ldr     x8, [x6, #BLOCK_CM_OFS]      // CM (block field 1)

    // Read num_temps from CM
    ldr     x3, [x8, #CM_NUM_TEMPS_OFS]      // num_temps (tagged)
    asr     x3, x3, #SMALLINT_SHIFT         // untag

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
    ldr     x7, [x6, #FP_METHOD_OFS]      // method (block's CM)
    ldr     x10, [x7, #CM_BYTECODES_OFS]     // CM_BYTECODES → ByteArray
    add     x23, x10, #OBJ_FIELDS_OFS      // skip header → data
    mov     x21, x23
    b       .Ldispatch

.Lprim_block_value_arg:
    // value: — 1-arg block evaluation
    // NO frame has been built yet. Stack: [arg, block, ...]
    // Strategy: replace block with home_rcv in place, call _activate_method with 1 arg.
    ldr     x5, [x19]          // SP
    ldr     x6, [x5, #8]       // block object

    ldr     x7, [x6, #BLOCK_HOME_RECEIVER_OFS]      // home receiver (block field 0)
    ldr     x8, [x6, #BLOCK_CM_OFS]      // CM (block field 1)

    // Replace block with home receiver on stack
    str     x7, [x5, #8]
    // Stack now: [arg, home_rcv, ...]

    // Read num_temps from block's CM
    ldr     x3, [x8, #CM_NUM_TEMPS_OFS]      // num_temps (tagged)
    asr     x3, x3, #SMALLINT_SHIFT

    // Activate: _activate_method(sp_ptr, fp_ptr, saved_ip, method, num_args=1, num_temps)
    mov     x5, x3             // num_temps
    mov     x4, #1             // 1 arg
    mov     x3, x8             // method = block's CM
    mov     x2, x21            // saved IP
    mov     x1, x20            // fp_ptr
    mov     x0, x19            // sp_ptr
    bl      _activate_method

    // Mark frame as block
    ldr     x6, [x20]          // new FP
    ldr     x10, [x6, #FP_FLAGS_OFS]    // flags
    orr     x10, x10, #(1 << FRAME_FLAGS_IS_BLOCK_SHIFT) // is_block = 1
    str     x10, [x6, #FP_FLAGS_OFS]

    // Set IP to block CM's bytecodes
    ldr     x7, [x6, #FP_METHOD_OFS]      // method from frame
    ldr     x10, [x7, #CM_BYTECODES_OFS]     // CM_BYTECODES
    add     x23, x10, #OBJ_FIELDS_OFS      // skip header
    mov     x21, x23
    b       .Ldispatch

.Lprim_perform:
    // perform: selector — dynamically send a 0-arg message
    // NO frame built. Stack: [selector, receiver, ...]
    ldr     x5, [x19]          // SP
    ldr     x14, [x5]          // selector
    ldr     x0, [x5, #8]       // receiver
    add     x5, x5, #8         // pop selector, receiver stays
    str     x5, [x19]
    mov     x10, #0             // perform: sends a 0-arg message
    b       .Llookup_method_for_receiver

.Lprim_halt:
    brk     #9                  // halt primitive — crash the VM

.Lbc_return_stack_top:
    // Pop return value, dismantle frame
    ldr     x6, [x19]          // SP
    ldr     x0, [x6]           // return value
    ldr     x7, [x20]          // FP (frame being dismantled)
    ldr     x9, [x7, #FP_FLAGS_OFS]     // flags
    ubfx    x9, x9, #FRAME_FLAGS_NUM_ARGS_SHIFT, #FRAME_FLAGS_NUM_ARGS_WIDTH
    add     x9, x9, #FP_ARG_BASE_WORDS
    lsl     x9, x9, #3
    add     x10, x7, x9        // new SP = FP + (2+num_args)*8
    str     x0, [x10]          // store result at new SP (replaces receiver)
    ldr     x11, [x7]          // saved caller FP
    ldr     x12, [x7, #FP_SAVED_IP_OFS]      // saved caller IP
    str     x10, [x19]         // write back SP
    str     x11, [x20]         // write back FP

    // Is this the entry frame? (FP we're dismantling == entry FP)
    cmp     x7, x24
    b.eq    .Lexit              // yes → exit interpreter with result in x0

    // Continue in caller: restore IP and recompute bytecodes base
    mov     x21, x12            // restore IP from saved caller IP

    // Recompute x23 = bytecodes base from caller's method
    ldr     x13, [x11, #FP_METHOD_OFS]    // caller method at new FP - 1*W
    ldr     x15, [x13, #CM_BYTECODES_OFS]    // CM_BYTECODES (field 4) → ByteArray pointer
    add     x23, x15, #OBJ_FIELDS_OFS      // skip ByteArray header → data
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
    ldr     x7, [x6, #FP_METHOD_OFS]      // current method
    ldr     x7, [x7, #CM_LITERALS_OFS]      // CM_LITERALS → Array
    add     x7, x7, #OBJ_FIELDS_OFS        // skip Array header
    ldr     x9, [x7, x5, lsl #3]   // x9 = block CM (from literals)
    ldr     x10, [x6, #FP_RECEIVER_OFS]    // x10 = current receiver (home receiver)
    // Allocate Block: om_alloc(om, block_class, FORMAT_FIELDS, 2)
    mov     x0, x26             // om
    ldr     x1, [x25, #CLASS_TABLE_BLOCK_OFS]     // class_table_obj field[1] = Block class
    mov     x2, #FORMAT_FIELDS
    mov     x3, #2              // 2 fields
    // Save volatile state
    stp     x9, x10, [sp, #-16]!
    bl      _om_alloc
    ldp     x9, x10, [sp], #16
    // Check for OOM — if NULL, trigger GC and retry
    cbnz    x0, .Lblock_alloc_ok

    // --- GC safe point: collect and retry ---
    // x26 points to GC context: [0..1] = from, [2..3] = to, [4..5] = starts, [6] = size
    // Save x9 (block CM), x10 (home receiver) as roots along with stack roots

    sub     x28, x21, x23       // preserve current bytecode offset

    // First, count the exact slot roots, then allocate exact temporary storage.
    ldr     x0, [x19]          // current Smalltalk SP
    ldr     x1, [x20]          // current FP
    mov     x2, #0
    mov     x3, #0
    bl      _gc_collect_stack_slots
    mov     x22, x0             // slot root count

    lsl     x16, x22, #3        // slot_buf bytes
    add     x17, x22, #4
    lsl     x17, x17, #3        // root_buf bytes (slot roots + explicit roots)
    add     x15, x16, x17
    add     x15, x15, #16       // save area for x9/x10
    add     x15, x15, #15
    and     x15, x15, #-16
    sub     sp, sp, x15

    add     x14, sp, x16
    add     x14, x14, x17
    str     x9, [x14]
    str     x10, [x14, #8]

    // Collect root slot addresses from frames and suspended operand stacks.
    ldr     x0, [x19]          // current Smalltalk SP
    ldr     x1, [x20]          // current FP
    add     x2, sp, #0         // slot_buf
    mov     x3, x22
    bl      _gc_collect_stack_slots

    add     x14, sp, x16
    add     x14, x14, x17
    ldr     x9, [x14]
    ldr     x10, [x14, #8]

    // Materialize slot roots into the root buffer right after the slot buffer.
    add     x11, sp, #0         // slot_buf
    add     x12, sp, x22, lsl #3       // root_buf
    mov     x13, #0
.Lgc_slot_roots_to_values:
    cmp     x13, x22
    b.ge    .Lgc_slot_roots_done
    ldr     x14, [x11, x13, lsl #3]  // slot address
    ldr     x15, [x14]               // root value
    str     x15, [x12, x13, lsl #3]
    add     x13, x13, #1
    b       .Lgc_slot_roots_to_values
.Lgc_slot_roots_done:

    // Add x9 (block CM), x10 (receiver), and x25 (class table) as explicit roots.
    str     x9, [x12, x22, lsl #3]
    add     x13, x22, #1
    str     x10, [x12, x13, lsl #3]
    add     x13, x13, #1
    str     x25, [x12, x13, lsl #3]
    add     x13, x13, #1

    // Also add the current method from the frame
    ldr     x0, [x20]          // FP
    ldr     x0, [x0, #FP_METHOD_OFS]     // method
    str     x0, [x12, x13, lsl #3]

    // gc_collect(roots, num_roots, from_space, to_space, from_start, from_end)
    mov     x0, x12             // roots
    add     x1, x22, #4         // num_roots
    mov     x2, x26             // from_space = gc_ctx[0..1]
    add     x3, x26, #16       // to_space = gc_ctx[2..3]
    ldr     x4, [x26, #GCCTX_FROM_START_OFS]     // from_start = gc_ctx[4]
    ldr     x5, [x26, #GCCTX_FROM_END_OFS]      // from_end = gc_ctx[1]
    bl      _gc_collect

    // Write updated slot-root values back to their exact stack/frame slots.
    add     x11, sp, #0         // slot_buf
    add     x12, sp, x22, lsl #3       // root_buf
    mov     x13, #0
.Lgc_values_to_slot_roots:
    cmp     x13, x22
    b.ge    .Lgc_values_done
    ldr     x15, [x11, x13, lsl #3]
    ldr     x6, [x12, x13, lsl #3]
    str     x6, [x15]
    add     x13, x13, #1
    b       .Lgc_values_to_slot_roots
.Lgc_values_done:

    // Read back updated roots
    ldr     x9, [x12, x22, lsl #3]   // updated block CM
    add     x13, x22, #1
    ldr     x10, [x12, x13, lsl #3]  // updated receiver
    add     x13, x13, #1
    ldr     x25, [x12, x13, lsl #3]  // updated class table

    lsl     x16, x22, #3
    add     x17, x22, #4
    lsl     x17, x17, #3
    add     x15, x16, x17
    add     x15, x15, #16
    add     x15, x15, #15
    and     x15, x15, #-16
    add     sp, sp, x15       // pop slot/root/save buffers

    // Swap from/to in GC context
    ldp     x0, x1, [x26]       // from_free, from_end
    ldp     x2, x3, [x26, #GCCTX_TO_FREE_OFS]  // to_free, to_end
    stp     x2, x3, [x26]       // new from = old to
    stp     x0, x1, [x26, #GCCTX_TO_FREE_OFS]  // new to = old from
    ldr     x0, [x26, #GCCTX_FROM_START_OFS]      // from_start
    ldr     x1, [x26, #GCCTX_TO_START_OFS]      // to_start
    str     x1, [x26, #GCCTX_FROM_START_OFS]
    str     x0, [x26, #GCCTX_TO_START_OFS]

    // Reset to-space free ptr to its start
    ldr     x0, [x26, #GCCTX_TO_START_OFS]      // new to_start
    ldr     x1, [x26, #GCCTX_SPACE_SIZE_OFS]      // space_size
    str     x0, [x26, #GCCTX_TO_FREE_OFS]      // to_free = to_start
    add     x0, x0, x1
    str     x0, [x26, #GCCTX_TO_END_OFS]      // to_end = to_start + size

    // Recompute bytecodes base and IP after moving the current method/bytecodes.
    ldr     x0, [x20]
    ldr     x0, [x0, #FP_METHOD_OFS]
    ldr     x1, [x0, #CM_BYTECODES_OFS]
    add     x23, x1, #OBJ_FIELDS_OFS
    add     x21, x23, x28

    // Retry allocation
    mov     x0, x26
    ldr     x1, [x25, #CLASS_TABLE_BLOCK_OFS]     // Block class
    mov     x2, #FORMAT_FIELDS
    mov     x3, #2
    stp     x9, x10, [sp, #-16]!
    bl      _om_alloc
    ldp     x9, x10, [sp], #16
    // If still NULL, we're truly OOM — crash
    cbz     x0, .Lblock_oom

.Lblock_alloc_ok:
    // x0 = new Block object
    str     x10, [x0, #BLOCK_HOME_RECEIVER_OFS]     // field 0 = home receiver
    str     x9, [x0, #BLOCK_CM_OFS]      // field 1 = CM
    // Push Block onto stack
    ldr     x6, [x19]          // SP
    sub     x6, x6, #8
    str     x0, [x6]
    str     x6, [x19]
    b       .Ldispatch

.Lblock_oom:
    brk     #2                  // unrecoverable OOM after GC

.Lbc_halt:
    // Return top of stack without dismantling frame
    ldr     x6, [x19]
    ldr     x0, [x6]
    b       .Lexit

.Lexit:
    EPILOGUE
    ret

.Lprim_receiver_type_error:
    brk     #11                 // primitive called with wrong tagged operand types

.Lprim_overflow:
    brk     #12                 // SmallInteger primitive overflow

.Lprimitive_failed:
    b       .Lactivate_found_method
