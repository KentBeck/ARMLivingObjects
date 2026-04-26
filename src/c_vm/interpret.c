#include "vm_defs.h"

#include <limits.h>
#include <signal.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

extern Oop oop_class(Oop oop, ObjPtr class_table);
extern Oop class_lookup(ObjPtr klass, Oop selector);
extern void activate_method(Oop **sp_ptr, ObjPtr *fp_ptr, uint64_t saved_ip,
                            Oop method, uint64_t num_args, uint64_t num_temps);
extern uint64_t tag_smallint(int64_t value);
extern int64_t untag_smallint(uint64_t tagged);
extern Oop prim_string_as_symbol(Oop receiver);
extern Oop prim_class_superclass(Oop receiver);
extern Oop prim_class_name(Oop receiver);
extern Oop prim_class_includes_selector(Oop receiver, Oop selector);
extern Oop prim_smalltalk_globals(void);
extern Oop prim_method_source_for_class_selector(Oop class_name, Oop selector, Om om);
extern Oop prim_read_fd_count(Oop fd, Oop count, Om om);
extern Oop prim_write_fd_string(Oop fd, Oop string);
extern ObjPtr om_alloc(Om free_ptr_var, Oop class_ptr, uint64_t format, uint64_t size);
extern Oop gc_is_registered_context(Om om);
extern void gc_collect(Oop *roots, uint64_t num_roots,
                       Om from_space, Om to_space,
                       uint64_t from_start, uint64_t from_end);
extern uint64_t gc_collect_stack_slots(Oop *sp, ObjPtr fp,
                                       Oop **slot_buf, uint64_t max_slots);
extern ObjPtr ensure_frame_context_global(ObjPtr fp, Om om);
extern ObjPtr global_context_class;
extern Oop cannot_return_selector_oop(void);
extern void txn_log_write(Oop *log, Oop obj, uint64_t field_index, Oop value);
extern Oop txn_log_read(Oop *log, Oop obj, uint64_t field_index, uint64_t *found);

enum
{
    GC_FROM_FREE = 0,
    GC_FROM_END = 1,
    GC_TO_FREE = 2,
    GC_TO_END = 3,
    GC_FROM_START = 4,
    GC_TO_START = 5,
    GC_SPACE_SIZE = 6,
    GC_TENURED_START = 7,
    GC_TENURED_END = 8,
    GC_REMEMBERED = 9
};

static uint32_t read_u32(uint8_t **ip)
{
    uint8_t *p = *ip;
    uint32_t value = (uint32_t)p[0] |
                     ((uint32_t)p[1] << 8) |
                     ((uint32_t)p[2] << 16) |
                     ((uint32_t)p[3] << 24);
    *ip = p + 4;
    return value;
}

static void push(Oop **sp_ptr, Oop value)
{
    Oop *sp = *sp_ptr - 1;
    *sp = value;
    *sp_ptr = sp;
}

static Oop pop(Oop **sp_ptr)
{
    Oop *sp = *sp_ptr;
    Oop value = *sp;
    *sp_ptr = sp + 1;
    return value;
}

static uint64_t frame_num_args_local(ObjPtr fp)
{
    return (fp[FRAME_FLAGS] >> FRAME_FLAGS_NUM_ARGS_SHIFT) & 0xFF;
}

static Oop frame_arg_local(ObjPtr fp, uint64_t index)
{
    uint64_t num_args = frame_num_args_local(fp);
    return fp[FP_ARG_BASE_WORDS + (num_args - 1 - index)];
}

static int frame_has_block_closure(ObjPtr fp)
{
    return (fp[FRAME_FLAGS] & FRAME_FLAGS_BLOCK_CLOSURE_MASK) != 0;
}

static int frame_has_context_local(ObjPtr fp)
{
    return (fp[FRAME_FLAGS] & FRAME_FLAGS_HAS_CONTEXT_MASK) != 0;
}

static void unsupported_bytecode(uint8_t opcode)
{
    (void)opcode;
    raise(SIGTRAP);
}

typedef enum
{
    PRIMITIVE_SUCCEEDED,
    PRIMITIVE_FAILED,
    PRIMITIVE_UNSUPPORTED
} PrimitiveResult;

static int is_smallint_value(Oop value)
{
    return (value & TAG_MASK) == TAG_SMALLINT;
}

static int is_character_value(Oop value)
{
    return (value & CHAR_TAG_MASK) == CHAR_TAG_VALUE;
}

static int is_object_value(Oop value)
{
    return (value & TAG_MASK) == TAG_OBJECT;
}

static int is_bytes_object(Oop value)
{
    return is_object_value(value) && OBJ_FORMAT((ObjPtr)value) == FORMAT_BYTES;
}

static int fits_smallint(int64_t value)
{
    return value >= (INT64_MIN >> SMALLINT_SHIFT) &&
           value <= (INT64_MAX >> SMALLINT_SHIFT);
}

static void replace_receiver_and_arg(Oop **sp_ptr, Oop result)
{
    Oop *sp = *sp_ptr + 1;
    *sp = result;
    *sp_ptr = sp;
}

static void replace_receiver_and_two_args(Oop **sp_ptr, Oop result)
{
    Oop *sp = *sp_ptr + 2;
    *sp = result;
    *sp_ptr = sp;
}

static void replace_receiver(Oop **sp_ptr, Oop result)
{
    **sp_ptr = result;
}

static uint64_t byte_txn_index(uint64_t index)
{
    return (UINT64_C(1) << 63) | index;
}

static void record_write_barrier(Om om, Oop receiver, uint64_t field_index,
                                 Oop value)
{
    uint64_t tenured_start = om[GC_TENURED_START];
    if (tenured_start == 0 || receiver < tenured_start || receiver >= om[GC_TENURED_END])
    {
        return;
    }

    if (!is_object_value(value) || value == 0 ||
        value < om[GC_FROM_START] || value >= om[GC_FROM_END])
    {
        return;
    }

    Oop *remembered = (Oop *)om[GC_REMEMBERED];
    if (remembered != NULL)
    {
        txn_log_write(remembered, receiver, field_index, value);
    }
}

static Oop lookup_method_for_receiver(Oop receiver, Oop selector,
                                      ObjPtr class_table)
{
    Oop klass = oop_class(receiver, class_table);
    if (klass == 0)
    {
        return 0;
    }
    return class_lookup((ObjPtr)klass, selector);
}

static void initialize_word_fields(ObjPtr object, uint64_t size)
{
    for (uint64_t index = 0; index < size; index++)
    {
        OBJ_FIELD(object, index) = TAGGED_NIL;
    }
}

static int has_gc_context_layout(Om om)
{
    if (gc_is_registered_context(om) == 0)
    {
        return 0;
    }

    uint64_t from_free = om[GC_FROM_FREE];
    uint64_t from_end = om[GC_FROM_END];
    uint64_t to_free = om[GC_TO_FREE];
    uint64_t to_end = om[GC_TO_END];
    uint64_t from_start = om[GC_FROM_START];
    uint64_t to_start = om[GC_TO_START];
    uint64_t space_size = om[GC_SPACE_SIZE];

    return space_size != 0 &&
           from_start < from_end &&
           to_start < to_end &&
           from_end == from_start + space_size &&
           to_end == to_start + space_size &&
           from_free >= from_start &&
           from_free <= from_end &&
           to_free >= to_start &&
           to_free <= to_end;
}

