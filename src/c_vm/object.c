#include "vm_defs.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define MAX_REGISTERED_GC_CONTEXTS 128

static uint64_t *registered_gc_contexts[MAX_REGISTERED_GC_CONTEXTS];
static uint64_t registered_gc_context_count = 0;
static uint64_t *registered_simple_oms[MAX_REGISTERED_GC_CONTEXTS];
static uint64_t registered_simple_om_starts[MAX_REGISTERED_GC_CONTEXTS];
static uint64_t registered_simple_om_ends[MAX_REGISTERED_GC_CONTEXTS];
static uint64_t *registered_simple_om_page_used[MAX_REGISTERED_GC_CONTEXTS];
static uint64_t *registered_simple_om_page_first[MAX_REGISTERED_GC_CONTEXTS];
static uint64_t *registered_simple_om_page_covering[MAX_REGISTERED_GC_CONTEXTS];
static uint8_t *registered_simple_om_page_dirty[MAX_REGISTERED_GC_CONTEXTS];
static uint64_t registered_simple_om_page_counts[MAX_REGISTERED_GC_CONTEXTS];
static uint64_t registered_simple_om_count = 0;

static uint64_t object_slot_count(ObjPtr object)
{
    if (OBJ_FORMAT(object) == FORMAT_BYTES)
    {
        return (OBJ_SIZE(object) + WORD_BYTES - 1) / WORD_BYTES;
    }
    return OBJ_SIZE(object);
}

uint64_t om_object_words(ObjPtr object);
uint64_t om_object_bytes(ObjPtr object);

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

static void convert_slot_offset_to_pointer(uint64_t *slot_ptr, uint64_t heap_base, uint64_t heap_used)
{
    uint64_t value = *slot_ptr;

    if ((value & TAG_MASK) != TAG_OBJECT)
    {
        return;
    }
    if (value < 8 || value > heap_used)
    {
        return;
    }

    *slot_ptr = (value - 8) + heap_base;
}

