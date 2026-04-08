#include "primitives.h"
#include <string.h> // For memcmp

uint64_t *global_symbol_table; // Define global symbol table

// Helper to get raw byte pointer and size from a FORMAT_BYTES object
static inline void get_byte_obj_data(uint64_t obj_ptr, uint8_t **data, uint64_t *size) {
    uint64_t *obj = (uint64_t *)obj_ptr;
    *data = (uint8_t *)&OBJ_FIELD(obj, 0);
    *size = OBJ_SIZE(obj);
}

// PRIM_STRING_EQ: Compares two strings byte by byte
// receiver: tagged object pointer to String
// arg: tagged object pointer to String
uint64_t prim_string_eq(uint64_t receiver, uint64_t arg) {
    // Both must be object pointers
    if (!is_object_ptr(receiver) || !is_object_ptr(arg)) {
        return tagged_false(); // Or error, depending on strictness
    }

    uint8_t *recv_data, *arg_data;
    uint64_t recv_size, arg_size;

    get_byte_obj_data(receiver, &recv_data, &recv_size);
    get_byte_obj_data(arg, &arg_data, &arg_size);

    if (recv_size != arg_size) {
        return tagged_false();
    }

    if (memcmp(recv_data, arg_data, recv_size) == 0) {
        return tagged_true();
    } else {
        return tagged_false();
    }
}

// PRIM_STRING_HASH: Calculates a hash for a string (FNV-1a)
// receiver: tagged object pointer to String
uint64_t prim_string_hash_fnv(uint64_t receiver) {
    if (!is_object_ptr(receiver)) {
        return tag_smallint(0); // Or error
    }

    uint8_t *data;
    uint64_t size;
    get_byte_obj_data(receiver, &data, &size);

    uint32_t hash = 0x811C9DC5; // FNV-1a 32-bit prime
    for (uint64_t i = 0; i < size; i++) {
        hash ^= data[i];
        hash *= 0x01000193; // FNV-1a 32-bit basis
    }
    return tag_smallint(hash);
}

// PRIM_STRING_AS_SYMBOL: Interns a string into the global symbol table
// receiver: tagged object pointer to String
uint64_t prim_string_as_symbol(uint64_t receiver) {
    if (!is_object_ptr(receiver)) {
        return tagged_nil(); // Or error
    }

    // Use the global_symbol_table
    if (global_symbol_table == NULL) {
        // Error: symbol table not initialized
        return tagged_nil();
    }

    // Iterate through global_symbol_table to see if string already exists
    uint64_t table_size = OBJ_SIZE(global_symbol_table);
    for (uint64_t i = 0; i < table_size; i++) {
        uint64_t existing_symbol = OBJ_FIELD(global_symbol_table, i);
        if (existing_symbol != tagged_nil()) {
            // Compare receiver string with existing_symbol string
            if (prim_string_eq(receiver, existing_symbol) == tagged_true()) {
                return existing_symbol; // Found existing symbol
            }
        }
    }

    // If not found, add receiver to global_symbol_table and return it
    // This is a simplified version. A real symbol table would grow dynamically.
    // For now, just find the first nil slot.
    for (uint64_t i = 0; i < table_size; i++) {
        if (OBJ_FIELD(global_symbol_table, i) == tagged_nil()) {
            OBJ_FIELD(global_symbol_table, i) = receiver;
            return receiver;
        }
    }

    // Symbol table full (for now, return nil or error)
    return tagged_nil();
}

// PRIM_SYMBOL_EQ: Compares two symbols for identity (pointer equality)
// receiver: tagged object pointer to Symbol
// arg: tagged object pointer to Symbol
uint64_t prim_symbol_eq(uint64_t receiver, uint64_t arg) {
    // Symbols are interned, so identity equality is sufficient
    if (receiver == arg) {
        return tagged_true();
    } else {
        return tagged_false();
    }
}

static uint64_t context_ip_for_frame(uint64_t *fp)
{
    uint64_t saved_ip = fp[FRAME_SAVED_IP];
    uint64_t *method = (uint64_t *)fp[FRAME_METHOD];
    if (method == NULL || !is_object_ptr((uint64_t)method))
    {
        return tagged_nil();
    }

    uint64_t bytecodes_oop = OBJ_FIELD(method, CM_BYTECODES);
    if (!is_object_ptr(bytecodes_oop))
    {
        return tagged_nil();
    }

    uint8_t *base = (uint8_t *)&OBJ_FIELD((uint64_t *)bytecodes_oop, 0);
    uint64_t size = OBJ_SIZE((uint64_t *)bytecodes_oop);
    uint8_t *ip = (uint8_t *)saved_ip;
    if (ip < base || ip > base + size)
    {
        return tagged_nil();
    }

    return tag_smallint((int64_t)(ip - base));
}

uint64_t *ensure_frame_context(uint64_t *fp, uint64_t *om, uint64_t context_class)
{
    if (fp == NULL || fp == (uint64_t *)0xCAFE)
    {
        return NULL;
    }

    if ((fp[FRAME_FLAGS] & 0xFF) != 0)
    {
        return (uint64_t *)fp[FRAME_CONTEXT];
    }

    uint64_t num_args = (fp[FRAME_FLAGS] >> 8) & 0xFF;
    uint64_t *method = (uint64_t *)fp[FRAME_METHOD];
    uint64_t num_temps = 0;
    if (method != NULL && is_object_ptr((uint64_t)method))
    {
        num_temps = (uint64_t)untag_smallint(OBJ_FIELD(method, CM_NUM_TEMPS));
    }

    uint64_t field_count = CONTEXT_VAR_BASE + num_args + num_temps;
    uint64_t *context = om_alloc(om, context_class, FORMAT_FIELDS, field_count);
    if (context == NULL)
    {
        return NULL;
    }

    uint64_t *caller_fp = (uint64_t *)fp[FRAME_SAVED_FP];
    uint64_t *sender = ensure_frame_context(caller_fp, om, context_class);
    OBJ_FIELD(context, CONTEXT_SENDER) = sender == NULL ? tagged_nil() : (uint64_t)sender;
    OBJ_FIELD(context, CONTEXT_IP) = context_ip_for_frame(fp);
    OBJ_FIELD(context, CONTEXT_METHOD) = (uint64_t)method;
    OBJ_FIELD(context, CONTEXT_RECEIVER) = fp[FRAME_RECEIVER];
    OBJ_FIELD(context, CONTEXT_FLAGS) = tag_smallint((int64_t)fp[FRAME_FLAGS]);
    OBJ_FIELD(context, CONTEXT_NUM_ARGS) = tag_smallint((int64_t)num_args);
    OBJ_FIELD(context, CONTEXT_NUM_TEMPS) = tag_smallint((int64_t)num_temps);

    for (uint64_t i = 0; i < num_args; i++)
    {
        OBJ_FIELD(context, CONTEXT_VAR_BASE + i) = frame_arg(fp, i);
    }
    for (uint64_t i = 0; i < num_temps; i++)
    {
        OBJ_FIELD(context, CONTEXT_VAR_BASE + num_args + i) = frame_temp(fp, i);
    }

    fp[FRAME_CONTEXT] = (uint64_t)context;
    fp[FRAME_FLAGS] |= 1;
    return context;
}
