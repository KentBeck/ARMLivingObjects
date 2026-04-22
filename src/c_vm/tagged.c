#include "vm_defs.h"

#include <stdint.h>

uint64_t tag_smallint(int64_t value)
{
    return ((uint64_t)value << SMALLINT_SHIFT) | TAG_SMALLINT;
}

int64_t untag_smallint(uint64_t tagged)
{
    return ((int64_t)tagged) >> SMALLINT_SHIFT;
}

uint64_t get_tag(uint64_t value)
{
    return value & TAG_MASK;
}

uint64_t is_smallint(uint64_t value)
{
    return (value & TAG_MASK) == TAG_SMALLINT;
}

uint64_t is_object_ptr(uint64_t value)
{
    return (value & TAG_MASK) == TAG_OBJECT;
}

uint64_t is_immediate_float(uint64_t value)
{
    return (value & TAG_MASK) == TAG_FLOAT;
}

uint64_t is_special(uint64_t value)
{
    return (value & TAG_MASK) == TAG_SPECIAL;
}

uint64_t tagged_nil(void)
{
    return TAGGED_NIL;
}

uint64_t tagged_true(void)
{
    return TAGGED_TRUE;
}

uint64_t tagged_false(void)
{
    return TAGGED_FALSE;
}

uint64_t is_nil(uint64_t value)
{
    return value == TAGGED_NIL;
}

uint64_t is_boolean(uint64_t value)
{
    return value == TAGGED_TRUE || value == TAGGED_FALSE;
}

uint64_t tag_character(uint64_t code_point)
{
    return (code_point << CHAR_SHIFT) | CHAR_TAG_VALUE;
}

uint64_t untag_character(uint64_t tagged)
{
    return tagged >> CHAR_SHIFT;
}

uint64_t is_character(uint64_t value)
{
    return (value & CHAR_TAG_MASK) == CHAR_TAG_VALUE;
}

uint64_t smallint_add(uint64_t a, uint64_t b)
{
    return a + b - TAG_SMALLINT;
}

uint64_t smallint_sub(uint64_t a, uint64_t b)
{
    return a - b + TAG_SMALLINT;
}

uint64_t smallint_less_than(uint64_t a, uint64_t b)
{
    return ((int64_t)a < (int64_t)b) ? TAGGED_TRUE : TAGGED_FALSE;
}

uint64_t smallint_equal(uint64_t a, uint64_t b)
{
    return a == b ? TAGGED_TRUE : TAGGED_FALSE;
}
