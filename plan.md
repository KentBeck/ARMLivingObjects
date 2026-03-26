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
- [x] activate a method with 0 args, 1 temp: verify temp is initialized to 0 (nil)
- [x] activate a method with 1 arg, 0 temps: verify arg is accessible above the frame
- [x] activate a method with 2 args, 1 temp: verify args and temp layout

### 3. Frame Field Access

- [x] read method from frame at FP - 1\*W
- [x] read flags from frame at FP - 2\*W
- [x] decode num_args from flags byte 1
- [x] decode is_block from flags byte 2
- [x] decode has_context from flags byte 0

### 4. Temporary Variable Access (temporary:in:)

Temps and args are split: args above the frame, temps below.
Access requires knowing num_args from the flags word.

- [x] access temp 0 (first temp after receiver) at FP - 5\*W
- [x] access arg 0 when num_args=1: located at FP + 2\*W
- [x] access arg 1 when num_args=2: located at FP + 3\*W
- [x] store into a temporary variable (STORE_TEMPORARY_VARIABLE, bytecode 5)

### 5. Return (commonCallerReturn)

Dismantle the frame: set SP = FP + frameStackedReceiverOffset,
restore FP from saved caller FP, restore IP from saved caller IP,
write return value at new SP top.

- [x] return from a 0-arg method: caller's FP and IP restored, result on stack
- [x] return from a 1-arg method: arg is popped, result replaces receiver+args
- [x] return from a 2-arg method: both args popped correctly

### 6. Bytecode Implementations

- [x] PUSH_SELF (bytecode 3): push receiver from current frame onto stack
- [x] PUSH_TEMPORARY_VARIABLE (bytecode 2): push temp N onto stack
- [x] PUSH_INSTANCE_VARIABLE (bytecode 1): push field N of receiver onto stack
- [x] PUSH_LITERAL (bytecode 0): push literal N from method's literal area
- [x] STORE_TEMPORARY_VARIABLE (bytecode 5): pop and store into temp N
- [x] STORE_INSTANCE_VARIABLE (bytecode 4): pop and store into receiver field N
- [x] RETURN_STACK_TOP (bytecode 7): return top of stack to caller
- [x] DUPLICATE (bytecode 12): push a copy of top of stack
- [x] POP (bytecode 11): discard top of stack — done as stack_pop

### 7. Tagged Pointers

2 tag bits in the low bits of every word. All values on the stack,
in object fields, and in temporaries are tagged.

    Tag bits (bits 1:0):
      00 = object pointer (8-byte aligned heap pointer)
      01 = SmallInteger (signed 62-bit value in bits 63:2)
      10 = immediate float (62-bit truncated double in bits 63:2)
      11 = special object (nil=0b0011, true=0b0111, false=0b1011)

Encoding/decoding:
SmallInteger N → (N << 2) | 0b01
SmallInteger decode → arithmetic right shift 2
Float F → truncate mantissa, shift, | 0b10
nil = 0x03
true = 0x07
false = 0x0B

- [x] encode SmallInteger 0 and decode it back
- [x] encode SmallInteger 42 and decode it back
- [x] encode SmallInteger -1 and decode it back
- [x] detect tag: SmallInteger has bits 1:0 == 01
- [x] detect tag: object pointer has bits 1:0 == 00
- [x] detect tag: immediate float has bits 1:0 == 10
- [x] detect tag: special object has bits 1:0 == 11
- [x] nil is the tagged value 0x03
- [x] true is the tagged value 0x07
- [x] false is the tagged value 0x0B
- [x] is_nil check: compare to 0x03
- [x] is_boolean check: value == 7 || value == 11
- [x] SmallInteger addition: decode both, add, re-encode
- [x] SmallInteger subtraction
- [x] SmallInteger less-than comparison: returns tagged true or false
- [x] SmallInteger equality: returns tagged true or false

### 8. Object Model

Object memory is a fixed-size buffer with a bump allocator.
Crash (trap) when memory is exhausted.

Every heap object has a 2-word header followed by fields:

    Header:
      word 0 = class pointer (tagged object pointer to a Class object)
      word 1 = size (number of field slots, not counting header)

    Fields (size words):
      slot 0, slot 1, ... slot N-1   (all tagged values)

Object pointers are 8-byte aligned, tag bits 00.
The allocator returns a pointer to word 0 (the class pointer).

- [ ] initialize object memory: fixed buffer, free pointer at start
- [ ] allocate an object with 0 fields: returns aligned pointer, advances free ptr
- [ ] allocate an object with 2 fields: size is correct
- [ ] crash on out-of-memory: allocating beyond buffer traps
- [ ] read class pointer from object (word 0)
- [ ] read size from object (word 1)
- [ ] read field 0 from an object (at header + 2\*W)
- [ ] write field 1 of an object
- [ ] object pointer has tag 00 (aligned)
- [ ] fields store tagged values (e.g., SmallInteger in a field)
- [ ] update bc_push_inst_var to work with object header layout
- [ ] update bc_store_inst_var to work with object header layout

### 9. Class and Method Dictionary

A class is an object with known field layout:
field 0 = superclass (tagged pointer or nil)
field 1 = method dictionary (tagged pointer)
field 2 = instance size

