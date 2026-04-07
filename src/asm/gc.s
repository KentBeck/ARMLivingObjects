// gc.s — Garbage collector primitives
//
// Object layout: [0]=class, [1]=format, [2]=size, [3..]=fields
// Forwarding: after copy, old obj[0] = (new_addr | GC_FORWARD_TAG)
// GC_FORWARD_TAG = 1 (bit 0). Real class ptrs are aligned, so bit 0 is always 0.

.include "macros.s"
.include "asm_constants_shared.s"

.equ GC_FORWARD_TAG, 1
.equ FP_SENTINEL, 0xCAFE

.global _gc_copy_object
.global _gc_is_forwarded
.global _gc_forwarding_ptr
.global _gc_collect
.global _gc_scan_stack
.global _gc_update_stack
.global _gc_scan_stack_with_eval
.global _gc_update_stack_with_eval
.global _gc_collect_stack_slots

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
    // For fields/indexable: total words = 3 + size
    // For bytes: total words = 3 + ceil(size / 8)
    ldr     x3, [x0, #OBJ_SIZE_OFS]      // x3 = obj[2] = size
    ldr     x4, [x0, #OBJ_FORMAT_OFS]       // x4 = obj[1] = format
    cmp     x4, #FORMAT_BYTES              // FORMAT_BYTES?
    b.ne    .Lgc_copy_not_bytes
    add     x4, x3, #7          // (byte_count + 7)
    lsr     x4, x4, #3          // / 8 = word count for data
    add     x4, x4, #OBJ_HEADER_WORDS          // + header words
    b       .Lgc_copy_size_done
.Lgc_copy_not_bytes:
    add     x4, x3, #OBJ_HEADER_WORDS          // header + slot_count (total words)
.Lgc_copy_size_done:

    ldr     x5, [x1]            // x5 = to_space free_ptr (destination)
    mov     x6, x5              // x6 = new_obj (return value)

    // Ensure the copy fits in to-space before writing into it.
    ldr     x10, [x1, #8]       // x10 = to_space end_ptr
    add     x11, x5, x4, lsl #3 // x11 = destination end
    cmp     x11, x10
    b.hi    .Lgc_copy_oom

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

.Lgc_copy_oom:
    brk     #10                 // GC to-space overflow

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
    PROLOGUE

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

    // Skip field scanning for FORMAT_BYTES objects (raw data, not pointers)
    ldr     x9, [x24, #8]       // obj[1] = format
    cmp     x9, #2              // FORMAT_BYTES = 2
    b.eq    .Lgc_scan_next_obj

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
    // x26 = size from obj[2]. For bytes, compute word count.
    ldr     x9, [x24, #8]       // format
    cmp     x9, #2              // FORMAT_BYTES?
    b.ne    .Lgc_adv_not_bytes
    add     x26, x26, #7
    lsr     x26, x26, #3        // ceil(byte_count / 8)
.Lgc_adv_not_bytes:
    add     x26, x26, #3        // + 3 header words
    add     x24, x24, x26, lsl #3  // scan += total_words * 8
    b       .Lgc_scan_loop

.Lgc_scan_done:
    EPILOGUE
    ret

// gc_scan_stack(fp, root_buf, max_roots) -> num_roots_found
// x0 = fp (current frame pointer)
// x1 = root_buf (output array of object pointers)
// x2 = max_roots (capacity of root_buf)
//
// Frame layout:
//   [FP + 8]  = saved IP
//   [FP + 0]  = saved caller FP
//   [FP - 8]  = method (CompiledMethod pointer)
//   [FP - 16] = flags
//   [FP - 24] = context slot
//   [FP - 32] = receiver
//   [FP - 40] = temp 0, [FP - 48] = temp 1, ...
//
// For each frame: collect receiver, method, and all temps that are object ptrs.
// An object ptr has tag bits == 00 and is non-zero.
_gc_scan_stack:
    PROLOGUE

    mov     x19, x0             // x19 = fp
    mov     x20, x1             // x20 = root_buf
    mov     x21, x2             // x21 = max_roots
    mov     x22, #0             // x22 = count

.Lgc_stack_frame_loop:
    // Check for sentinel FP (0xCAFE or 0)
    cmp     x19, #0
    b.eq    .Lgc_stack_done
    mov     x23, #FP_SENTINEL
    cmp     x19, x23
    b.eq    .Lgc_stack_done

    // Collect receiver: [FP - 32]
    ldr     x0, [x19, #-32]
    bl      .Lgc_maybe_add_root

    // Collect method: [FP - 8]
    ldr     x0, [x19, #-8]
    bl      .Lgc_maybe_add_root

    // Collect context slot: [FP - 24]
    ldr     x0, [x19, #-24]
    bl      .Lgc_maybe_add_root

    // Get num_temps from method object
    ldr     x23, [x19, #-8]     // x23 = method ptr
    // OBJ_FIELD(method, CM_NUM_TEMPS) = method[3 + 2] = method[5]
    ldr     x23, [x23, #40]     // method[5] = num_temps (tagged SmallInt)
    asr     x23, x23, #2        // untag

    // Collect temps: [FP - 40], [FP - 48], ...
    mov     x24, #0             // x24 = temp index
.Lgc_stack_temps:
    cmp     x24, x23
    b.ge    .Lgc_stack_next_frame
    add     x25, x24, #5        // slot offset = 5 + temp_index (in words from FP)
    neg     x25, x25            // negative offset
    ldr     x0, [x19, x25, lsl #3]  // FP[-(5 + i)] = FP - (5+i)*8
    bl      .Lgc_maybe_add_root
    add     x24, x24, #1
    b       .Lgc_stack_temps

.Lgc_stack_next_frame:
    ldr     x19, [x19]          // FP = saved caller FP
    b       .Lgc_stack_frame_loop

.Lgc_stack_done:
    mov     x0, x22             // return count
    EPILOGUE
    ret

// Helper: if x0 is an object ptr (tag 00, non-zero), add to root_buf
// Only touches x0 and callee-saved x20-x22 (which are managed by caller)
.Lgc_maybe_add_root:
    cbz     x0, .Lgc_not_root
    tst     x0, #3
    b.ne    .Lgc_not_root
    cmp     x22, x21            // buffer full?
    b.ge    .Lgc_not_root
    str     x0, [x20, x22, lsl #3]
    add     x22, x22, #1
.Lgc_not_root:
    ret

// gc_scan_stack_with_eval(sp, fp, root_buf, max_roots) -> num_roots_found
// x0 = current Smalltalk SP
// x1 = current frame pointer
// x2 = root_buf
// x3 = max_roots
//
// Walks the frame chain and collects:
// - receiver, method, context, temps
// - suspended operand stack slots for each frame
//
// For the current frame, suspended operands are in [sp, frame_bottom).
// For caller frames, the suspended SP is the child's return SP:
//   child_fp + (2 + child_num_args) * 8
_gc_scan_stack_with_eval:
    PROLOGUE

    mov     x19, x0             // x19 = suspended SP for current frame
    mov     x20, x1             // x20 = fp
    mov     x21, x2             // x21 = root_buf
    mov     x22, x3             // x22 = max_roots
    mov     x23, #0             // x23 = count

.Lgcse_frame_loop:
    cmp     x20, #0
    b.eq    .Lgcse_done
    mov     x24, #FP_SENTINEL
    cmp     x20, x24
    b.eq    .Lgcse_done

    // Collect receiver, method, and context.
    ldr     x0, [x20, #-32]
    bl      .Lgc_maybe_add_root_eval
    ldr     x0, [x20, #-8]
    bl      .Lgc_maybe_add_root_eval
    ldr     x0, [x20, #-24]
    bl      .Lgc_maybe_add_root_eval

    // Get num_temps from method.
    ldr     x24, [x20, #-8]
    ldr     x24, [x24, #40]
    asr     x24, x24, #2

    // Collect temps.
    mov     x25, #0
.Lgcse_temps:
    cmp     x25, x24
    b.ge    .Lgcse_eval
    add     x26, x25, #5
    neg     x26, x26
    ldr     x0, [x20, x26, lsl #3]
    bl      .Lgc_maybe_add_root_eval
    add     x25, x25, #1
    b       .Lgcse_temps

.Lgcse_eval:
    // frame_bottom = FP - (4 + num_temps) * 8
    add     x25, x24, #4
    sub     x26, x20, x25, lsl #3
.Lgcse_eval_loop:
    cmp     x19, x26
    b.hs    .Lgcse_next_frame
    ldr     x0, [x19]
    bl      .Lgc_maybe_add_root_eval
    add     x19, x19, #8
    b       .Lgcse_eval_loop

.Lgcse_next_frame:
    ldr     x24, [x20, #FP_FLAGS_OFS]
    ubfx    x24, x24, #FRAME_FLAGS_NUM_ARGS_SHIFT, #FRAME_FLAGS_NUM_ARGS_WIDTH
    add     x24, x24, #FP_ARG_BASE_WORDS
    add     x19, x20, x24, lsl #3   // caller's suspended SP
    ldr     x20, [x20]              // caller FP
    b       .Lgcse_frame_loop

.Lgcse_done:
    mov     x0, x23
    EPILOGUE
    ret

.Lgc_maybe_add_root_eval:
    cbz     x0, .Lgc_not_root_eval
    tst     x0, #3
    b.ne    .Lgc_not_root_eval
    cmp     x23, x22
    b.ge    .Lgc_not_root_eval
    str     x0, [x21, x23, lsl #3]
    add     x23, x23, #1
.Lgc_not_root_eval:
    ret

// gc_update_stack(fp, from_start, from_end)
// x0 = fp, x1 = from_start, x2 = from_end
// Walk stack frames, follow forwarding pointers for any from-space reference.
_gc_update_stack:
    PROLOGUE
    mov     x19, x0             // x19 = fp
    mov     x20, x1             // x20 = from_start
    mov     x21, x2             // x21 = from_end

.Lgu_frame_loop:
    cbz     x19, .Lgu_done
    mov     x22, #FP_SENTINEL
    cmp     x19, x22
    b.eq    .Lgu_done

    // Update receiver: [FP - 32]
    sub     x0, x19, #32
    bl      .Lgu_update_slot

    // Update method: [FP - 8]
    sub     x0, x19, #8
    bl      .Lgu_update_slot

    // Update context: [FP - 24]
    sub     x0, x19, #24
    bl      .Lgu_update_slot

    // Get num_temps from method
    ldr     x22, [x19, #-8]     // method (already updated)
    ldr     x22, [x22, #40]     // num_temps (tagged)
    asr     x22, x22, #2        // untag

    // Update temps
    mov     x23, #0
.Lgu_temps:
    cmp     x23, x22
    b.ge    .Lgu_next_frame
    add     x24, x23, #5
    neg     x24, x24
    add     x0, x19, x24, lsl #3   // &FP[-(5+i)]
    bl      .Lgu_update_slot
    add     x23, x23, #1
    b       .Lgu_temps

.Lgu_next_frame:
    ldr     x19, [x19]
    b       .Lgu_frame_loop

.Lgu_done:
    EPILOGUE
    ret

// Helper: update a slot at address x0 if it points to from-space
.Lgu_update_slot:
    ldr     x3, [x0]            // value in slot
    tst     x3, #3              // tagged?
    b.ne    .Lgu_slot_done
    cbz     x3, .Lgu_slot_done
    cmp     x3, x20             // >= from_start?
    b.lo    .Lgu_slot_done
    cmp     x3, x21             // < from_end?
    b.hs    .Lgu_slot_done
    // It's a from-space pointer — check if forwarded
    ldr     x4, [x3]            // obj[0]
    tst     x4, #1              // forwarding tag?
    b.eq    .Lgu_slot_done
    bic     x4, x4, #1          // clear tag
    str     x4, [x0]            // update slot
.Lgu_slot_done:
    ret

// gc_update_stack_with_eval(sp, fp, from_start, from_end)
// x0 = current Smalltalk SP
// x1 = current frame pointer
// x2 = from_start
// x3 = from_end
//
// Updates both frame slots and suspended operand stack slots for every frame.
_gc_update_stack_with_eval:
    PROLOGUE
    mov     x19, x0             // x19 = suspended SP for current frame
    mov     x20, x1             // x20 = fp
    mov     x21, x2             // x21 = from_start
    mov     x22, x3             // x22 = from_end

.Lguse_frame_loop:
    cbz     x20, .Lguse_done
    mov     x23, #FP_SENTINEL
    cmp     x20, x23
    b.eq    .Lguse_done

    // Update receiver, method, and context.
    sub     x0, x20, #32
    bl      .Lgu_update_slot
    sub     x0, x20, #8
    bl      .Lgu_update_slot
    sub     x0, x20, #24
    bl      .Lgu_update_slot

    // Get num_temps from the updated method.
    ldr     x23, [x20, #-8]
    ldr     x23, [x23, #40]
    asr     x23, x23, #2

    // Update temps.
    mov     x24, #0
.Lguse_temps:
    cmp     x24, x23
    b.ge    .Lguse_eval
    add     x25, x24, #5
    neg     x25, x25
    add     x0, x20, x25, lsl #3
    bl      .Lgu_update_slot
    add     x24, x24, #1
    b       .Lguse_temps

.Lguse_eval:
    // frame_bottom = FP - (4 + num_temps) * 8
    add     x24, x23, #4
    sub     x25, x20, x24, lsl #3
.Lguse_eval_loop:
    cmp     x19, x25
    b.hs    .Lguse_next_frame
    mov     x0, x19
    bl      .Lgu_update_slot
    add     x19, x19, #8
    b       .Lguse_eval_loop

.Lguse_next_frame:
    ldr     x24, [x20, #FP_FLAGS_OFS]
    ubfx    x24, x24, #FRAME_FLAGS_NUM_ARGS_SHIFT, #FRAME_FLAGS_NUM_ARGS_WIDTH
    add     x24, x24, #FP_ARG_BASE_WORDS
    add     x19, x20, x24, lsl #3   // caller's suspended SP
    ldr     x20, [x20]
    b       .Lguse_frame_loop

.Lguse_done:
    EPILOGUE
    ret

// gc_collect_stack_slots(sp, fp, slot_buf, max_slots) -> num_slots
// x0 = current Smalltalk SP
// x1 = current frame pointer
// x2 = slot_buf (array of uint64_t* slot addresses)
// x3 = max_slots
//
// Collects the addresses of all slots that currently contain object-pointer roots
// reachable from the
// current frame chain:
// - receiver, method, context, temps
// - suspended operand stack slots for each frame
//
// Returns the total number of matching slots, even if max_slots is smaller.
// The caller can first call with slot_buf=0,max_slots=0 to count roots,
// then allocate an exact-sized buffer, call again to fill it, materialize the
// values, run gc_collect, and write the updated root values back to these
// exact addresses.
_gc_collect_stack_slots:
    PROLOGUE
    mov     x19, x0             // x19 = suspended SP
    mov     x20, x1             // x20 = fp
    mov     x21, x2             // x21 = slot_buf
    mov     x22, x3             // x22 = max_slots
    mov     x23, #0             // x23 = count

.Lgcss_frame_loop:
    cbz     x20, .Lgcss_done
    mov     x24, #FP_SENTINEL
    cmp     x20, x24
    b.eq    .Lgcss_done

    sub     x0, x20, #32
    bl      .Lgcss_add_slot
    sub     x0, x20, #8
    bl      .Lgcss_add_slot
    sub     x0, x20, #24
    bl      .Lgcss_add_slot

    ldr     x24, [x20, #-8]
    ldr     x24, [x24, #40]
    asr     x24, x24, #2

    mov     x25, #0
.Lgcss_temps:
    cmp     x25, x24
    b.ge    .Lgcss_eval
    add     x26, x25, #5
    neg     x26, x26
    add     x0, x20, x26, lsl #3
    bl      .Lgcss_add_slot
    add     x25, x25, #1
    b       .Lgcss_temps

.Lgcss_eval:
    add     x25, x24, #4
    sub     x26, x20, x25, lsl #3
.Lgcss_eval_loop:
    cmp     x19, x26
    b.hs    .Lgcss_next_frame
    mov     x0, x19
    bl      .Lgcss_add_slot
    add     x19, x19, #8
    b       .Lgcss_eval_loop

.Lgcss_next_frame:
    ldr     x24, [x20, #FP_FLAGS_OFS]
    ubfx    x24, x24, #FRAME_FLAGS_NUM_ARGS_SHIFT, #FRAME_FLAGS_NUM_ARGS_WIDTH
    add     x24, x24, #FP_ARG_BASE_WORDS
    add     x19, x20, x24, lsl #3
    ldr     x20, [x20]
    b       .Lgcss_frame_loop

.Lgcss_done:
    mov     x0, x23
    EPILOGUE
    ret

.Lgcss_add_slot:
    ldr     x24, [x0]
    cbz     x24, .Lgcss_slot_done
    tst     x24, #3
    b.ne    .Lgcss_slot_done
    cmp     x23, x22
    b.ge    .Lgcss_count_only
    cbz     x21, .Lgcss_count_only
    str     x0, [x21, x23, lsl #3]
.Lgcss_count_only:
    add     x23, x23, #1
.Lgcss_slot_done:
    ret