static int collect_and_retry_allocation(Oop **sp_ptr, ObjPtr *fp_ptr,
                                        uint8_t **ip_ptr, uint8_t **bytecode_base_ptr,
                                        ObjPtr *class_table_ptr, Om om)
{
    if (!has_gc_context_layout(om))
    {
        return 0;
    }

    uint64_t slot_count = gc_collect_stack_slots(*sp_ptr, *fp_ptr, NULL, 0);
    Oop **slots = NULL;
    Oop *roots = NULL;

    if (slot_count != 0)
    {
        slots = (Oop **)malloc((size_t)(slot_count * sizeof(Oop *)));
        if (slots == NULL)
        {
            return 0;
        }
    }

    roots = (Oop *)malloc((size_t)((slot_count + 1) * sizeof(Oop)));
    if (roots == NULL)
    {
        free(slots);
        return 0;
    }

    (void)gc_collect_stack_slots(*sp_ptr, *fp_ptr, slots, slot_count);
    for (uint64_t index = 0; index < slot_count; index++)
    {
        roots[index] = *slots[index];
    }
    roots[slot_count] = (uint64_t)*class_table_ptr;

    gc_collect(roots, slot_count + 1, om, om + GC_TO_FREE,
               om[GC_FROM_START], om[GC_FROM_END]);

    for (uint64_t index = 0; index < slot_count; index++)
    {
        *slots[index] = roots[index];
    }
    *class_table_ptr = (ObjPtr)roots[slot_count];

    uint64_t from_free = om[GC_FROM_FREE];
    uint64_t from_end = om[GC_FROM_END];
    uint64_t from_start = om[GC_FROM_START];
    om[GC_FROM_FREE] = om[GC_TO_FREE];
    om[GC_FROM_END] = om[GC_TO_END];
    om[GC_TO_FREE] = from_free;
    om[GC_TO_END] = from_end;
    om[GC_FROM_START] = om[GC_TO_START];
    om[GC_TO_START] = from_start;
    om[GC_TO_FREE] = om[GC_TO_START];
    om[GC_TO_END] = om[GC_TO_START] + om[GC_SPACE_SIZE];

    uint64_t ip_offset = (uint64_t)(*ip_ptr - *bytecode_base_ptr);
    ObjPtr current_method = (ObjPtr)(*fp_ptr)[FRAME_METHOD];
    ObjPtr bytecodes = (ObjPtr)OBJ_FIELD(current_method, CM_BYTECODES);
    *bytecode_base_ptr = (uint8_t *)&OBJ_FIELD(bytecodes, 0);
    *ip_ptr = *bytecode_base_ptr + ip_offset;

    free(roots);
    free(slots);
    return 1;
}

static ObjPtr ensure_frame_context_with_retry(Oop **sp_ptr, ObjPtr *fp_ptr,
                                              uint8_t **ip_ptr, uint8_t **bytecode_base_ptr,
                                              ObjPtr *class_table_ptr, Om om)
{
    ObjPtr context = ensure_frame_context_global(*fp_ptr, om);
    if (context != NULL)
    {
        return context;
    }

    if (!collect_and_retry_allocation(sp_ptr, fp_ptr, ip_ptr, bytecode_base_ptr, class_table_ptr, om))
    {
        return NULL;
    }

    return ensure_frame_context_global(*fp_ptr, om);
}

static uint8_t cannot_return_after_bytecodes[] = {BC_RETURN_STACK_TOP};

static PrimitiveResult try_allocation_primitive(Oop **sp_ptr, Oop primitive,
                                                uint64_t arg_count, Om om,
                                                ObjPtr *fp_ptr, uint8_t **ip_ptr,
                                                uint8_t **bytecode_base_ptr,
                                                ObjPtr *class_table_ptr)
{
    Oop *sp = *sp_ptr;
    Oop receiver = sp[arg_count];

    switch (primitive)
    {
    case PRIM_BASIC_NEW:
    {
        if (arg_count != 0)
        {
            return PRIMITIVE_FAILED;
        }
        if (!is_object_value(receiver))
        {
            raise(SIGTRAP);
            return PRIMITIVE_UNSUPPORTED;
        }
        Oop size_oop = OBJ_FIELD((ObjPtr)receiver, CLASS_INST_SIZE);
        if (!is_smallint_value(size_oop))
        {
            return PRIMITIVE_FAILED;
        }
        int64_t size = untag_smallint(size_oop);
        if (size < 0)
        {
            return PRIMITIVE_FAILED;
        }
        ObjPtr object = om_alloc(om, receiver, FORMAT_FIELDS, (uint64_t)size);
        if (object == NULL)
        {
            if (!collect_and_retry_allocation(sp_ptr, fp_ptr, ip_ptr, bytecode_base_ptr, class_table_ptr, om))
            {
                raise(SIGTRAP);
                return PRIMITIVE_UNSUPPORTED;
            }
            sp = *sp_ptr;
            receiver = sp[arg_count];
            size = untag_smallint(OBJ_FIELD((ObjPtr)receiver, CLASS_INST_SIZE));
            object = om_alloc(om, receiver, FORMAT_FIELDS, (uint64_t)size);
            if (object == NULL)
            {
                raise(SIGTRAP);
                return PRIMITIVE_UNSUPPORTED;
            }
        }
        initialize_word_fields(object, (uint64_t)size);
        replace_receiver(sp_ptr, (Oop)object);
        return PRIMITIVE_SUCCEEDED;
    }

    case PRIM_BASIC_NEW_SIZE:
    {
        if (arg_count != 1)
        {
            return PRIMITIVE_FAILED;
        }
        if (!is_object_value(receiver))
        {
            raise(SIGTRAP);
            return PRIMITIVE_UNSUPPORTED;
        }
        Oop size_oop = sp[0];
        if (!is_smallint_value(size_oop))
        {
            return PRIMITIVE_FAILED;
        }
        int64_t size = untag_smallint(size_oop);
        if (size < 0)
        {
            return PRIMITIVE_FAILED;
        }
        Oop format_oop = OBJ_FIELD((ObjPtr)receiver, CLASS_INST_FORMAT);
        if (!is_smallint_value(format_oop))
        {
            return PRIMITIVE_FAILED;
        }
        int64_t format = untag_smallint(format_oop);
        if (format == FORMAT_FIELDS || (format != FORMAT_INDEXABLE && format != FORMAT_BYTES))
        {
            return PRIMITIVE_FAILED;
        }
        ObjPtr object = om_alloc(om, receiver, (uint64_t)format, (uint64_t)size);
        if (object == NULL)
        {
            if (!collect_and_retry_allocation(sp_ptr, fp_ptr, ip_ptr, bytecode_base_ptr, class_table_ptr, om))
            {
                raise(SIGTRAP);
                return PRIMITIVE_UNSUPPORTED;
            }
            sp = *sp_ptr;
            receiver = sp[arg_count];
            size = untag_smallint(sp[0]);
            format = untag_smallint(OBJ_FIELD((ObjPtr)receiver, CLASS_INST_FORMAT));
            object = om_alloc(om, receiver, (uint64_t)format, (uint64_t)size);
            if (object == NULL)
            {
                raise(SIGTRAP);
                return PRIMITIVE_UNSUPPORTED;
            }
        }
        if (format == FORMAT_INDEXABLE)
        {
            initialize_word_fields(object, (uint64_t)size);
        }
        replace_receiver_and_arg(sp_ptr, (Oop)object);
        return PRIMITIVE_SUCCEEDED;
    }

    default:
        return PRIMITIVE_UNSUPPORTED;
    }
}

