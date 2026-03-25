# ARM Living Objects — Smalltalk Contexts on ARM64

Prototype of Cog-style stack frames for Smalltalk contexts, in ARM64 machine code.

Stack grows downward. Frame layout (from FP, offsets in words):

    [FP + 2]  saved caller IP
    [FP + 1]  saved caller FP
    [FP + 0]  method
    [FP - 1]  flags (has_context | is_block | num_args)
    [FP - 2]  context (nil or married context pointer)
    [FP - 3]  receiver
    [FP - 4]  temp 0
    [FP - 5]  temp 1
    ...

Arguments live above the frame in the caller's stack area.

## Tests

- [x] push a value onto a stack and read it back
- [ ] push two values and pop one, reading the remaining top
- [ ] activate a method: build a frame with saved IP, saved FP, method, flags, context slot, receiver
- [ ] read receiver from a frame at a known offset from FP
- [ ] read a temporary variable from a frame
- [ ] read an argument from a frame (located above the frame in caller's area)
- [ ] return from a frame: restore caller FP and IP, pop args, push result
- [ ] push receiver bytecode: push receiver from current frame onto stack
- [ ] marry a frame to a context: store tagged FP in context's sender field
- [ ] detect a married context via SmallInteger tag in sender field
