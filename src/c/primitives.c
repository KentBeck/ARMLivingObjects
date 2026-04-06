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
