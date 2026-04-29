#include "vm_defs.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define MAX_REGISTERED_GC_CONTEXTS 128

static uint64_t *registered_gc_contexts[MAX_REGISTERED_GC_CONTEXTS];
static uint64_t registered_gc_context_count = 0;
static uint64_t *registered_simple_oms[MAX_REGISTERED_GC_CONTEXTS];
static uint64_t registered_simple_om_starts[MAX_REGISTERED_GC_CONTEXTS];
static uint64_t registered_simple_om_ends[MAX_REGISTERED_GC_CONTEXTS];
static uint64_t registered_simple_om_count = 0;

void gc_register_context(Om gc_ctx)
{
    for (uint64_t index = 0; index < registered_gc_context_count; index++)
    {
        if (registered_gc_contexts[index] == gc_ctx)
        {
            return;
        }
    }

    if (registered_gc_context_count < MAX_REGISTERED_GC_CONTEXTS)
    {
        registered_gc_contexts[registered_gc_context_count++] = gc_ctx;
    }
}

Oop gc_is_registered_context(Om om)
{
    for (uint64_t index = 0; index < registered_gc_context_count; index++)
    {
        if (registered_gc_contexts[index] == om)
        {
            return 1;
        }
    }
    return 0;
}

void om_init(void *buffer, uint64_t size_bytes, Om free_ptr_var)
{
    free_ptr_var[0] = (uint64_t)buffer;
    free_ptr_var[1] = (uint64_t)((uint8_t *)buffer + size_bytes);

    for (uint64_t index = 0; index < registered_simple_om_count; index++)
    {
        if (registered_simple_oms[index] == free_ptr_var)
        {
            registered_simple_om_starts[index] = (uint64_t)buffer;
            registered_simple_om_ends[index] = (uint64_t)((uint8_t *)buffer + size_bytes);
            return;
        }
    }

    if (registered_simple_om_count < MAX_REGISTERED_GC_CONTEXTS)
    {
        registered_simple_oms[registered_simple_om_count] = free_ptr_var;
        registered_simple_om_starts[registered_simple_om_count] = (uint64_t)buffer;
        registered_simple_om_ends[registered_simple_om_count] = (uint64_t)((uint8_t *)buffer + size_bytes);
        registered_simple_om_count++;
    }
}

uint64_t om_registered_start(Om om)
{
    for (uint64_t index = 0; index < registered_simple_om_count; index++)
    {
        if (registered_simple_oms[index] == om)
        {
            return registered_simple_om_starts[index];
        }
    }
    return 0;
}

uint64_t om_registered_end(Om om)
{
    for (uint64_t index = 0; index < registered_simple_om_count; index++)
    {
        if (registered_simple_oms[index] == om)
        {
            return registered_simple_om_ends[index];
        }
    }
    return 0;
}

ObjPtr om_alloc(Om free_ptr_var, Oop class_ptr, uint64_t format, uint64_t size)
{
    ObjPtr object = (ObjPtr)free_ptr_var[0];
    ObjPtr end = (ObjPtr)free_ptr_var[1];
    uint64_t slot_words = format == FORMAT_BYTES ? (size + WORD_BYTES - 1) / WORD_BYTES : size;
    uint64_t total_words = OBJ_HEADER_WORDS + slot_words;
    ObjPtr new_free = object + total_words;

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