static uint64_t block_copied_count(Oop block)
{
    return OBJ_SIZE((ObjPtr)block) - BLOCK_COPIED_BASE;
}

static Oop block_closure_for_frame(ObjPtr fp)
{
    if (frame_has_context_local(fp))
    {
        return OBJ_FIELD((ObjPtr)fp[FRAME_CONTEXT], CONTEXT_CLOSURE);
    }
    if (frame_has_block_closure(fp))
    {
        return fp[FRAME_CONTEXT];
    }
    return TAGGED_NIL;
}

static Oop block_home_context_for_frame(ObjPtr fp)
{
    if (frame_has_context_local(fp))
    {
        return OBJ_FIELD((ObjPtr)fp[FRAME_CONTEXT], CONTEXT_HOME);
    }
    if (frame_has_block_closure(fp))
    {
        return OBJ_FIELD((ObjPtr)fp[FRAME_CONTEXT], BLOCK_HOME_CONTEXT);
    }
    return TAGGED_NIL;
}

static void mark_block_frame(ObjPtr fp, Oop block)
{
    fp[FRAME_FLAGS] |= (UINT64_C(1) << FRAME_FLAGS_IS_BLOCK_SHIFT) |
                       FRAME_FLAGS_BLOCK_CLOSURE_MASK;
    fp[FRAME_CONTEXT] = block;
}

static void populate_block_copied_values(ObjPtr fp, Oop block, uint64_t copied_count)
{
    ObjPtr block_object = (ObjPtr)block;
    for (uint64_t index = 0; index < copied_count; index++)
    {
        fp[-(int64_t)(FP_TEMP_BASE_WORDS + index)] = OBJ_FIELD(block_object, BLOCK_COPIED_BASE + index);
    }
}

static PrimitiveResult try_block_primitive(Oop **sp_ptr, ObjPtr *fp_ptr,
                                          uint8_t **ip_ptr, uint8_t **bytecode_base_ptr,
                                          Oop primitive, uint64_t arg_count,
                                          ObjPtr class_table)
{
    switch (primitive)
    {
    case PRIM_BLOCK_VALUE:
    case PRIM_BLOCK_VALUE_ARG:
        break;
    default:
        return PRIMITIVE_UNSUPPORTED;
    }

    if ((primitive == PRIM_BLOCK_VALUE && arg_count != 0) ||
        (primitive == PRIM_BLOCK_VALUE_ARG && arg_count != 1))
    {
        return PRIMITIVE_FAILED;
    }

    Oop *sp = *sp_ptr;
    Oop block = sp[arg_count];
    if (!is_object_value(block) ||
        OBJ_CLASS((ObjPtr)block) != OBJ_FIELD(class_table, CLASS_TABLE_BLOCK))
    {
        raise(SIGTRAP);
        return PRIMITIVE_UNSUPPORTED;
    }

    ObjPtr block_object = (ObjPtr)block;
    Oop home_receiver = OBJ_FIELD(block_object, BLOCK_HOME_RECEIVER);
    Oop block_method = OBJ_FIELD(block_object, BLOCK_CM);
    uint64_t copied_count = block_copied_count(block);
    uint64_t method_temps = (uint64_t)untag_smallint(OBJ_FIELD((ObjPtr)block_method, CM_NUM_TEMPS));

    if (primitive == PRIM_BLOCK_VALUE)
    {
        (void)pop(sp_ptr);
        push(sp_ptr, home_receiver);
    }
    else
    {
        sp[arg_count] = home_receiver;
    }

    activate_method(sp_ptr, fp_ptr, (uint64_t)*ip_ptr, block_method, arg_count,
                    method_temps + copied_count);
    populate_block_copied_values(*fp_ptr, block, copied_count);
    mark_block_frame(*fp_ptr, block);

    ObjPtr new_method = (ObjPtr)(*fp_ptr)[FRAME_METHOD];
    ObjPtr bytecodes = (ObjPtr)OBJ_FIELD(new_method, CM_BYTECODES);
    *bytecode_base_ptr = (uint8_t *)&OBJ_FIELD(bytecodes, 0);
    *ip_ptr = *bytecode_base_ptr;
    return PRIMITIVE_SUCCEEDED;
}

static PrimitiveResult try_smallint_primitive(Oop **sp_ptr, Oop primitive,
                                             uint64_t arg_count)
{
    switch (primitive)
    {
    case PRIM_SMALLINT_ADD:
    case PRIM_SMALLINT_SUB:
    case PRIM_SMALLINT_LT:
    case PRIM_SMALLINT_EQ:
    case PRIM_SMALLINT_MUL:
        break;
    default:
        return PRIMITIVE_UNSUPPORTED;
    }

    if (arg_count != 1)
    {
        return PRIMITIVE_FAILED;
    }

    Oop *sp = *sp_ptr;
    Oop arg = sp[0];
    Oop receiver = sp[1];

    if (!is_smallint_value(receiver))
    {
        raise(SIGTRAP);
        return PRIMITIVE_UNSUPPORTED;
    }

    if (!is_smallint_value(arg))
    {
        return PRIMITIVE_FAILED;
    }

    int64_t lhs = untag_smallint(receiver);
    int64_t rhs = untag_smallint(arg);
    int64_t value = 0;

    switch (primitive)
    {
    case PRIM_SMALLINT_ADD:
        if (__builtin_add_overflow(lhs, rhs, &value) || !fits_smallint(value))
        {
            return PRIMITIVE_FAILED;
        }
        replace_receiver_and_arg(sp_ptr, tag_smallint(value));
        return PRIMITIVE_SUCCEEDED;

    case PRIM_SMALLINT_SUB:
        if (__builtin_sub_overflow(lhs, rhs, &value) || !fits_smallint(value))
        {
            return PRIMITIVE_FAILED;
        }
        replace_receiver_and_arg(sp_ptr, tag_smallint(value));
        return PRIMITIVE_SUCCEEDED;

    case PRIM_SMALLINT_MUL:
        if (__builtin_mul_overflow(lhs, rhs, &value) || !fits_smallint(value))
        {
            return PRIMITIVE_FAILED;
        }
        replace_receiver_and_arg(sp_ptr, tag_smallint(value));
        return PRIMITIVE_SUCCEEDED;

    case PRIM_SMALLINT_LT:
        replace_receiver_and_arg(sp_ptr, lhs < rhs ? TAGGED_TRUE : TAGGED_FALSE);
        return PRIMITIVE_SUCCEEDED;

    case PRIM_SMALLINT_EQ:
        replace_receiver_and_arg(sp_ptr, lhs == rhs ? TAGGED_TRUE : TAGGED_FALSE);
        return PRIMITIVE_SUCCEEDED;

    default:
        return PRIMITIVE_UNSUPPORTED;
    }
}