static uint64_t om_registration_index(Om om)
{
    for (uint64_t index = 0; index < registered_simple_om_count; index++)
    {
        if (registered_simple_oms[index] == om)
        {
            return index;
        }
    }
    return UINT64_MAX;
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
    uint64_t page_count = (size_bytes + OM_PAGE_BYTES - 1) / OM_PAGE_BYTES;

    free_ptr_var[0] = (uint64_t)buffer;
    free_ptr_var[1] = (uint64_t)((uint8_t *)buffer + size_bytes);

    for (uint64_t index = 0; index < registered_simple_om_count; index++)
    {
        if (registered_simple_oms[index] == free_ptr_var)
        {
            registered_simple_om_starts[index] = (uint64_t)buffer;
            registered_simple_om_ends[index] = (uint64_t)((uint8_t *)buffer + size_bytes);
            registered_simple_om_page_counts[index] = page_count;
            free(registered_simple_om_page_used[index]);
            free(registered_simple_om_page_first[index]);
            free(registered_simple_om_page_covering[index]);
            free(registered_simple_om_page_dirty[index]);
            registered_simple_om_page_used[index] = (uint64_t *)calloc((size_t)page_count, sizeof(uint64_t));
            registered_simple_om_page_first[index] = (uint64_t *)calloc((size_t)page_count, sizeof(uint64_t));
            registered_simple_om_page_covering[index] = (uint64_t *)calloc((size_t)page_count, sizeof(uint64_t));
            registered_simple_om_page_dirty[index] = (uint8_t *)calloc((size_t)page_count, sizeof(uint8_t));
            return;
        }
    }

    if (registered_simple_om_count < MAX_REGISTERED_GC_CONTEXTS)
    {
        registered_simple_oms[registered_simple_om_count] = free_ptr_var;
        registered_simple_om_starts[registered_simple_om_count] = (uint64_t)buffer;
        registered_simple_om_ends[registered_simple_om_count] = (uint64_t)((uint8_t *)buffer + size_bytes);
        registered_simple_om_page_counts[registered_simple_om_count] = page_count;
        registered_simple_om_page_used[registered_simple_om_count] = (uint64_t *)calloc((size_t)page_count, sizeof(uint64_t));
        registered_simple_om_page_first[registered_simple_om_count] = (uint64_t *)calloc((size_t)page_count, sizeof(uint64_t));
        registered_simple_om_page_covering[registered_simple_om_count] = (uint64_t *)calloc((size_t)page_count, sizeof(uint64_t));
        registered_simple_om_page_dirty[registered_simple_om_count] = (uint8_t *)calloc((size_t)page_count, sizeof(uint8_t));
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

Om om_registered_for_address(uint64_t address)
{
    for (uint64_t index = 0; index < registered_simple_om_count; index++)
    {
        if (address >= registered_simple_om_starts[index] &&
            address < registered_simple_om_ends[index])
        {
            return registered_simple_oms[index];
        }
    }
    return NULL;
}

uint64_t om_page_bytes(void)
{
    return OM_PAGE_BYTES;
}

uint64_t om_page_count(Om om)
{
    uint64_t index = om_registration_index(om);
    return index == UINT64_MAX ? 0 : registered_simple_om_page_counts[index];
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
    uint64_t index = om_registration_index(om);
    if (index == UINT64_MAX || page_id >= registered_simple_om_page_counts[index])
    {
        return 0;
    }
    return registered_simple_om_page_used[index][page_id];
}

uint64_t om_page_is_dirty(Om om, uint64_t page_id)
{
    uint64_t index = om_registration_index(om);
    if (index == UINT64_MAX || page_id >= registered_simple_om_page_counts[index])
    {
        return 0;
    }
    return registered_simple_om_page_dirty[index][page_id] != 0;
}

uint64_t om_dirty_page_count(Om om)
{
    uint64_t index = om_registration_index(om);
    uint64_t count = 0;

    if (index == UINT64_MAX)
    {
        return 0;
    }

    for (uint64_t page_id = 0; page_id < registered_simple_om_page_counts[index]; page_id++)
    {
        if (registered_simple_om_page_dirty[index][page_id] != 0)
        {
            count++;
        }
    }
    return count;
}

void om_clear_dirty_pages(Om om)
{
    uint64_t index = om_registration_index(om);

    if (index == UINT64_MAX)
    {
        return;
    }

    memset(registered_simple_om_page_dirty[index], 0,
           (size_t)registered_simple_om_page_counts[index] * sizeof(uint8_t));
}

void om_mark_object_dirty(Om om, ObjPtr object)
{
    uint64_t index = om_registration_index(om);
    uint64_t object_start;
    uint64_t object_end;
    uint64_t start_page;
    uint64_t end_page;

    if (index == UINT64_MAX || object == NULL)
    {
        return;
    }

    object_start = (uint64_t)object;
    object_end = object_start + om_object_bytes(object);
    start_page = om_page_id_for_address(om, object_start);
    end_page = om_page_id_for_address(om, object_end - 1);
    if (start_page == UINT64_MAX || end_page == UINT64_MAX)
    {
        return;
    }

    for (uint64_t page_id = start_page; page_id <= end_page; page_id++)
    {
        registered_simple_om_page_dirty[index][page_id] = 1;
    }
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
    uint64_t next_page;
    uint64_t index = om_registration_index(om);

    if (object == NULL)
    {
        return NULL;
    }

    next = (uint64_t)object + om_object_bytes(object);
    if (next >= end)
    {
        return NULL;
    }

    next_page = om_page_id_for_address(om, next);
    if (next_page == UINT64_MAX)
    {
        return NULL;
    }

    if ((next - om_page_start(om, next_page)) < om_page_used_bytes(om, next_page))
    {
        return (ObjPtr)next;
    }

    if (index == UINT64_MAX)
    {
        return NULL;
    }

    for (uint64_t page_id = next_page + 1; page_id < registered_simple_om_page_counts[index]; page_id++)
    {
        if (registered_simple_om_page_first[index][page_id] != 0)
        {
            return (ObjPtr)registered_simple_om_page_first[index][page_id];
        }
    }
    return NULL;
}

ObjPtr om_page_covering_object(Om om, uint64_t page_id)
{
    uint64_t index = om_registration_index(om);
    if (index == UINT64_MAX || page_id >= registered_simple_om_page_counts[index])
    {
        return NULL;
    }
    return (ObjPtr)registered_simple_om_page_covering[index][page_id];
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
    uint64_t index = om_registration_index(om);
    if (index == UINT64_MAX || page_id >= registered_simple_om_page_counts[index])
    {
        return NULL;
    }
    return (ObjPtr)registered_simple_om_page_first[index][page_id];
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
        uint64_t page_end = om_page_start(source_om, page_id) + om_page_used_bytes(source_om, page_id);
        ObjPtr current = om_page_covering_object(source_om, page_id);

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

void image_offsets_to_live_pointers_paged(uint8_t *image_copy, uint64_t used_size,
                                          uint64_t heap_base,
                                          const uint64_t *page_used_table,
                                          const uint64_t *page_first_table,
                                          const uint64_t *page_covering_table,
                                          uint64_t page_count)
{
    if (image_copy == NULL || used_size == 0 || page_used_table == NULL ||
        page_first_table == NULL || page_covering_table == NULL)
    {
        return;
    }

    for (uint64_t page_id = 0; page_id < page_count; page_id++)
    {
        uint64_t page_start_offset = page_id * OM_PAGE_BYTES;
        uint64_t page_end_offset = page_start_offset + page_used_table[page_id];
        ObjPtr current = page_covering_table[page_id] == 0
                             ? NULL
                             : (ObjPtr)(image_copy + (page_covering_table[page_id] - 8));

        if (page_start_offset >= used_size)
        {
            break;
        }

        while (current != NULL &&
               ((uint64_t)current - (uint64_t)image_copy) < used_size &&
               ((uint64_t)current - (uint64_t)image_copy) < page_end_offset)
        {
            uint64_t slot_count = object_slot_count(current);

            convert_slot_offset_to_pointer(&OBJ_CLASS(current), heap_base, used_size);

            if (OBJ_FORMAT(current) != FORMAT_BYTES)
            {
                for (uint64_t slot = 0; slot < slot_count; slot++)
                {
                    convert_slot_offset_to_pointer(&OBJ_FIELD(current, slot), heap_base, used_size);
                }
            }

            {
                uint64_t next = (uint64_t)current + om_object_bytes(current);
                uint64_t next_page = next >= heap_base + used_size
                                         ? UINT64_MAX
                                         : (next - heap_base) / OM_PAGE_BYTES;

                if (next >= heap_base + used_size)
                {
                    current = NULL;
                }
                else if ((next - (heap_base + (next_page * OM_PAGE_BYTES))) < page_used_table[next_page])
                {
                    current = (ObjPtr)next;
                }
                else
                {
                    current = NULL;
                    for (uint64_t future_page = next_page + 1; future_page < page_count; future_page++)
                    {
                        if (page_first_table[future_page] != 0)
                        {
                            current = (ObjPtr)(image_copy + (page_first_table[future_page] - 8));
                            break;
                        }
                    }
                }
            }
            if (current != NULL &&
                ((uint64_t)current - (uint64_t)image_copy) >= used_size)
            {
                current = NULL;
            }
        }
    }
}

ObjPtr om_alloc(Om free_ptr_var, Oop class_ptr, uint64_t format, uint64_t size)
{
    ObjPtr object = (ObjPtr)free_ptr_var[0];
    ObjPtr end = (ObjPtr)free_ptr_var[1];
    uint64_t slot_words = format == FORMAT_BYTES ? (size + WORD_BYTES - 1) / WORD_BYTES : size;
    uint64_t total_words = OBJ_HEADER_WORDS + slot_words;
    uint64_t total_bytes = total_words * WORD_BYTES;
    uint64_t heap_start = om_registered_start(free_ptr_var);
    uint64_t relative_offset = heap_start == 0 ? 0 : ((uint64_t)object - heap_start);
    uint64_t page_offset = relative_offset % OM_PAGE_BYTES;
    uint64_t bytes_left_in_page = OM_PAGE_BYTES - page_offset;
    uint64_t next_page_start = heap_start == 0
                                   ? (((uint64_t)object) + OM_PAGE_BYTES - 1) & ~(uint64_t)(OM_PAGE_BYTES - 1)
                                   : heap_start + (((relative_offset + OM_PAGE_BYTES - 1) / OM_PAGE_BYTES) * OM_PAGE_BYTES);
    uint64_t metadata_index = om_registration_index(free_ptr_var);
    ObjPtr new_free;

    if (page_offset != 0 &&
        total_bytes <= OM_PAGE_BYTES &&
        total_bytes > bytes_left_in_page)
    {
        object = (ObjPtr)next_page_start;
    }

    new_free = object + total_words;

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

    if (metadata_index != UINT64_MAX)
    {
        uint64_t object_start = (uint64_t)object;
        uint64_t object_end = object_start + total_bytes;
        uint64_t start_page = om_page_id_for_address(free_ptr_var, object_start);
        uint64_t end_page = om_page_id_for_address(free_ptr_var, object_end - 1);

        for (uint64_t page_id = start_page; page_id <= end_page; page_id++)
        {
            uint64_t page_start = om_page_start(free_ptr_var, page_id);
            uint64_t page_end = page_start + OM_PAGE_BYTES;
            uint64_t used_end = object_end < page_end ? object_end : page_end;
            uint64_t used_bytes = used_end > page_start ? used_end - page_start : 0;

            if (registered_simple_om_page_covering[metadata_index][page_id] == 0)
            {
                registered_simple_om_page_covering[metadata_index][page_id] = object_start;
            }
            if (page_id == start_page &&
                registered_simple_om_page_first[metadata_index][page_id] == 0)
            {
                registered_simple_om_page_first[metadata_index][page_id] = object_start;
            }
            if (registered_simple_om_page_used[metadata_index][page_id] < used_bytes)
            {
                registered_simple_om_page_used[metadata_index][page_id] = used_bytes;
            }
        }
    }

    free_ptr_var[0] = (uint64_t)new_free;
    return object;
}
