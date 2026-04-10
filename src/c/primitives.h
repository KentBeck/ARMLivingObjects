#ifndef PRIMITIVES_H
#define PRIMITIVES_H

#include <stdint.h>
#include "test_defs.h" // For tagged_nil, tagged_true, tagged_false, etc.

extern uint64_t *global_symbol_table; // Declare global symbol table
extern uint64_t *global_symbol_class;
extern uint64_t *global_context_class;
extern uint64_t *global_smalltalk_dictionary;

uint64_t prim_string_eq(uint64_t receiver, uint64_t arg);
uint64_t prim_string_hash_fnv(uint64_t receiver);
uint64_t prim_string_as_symbol(uint64_t receiver); // No symbol_table arg needed
uint64_t prim_symbol_eq(uint64_t receiver, uint64_t arg);
uint64_t intern_cstring_symbol(uint64_t *om, const char *text);
uint64_t lookup_cstring_symbol(const char *text);
uint64_t *ensure_frame_context(uint64_t *fp, uint64_t *om, uint64_t context_class);
uint64_t *ensure_frame_context_global(uint64_t *fp, uint64_t *om);
uint64_t cannot_return_selector_oop(void);

#endif
