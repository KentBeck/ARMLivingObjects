#include "vm_defs.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

void om_init(void *buffer, uint64_t size_bytes, uint64_t *free_ptr_var)
{
    free_ptr_var[0] = (uint64_t)buffer;
    free_ptr_var[1] = (uint64_t)((uint8_t *)buffer + size_bytes);
}

uint64_t *om_alloc(uint64_t *free_ptr_var, uint64_t class_ptr, uint64_t format, uint64_t size)
{
    uint64_t *object = (uint64_t *)free_ptr_var[0];
    uint64_t *end = (uint64_t *)free_ptr_var[1];
    uint64_t slot_words = format == FORMAT_BYTES ? (size + WORD_BYTES - 1) / WORD_BYTES : size;
    uint64_t total_words = OBJ_HEADER_WORDS + slot_words;
    uint64_t *new_free = object + total_words;

    if (new_free > end)
    {
        return NULL;
    }

    OBJ_CLASS(object) = class_ptr;
    OBJ_FORMAT(object) = format;
    OBJ_SIZE(object) = size;
    if (slot_words != 0)
    {
        memset(&OBJ_FIELD(object, 0), 0, (size_t)(slot_words * WORD_BYTES));
    }

    free_ptr_var[0] = (uint64_t)new_free;
    return object;
}
