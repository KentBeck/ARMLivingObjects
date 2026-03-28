# ARM Living Objects — Completed Work

## LivingObjects Bytecodes

    0  PUSH_LITERAL            (+ 4-byte index)
    1  PUSH_INSTANCE_VARIABLE  (+ 4-byte offset)
    2  PUSH_TEMPORARY_VARIABLE (+ 4-byte offset)
    3  PUSH_SELF
    4  STORE_INSTANCE_VARIABLE (+ 4-byte offset)
    5  STORE_TEMPORARY_VARIABLE(+ 4-byte offset)
    6  SEND_MESSAGE            (+ 4-byte selector index + 4-byte arg count)
    7  RETURN_STACK_TOP
    8  JUMP                    (+ 4-byte target)
    9  JUMP_IF_TRUE            (+ 4-byte target)
    10 JUMP_IF_FALSE           (+ 4-byte target)
    11 POP
    12 DUPLICATE
    14 PUSH_CLOSURE             (+ 4-byte literal_index)

## Frame Layout

Stack grows downward. Caller pushes arguments, then send builds the frame.
The frame pointer (FP) points at the saved caller FP slot.

    higher addresses (caller's area)
    ─────────────────────────────────
    [FP + 3*W]  arg N-1      ← last argument (pushed first)
    ...
    [FP + 2*W]  arg 0        ← first argument (pushed last, just before receiver)
                receiver     ← pushed by caller as part of send
    ─────────────────────────────────
    [FP + 1*W]  saved caller IP
    [FP + 0]    saved caller FP   ← FP points here
    [FP - 1*W]  method
    [FP - 2*W]  flags             (byte 0: has_context, byte 1: num_args, byte 2: is_block)
    [FP - 3*W]  context slot      (0 = no context, or pointer to married context)
    [FP - 4*W]  receiver          (pushed again for fast access)
    [FP - 5*W]  temp 0
    [FP - 6*W]  temp 1
    ...                           evaluation stack continues downward
    ─────────────────────────────────
    lower addresses (SP points to topmost occupied slot)

Word size (W) = 8 bytes on ARM64.

## Completed Sections

### 1. Stack Primitives ✅
### 2. Method Activation ✅
### 3. Frame Field Access ✅
### 4. Temporary Variable Access ✅
### 5. Return ✅
### 6. Bytecode Implementations ✅
### 7. Tagged Pointers ✅
### 8. Object Model ✅
### 9. Class and Method Dictionary ✅
### 10. Bytecode Dispatch Loop ✅
### 11. Message Send ✅
### 12. Primitives (SmallInt +, -, *, <, =, at:, at:put:) ✅
### 12b. Blocks and ifTrue:ifFalse: ✅
### 13. End-to-End Scenarios (factorial, nested sends) ✅
### 14. In-Memory Transactions ✅
### 14b. Heap-allocated class table (globals) ✅
### 15. Generational Garbage Collection ✅
### 16. Persistence ✅

282 tests, all passing.