A method dictionary maps selector indices to compiled method pointers.
For the prototype, a simple linear array of (selector, method) pairs.

A compiled method is an object:
field 0 = num_args
field 1 = num_temps
field 2 = literal count
field 3..N = literals
followed by bytecode bytes (in a separate byte array or inline)

- [ ] create a class object with superclass, method dict, instance size
- [ ] create a method dictionary with one entry
- [ ] look up a selector in a method dictionary: found
- [ ] look up a selector in a method dictionary: not found
- [ ] look up with superclass chain: found in superclass
- [ ] create a compiled method object with bytecodes and literals
- [ ] read num_args and num_temps from a compiled method

### 10. Bytecode Dispatch Loop

The interpreter loop: fetch bytecode at IP, dispatch to handler,
advance IP, repeat. IP is a pointer into the compiled method's
bytecode array.

    loop:
      ldrb  opcode, [IP], #1     // fetch and advance
      adr   x_table, dispatch_table
      ldr   x_handler, [x_table, opcode, lsl #3]
      br    x_handler            // tail to handler
      // each handler jumps back to loop

- [ ] dispatch a single PUSH_LITERAL bytecode and stop
- [ ] dispatch PUSH_LITERAL then RETURN_STACK_TOP: value returned
- [ ] dispatch PUSH_SELF then RETURN_STACK_TOP
- [ ] dispatch PUSH_TEMP, PUSH_TEMP, sequence
- [ ] dispatch STORE_TEMP then PUSH_TEMP: round-trip through dispatch
- [ ] dispatch JUMP: IP advances to target
- [ ] dispatch JUMP_IF_TRUE with tagged true: jumps
- [ ] dispatch JUMP_IF_TRUE with tagged false: falls through
- [ ] dispatch JUMP_IF_FALSE with tagged false: jumps
- [ ] dispatch JUMP_IF_FALSE with tagged true: falls through

### 11. Message Send (SEND_MESSAGE bytecode 6)

The send bytecode: pop args and receiver, look up selector in
receiver's class, activate the found method, continue dispatch
in the new method.

- [ ] send a 0-arg message: look up, activate, dispatch callee
- [ ] send a 1-arg message: arg and receiver popped, method activated
- [ ] send to superclass: method found in superclass
- [ ] message not understood: signal error (for now, halt)
- [ ] full scenario: create object, send message, method pushes self, returns

### 12. Primitives

Primitive methods short-circuit bytecode execution. The method header
indicates a primitive index. If the primitive succeeds, it pushes the
result and returns without entering the bytecodes.

- [ ] SmallInteger + primitive: tagged add, push result, return
- [ ] SmallInteger - primitive
- [ ] SmallInteger < primitive: returns tagged true/false
- [ ] SmallInteger = primitive
- [ ] at: primitive (array field access by tagged index)
- [ ] at:put: primitive (array field store)
- [ ] new primitive: allocate instance of a class
- [ ] primitive failure: fall through to bytecode execution

### 13. End-to-End Scenarios

- [x] call a method, push self, return: receiver is on top of caller's stack
- [x] call a method with 1 arg, push arg, return: arg value on caller's stack
- [x] call a method, store into temp, push temp, return: temp value on caller's stack
- [x] call a method, push instance variable, return: field value on caller's stack
- [x] nested send: method A calls method B, B returns, A returns, result on original stack
- [ ] call a method, push self, push arg, send message (nested), return result up
- [ ] SmallInteger factorial via recursive message send
- [ ] create a Point object, send #x to get its x field
- [ ] send #+ to two SmallIntegers through the dispatch loop
- [ ] conditional: push value, jump_if_false, two branches return different results

---

## Deferred: Stack Infrastructure (implement when needed)

### D1. Marriage — Lazy Context Creation

- [ ] marry a frame: create a context struct, store tagged FP in sender
- [ ] detect a married context: sender has SmallInteger tag (bit 0 set)
- [ ] detect a single context: sender is a pointer (bit 0 clear) or nil
- [ ] find frame from married context: clear tag bit from sender to get FP
- [ ] validate marriage: context's IP field matches frame's caller saved FP
- [ ] frame's context slot points back to the context
- [ ] frame's has_context flag is set after marriage
- [ ] ensure only one context per frame (no polygamy)

### D2. Widowhood — Context Outlives Frame

- [ ] detect a widowed context: sender is tagged but frame no longer valid
- [ ] widow a context: nil out sender and IP fields
- [ ] widowed context retains method, receiver, and arguments

### D3. Divorce — Flushing Frames to Heap

- [ ] divorce a single frame: context becomes single, sender = caller context
- [ ] divorce updates context's IP to the bytecode offset
- [ ] divorce a chain of frames: contexts linked through sender fields

### D4. Block Activation

- [ ] activate a block with 0 args: is_block flag set, method from outerContext
- [ ] activate a block with 1 copied value: copied value is accessible as temp
- [ ] block's receiver is the home method's receiver
- [ ] non-local return from block

### D5. Stack Pages & Overflow

- [ ] allocate a stack page of fixed size
- [ ] detect stack overflow: SP < stackLimit after frame build
- [ ] link two stack pages via married context in base frame
- [ ] return from base frame to a married context on another page
- [ ] interrupt check via stackLimit (Deutsch's trick)