static PrimitiveResult try_object_primitive(Oop **sp_ptr, Oop primitive,
                                           uint64_t arg_count, ObjPtr class_table)
{
    Oop *sp = *sp_ptr;
    Oop receiver = sp[arg_count];

    switch (primitive)
    {
    case PRIM_IDENTITY_EQ:
        if (arg_count != 1)
        {
            return PRIMITIVE_FAILED;
        }
        replace_receiver_and_arg(sp_ptr, receiver == sp[0] ? TAGGED_TRUE : TAGGED_FALSE);
        return PRIMITIVE_SUCCEEDED;

    case PRIM_BASIC_CLASS:
        if (arg_count != 0)
        {
            return PRIMITIVE_FAILED;
        }
        replace_receiver(sp_ptr, oop_class(receiver, class_table));
        return PRIMITIVE_SUCCEEDED;

    case PRIM_HASH:
        if (arg_count != 0)
        {
            return PRIMITIVE_FAILED;
        }
        if (is_smallint_value(receiver))
        {
            replace_receiver(sp_ptr, receiver);
        }
        else
        {
            replace_receiver(sp_ptr, tag_smallint((int64_t)((receiver >> 3) & 0x3FFFFFFF)));
        }
        return PRIMITIVE_SUCCEEDED;

    default:
        return PRIMITIVE_UNSUPPORTED;
    }
}

static PrimitiveResult try_live_image_primitive(Oop **sp_ptr, Oop primitive,
                                               uint64_t arg_count, Om om,
                                               ObjPtr *fp_ptr, uint8_t **ip_ptr,
                                               uint8_t **bytecode_base_ptr,
                                               ObjPtr *class_table_ptr)
{
    Oop *sp = *sp_ptr;
    Oop receiver = sp[arg_count];

    switch (primitive)
    {
    case PRIM_CLASS_SUPERCLASS:
        if (arg_count != 0)
        {
            return PRIMITIVE_FAILED;
        }
        if (!is_object_value(receiver))
        {
            raise(SIGTRAP);
            return PRIMITIVE_UNSUPPORTED;
        }
        replace_receiver(sp_ptr, prim_class_superclass(receiver));
        return PRIMITIVE_SUCCEEDED;

    case PRIM_CLASS_NAME:
        if (arg_count != 0)
        {
            return PRIMITIVE_FAILED;
        }
        if (!is_object_value(receiver))
        {
            raise(SIGTRAP);
            return PRIMITIVE_UNSUPPORTED;
        }
        replace_receiver(sp_ptr, prim_class_name(receiver));
        return PRIMITIVE_SUCCEEDED;

    case PRIM_CLASS_INCLUDES_SELECTOR:
        if (arg_count != 1)
        {
            return PRIMITIVE_FAILED;
        }
        if (!is_object_value(receiver))
        {
            raise(SIGTRAP);
            return PRIMITIVE_UNSUPPORTED;
        }
        replace_receiver_and_arg(sp_ptr, prim_class_includes_selector(receiver, sp[0]));
        return PRIMITIVE_SUCCEEDED;

    case PRIM_SMALLTALK_GLOBALS:
        if (arg_count != 0)
        {
            return PRIMITIVE_FAILED;
        }
        replace_receiver(sp_ptr, prim_smalltalk_globals());
        return PRIMITIVE_SUCCEEDED;

    case PRIM_METHOD_SOURCE_FOR_CLASS_SELECTOR:
        if (arg_count != 2)
        {
            return PRIMITIVE_FAILED;
        }
        {
            Oop result = prim_method_source_for_class_selector(sp[1], sp[0], om);
            if (result == TAGGED_NIL)
            {
                replace_receiver_and_two_args(sp_ptr, TAGGED_NIL);
                return PRIMITIVE_SUCCEEDED;
            }
            if (!is_object_value(result))
            {
                if (!collect_and_retry_allocation(sp_ptr, fp_ptr, ip_ptr, bytecode_base_ptr, class_table_ptr, om))
                {
                    raise(SIGTRAP);
                    return PRIMITIVE_UNSUPPORTED;
                }
                sp = *sp_ptr;
                result = prim_method_source_for_class_selector(sp[1], sp[0], om);
                if (!is_object_value(result) && result != TAGGED_NIL)
                {
                    raise(SIGTRAP);
                    return PRIMITIVE_UNSUPPORTED;
                }
            }
            replace_receiver_and_two_args(sp_ptr, result);
            return PRIMITIVE_SUCCEEDED;
        }

    case PRIM_READ_FD_COUNT:
        if (arg_count != 2)
        {
            return PRIMITIVE_FAILED;
        }
        {
            Oop result = prim_read_fd_count(sp[1], sp[0], om);
            if (result == TAGGED_NIL)
            {
                replace_receiver_and_two_args(sp_ptr, TAGGED_NIL);
                return PRIMITIVE_SUCCEEDED;
            }
            if (!is_object_value(result))
            {
                if (!collect_and_retry_allocation(sp_ptr, fp_ptr, ip_ptr, bytecode_base_ptr, class_table_ptr, om))
                {
                    raise(SIGTRAP);
                    return PRIMITIVE_UNSUPPORTED;
                }
                sp = *sp_ptr;
                result = prim_read_fd_count(sp[1], sp[0], om);
                if (!is_object_value(result) && result != TAGGED_NIL)
                {
                    raise(SIGTRAP);
                    return PRIMITIVE_UNSUPPORTED;
                }
            }
            replace_receiver_and_two_args(sp_ptr, result);
            return PRIMITIVE_SUCCEEDED;
        }

    case PRIM_WRITE_FD_STRING:
        if (arg_count != 2)
        {
            return PRIMITIVE_FAILED;
        }
        replace_receiver_and_two_args(sp_ptr, prim_write_fd_string(sp[1], sp[0]));
        return PRIMITIVE_SUCCEEDED;

    default:
        return PRIMITIVE_UNSUPPORTED;
    }
}

