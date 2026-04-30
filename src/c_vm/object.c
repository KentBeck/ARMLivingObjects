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

static uint64_t object_slot_count(ObjPtr object)
{
    if (OBJ_FORMAT(object) == FORMAT_BYTES)
    {
        return (OBJ_SIZE(object) + WORD_BYTES - 1) / WORD_BYTES;
    }
    return OBJ_SIZE(object);
}

static void convert_slot_pointer_to_offset(uint64_t *slot_ptr, uint64_t heap_start, uint64_t heap_limit)
{
    uint64_t value = *slot_ptr;

    if ((value & TAG_MASK) != TAG_OBJECT || value == 0)
    {
        return;
    }
    if (value < heap_start || value >= heap_limit)
    {
        return;
    }

    *slot_ptr = (value - heap_start) + 8;
}

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

uint64_t om_page_bytes(void)
{
    return OM_PAGE_BYTES;
}

uint64_t om_page_count(Om om)
{
    uint64_t start = om_registered_start(om);
    uint64_t end = om_registered_end(om);
    uint64_t size;

    if (start == 0 || end <= start)
    {
        return 0;
    }

    size = end - start;
    return (size + OM_PAGE_BYTES - 1) / OM_PAGE_BYTES;
}

uint64_t om_page_id_for_address(Om om, uint64_t address)
{
    uint64_t start = om_registered_start(om);
    uint64_t end = om_registered_end(om);

    if (start == 0 || address < start || address >= end)
    {
        return UINT64_MAX;
    }

    return (address - start) / OM_PAGE_BYTES;
}

uint64_t om_page_start(Om om, uint64_t page_id)
{
    uint64_t start = om_registered_start(om);
    uint64_t count = om_page_count(om);

    if (start == 0 || page_id >= count)
    {
        return 0;
    }

    return start + (page_id * OM_PAGE_BYTES);
}

uint64_t om_page_used_bytes(Om om, uint64_t page_id)
{
    uint64_t page_start = om_page_start(om, page_id);
    uint64_t page_end;
    uint64_t free_ptr;

    if (page_start == 0)
    {
        return 0;
    }

    page_end = page_start + OM_PAGE_BYTES;
    free_ptr = om[0];

    if (free_ptr <= page_start)
    {
        return 0;
    }
    if (free_ptr >= page_end)
    {
        return OM_PAGE_BYTES;
    }
    return free_ptr - page_start;
}

uint64_t om_object_spans_pages(Om om, ObjPtr object)
{
    uint64_t size_bytes;
    uint64_t start_page;
    uint64_t end_page;

    if (object == NULL)
    {
        return 0;
    }

    size_bytes = (OBJ_HEADER_WORDS +
                  (OBJ_FORMAT(object) == FORMAT_BYTES
                       ? (OBJ_SIZE(object) + WORD_BYTES - 1) / WORD_BYTES
                       : OBJ_SIZE(object))) *
                 WORD_BYTES;

    start_page = om_page_id_for_address(om, (uint64_t)object);
    end_page = om_page_id_for_address(om, ((uint64_t)object + size_bytes) - 1);
    if (start_page == UINT64_MAX || end_page == UINT64_MAX)
    {
        return 0;
    }
    return start_page != end_page;
}

uint64_t om_object_words(ObjPtr object)
{
    if (object == NULL)
    {
        return 0;
    }
    if (OBJ_FORMAT(object) == FORMAT_BYTES)
    {
        return OBJ_HEADER_WORDS + ((OBJ_SIZE(object) + WORD_BYTES - 1) / WORD_BYTES);
    }
    return OBJ_HEADER_WORDS + OBJ_SIZE(object);
}

uint64_t om_object_bytes(ObjPtr object)
{
    return om_object_words(object) * WORD_BYTES;
}

ObjPtr om_next_object(Om om, ObjPtr object)
{
    uint64_t end = om[0];
    uint64_t next;

    if (object == NULL)
    {
        return NULL;
    }

    next = (uint64_t)object + om_object_bytes(object);
    if (next >= end)
    {
        return NULL;
    }
    return (ObjPtr)next;
}

ObjPtr om_page_covering_object(Om om, uint64_t page_id)
{
    uint64_t page_start = om_page_start(om, page_id);
    uint64_t heap_start = om_registered_start(om);
    uint64_t heap_end = om[0];
    ObjPtr current;

    if (page_start == 0 || heap_start == 0 || heap_start >= heap_end)
    {
        return NULL;
    }

    current = (ObjPtr)heap_start;
    while (current != NULL && (uint64_t)current < heap_end)
    {
        uint64_t object_start = (uint64_t)current;
        uint64_t object_end = object_start + om_object_bytes(current);
        if (page_start >= object_start && page_start < object_end)
        {
            return current;
        }
        current = om_next_object(om, current);
    }
    return NULL;
}

uint64_t om_page_starts_with_continuation(Om om, uint64_t page_id)
{
    ObjPtr covering = om_page_covering_object(om, page_id);
    uint64_t page_start = om_page_start(om, page_id);

    if (covering == NULL || page_start == 0)
    {
        return 0;
    }

    return (uint64_t)covering < page_start;
}

ObjPtr om_page_first_object_start(Om om, uint64_t page_id)
{
    uint64_t page_start = om_page_start(om, page_id);
    uint64_t page_end = page_start == 0 ? 0 : page_start + om_page_used_bytes(om, page_id);
    uint64_t heap_start = om_registered_start(om);
    ObjPtr current;

    if (page_start == 0 || heap_start == 0 || page_end <= page_start)
    {
        return NULL;
    }

    current = (ObjPtr)heap_start;
    while (current != NULL && (uint64_t)current < page_end)
    {
        if ((uint64_t)current >= page_start)
        {
            return current;
        }
        current = om_next_object(om, current);
    }
    return NULL;
}

void image_live_pointers_to_offsets_paged(Om source_om, uint8_t *image_copy, uint64_t used_size)
{
    uint64_t heap_start = om_registered_start(source_om);
    uint64_t heap_limit = heap_start + used_size;
    uint64_t page_count = om_page_count(source_om);

    if (heap_start == 0 || image_copy == NULL || used_size == 0)
    {
        return;
    }

    for (uint64_t page_id = 0; page_id < page_count; page_id++)
    {
        uint64_t page_start = om_page_start(source_om, page_id);
        uint64_t page_end = page_start + om_page_used_bytes(source_om, page_id);
        ObjPtr current = om_page_first_object_start(source_om, page_id);

        while (current != NULL &&
               (uint64_t)current < heap_limit &&
               (uint64_t)current < page_end)
        {
            uint64_t offset = (uint64_t)current - heap_start;
            ObjPtr image_object = (ObjPtr)(image_copy + offset);
            uint64_t slot_count = object_slot_count(current);

            convert_slot_pointer_to_offset(&OBJ_CLASS(image_object), heap_start, heap_limit);

            if (OBJ_FORMAT(current) != FORMAT_BYTES)
            {
                for (uint64_t slot = 0; slot < slot_count; slot++)
                {
                    convert_slot_pointer_to_offset(&OBJ_FIELD(image_object, slot), heap_start, heap_limit);
                }
            }

            current = om_next_object(source_om, current);
        }
    }
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
