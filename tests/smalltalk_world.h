#ifndef SMALLTALK_WORLD_H
#define SMALLTALK_WORLD_H

// Canonical "fresh Smalltalk world" for tests that need to run Smalltalk
// source end-to-end. Handles:
//   - fresh object memory
//   - core class skeleton (Class, Object, SmallInteger, String, Symbol, Array,
//     BlockClosure, Character, True, False, UndefinedObject, Association,
//     Dictionary)
//   - symbol table + global Smalltalk dictionary populated with class names
//   - primitive methods wired onto the core classes
//   - a framework class_table for send_selector helpers
//
// The world captures existing global state at init and restores it at
// teardown, so a test can construct a world, use it, and not pollute other
// tests that share globals (global_symbol_table etc.).

#include <stdint.h>
#include "test_defs.h"

typedef struct
{
    // Backing memory (caller-owned).
    uint64_t om[2];

    // Core classes.
    uint64_t *class_class;
    uint64_t *object_class;
    uint64_t *smallint_class;
    uint64_t *block_class;
    uint64_t *undefined_class;
    uint64_t *true_class;
    uint64_t *false_class;
    uint64_t *string_class;
    uint64_t *symbol_class;
    uint64_t *character_class;
    uint64_t *array_class;
    uint64_t *association_class;
    uint64_t *dictionary_class;
    uint64_t *context_class;

    // Symbol table + Smalltalk dictionary.
    uint64_t *symbol_table;
    uint64_t *smalltalk_dict;

    // Class table used by send_selector helpers (field 0 = SmallInteger,
    // 1 = BlockClosure, 2 = True, 3 = False, 4 = Character,
    // 5 = UndefinedObject).
    uint64_t *class_table;

    // Saved globals for restore on teardown.
    uint64_t *saved_symbol_table;
    uint64_t *saved_symbol_class;
    uint64_t *saved_context_class;
    uint64_t *saved_smalltalk_dict;
} SmalltalkWorld;

// Build a fresh world into the caller-provided buffer. Installs core classes
// with primitive methods. Captures the current values of global state so
// smalltalk_world_teardown can restore them.
void smalltalk_world_init(SmalltalkWorld *world, void *buffer, uint64_t buffer_size) LO_ALLOCATES;

// Restore the global state that was captured by smalltalk_world_init.
void smalltalk_world_teardown(SmalltalkWorld *world) LO_NO_ALLOC;

// Define a new class. If `ivars`/`ivar_count` is empty, no inst_vars array is
// allocated. Registers the class in the Smalltalk dictionary under `name`.
uint64_t *smalltalk_world_define_class(SmalltalkWorld *world, const char *name,
                                       uint64_t *superclass,
                                       const char **ivars, int ivar_count,
                                       int format) LO_ALLOCATES;

// Load a .st file from `path` and install its methods via the C bootstrap.
// Returns 1 on success, 0 on any failure (file read, chunk parse, or install).
int smalltalk_world_install_st_file(SmalltalkWorld *world, const char *path) LO_ALLOCATES;

// Load a .st file that starts with a class declaration, define that class, and
// install its methods. Returns the class on success or NULL on failure.
uint64_t *smalltalk_world_install_class_file(SmalltalkWorld *world, const char *path) LO_ALLOCATES;

// Load a .st file that starts with a class declaration for an already existing
// class, verify the declaration matches the current class, and install methods.
// Returns the existing class on success or NULL on failure.
uint64_t *smalltalk_world_install_existing_class_file(SmalltalkWorld *world, const char *path) LO_ALLOCATES;

// Look up a class by name, walking the Smalltalk dictionary. Safe across GC
// since it re-fetches the live pointer each time.
ObjPtr smalltalk_world_lookup_class(SmalltalkWorld *world, const char *name) LO_NO_ALLOC;

// Store and fetch arbitrary globals from the live Smalltalk dictionary.
void smalltalk_world_put_global(SmalltalkWorld *world, const char *name, Oop value) LO_ALLOCATES;
Oop smalltalk_world_lookup_global(SmalltalkWorld *world, const char *name) LO_NO_ALLOC;

// Send a unary message to a receiver of a known class. Walks the class
// hierarchy for method lookup and runs the interpreter.
Oop sw_send0(SmalltalkWorld *world, TestContext *ctx, Oop receiver,
             ObjPtr receiver_class, const char *selector) LO_MAY_GC;
Oop sw_send1(SmalltalkWorld *world, TestContext *ctx, Oop receiver,
             ObjPtr receiver_class, const char *selector, Oop arg) LO_MAY_GC;
Oop sw_send2(SmalltalkWorld *world, TestContext *ctx, Oop receiver,
             ObjPtr receiver_class, const char *selector,
             Oop arg0, Oop arg1) LO_MAY_GC;

// Make a byte String in the world's OM.
ObjPtr sw_make_string(SmalltalkWorld *world, const char *text) LO_ALLOCATES;

#endif