static PrimitiveResult try_character_primitive(Oop **sp_ptr, Oop primitive,
                                              uint64_t arg_count)
{
    switch (primitive)
    {
    case PRIM_CHAR_VALUE:
    case PRIM_AS_CHARACTER:
    case PRIM_CHAR_IS_LETTER:
    case PRIM_CHAR_IS_DIGIT:
    case PRIM_CHAR_UPPERCASE:
    case PRIM_CHAR_LOWERCASE:
    case PRIM_PRINT_CHAR:
        break;
    default:
        return PRIMITIVE_UNSUPPORTED;
    }

    if (arg_count != 0)
    {
        return PRIMITIVE_FAILED;
    }

    Oop receiver = **sp_ptr;
    uint64_t code_point;

    switch (primitive)
    {
    case PRIM_CHAR_VALUE:
        if (!is_character_value(receiver))
        {
            raise(SIGTRAP);
            return PRIMITIVE_UNSUPPORTED;
        }
        replace_receiver(sp_ptr, tag_smallint((int64_t)(receiver >> CHAR_SHIFT)));
        return PRIMITIVE_SUCCEEDED;

    case PRIM_AS_CHARACTER:
        if (!is_smallint_value(receiver))
        {
            raise(SIGTRAP);
            return PRIMITIVE_UNSUPPORTED;
        }
        replace_receiver(sp_ptr, ((uint64_t)untag_smallint(receiver) << CHAR_SHIFT) | CHAR_TAG_VALUE);
        return PRIMITIVE_SUCCEEDED;

    case PRIM_CHAR_IS_LETTER:
        if (!is_character_value(receiver))
        {
            raise(SIGTRAP);
            return PRIMITIVE_UNSUPPORTED;
        }
        code_point = receiver >> CHAR_SHIFT;
        replace_receiver(sp_ptr, ((code_point >= 'A' && code_point <= 'Z') ||
                                  (code_point >= 'a' && code_point <= 'z'))
                                     ? TAGGED_TRUE
                                     : TAGGED_FALSE);
        return PRIMITIVE_SUCCEEDED;

    case PRIM_CHAR_IS_DIGIT:
        if (!is_character_value(receiver))
        {
            raise(SIGTRAP);
            return PRIMITIVE_UNSUPPORTED;
        }
        code_point = receiver >> CHAR_SHIFT;
        replace_receiver(sp_ptr, (code_point >= '0' && code_point <= '9') ? TAGGED_TRUE : TAGGED_FALSE);
        return PRIMITIVE_SUCCEEDED;

    case PRIM_CHAR_UPPERCASE:
        if (!is_character_value(receiver))
        {
            raise(SIGTRAP);
            return PRIMITIVE_UNSUPPORTED;
        }
        code_point = receiver >> CHAR_SHIFT;
        if (code_point >= 'a' && code_point <= 'z')
        {
            receiver = ((code_point - ('a' - 'A')) << CHAR_SHIFT) | CHAR_TAG_VALUE;
        }
        replace_receiver(sp_ptr, receiver);
        return PRIMITIVE_SUCCEEDED;

    case PRIM_CHAR_LOWERCASE:
        if (!is_character_value(receiver))
        {
            raise(SIGTRAP);
            return PRIMITIVE_UNSUPPORTED;
        }
        code_point = receiver >> CHAR_SHIFT;
        if (code_point >= 'A' && code_point <= 'Z')
        {
            receiver = ((code_point + ('a' - 'A')) << CHAR_SHIFT) | CHAR_TAG_VALUE;
        }
        replace_receiver(sp_ptr, receiver);
        return PRIMITIVE_SUCCEEDED;

    case PRIM_PRINT_CHAR:
        if (!is_character_value(receiver))
        {
            raise(SIGTRAP);
            return PRIMITIVE_UNSUPPORTED;
        }
        code_point = receiver >> CHAR_SHIFT;
        {
            uint8_t byte = (uint8_t)code_point;
            (void)write(STDOUT_FILENO, &byte, 1);
        }
        replace_receiver(sp_ptr, receiver);
        return PRIMITIVE_SUCCEEDED;

    default:
        return PRIMITIVE_UNSUPPORTED;
    }
}

static PrimitiveResult try_string_symbol_primitive(Oop **sp_ptr, Oop primitive,
                                                  uint64_t arg_count)
{
    Oop *sp = *sp_ptr;
    Oop receiver = sp[arg_count];

    switch (primitive)
    {
    case PRIM_STRING_EQ:
    {
        if (arg_count != 1)
        {
            return PRIMITIVE_FAILED;
        }
        if (!is_bytes_object(receiver))
        {
            raise(SIGTRAP);
            return PRIMITIVE_UNSUPPORTED;
        }
        Oop arg = sp[0];
        if (!is_bytes_object(arg))
        {
            return PRIMITIVE_FAILED;
        }

        ObjPtr receiver_object = (ObjPtr)receiver;
        ObjPtr arg_object = (ObjPtr)arg;
        Oop result = TAGGED_FALSE;
        if (OBJ_SIZE(receiver_object) == OBJ_SIZE(arg_object) &&
            memcmp(&OBJ_FIELD(receiver_object, 0), &OBJ_FIELD(arg_object, 0),
                   (size_t)OBJ_SIZE(receiver_object)) == 0)
        {
            result = TAGGED_TRUE;
        }

        replace_receiver_and_arg(sp_ptr, result);
        return PRIMITIVE_SUCCEEDED;
    }

    case PRIM_STRING_HASH_FNV:
    {
        if (arg_count != 0)
        {
            return PRIMITIVE_FAILED;
        }
        if (!is_bytes_object(receiver))
        {
            raise(SIGTRAP);
            return PRIMITIVE_UNSUPPORTED;
        }

        ObjPtr receiver_object = (ObjPtr)receiver;
        uint8_t *bytes = (uint8_t *)&OBJ_FIELD(receiver_object, 0);
        uint32_t hash = 0x811C9DC5u;
        for (uint64_t index = 0; index < OBJ_SIZE(receiver_object); index++)
        {
            hash ^= bytes[index];
            hash *= 0x01000193u;
        }

        replace_receiver(sp_ptr, tag_smallint((int64_t)hash));
        return PRIMITIVE_SUCCEEDED;
    }

    case PRIM_STRING_AS_SYMBOL:
        if (arg_count != 0)
        {
            return PRIMITIVE_FAILED;
        }
        if (!is_bytes_object(receiver))
        {
            raise(SIGTRAP);
            return PRIMITIVE_UNSUPPORTED;
        }
        replace_receiver(sp_ptr, prim_string_as_symbol(receiver));
        return PRIMITIVE_SUCCEEDED;

    case PRIM_SYMBOL_EQ:
        if (arg_count != 1)
        {
            return PRIMITIVE_FAILED;
        }
        if (!is_bytes_object(receiver))
        {
            raise(SIGTRAP);
            return PRIMITIVE_UNSUPPORTED;
        }
        if (!is_bytes_object(sp[0]))
        {
            return PRIMITIVE_FAILED;
        }
        replace_receiver_and_arg(sp_ptr, receiver == sp[0] ? TAGGED_TRUE : TAGGED_FALSE);
        return PRIMITIVE_SUCCEEDED;

    default:
        return PRIMITIVE_UNSUPPORTED;
    }
}

