// object.s — Object memory: fixed-size bump allocator
//
// Object layout (3-word header + slots):
//   word 0 = class pointer (tagged object pointer to a Class)
//   word 1 = format (0=fields, 1=indexable, 2=bytes)
//   word 2 = size (slot count or byte count)
//   word 3..N = slots (tagged values or raw bytes)

.include "macros.s"

.global _om_init
.global _om_alloc

.align 2

// om_init(buffer, buffer_size_bytes, free_ptr_out)
// x0 = pointer to buffer
// x1 = buffer size in bytes
// x2 = pointer to free_ptr variable (uint64_t*)
_om_init:
    str     x0, [x2]           // free_ptr = buffer start
    add     x1, x0, x1         // end = buffer + size
    str     x1, [x2, #8]       // store end after free_ptr
    ret

// om_alloc(free_ptr_var, class_ptr, format, size) -> object pointer
// x0 = pointer to {free_ptr, end_ptr} pair
// x1 = class pointer (tagged)
// x2 = format (0=fields, 1=indexable, 2=bytes)
// x3 = size (slot count for format 0/1, byte count for format 2)
// Returns: object pointer (untagged, aligned)
_om_alloc:
    ldr     x4, [x0]           // x4 = free_ptr
    ldr     x5, [x0, #8]       // x5 = end_ptr

    // Calculate total words: 3 (header) + slot_words
    cmp     x2, #2
    b.eq    .Lalloc_bytes
    mov     x6, x3             // format 0/1: slot_words = size
    b       .Lalloc_calc
.Lalloc_bytes:
    add     x6, x3, #7         // (size + 7)
    lsr     x6, x6, #3         // / 8
.Lalloc_calc:
    add     x6, x6, #3         // + 3 header words
    lsl     x7, x6, #3         // total bytes

    // Check OOM
    add     x8, x4, x7         // new_free = free_ptr + total_bytes
    cmp     x8, x5
    b.hi    .Loom

    // Write header
    str     x1, [x4]           // word 0: class pointer
    str     x2, [x4, #8]       // word 1: format
    str     x3, [x4, #16]      // word 2: size

    // Zero-initialize slots
    mov     x9, x4
    add     x9, x9, #24        // skip header
    mov     x10, x6
    sub     x10, x10, #3       // slot_words
    cbz     x10, .Lalloc_done
.Lalloc_zero:
    str     xzr, [x9], #8
    sub     x10, x10, #1
    cbnz    x10, .Lalloc_zero

.Lalloc_done:
    str     x8, [x0]           // update free_ptr
    mov     x0, x4              // return object pointer
    ret

.Loom:
    stp     x29, x30, [sp, #-16]!
    bl      _debug_oom
    ldp     x29, x30, [sp], #16
    brk     #1                  // crash on OOM

