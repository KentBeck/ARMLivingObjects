// Transaction log operations
// Log layout: [0] = count, then triples starting at [1]
//   triple i: [1 + i*3] = object_ptr, [2 + i*3] = field_index, [3 + i*3] = new_value
// field_index encoding:
//   bit 63 = 0 -> word field index (obj[3 + index])
//   bit 63 = 1 -> byte index (obj bytes at OBJ_FIELDS_OFS + index), value is tagged SmallInt

.include "asm_constants_shared.s"

.global _txn_log_write
.global _txn_log_read
.global _txn_commit
.global _txn_commit_durable
.global _txn_abort

.align 2

// txn_log_write(log, obj, field_index, value)
// x0 = log, x1 = obj, x2 = field_index, x3 = value
// Append a (obj, field_index, value) triple to the log.
_txn_log_write:
    ldr     x4, [x0]               // x4 = count
    add     x5, x4, x4, lsl #1     // x5 = count * 3
    add     x5, x5, #1             // x5 = 1 + count*3 (offset to new triple)
    str     x1, [x0, x5, lsl #3]   // log[1 + count*3] = obj
    add     x5, x5, #1
    str     x2, [x0, x5, lsl #3]   // log[2 + count*3] = field_index
    add     x5, x5, #1
    str     x3, [x0, x5, lsl #3]   // log[3 + count*3] = value
    add     x4, x4, #1             // count++
    str     x4, [x0]               // log[0] = count
    ret

// txn_log_read(log, obj, field_index, found_ptr) -> value
// x0 = log, x1 = obj, x2 = field_index, x3 = found_ptr
// Scan log backwards for matching (obj, field_index).
// If found: *found_ptr = 1, return value.
// If not found: *found_ptr = 0, return 0.
_txn_log_read:
    ldr     x4, [x0]               // x4 = count
    cbz     x4, .Lread_not_found   // empty log
    // Start from last entry (index count-1), scan backwards
    sub     x5, x4, #1             // x5 = i = count - 1
.Lread_loop:
    add     x6, x5, x5, lsl #1     // x6 = i * 3
    add     x6, x6, #1             // x6 = 1 + i*3
    ldr     x7, [x0, x6, lsl #3]   // x7 = log[1 + i*3] = stored obj
    add     x6, x6, #1
    ldr     x8, [x0, x6, lsl #3]   // x8 = log[2 + i*3] = stored field_index
    cmp     x7, x1                 // obj match?
    b.ne    .Lread_next
    cmp     x8, x2                 // field_index match?
    b.ne    .Lread_next
    // Found it
    add     x6, x6, #1
    ldr     x0, [x0, x6, lsl #3]   // return value = log[3 + i*3]
    mov     x9, #1
    str     x9, [x3]               // *found_ptr = 1
    ret
.Lread_next:
    cbz     x5, .Lread_not_found   // i == 0, done
    sub     x5, x5, #1             // i--
    b       .Lread_loop
.Lread_not_found:
    mov     x0, #0                 // return 0
    str     x0, [x3]               // *found_ptr = 0
    ret

// txn_commit(log)
// x0 = log
// Apply each (obj, field_index, value) triple to the actual object,
// then clear the log.
_txn_commit:
    ldr     x1, [x0]               // x1 = count
    cbz     x1, .Lcommit_done      // empty log
    mov     x2, #0                 // x2 = i = 0
.Lcommit_loop:
    add     x3, x2, x2, lsl #1     // x3 = i * 3
    add     x3, x3, #1             // x3 = 1 + i*3
    ldr     x4, [x0, x3, lsl #3]   // x4 = obj
    add     x3, x3, #1
    ldr     x5, [x0, x3, lsl #3]   // x5 = field_index
    add     x3, x3, #1
    ldr     x6, [x0, x3, lsl #3]   // x6 = value
    // If field_index has bit63 set, this is a byte write entry.
    tbnz    x5, #63, .Lcommit_byte_write

    // Word slot write: OBJ_FIELD(obj, field_index) = value
    add     x5, x5, #3             // x5 = 3 + field_index
    str     x6, [x4, x5, lsl #3]   // obj[3 + field_index] = value
    b       .Lcommit_next

.Lcommit_byte_write:
    // Clear marker bit: index = field_index & ((1<<63)-1)
    lsl     x9, x5, #1
    lsr     x9, x9, #1
    asr     x10, x6, #SMALLINT_SHIFT       // untag SmallInt value -> byte
    add     x11, x4, #OBJ_FIELDS_OFS
    strb    w10, [x11, x9]

.Lcommit_next:
    add     x2, x2, #1             // i++
    cmp     x2, x1                 // i < count?
    b.lt    .Lcommit_loop
.Lcommit_done:
    str     xzr, [x0]              // log[0] = 0 (clear)
    ret

// txn_commit_durable(log)
// x0 = log
// Durable transactions currently share the same write-install behavior as
// normal commit, but use a distinct entry point so the VM can grow a separate
// durability path without changing the Transaction API again.
_txn_commit_durable:
    b       _txn_commit


// txn_abort(log)
// x0 = log
// Discard all log entries (set count to 0).
_txn_abort:
    str     xzr, [x0]              // log[0] = 0
    ret