static PrimitiveResult try_indexed_primitive(Oop **sp_ptr, Oop primitive,
                                            uint64_t arg_count, Oop *txn_log)
{
    Oop *sp = *sp_ptr;
    Oop receiver = sp[arg_count];

    switch (primitive)
    {
    case PRIM_SIZE:
        if (arg_count != 0)
        {
            return PRIMITIVE_FAILED;
        }
        if (!is_object_value(receiver))
        {
            raise(SIGTRAP);
            return PRIMITIVE_UNSUPPORTED;
        }
        replace_receiver(sp_ptr, tag_smallint((int64_t)OBJ_SIZE((ObjPtr)receiver)));
        return PRIMITIVE_SUCCEEDED;

    case PRIM_AT:
    {
        if (arg_count != 1)
        {
            return PRIMITIVE_FAILED;
        }
        if (!is_object_value(receiver))
        {
            raise(SIGTRAP);
            return PRIMITIVE_UNSUPPORTED;
        }
        Oop index_oop = sp[0];
        if (!is_smallint_value(index_oop))
        {
            return PRIMITIVE_FAILED;
        }
        int64_t one_based_index = untag_smallint(index_oop);
        if (one_based_index < 1)
        {
            return PRIMITIVE_FAILED;
        }
        uint64_t index = (uint64_t)(one_based_index - 1);
        ObjPtr receiver_object = (ObjPtr)receiver;
        uint64_t format = OBJ_FORMAT(receiver_object);
        if (format == FORMAT_FIELDS || index >= OBJ_SIZE(receiver_object))
        {
            return PRIMITIVE_FAILED;
        }

        Oop value = 0;
        if (format == FORMAT_BYTES)
        {
            if (txn_log != NULL)
            {
                uint64_t found = 0;
                value = txn_log_read(txn_log, receiver, byte_txn_index(index), &found);
                if (found != 0)
                {
                    replace_receiver_and_arg(sp_ptr, value);
                    return PRIMITIVE_SUCCEEDED;
                }
            }
            value = tag_smallint((int64_t)((uint8_t *)&OBJ_FIELD(receiver_object, 0))[index]);
        }
        else if (format == FORMAT_INDEXABLE)
        {
            if (txn_log != NULL)
            {
                uint64_t found = 0;
                value = txn_log_read(txn_log, receiver, index, &found);
                if (found != 0)
                {
                    replace_receiver_and_arg(sp_ptr, value);
                    return PRIMITIVE_SUCCEEDED;
                }
            }
            value = OBJ_FIELD(receiver_object, index);
        }
        else
        {
            return PRIMITIVE_FAILED;
        }

        replace_receiver_and_arg(sp_ptr, value);
        return PRIMITIVE_SUCCEEDED;
    }

    case PRIM_AT_PUT:
    {
        if (arg_count != 2)
        {
            return PRIMITIVE_FAILED;
        }
        if (!is_object_value(receiver))
        {
            raise(SIGTRAP);
            return PRIMITIVE_UNSUPPORTED;
        }
        Oop index_oop = sp[1];
        Oop value = sp[0];
        if (!is_smallint_value(index_oop))
        {
            return PRIMITIVE_FAILED;
        }
        int64_t one_based_index = untag_smallint(index_oop);
        if (one_based_index < 1)
        {
            return PRIMITIVE_FAILED;
        }
        uint64_t index = (uint64_t)(one_based_index - 1);
        ObjPtr receiver_object = (ObjPtr)receiver;
        uint64_t format = OBJ_FORMAT(receiver_object);
        if (format == FORMAT_FIELDS || index >= OBJ_SIZE(receiver_object))
        {
            return PRIMITIVE_FAILED;
        }

        if (format == FORMAT_BYTES)
        {
            if (txn_log != NULL)
            {
                txn_log_write(txn_log, receiver, byte_txn_index(index), value);
            }
            else
            {
                if (!is_smallint_value(value))
                {
                    return PRIMITIVE_FAILED;
                }
                int64_t byte_value = untag_smallint(value);
                if (byte_value < 0 || byte_value > 255)
                {
                    return PRIMITIVE_FAILED;
                }
                ((uint8_t *)&OBJ_FIELD(receiver_object, 0))[index] = (uint8_t)byte_value;
            }
        }
        else if (format == FORMAT_INDEXABLE)
        {
            if (txn_log != NULL)
            {
                txn_log_write(txn_log, receiver, index, value);
            }
            else
            {
                OBJ_FIELD(receiver_object, index) = value;
            }
        }
        else
        {
            return PRIMITIVE_FAILED;
        }

        replace_receiver_and_two_args(sp_ptr, receiver);
        return PRIMITIVE_SUCCEEDED;
    }

    default:
        return PRIMITIVE_UNSUPPORTED;
    }
}

