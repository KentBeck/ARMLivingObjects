// persist.s — Image serialization: pointer <-> offset conversion
//
// Objects are contiguous in the buffer. Each object:
//   [0] = class ptr, [1] = format, [2] = size, [3..] = fields
// A value is a pointer if: tag bits (low 2) == 00, non-zero,
//   and falls within [heap_base, heap_base + size).

.include "macros.s"
.include "asm_constants_shared.s"

.global _image_pointers_to_offsets
.global _image_offsets_to_pointers

.align 2

// image_pointers_to_offsets(buf, size, heap_base)
// x0 = buf, x1 = size (bytes used), x2 = heap_base
// Convert pointers in [heap_base, heap_base+size) to offsets from heap_base.
_image_pointers_to_offsets:
    PROLOGUE
    mov     x19, x0             // x19 = buf
    add     x20, x0, x1         // x20 = buf + size (end)
    mov     x21, x2             // x21 = heap_base
    add     x22, x2, x1         // x22 = heap_base + size (range end)
    mov     x23, x0             // x23 = current object ptr

.Lp2o_obj_loop:
    cmp     x23, x20
    b.hs    .Lp2o_done

    // Convert class ptr: obj[0]
    mov     x0, x23             // slot address = &obj[0]
    bl      .Lp2o_convert_slot

    // Read format and size
    ldr     x24, [x23, #OBJ_FORMAT_OFS]      // format
    ldr     x25, [x23, #OBJ_SIZE_OFS]     // size

    // Skip field conversion for FORMAT_BYTES
    cmp     x24, #FORMAT_BYTES
    b.eq    .Lp2o_advance

    // Convert fields: obj[3] through obj[3+size-1]
    mov     x26, #0
.Lp2o_fields:
    cmp     x26, x25
    b.ge    .Lp2o_advance
    add     x27, x26, #OBJ_HEADER_WORDS
    add     x0, x23, x27, lsl #3   // &obj[3 + i]
    bl      .Lp2o_convert_slot
    add     x26, x26, #1
    b       .Lp2o_fields

.Lp2o_advance:
    // Advance to next object
    cmp     x24, #FORMAT_BYTES
    b.ne    .Lp2o_adv_words
    add     x25, x25, #7
    lsr     x25, x25, #3        // ceil(bytes/8)
.Lp2o_adv_words:
    add     x25, x25, #OBJ_HEADER_WORDS        // + header
    add     x23, x23, x25, lsl #3
    b       .Lp2o_obj_loop

.Lp2o_done:
    EPILOGUE
    ret

// Helper: convert slot at address x0 from pointer to offset
.Lp2o_convert_slot:
    ldr     x3, [x0]
    tst     x3, #3
    b.ne    .Lp2o_slot_done     // tagged value, skip
    cmp     x3, x21             // >= heap_base?
    b.lo    .Lp2o_slot_done
    cmp     x3, x22             // < heap_base + size?
    b.hs    .Lp2o_slot_done
    sub     x3, x3, x21         // offset = ptr - heap_base
    // Mark as offset: set bit 2 to distinguish from tagged values
    // Actually offsets are 8-byte aligned (bits 0-2 = 0).
    // Tagged SmallInt has bit 0 set. Nil/true/false have bits 0-1 set.
    // So offset (bits 0-2 = 0) is distinguishable. No extra marking needed.
    str     x3, [x0]
.Lp2o_slot_done:
    ret

// image_offsets_to_pointers(buf, size, new_base)
// x0 = buf, x1 = size, x2 = new_base
// Convert offsets back to pointers by adding new_base.
// An offset is: tag bits == 00, non-zero, and < size.
_image_offsets_to_pointers:
    PROLOGUE
    mov     x19, x0             // buf
    add     x20, x0, x1         // end
    mov     x21, x2             // new_base
    mov     x22, x1             // size
    mov     x23, x0             // current object

.Lo2p_obj_loop:
    cmp     x23, x20
    b.hs    .Lo2p_done

    // Convert class ptr
    mov     x0, x23
    bl      .Lo2p_convert_slot

    ldr     x24, [x23, #OBJ_FORMAT_OFS]      // format
    ldr     x25, [x23, #OBJ_SIZE_OFS]     // size

    cmp     x24, #FORMAT_BYTES
    b.eq    .Lo2p_advance

    mov     x26, #0
.Lo2p_fields:
    cmp     x26, x25
    b.ge    .Lo2p_advance
    add     x27, x26, #OBJ_HEADER_WORDS
    add     x0, x23, x27, lsl #3
    bl      .Lo2p_convert_slot
    add     x26, x26, #1
    b       .Lo2p_fields

.Lo2p_advance:
    cmp     x24, #FORMAT_BYTES
    b.ne    .Lo2p_adv_words
    add     x25, x25, #7
    lsr     x25, x25, #3
.Lo2p_adv_words:
    add     x25, x25, #OBJ_HEADER_WORDS
    add     x23, x23, x25, lsl #3
    b       .Lo2p_obj_loop

.Lo2p_done:
    EPILOGUE
    ret

// Helper: convert slot at x0 from offset to pointer
// An offset is stored as (real_offset + 8) to distinguish from NULL (0).
// On save: ptr -> (ptr - heap_base + 8). On load: (offset - 8 + new_base).
// But we use a simpler scheme: offsets are 8-byte aligned with tag 00.
// To distinguish offset 0 from NULL, pointers_to_offsets never produces 0:
// we add 8 as a bias. offsets_to_pointers subtracts 8.
// ACTUALLY: simplest approach — first object is at offset 0 in the heap.
// We avoid the ambiguity by noting that the p2o step only converts values
// that are in [heap_base, heap_base+size). NULL=0 is never in that range
// (heap_base is a large address). So offset 0 in the image IS always valid.
// On load, 0 tag-00 and < size → convert.
.Lo2p_convert_slot:
    ldr     x3, [x0]
    tst     x3, #3
    b.ne    .Lo2p_slot_done     // tagged value, skip
    cmp     x3, x22             // < size? (valid offset)
    b.hs    .Lo2p_slot_done     // external ptr or truly out of range
    add     x3, x3, x21         // ptr = offset + new_base
    str     x3, [x0]
.Lo2p_slot_done:
    ret
