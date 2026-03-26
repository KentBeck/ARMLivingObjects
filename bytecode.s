// bytecode.s — Bytecode implementations
// All bytecodes take (sp_ptr, fp_ptr) as first two args.

.global _bc_push_self
.global _bc_push_temp
.global _bc_push_inst_var
.global _bc_push_literal
.global _bc_store_temp
.global _bc_store_inst_var
.global _bc_return_stack_top
.global _bc_duplicate
.global _bc_pop

.align 2

// Bytecode 3: PUSH_SELF — push receiver from FP - 4*W onto stack
_bc_push_self:
    ldr     x2, [x1]           // x2 = FP
    ldr     x3, [x2, #-32]     // receiver at FP - 4*8
    ldr     x4, [x0]           // x4 = SP
    sub     x4, x4, #8
    str     x3, [x4]
    str     x4, [x0]
    ret

// Bytecode 2: PUSH_TEMPORARY_VARIABLE — push temp N onto stack
// x2 = temp index
_bc_push_temp:
    ldr     x3, [x1]           // FP
    add     x4, x2, #5
    lsl     x4, x4, #3
    sub     x5, x3, x4
    ldr     x6, [x5]
    ldr     x7, [x0]           // SP
    sub     x7, x7, #8
    str     x6, [x7]
    str     x7, [x0]
    ret

// Bytecode 1: PUSH_INSTANCE_VARIABLE — push field N of receiver
// x2 = field index
// Receiver is an object pointer; fields start after 3-word header (offset 24).
_bc_push_inst_var:
    ldr     x3, [x1]           // FP
    ldr     x4, [x3, #-32]     // receiver pointer
    add     x4, x4, #24        // skip 3-word header
    lsl     x5, x2, #3
    add     x6, x4, x5
    ldr     x7, [x6]
    ldr     x8, [x0]           // SP
    sub     x8, x8, #8
    str     x7, [x8]
    str     x8, [x0]
    ret

// Bytecode 0: PUSH_LITERAL — push literal N from method's literal area
// x2 = literal index
// Method is a CompiledMethod object at FP - 1*W.
// Literals start at CM_FIRST_LITERAL (field 3) = header(3) + 3 = offset 48 bytes.
_bc_push_literal:
    ldr     x3, [x1]           // FP
    ldr     x4, [x3, #-8]      // method (CompiledMethod object pointer)
    add     x4, x4, #48        // skip to CM_FIRST_LITERAL
    lsl     x5, x2, #3
    add     x6, x4, x5
    ldr     x7, [x6]
    ldr     x8, [x0]           // SP
    sub     x8, x8, #8
    str     x7, [x8]
    str     x8, [x0]
    ret

// Bytecode 5: STORE_TEMPORARY_VARIABLE — pop and store into temp N
// x2 = temp index
_bc_store_temp:
    ldr     x3, [x0]           // SP
    ldr     x4, [x3]           // pop value
    add     x3, x3, #8
    str     x3, [x0]
    ldr     x5, [x1]           // FP
    add     x6, x2, #5
    lsl     x6, x6, #3
    sub     x7, x5, x6
    str     x4, [x7]
    ret

// Bytecode 4: STORE_INSTANCE_VARIABLE — pop and store into receiver field N
// x2 = field index
// Fields start after 3-word header (offset 24).
_bc_store_inst_var:
    ldr     x3, [x0]           // SP
    ldr     x4, [x3]           // pop value
    add     x3, x3, #8
    str     x3, [x0]
    ldr     x5, [x1]           // FP
    ldr     x6, [x5, #-32]     // receiver pointer
    add     x6, x6, #24        // skip 3-word header
    lsl     x7, x2, #3
    add     x8, x6, x7
    str     x4, [x8]
    ret

// Bytecode 7: RETURN_STACK_TOP — pop top of stack and return to caller
// x2 = pointer to IP variable
_bc_return_stack_top:
    ldr     x3, [x0]           // SP
    ldr     x4, [x3]           // return value
    ldr     x5, [x1]           // FP
    ldr     x6, [x5, #-16]     // flags
    ubfx    x6, x6, #8, #8     // num_args
    add     x6, x6, #2
    lsl     x6, x6, #3
    add     x7, x5, x6         // new SP
    str     x4, [x7]           // store return value
    ldr     x8, [x5]           // saved caller FP
    str     x8, [x1]
    ldr     x9, [x5, #8]       // saved caller IP
    str     x9, [x2]
    str     x7, [x0]
    ret

// Bytecode 12: DUPLICATE — push a copy of top of stack
_bc_duplicate:
    ldr     x1, [x0]
    ldr     x2, [x1]
    sub     x1, x1, #8
    str     x2, [x1]
    str     x1, [x0]
    ret

// Bytecode 11: POP — discard top of stack
_bc_pop:
    ldr     x1, [x0]
    add     x1, x1, #8
    str     x1, [x0]
    ret