Oop interpret(Oop **sp_ptr, Oop **fp_ptr, uint8_t *ip,
              ObjPtr class_table, Om om, Oop *txn_log)
{
    uint8_t *bytecode_base = ip;
    ObjPtr entry_fp = (ObjPtr)*fp_ptr;

    for (;;)
    {
        uint8_t opcode = *ip++;

        switch (opcode)
        {
        case BC_PUSH_LITERAL:
        {
            uint32_t literal_index = read_u32(&ip);
            ObjPtr method = (ObjPtr)(*fp_ptr)[FRAME_METHOD];
            ObjPtr literals = (ObjPtr)OBJ_FIELD(method, CM_LITERALS);
            push(sp_ptr, OBJ_FIELD(literals, literal_index));
            break;
        }

        case BC_PUSH_INST_VAR:
        {
            uint32_t field_index = read_u32(&ip);
            ObjPtr receiver = (ObjPtr)(*fp_ptr)[FRAME_RECEIVER];
            if (txn_log != NULL)
            {
                uint64_t found = 0;
                Oop value = txn_log_read(txn_log, (Oop)receiver, field_index, &found);
                if (found != 0)
                {
                    push(sp_ptr, value);
                    break;
                }
            }
            push(sp_ptr, OBJ_FIELD(receiver, field_index));
            break;
        }

        case BC_PUSH_TEMP:
        {
            uint32_t temp_index = read_u32(&ip);
            push(sp_ptr, (*fp_ptr)[-(int64_t)(FP_TEMP_BASE_WORDS + temp_index)]);
            break;
        }

        case BC_PUSH_SELF:
            push(sp_ptr, (*fp_ptr)[FRAME_RECEIVER]);
            break;

        case BC_STORE_INST_VAR:
        {
            uint32_t field_index = read_u32(&ip);
            Oop value = pop(sp_ptr);
            ObjPtr receiver = (ObjPtr)(*fp_ptr)[FRAME_RECEIVER];
            if (txn_log != NULL)
            {
                txn_log_write(txn_log, (Oop)receiver, field_index, value);
            }
            else
            {
                OBJ_FIELD(receiver, field_index) = value;
                record_write_barrier(om, (Oop)receiver, field_index, value);
            }
            break;
        }

        case BC_STORE_TEMP:
        {
            uint32_t temp_index = read_u32(&ip);
            (*fp_ptr)[-(int64_t)(FP_TEMP_BASE_WORDS + temp_index)] = pop(sp_ptr);
            break;
        }

        case BC_SEND_MESSAGE:
        {
            uint32_t selector_index = read_u32(&ip);
            uint32_t arg_count = read_u32(&ip);
            ObjPtr current_method = (ObjPtr)(*fp_ptr)[FRAME_METHOD];
            ObjPtr literals = (ObjPtr)OBJ_FIELD(current_method, CM_LITERALS);
            Oop selector = OBJ_FIELD(literals, selector_index);
            Oop receiver = (*sp_ptr)[arg_count];
            Oop method = lookup_method_for_receiver(receiver, selector, class_table);

            if (method == 0)
            {
                unsupported_bytecode(opcode);
                break;
            }

            int primitive_completed = 0;
            for (;;)
            {
                Oop primitive = OBJ_FIELD((ObjPtr)method, CM_PRIMITIVE);
                if (primitive == tag_smallint(PRIM_NONE))
                {
                    break;
                }

                uint64_t primitive_id = (uint64_t)untag_smallint(primitive);
                if (primitive_id == PRIM_PERFORM)
                {
                    if (arg_count != 1)
                    {
                        break;
                    }

                    selector = (*sp_ptr)[0];
                    receiver = (*sp_ptr)[1];
                    *sp_ptr = *sp_ptr + 1;
                    arg_count = 0;
                    method = lookup_method_for_receiver(receiver, selector, class_table);
                    if (method == 0)
                    {
                        unsupported_bytecode(opcode);
                        primitive_completed = 1;
                        break;
                    }
                    continue;
                }

                if (primitive_id == PRIM_HALT)
                {
                    unsupported_bytecode(opcode);
                    primitive_completed = 1;
                    break;
                }

                PrimitiveResult result = try_allocation_primitive(sp_ptr, primitive_id, arg_count, om,
                                                                  fp_ptr, &ip, &bytecode_base,
                                                                  &class_table);
                if (result == PRIMITIVE_UNSUPPORTED)
                {
                    result = try_block_primitive(sp_ptr, fp_ptr, &ip, &bytecode_base,
                                                 primitive_id, arg_count, class_table);
                }
                if (result == PRIMITIVE_UNSUPPORTED)
                {
                    result = try_smallint_primitive(sp_ptr, primitive_id, arg_count);
                }
                if (result == PRIMITIVE_UNSUPPORTED)
                {
                    result = try_object_primitive(sp_ptr, primitive_id, arg_count, class_table);
                }
                if (result == PRIMITIVE_UNSUPPORTED)
                {
                    result = try_live_image_primitive(sp_ptr, primitive_id, arg_count, om,
                                                      fp_ptr, &ip, &bytecode_base,
                                                      &class_table);
                }
                if (result == PRIMITIVE_UNSUPPORTED)
                {
                    result = try_character_primitive(sp_ptr, primitive_id, arg_count);
                }
                if (result == PRIMITIVE_UNSUPPORTED)
                {
                    result = try_string_symbol_primitive(sp_ptr, primitive_id, arg_count);
                }
                if (result == PRIMITIVE_UNSUPPORTED)
                {
                    result = try_indexed_primitive(sp_ptr, primitive_id, arg_count, txn_log);
                }
                if (result == PRIMITIVE_SUCCEEDED)
                {
                    primitive_completed = 1;
                    break;
                }
                if (result == PRIMITIVE_UNSUPPORTED)
                {
                    unsupported_bytecode(opcode);
                    primitive_completed = 1;
                    break;
                }

                break;
            }

            if (primitive_completed)
            {
                break;
            }

            uint64_t num_temps = (uint64_t)untag_smallint(OBJ_FIELD((ObjPtr)method, CM_NUM_TEMPS));
            activate_method(sp_ptr, fp_ptr, (uint64_t)ip, method, arg_count, num_temps);

            ObjPtr new_method = (ObjPtr)(*fp_ptr)[FRAME_METHOD];
            ObjPtr bytecodes = (ObjPtr)OBJ_FIELD(new_method, CM_BYTECODES);
            bytecode_base = (uint8_t *)&OBJ_FIELD(bytecodes, 0);
            ip = bytecode_base;
            break;
        }

        case BC_RETURN_STACK_TOP:
        {
            Oop return_value = **sp_ptr;
            ObjPtr fp = (ObjPtr)*fp_ptr;
            uint64_t num_args = frame_num_args_local(fp);
            Oop *new_sp = fp + FP_ARG_BASE_WORDS + num_args;
            ObjPtr caller_fp = (ObjPtr)fp[FRAME_SAVED_FP];
            uint8_t *caller_ip = (uint8_t *)fp[FRAME_SAVED_IP];

            *new_sp = return_value;
            *sp_ptr = new_sp;
            *fp_ptr = caller_fp;

            if (fp == entry_fp)
            {
                return return_value;
            }

            ip = caller_ip;
            if (caller_fp != 0 && caller_fp != (ObjPtr)0xCAFE)
            {
                ObjPtr caller_method = (ObjPtr)caller_fp[FRAME_METHOD];
                ObjPtr bytecodes = (ObjPtr)OBJ_FIELD(caller_method, CM_BYTECODES);
                bytecode_base = (uint8_t *)&OBJ_FIELD(bytecodes, 0);
            }
            break;
        }

        case BC_JUMP:
        {
            uint32_t offset = read_u32(&ip);
            ip = bytecode_base + offset;
            break;
        }

        case BC_JUMP_IF_TRUE:
        {
            uint32_t offset = read_u32(&ip);
            Oop condition = pop(sp_ptr);
            if (condition == TAGGED_TRUE)
            {
                ip = bytecode_base + offset;
            }
            break;
        }

        case BC_JUMP_IF_FALSE:
        {
            uint32_t offset = read_u32(&ip);
            Oop condition = pop(sp_ptr);
            if (condition == TAGGED_FALSE)
            {
                ip = bytecode_base + offset;
            }
            break;
        }

        case BC_POP:
            (void)pop(sp_ptr);
            break;

        case BC_DUPLICATE:
            push(sp_ptr, **sp_ptr);
            break;

        case BC_HALT:
            return **sp_ptr;

        case BC_PUSH_CLOSURE:
        {
            uint32_t literal_index = read_u32(&ip);
            ObjPtr fp = (ObjPtr)*fp_ptr;
            ObjPtr current_method = (ObjPtr)fp[FRAME_METHOD];
            uint64_t num_args = frame_num_args_local(fp);
            uint64_t num_temps = (uint64_t)untag_smallint(OBJ_FIELD(current_method, CM_NUM_TEMPS));
            Oop current_closure = block_closure_for_frame(fp);
            if (current_closure != TAGGED_NIL)
            {
                num_temps += block_copied_count(current_closure);
            }
            uint64_t copied_count = num_args + num_temps;

            ObjPtr literals = (ObjPtr)OBJ_FIELD(current_method, CM_LITERALS);
            Oop block_method = OBJ_FIELD(literals, literal_index);
            Oop block_class = OBJ_FIELD(class_table, CLASS_TABLE_BLOCK);
            if (ensure_frame_context_global(fp, om) == NULL)
            {
                if (global_context_class != NULL &&
                    !collect_and_retry_allocation(sp_ptr, fp_ptr, &ip, &bytecode_base, &class_table, om))
                {
                    unsupported_bytecode(opcode);
                    break;
                }

                fp = *fp_ptr;
                current_method = (ObjPtr)fp[FRAME_METHOD];
                num_args = frame_num_args_local(fp);
                num_temps = (uint64_t)untag_smallint(OBJ_FIELD(current_method, CM_NUM_TEMPS));
                current_closure = block_closure_for_frame(fp);
                if (current_closure != TAGGED_NIL)
                {
                    num_temps += block_copied_count(current_closure);
                }
                copied_count = num_args + num_temps;

                if (global_context_class != NULL && ensure_frame_context_global(fp, om) == NULL)
                {
                    unsupported_bytecode(opcode);
                    break;
                }

                literals = (ObjPtr)OBJ_FIELD(current_method, CM_LITERALS);
                block_method = OBJ_FIELD(literals, literal_index);
                block_class = OBJ_FIELD(class_table, CLASS_TABLE_BLOCK);
            }
            ObjPtr home_context = frame_has_context_local(fp) ? (ObjPtr)fp[FRAME_CONTEXT] : NULL;
            ObjPtr block = om_alloc(om, block_class, FORMAT_FIELDS, BLOCK_COPIED_BASE + copied_count);
            if (block == NULL)
            {
                if (!collect_and_retry_allocation(sp_ptr, fp_ptr, &ip, &bytecode_base, &class_table, om))
                {
                    unsupported_bytecode(opcode);
                    break;
                }

                fp = *fp_ptr;
                current_method = (ObjPtr)fp[FRAME_METHOD];
                num_args = frame_num_args_local(fp);
                num_temps = (uint64_t)untag_smallint(OBJ_FIELD(current_method, CM_NUM_TEMPS));
                current_closure = block_closure_for_frame(fp);
                if (current_closure != TAGGED_NIL)
                {
                    num_temps += block_copied_count(current_closure);
                }
                copied_count = num_args + num_temps;
                literals = (ObjPtr)OBJ_FIELD(current_method, CM_LITERALS);
                block_method = OBJ_FIELD(literals, literal_index);
                block_class = OBJ_FIELD(class_table, CLASS_TABLE_BLOCK);
                home_context = frame_has_context_local(fp) ? (ObjPtr)fp[FRAME_CONTEXT] : NULL;
                block = om_alloc(om, block_class, FORMAT_FIELDS, BLOCK_COPIED_BASE + copied_count);
                if (block == NULL)
                {
                    unsupported_bytecode(opcode);
                    break;
                }
            }

            OBJ_FIELD(block, BLOCK_HOME_CONTEXT) = home_context == NULL ? TAGGED_NIL : (Oop)home_context;
            OBJ_FIELD(block, BLOCK_HOME_RECEIVER) = fp[FRAME_RECEIVER];
            OBJ_FIELD(block, BLOCK_CM) = block_method;

            uint64_t copied_index = 0;
            for (uint64_t index = 0; index < num_args; index++)
            {
                OBJ_FIELD(block, BLOCK_COPIED_BASE + copied_index) = frame_arg_local(fp, index);
                copied_index++;
            }
            for (uint64_t index = 0; index < num_temps; index++)
            {
                OBJ_FIELD(block, BLOCK_COPIED_BASE + copied_index) =
                    fp[-(int64_t)(FP_TEMP_BASE_WORDS + index)];
                copied_index++;
            }

            push(sp_ptr, (Oop)block);
            break;
        }

        case BC_PUSH_ARG:
        {
            uint32_t arg_index = read_u32(&ip);
            push(sp_ptr, frame_arg_local(*fp_ptr, arg_index));
            break;
        }

        case BC_PUSH_THIS_CONTEXT:
        {
            ObjPtr context = ensure_frame_context_with_retry(sp_ptr, fp_ptr, &ip, &bytecode_base, &class_table, om);
            if (context == NULL)
            {
                unsupported_bytecode(opcode);
                break;
            }
            push(sp_ptr, (Oop)context);
            break;
        }

        case BC_RETURN_NON_LOCAL:
        {
            Oop return_value = **sp_ptr;
            ObjPtr current_fp = (ObjPtr)*fp_ptr;
            Oop home_context = block_home_context_for_frame(current_fp);
            Oop closure = block_closure_for_frame(current_fp);
            ObjPtr home_fp = current_fp;

            while (home_fp != NULL && home_fp != (ObjPtr)0xCAFE)
            {
                if (frame_has_context_local(home_fp) && home_fp[FRAME_CONTEXT] == home_context)
                {
                    uint64_t num_args = frame_num_args_local(home_fp);
                    Oop *new_sp = home_fp + FP_ARG_BASE_WORDS + num_args;
                    ObjPtr caller_fp = (ObjPtr)home_fp[FRAME_SAVED_FP];
                    uint8_t *caller_ip = (uint8_t *)home_fp[FRAME_SAVED_IP];

                    *new_sp = return_value;
                    *sp_ptr = new_sp;
                    *fp_ptr = caller_fp;

                    if (home_fp == entry_fp)
                    {
                        return return_value;
                    }

                    ip = caller_ip;
                    if (caller_fp != NULL && caller_fp != (ObjPtr)0xCAFE)
                    {
                        ObjPtr caller_method = (ObjPtr)caller_fp[FRAME_METHOD];
                        ObjPtr bytecodes = (ObjPtr)OBJ_FIELD(caller_method, CM_BYTECODES);
                        bytecode_base = (uint8_t *)&OBJ_FIELD(bytecodes, 0);
                    }
                    break;
                }
                home_fp = (ObjPtr)home_fp[FRAME_SAVED_FP];
            }

            if (home_fp != NULL && home_fp != (ObjPtr)0xCAFE)
            {
                break;
            }

            if (closure == TAGGED_NIL)
            {
                unsupported_bytecode(opcode);
                break;
            }

            Oop selector = cannot_return_selector_oop();
            Oop method = lookup_method_for_receiver(closure, selector, class_table);
            if (method == 0)
            {
                unsupported_bytecode(opcode);
                break;
            }

            Oop *sp = *sp_ptr;
            sp -= 1;
            sp[0] = return_value;
            sp[1] = closure;
            *sp_ptr = sp;

            Oop primitive = OBJ_FIELD((ObjPtr)method, CM_PRIMITIVE);
            if (primitive != tag_smallint(PRIM_NONE))
            {
                unsupported_bytecode(opcode);
                break;
            }
            uint64_t num_temps = (uint64_t)untag_smallint(OBJ_FIELD((ObjPtr)method, CM_NUM_TEMPS));
            activate_method(sp_ptr, fp_ptr, (uint64_t)cannot_return_after_bytecodes, method, 1, num_temps);
            ObjPtr new_method = (ObjPtr)(*fp_ptr)[FRAME_METHOD];
            ObjPtr bytecodes = (ObjPtr)OBJ_FIELD(new_method, CM_BYTECODES);
            bytecode_base = (uint8_t *)&OBJ_FIELD(bytecodes, 0);
            ip = bytecode_base;
            break;
        }

        case BC_PUSH_GLOBAL:
        {
            uint32_t literal_index = read_u32(&ip);
            ObjPtr method = (ObjPtr)(*fp_ptr)[FRAME_METHOD];
            ObjPtr literals = (ObjPtr)OBJ_FIELD(method, CM_LITERALS);
            Oop association = OBJ_FIELD(literals, literal_index);
            if (!is_object_value(association))
            {
                unsupported_bytecode(opcode);
                break;
            }
            push(sp_ptr, OBJ_FIELD((ObjPtr)association, 1));
            break;
        }

        default:
            unsupported_bytecode(opcode);
            break;
        }
    }
}
