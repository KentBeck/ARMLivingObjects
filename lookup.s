// lookup.s — Method dictionary lookup and class-based method resolution

.global _md_lookup
.global _class_lookup

.align 2

// md_lookup(method_dict, selector) -> method pointer or 0
// x0 = pointer to method dictionary Array object (3-word header + slots)
// x1 = selector (tagged SmallInteger)
// Slots are (selector, method) pairs.
// Returns: method pointer or 0 if not found.
_md_lookup:
    ldr     x2, [x0, #16]      // x2 = size (number of slots)
    add     x3, x0, #24        // x3 = pointer to slot 0 (skip header)
    mov     x4, #0
.Lmd_loop:
    cmp     x4, x2
    b.ge    .Lmd_not_found
    ldr     x5, [x3, x4, lsl #3]
    cmp     x5, x1
    b.eq    .Lmd_found
    add     x4, x4, #2         // skip to next pair
    b       .Lmd_loop
.Lmd_found:
    add     x4, x4, #1
    ldr     x0, [x3, x4, lsl #3]
    ret
.Lmd_not_found:
    mov     x0, #0
    ret

// class_lookup(class, selector) -> method pointer or 0
// x0 = pointer to Class object
// x1 = selector (tagged SmallInteger)
// Walks superclass chain. Returns method pointer or 0.
_class_lookup:
    stp     x29, x30, [sp, #-16]!
    mov     x29, sp
    stp     x19, x20, [sp, #-16]!
    mov     x19, x1             // preserve selector
    mov     x20, x0             // current class
.Lcl_loop:
    cbz     x20, .Lcl_not_found
    cmp     x20, #3             // tagged nil
    b.eq    .Lcl_not_found
    // Load method dict: class field 1 at offset 32
    ldr     x0, [x20, #32]
    cmp     x0, #3              // nil method dict?
    b.eq    .Lcl_super
    mov     x1, x19
    bl      _md_lookup
    cbnz    x0, .Lcl_done
.Lcl_super:
    // Follow superclass: class field 0 at offset 24
    ldr     x20, [x20, #24]
    b       .Lcl_loop
.Lcl_not_found:
    mov     x0, #0
.Lcl_done:
    ldp     x19, x20, [sp], #16
    ldp     x29, x30, [sp], #16
    ret

