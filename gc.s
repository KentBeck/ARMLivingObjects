// gc.s — Garbage collector primitives
//
// Object layout: [0]=class, [1]=format, [2]=size, [3..]=fields
// Forwarding: after copy, old obj[0] = (new_addr | GC_FORWARD_TAG)
// GC_FORWARD_TAG = 1 (bit 0). Real class ptrs are aligned, so bit 0 is always 0.

.global _gc_copy_object
.global _gc_is_forwarded
.global _gc_forwarding_ptr
.global _gc_collect

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


// gc_collect(roots, num_roots, from_space, to_space, from_start, from_end)
// x0 = roots array (uint64_t*)
// x1 = num_roots
// x2 = from_space {free_ptr, end_ptr} (unused, kept for API symmetry)
// x3 = to_space {free_ptr, end_ptr}
// x4 = from_start (start address of from-space)
// x5 = from_end (end address of from-space)
//
// Cheney's algorithm:
// 1. Copy root objects to to-space, update root pointers
// 2. Scan to-space objects, copy/update any from-space references in fields
// 3. Scan pointer chases free pointer until they meet
_gc_collect:
    stp     x29, x30, [sp, #-16]!
    stp     x19, x20, [sp, #-16]!
    stp     x21, x22, [sp, #-16]!
    stp     x23, x24, [sp, #-16]!
    stp     x25, x26, [sp, #-16]!

    mov     x19, x0             // x19 = roots
    mov     x20, x1             // x20 = num_roots
    mov     x21, x3             // x21 = to_space
    mov     x22, x4             // x22 = from_start
    mov     x23, x5             // x23 = from_end

    // Record scan pointer = current to_space free_ptr (before copying roots)
    ldr     x24, [x21]          // x24 = scan pointer

    // Phase 1: Copy roots to to-space
    mov     x25, #0             // x25 = root index
.Lgc_roots_loop:
    cmp     x25, x20
    b.ge    .Lgc_roots_done
    ldr     x0, [x19, x25, lsl #3]  // root value (tagged)
    // Only copy if it's an object pointer in from-space (tag 00, in range)
    tst     x0, #3              // check tag bits
    b.ne    .Lgc_root_skip      // tagged value, skip
    cmp     x0, x22             // >= from_start?
    b.lo    .Lgc_root_skip
    cmp     x0, x23             // < from_end?
    b.hs    .Lgc_root_skip
    // It's a from-space pointer, copy it
    mov     x1, x21             // to_space
    bl      _gc_copy_object
    str     x0, [x19, x25, lsl #3]  // update root
.Lgc_root_skip:
    add     x25, x25, #1
    b       .Lgc_roots_loop
.Lgc_roots_done:

    // Phase 2: Scan to-space (Cheney scan)
    // x24 = scan pointer, to_space[0] = free pointer (moves as we copy)
.Lgc_scan_loop:
    ldr     x25, [x21]          // x25 = to_space free_ptr
    cmp     x24, x25            // scan caught up to free?
    b.hs    .Lgc_scan_done

    // x24 points to an object in to-space
    // Read its size: obj[2] = field count
    ldr     x26, [x24, #16]     // x26 = size (field count)

    // Scan the class pointer (obj[0]) — it could be in from-space
    ldr     x0, [x24]           // class ptr
    tst     x0, #3
    b.ne    .Lgc_scan_class_done
    cmp     x0, x22
    b.lo    .Lgc_scan_class_done
    cmp     x0, x23
    b.hs    .Lgc_scan_class_done
    mov     x1, x21
    bl      _gc_copy_object
    str     x0, [x24]           // update class ptr
.Lgc_scan_class_done:

    // Scan fields: obj[3] through obj[3 + size - 1]
    mov     x25, #0             // field index
.Lgc_scan_fields:
    cmp     x25, x26
    b.ge    .Lgc_scan_next_obj
    add     x9, x25, #3         // slot = 3 + field_index
    ldr     x0, [x24, x9, lsl #3]  // field value
    tst     x0, #3              // tagged?
    b.ne    .Lgc_scan_field_skip
    cmp     x0, x22             // in from-space?
    b.lo    .Lgc_scan_field_skip
    cmp     x0, x23
    b.hs    .Lgc_scan_field_skip
    // From-space pointer — copy/forward
    mov     x1, x21
    bl      _gc_copy_object
    add     x9, x25, #3
    str     x0, [x24, x9, lsl #3]  // update field
.Lgc_scan_field_skip:
    add     x25, x25, #1
    b       .Lgc_scan_fields

.Lgc_scan_next_obj:
    // Advance scan pointer past this object
    add     x26, x26, #3        // total words = 3 + size
    add     x24, x24, x26, lsl #3  // scan += total_words * 8
    b       .Lgc_scan_loop

.Lgc_scan_done:
    ldp     x25, x26, [sp], #16
    ldp     x23, x24, [sp], #16
    ldp     x21, x22, [sp], #16
    ldp     x19, x20, [sp], #16
    ldp     x29, x30, [sp], #16
    ret