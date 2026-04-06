#ifndef PRIMITIVES_H
#define PRIMITIVES_H

#include <stdint.h>
#include "test_defs.h" // For tagged_nil, tagged_true, tagged_false, etc.

extern uint64_t *global_symbol_table; // Declare global symbol table

uint64_t prim_string_eq(uint64_t receiver, uint64_t arg);
uint64_t prim_string_hash_fnv(uint64_t receiver);
uint64_t prim_string_as_symbol(uint64_t receiver); // No symbol_table arg needed
uint64_t prim_symbol_eq(uint64_t receiver, uint64_t arg);

#endif
