// gc.s — Garbage collector primitives
//
// Object layout: [0]=class, [1]=format, [2]=size, [3..]=fields
// Forwarding: after copy, old obj[0] = (new_addr | GC_FORWARD_TAG)
// GC_FORWARD_TAG = 1 (bit 0). Real class ptrs are aligned, so bit 0 is always 0.

.global _gc_copy_object
.global _gc_is_forwarded
.global _gc_forwarding_ptr

.align 2

// gc_copy_object(obj, to_space) -> new_obj
// x0 = pointer to object in from-space
// x1 = pointer to to_space {free_ptr, end_ptr}
// Returns pointer to new copy in x0. Advances to_space[0].
// If already forwarded, returns existing forwarding address.
_gc_copy_object:
    // Check if already forwarded
    ldr     x2, [x0]            // x2 = obj[0] (class or forwarding ptr)
    tst     x2, #1              // forwarding tag set?
    b.ne    .Lgc_already_forwarded

    // Not forwarded — copy the object
    // Object total size = (3 + obj[2]) words = (3 + size) * 8 bytes
    ldr     x3, [x0, #16]      // x3 = obj[2] = field count (size)
    add     x4, x3, #3          // x4 = 3 + size (total words)

    ldr     x5, [x1]            // x5 = to_space free_ptr (destination)
    mov     x6, x5              // x6 = new_obj (return value)

    // Copy x4 words from x0 to x5
    mov     x7, x0              // x7 = src
    mov     x8, x4              // x8 = word count
.Lgc_copy_loop:
    cbz     x8, .Lgc_copy_done
    ldr     x9, [x7], #8
    str     x9, [x5], #8
    sub     x8, x8, #1
    b       .Lgc_copy_loop

.Lgc_copy_done:
    // Update to_space free_ptr
    str     x5, [x1]

    // Leave forwarding pointer in old object
    orr     x9, x6, #1          // new_addr | GC_FORWARD_TAG
    str     x9, [x0]            // old obj[0] = forwarding ptr

    mov     x0, x6              // return new_obj
    ret

.Lgc_already_forwarded:
    // x2 = forwarding ptr with tag
    bic     x0, x2, #1          // clear tag bit -> real address
    ret

// gc_is_forwarded(obj) -> 0 or 1
// x0 = pointer to object
_gc_is_forwarded:
    ldr     x1, [x0]            // obj[0]
    and     x0, x1, #1          // return bit 0
    ret

// gc_forwarding_ptr(obj) -> address
// x0 = pointer to forwarded object
_gc_forwarding_ptr:
    ldr     x1, [x0]            // obj[0] = forwarding ptr with tag
    bic     x0, x1, #1          // clear tag
    ret

