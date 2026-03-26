// tagged.s — Tagged pointer encoding/decoding, type checks, SmallInteger arithmetic
// Tag bits (1:0): 00=object pointer, 01=SmallInteger, 10=float, 11=special

.global _tag_smallint
.global _untag_smallint
.global _get_tag
.global _is_smallint
.global _is_object_ptr
.global _is_immediate_float
.global _is_special
.global _tagged_nil
.global _tagged_true
.global _tagged_false
.global _is_nil
.global _is_boolean
.global _smallint_add
.global _smallint_sub
.global _smallint_less_than
.global _smallint_equal

.align 2

// tag_smallint(int64_t value) -> uint64_t
// Encode: (value << 2) | 0b01
_tag_smallint:
    lsl     x0, x0, #2
    orr     x0, x0, #1
    ret

// untag_smallint(uint64_t tagged) -> int64_t
// Decode: arithmetic right shift 2
_untag_smallint:
    asr     x0, x0, #2
    ret

// get_tag(uint64_t value) -> uint64_t
_get_tag:
    and     x0, x0, #3
    ret

// is_smallint(uint64_t value) -> uint64_t (1 or 0)
_is_smallint:
    and     x0, x0, #3
    cmp     x0, #1
    cset    x0, eq
    ret

// is_object_ptr(uint64_t value) -> uint64_t (1 or 0)
_is_object_ptr:
    and     x0, x0, #3
    cmp     x0, #0
    cset    x0, eq
    ret

// is_immediate_float(uint64_t value) -> uint64_t (1 or 0)
_is_immediate_float:
    and     x0, x0, #3
    cmp     x0, #2
    cset    x0, eq
    ret

// is_special(uint64_t value) -> uint64_t (1 or 0)
_is_special:
    and     x0, x0, #3
    cmp     x0, #3
    cset    x0, eq
    ret

// tagged_nil() -> uint64_t   = 0x03
_tagged_nil:
    mov     x0, #3
    ret

// tagged_true() -> uint64_t  = 0x07
_tagged_true:
    mov     x0, #7
    ret

// tagged_false() -> uint64_t = 0x0B
_tagged_false:
    mov     x0, #11
    ret

// is_nil(uint64_t value) -> uint64_t (1 or 0)
_is_nil:
    cmp     x0, #3
    cset    x0, eq
    ret

// is_boolean(uint64_t value) -> uint64_t (1 or 0)
// true=0x07, false=0x0B
_is_boolean:
    cmp     x0, #7
    cset    x1, eq
    cmp     x0, #11
    cset    x2, eq
    orr     x0, x1, x2
    ret

// smallint_add(uint64_t a, uint64_t b) -> uint64_t
// a + b - 1 (tag correction: 01+01=10, -1=01)
_smallint_add:
    add     x0, x0, x1
    sub     x0, x0, #1
    ret

// smallint_sub(uint64_t a, uint64_t b) -> uint64_t
// a - b + 1 (tag correction: 01-01=00, +1=01)
_smallint_sub:
    sub     x0, x0, x1
    add     x0, x0, #1
    ret

// smallint_less_than(uint64_t a, uint64_t b) -> uint64_t
// Returns tagged true (0x07) or tagged false (0x0B).
_smallint_less_than:
    cmp     x0, x1
    mov     x0, #7
    mov     x1, #11
    csel    x0, x0, x1, lt
    ret

// smallint_equal(uint64_t a, uint64_t b) -> uint64_t
_smallint_equal:
    cmp     x0, x1
    mov     x0, #7
    mov     x1, #11
    csel    x0, x0, x1, eq
    ret

