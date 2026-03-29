// lookup.s — Class resolution, method dictionary lookup, class-based method resolution

.include "macros.s"

.global _oop_class
.global _md_lookup
.global _class_lookup

.align 2

// oop_class(oop, class_table_obj) -> class pointer
// x0 = OOP (tagged value)
// x1 = class table object (heap object, FORMAT_INDEXABLE)
//       fields at [x1 + 24 + index*8]  (3-word header)
// Returns class pointer in x0.
_oop_class:
    // Check tag bits
    tst     x0, #1              // bit 0 set → SmallInteger (tag 01) or special (tag 11)
    b.eq    .Loop_heap
    tst     x0, #2              // bit 1 set → special (tag 11)
    b.ne    .Loop_special
    // SmallInteger (tag 01)
    ldr     x0, [x1, #24]      // class_table[0] (offset 24 = header)
    ret
.Loop_special:
    // Check low 4 bits for Character (0x0F)
    and     x2, x0, #0x0F
    cmp     x2, #0x0F
    b.eq    .Loop_character
    cmp     x0, #7              // tagged true
    b.eq    .Loop_true
    cmp     x0, #11             // tagged false
    b.eq    .Loop_false
    // nil or other special — no class yet, return 0
    mov     x0, #0
    ret
.Loop_true:
    ldr     x0, [x1, #40]      // class_table[2] (offset 24 + 16)
    ret
.Loop_false:
    ldr     x0, [x1, #48]      // class_table[3] (offset 24 + 24)
    ret
.Loop_character:
    ldr     x0, [x1, #56]      // class_table[4] (offset 24 + 32)
    ret
.Loop_heap:
    ldr     x0, [x0]           // header word 0 = class pointer
    ret

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
    PROLOGUE
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
    EPILOGUE
    ret

