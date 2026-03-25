# ARM Living Objects — Smalltalk Contexts on ARM64

Prototype of Cog-style stack frames for Smalltalk contexts, in ARM64 machine code.
Based on Eliot Miranda's blog post: "Under Cover Contexts and the Big Frame-Up"

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

## Tests

### 1. Stack Primitives

- [x] push a value onto a stack and read it back
- [x] push two values and pop one, reading the remaining top

### 2. Method Activation (internalActivateNewMethod)

Build a frame on send. Caller pushes receiver and args, then:
push saved IP, push saved FP, set FP = SP, push method, push flags,
push nil context slot, push receiver (again), push nil for each temp.

- [x] activate a method with 0 args, 0 temps: verify frame is built with correct layout
- [x] read receiver from a frame at FP - 4\*W
- [ ] activate a method with 0 args, 1 temp: verify temp is initialized to 0 (nil)
- [ ] activate a method with 1 arg, 0 temps: verify arg is accessible above the frame
- [ ] activate a method with 2 args, 1 temp: verify args and temp layout

### 3. Frame Field Access

- [ ] read method from frame at FP - 1\*W
- [ ] read flags from frame at FP - 2\*W
- [ ] decode num_args from flags byte 1
- [ ] decode is_block from flags byte 2
- [ ] decode has_context from flags byte 0

### 4. Temporary Variable Access (temporary:in:)

Temps and args are split: args above the frame, temps below.
Access requires knowing num_args from the flags word.

- [ ] access temp 0 (first temp after receiver) at FP - 5\*W
- [ ] access arg 0 when num_args=1: located at FP + 2\*W
- [ ] access arg 1 when num_args=2: located at FP + 3\*W
- [ ] store into a temporary variable (STORE_TEMPORARY_VARIABLE, bytecode 5)

### 5. Return (commonCallerReturn)

Dismantle the frame: set SP = FP + frameStackedReceiverOffset,
restore FP from saved caller FP, restore IP from saved caller IP,
write return value at new SP top.

- [ ] return from a 0-arg method: caller's FP and IP restored, result on stack
- [ ] return from a 1-arg method: arg is popped, result replaces receiver+args
- [ ] return from a 2-arg method: both args popped correctly

### 6. Bytecode Implementations

- [ ] PUSH_SELF (bytecode 3): push receiver from current frame onto stack
- [ ] PUSH_TEMPORARY_VARIABLE (bytecode 2): push temp N onto stack
- [ ] PUSH_INSTANCE_VARIABLE (bytecode 1): push field N of receiver onto stack
- [ ] PUSH_LITERAL (bytecode 0): push literal N from method's literal area
- [ ] STORE_TEMPORARY_VARIABLE (bytecode 5): pop and store into temp N
- [ ] STORE_INSTANCE_VARIABLE (bytecode 4): pop and store into receiver field N
- [ ] RETURN_STACK_TOP (bytecode 7): return top of stack to caller
- [ ] DUPLICATE (bytecode 12): push a copy of top of stack
- [ ] POP (bytecode 11): discard top of stack — done as stack_pop

### 7. Marriage — Lazy Context Creation

A context object is created only when thisContext is referenced.
The context's sender field stores the frame pointer with the SmallInteger
tag bit set (bit 0 = 1). The context's IP field stores the caller's saved FP
(for validation). The frame's context slot is set to point at the context,
and the has_context flag byte is set to 1.

- [ ] marry a frame: create a context struct, store tagged FP in sender
- [ ] detect a married context: sender has SmallInteger tag (bit 0 set)
- [ ] detect a single context: sender is a pointer (bit 0 clear) or nil
- [ ] find frame from married context: clear tag bit from sender to get FP
- [ ] validate marriage: context's IP field matches frame's caller saved FP
- [ ] frame's context slot points back to the context
- [ ] frame's has_context flag is set after marriage
- [ ] ensure only one context per frame (no polygamy)

### 8. Widowhood — Context Outlives Frame

When a married context's frame has exited, the context is "widowed."
Detection: the frame pointer in sender points into a stack page, but
the frame at that position no longer matches (different context field
or page is free). Widowing nils out sender and IP.

- [ ] detect a widowed context: sender is tagged but frame no longer valid
- [ ] widow a context: nil out sender and IP fields
- [ ] widowed context retains method, receiver, and arguments
- [ ] widowed context does NOT retain non-argument temporaries

### 9. Divorce — Flushing Frames to Heap

On snapshot or when a stack page must be reclaimed, all frames on the page
are converted to single contexts linked through sender fields.
updateStateOfSpouseContextForFrame copies the current stack state
(receiver, args, temps, stack depth) into the context.

- [ ] divorce a single frame: context becomes single, sender = caller context
- [ ] divorce updates context's IP to the bytecode offset
- [ ] divorce updates context's stack pointer (temp count)
- [ ] divorce a chain of frames: contexts linked through sender fields
- [ ] after divorce, the stack page is marked free (baseFP = 0)

### 10. Block Activation (activateNewClosureMethod)

A block closure has an outerContext. On activation:
push saved IP, push saved FP, set FP = SP, push method (from outerContext),
push flags (is_block=true), push nil context, push receiver (from outerContext),
push copied values from the closure.

- [ ] activate a block with 0 args: is_block flag set, method from outerContext
- [ ] activate a block with 1 copied value: copied value is accessible as temp
- [ ] block's receiver is the home method's receiver (from outerContext)
- [ ] non-local return from block: walk closure's outerContext chain to find home

### 11. Stack Pages

The stack is divided into fixed-size pages. Each page holds ~20 activations.
Pages are linked: a base frame's saved caller FP is null, and its saved
caller IP slot holds the caller context (spouse of the frame on the page below).

- [ ] allocate a stack page of fixed size (e.g., 1024 bytes)
- [ ] detect base frame: saved caller FP == 0
- [ ] base frame's caller context is in the saved IP slot
- [ ] link two stack pages via married context in base frame
- [ ] track page usage: baseFP, headFP, headSP
- [ ] mark a page as free: baseFP = 0

### 12. Stack Overflow

On every frame build, check SP < stackLimit. On overflow:
marry the current top frame, allocate a new page, move frames
to the new page, link via base frame.

- [ ] detect stack overflow: SP < stackLimit after frame build
- [ ] on overflow, marry current frame and move to new page
- [ ] if no free pages, flush LRU page (divorce all its frames) and reuse

### 13. Stack Page Return (baseReturn)

Returning from a base frame must follow the caller context link
to find the frame to resume in, possibly on another stack page
or in a single context that needs a new frame built for it.

- [ ] return from base frame to a married context on another page
- [ ] return from base frame to a single context: build a new frame for it

### 14. Interrupt Check via stackLimit

Peter Deutsch's trick: an interrupt handler sets stackLimit to all-ones
so the next stack overflow check triggers, allowing the VM to break out
of execution to process events. Also checked on backward branches.

- [ ] normal stackLimit: overflow check passes for normal execution
- [ ] interrupt: set stackLimit to max, next frame build triggers event check
- [ ] backward branch checks stackLimit for breaking out of infinite loops
