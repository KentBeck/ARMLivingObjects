#ifndef PRIMITIVES_H
#define PRIMITIVES_H

#include <stdint.h>
#include "test_defs.h" // For tagged_nil, tagged_true, tagged_false, etc.

extern uint64_t *global_symbol_table; // Declare global symbol table
extern uint64_t *global_symbol_class;
extern uint64_t *global_context_class;
extern uint64_t *global_smalltalk_dictionary;
Oop prim_class_superclass(Oop receiver) LO_NO_ALLOC;
Oop prim_class_name(Oop receiver) LO_NO_ALLOC;
Oop prim_class_includes_selector(Oop receiver, Oop selector) LO_NO_ALLOC;
Oop prim_smalltalk_globals(void) LO_NO_ALLOC;
Oop prim_method_source_for_class_selector(Oop class_name, Oop selector, Om om) LO_ALLOCATES;
Oop prim_read_fd_count(Oop fd, Oop count, Om om) LO_ALLOCATES;
Oop prim_write_fd_string(Oop fd, Oop string) LO_NO_ALLOC;

Oop prim_string_eq(Oop receiver, Oop arg) LO_NO_ALLOC;
Oop prim_string_hash_fnv(Oop receiver) LO_NO_ALLOC;
Oop prim_string_as_symbol(Oop receiver) LO_NO_ALLOC; // No symbol_table arg needed
Oop prim_symbol_eq(Oop receiver, Oop arg) LO_NO_ALLOC;
Oop intern_cstring_symbol(Om om, const char *text) LO_ALLOCATES;
Oop lookup_cstring_symbol(const char *text) LO_NO_ALLOC;
ObjPtr ensure_frame_context(ObjPtr fp, Om om, Oop context_class) LO_ALLOCATES;
ObjPtr ensure_frame_context_global(ObjPtr fp, Om om) LO_ALLOCATES;
Oop cannot_return_selector_oop(void) LO_NO_ALLOC;

#endif
