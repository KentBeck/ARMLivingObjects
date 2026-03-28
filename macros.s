// macros.s — Shared assembly macros
//
// PROLOGUE / EPILOGUE: save/restore ALL callee-saved registers.
// Use at the start and end of every function that calls other functions
// or uses x19-x28. Saves x19-x28, x29 (FP), x30 (LR).
//
// Convention:
//   x0-x15  = scratch (caller-saved, may be clobbered by any bl)
//   x16-x18 = platform reserved
//   x19-x28 = callee-saved (always preserved by PROLOGUE/EPILOGUE)
//   x29     = frame pointer (saved)
//   x30     = link register (saved)

.macro PROLOGUE
    stp     x29, x30, [sp, #-16]!
    stp     x19, x20, [sp, #-16]!
    stp     x21, x22, [sp, #-16]!
    stp     x23, x24, [sp, #-16]!
    stp     x25, x26, [sp, #-16]!
    stp     x27, x28, [sp, #-16]!
.endm

.macro EPILOGUE
    ldp     x27, x28, [sp], #16
    ldp     x25, x26, [sp], #16
    ldp     x23, x24, [sp], #16
    ldp     x21, x22, [sp], #16
    ldp     x19, x20, [sp], #16
    ldp     x29, x30, [sp], #16
.endm

