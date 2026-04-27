#ifndef VM_DEFS_H
#define VM_DEFS_H

#include <stdint.h>

typedef uint64_t Oop;     // Tagged VM value: object pointer, SmallInt, Character, nil/true/false.
typedef uint64_t *ObjPtr; // Raw pointer into object memory; unsafe across moving GC.
typedef uint64_t *Om;     // Object-memory allocator or semispace GC context.

// Object-memory allocation contract markers.
//
// LO_NO_ALLOC: does not allocate in Living Objects object memory.
// LO_ALLOCATES: may call om_alloc directly or transitively, but does not
//   trigger moving GC under the current contract.
// LO_MAY_GC: may trigger moving GC. Raw object-memory pointers must not be
//   held across these calls unless they are rooted and reloaded afterward.
//
// Clang records these in the AST for future static checks; other compilers
// treat them as documentation-only.
#if defined(__clang__)
#define LO_NO_ALLOC __attribute__((annotate("lo_no_alloc")))
#define LO_ALLOCATES __attribute__((annotate("lo_allocates")))
#define LO_MAY_GC __attribute__((annotate("lo_may_gc")))
#else
#define LO_NO_ALLOC
#define LO_ALLOCATES
#define LO_MAY_GC
#endif

#define WORD_BYTES 8

// Object header layout.
#define OBJ_HEADER_WORDS 3
#define OBJ_CLASS_OFS 0
#define OBJ_FORMAT_OFS 8
#define OBJ_SIZE_OFS 16
#define OBJ_FIELDS_OFS 24

#define OBJ_CLASS(obj) ((obj)[0])
#define OBJ_FORMAT(obj) ((obj)[1])
#define OBJ_SIZE(obj) ((obj)[2])
#define OBJ_FIELD(obj, n) ((obj)[3 + (n)])

// Object formats.
#define FORMAT_FIELDS 0
#define FORMAT_INDEXABLE 1
#define FORMAT_BYTES 2

// Tagged value layout.
#define TAG_MASK 0x3
#define TAG_OBJECT 0
#define TAG_SMALLINT 1
#define TAG_FLOAT 2
#define TAG_SPECIAL 3
#define SMALLINT_SHIFT 2

#define TAGGED_NIL 0x03
#define TAGGED_TRUE 0x07
#define TAGGED_FALSE 0x0B

#define CHAR_TAG_MASK 0x0F
#define CHAR_TAG_VALUE 0x0F
#define CHAR_SHIFT 4

// Class table object fields.
#define CLASS_TABLE_SMALLINT 0
#define CLASS_TABLE_BLOCK 1
#define CLASS_TABLE_TRUE 2
#define CLASS_TABLE_FALSE 3
#define CLASS_TABLE_CHARACTER 4
#define CLASS_TABLE_UNDEFINED_OBJECT 5

#define CLASS_TABLE_SMALLINT_OFS 24
#define CLASS_TABLE_BLOCK_OFS 32
#define CLASS_TABLE_TRUE_OFS 40
#define CLASS_TABLE_FALSE_OFS 48
#define CLASS_TABLE_CHARACTER_OFS 56
#define CLASS_TABLE_UNDEFINED_OBJECT_OFS 64

// Class object fields.
#define CLASS_SUPERCLASS 0
#define CLASS_METHOD_DICT 1
#define CLASS_INST_SIZE 2
#define CLASS_INST_FORMAT 3
#define CLASS_INST_VARS 4

#define CLASS_SUPERCLASS_OFS 24
#define CLASS_METHOD_DICT_OFS 32
#define CLASS_INST_SIZE_OFS 40
#define CLASS_INST_FORMAT_OFS 48
#define CLASS_INST_VARS_OFS 56

// CompiledMethod fields.
#define CM_PRIMITIVE 0
#define CM_NUM_ARGS 1
#define CM_NUM_TEMPS 2
#define CM_LITERALS 3
#define CM_BYTECODES 4
#define CM_SOURCE 5

#define CM_PRIMITIVE_OFS 24
#define CM_NUM_ARGS_OFS 32
#define CM_NUM_TEMPS_OFS 40
#define CM_LITERALS_OFS 48
#define CM_BYTECODES_OFS 56
#define CM_SOURCE_OFS 64

// Primitive IDs.
#define PRIM_NONE 0
#define PRIM_SMALLINT_ADD 1
#define PRIM_SMALLINT_SUB 2
#define PRIM_SMALLINT_LT 3
#define PRIM_SMALLINT_EQ 4
#define PRIM_SMALLINT_MUL 5
#define PRIM_AT 6
#define PRIM_AT_PUT 7
#define PRIM_BASIC_NEW 8
#define PRIM_BLOCK_VALUE 9
#define PRIM_BASIC_NEW_SIZE 10
#define PRIM_SIZE 11
#define PRIM_IDENTITY_EQ 12
#define PRIM_BASIC_CLASS 13
#define PRIM_HASH 14
#define PRIM_PRINT_CHAR 15
#define PRIM_BLOCK_VALUE_ARG 16
#define PRIM_PERFORM 17
#define PRIM_HALT 18
#define PRIM_CHAR_VALUE 19
#define PRIM_AS_CHARACTER 20
#define PRIM_CHAR_IS_LETTER 21
#define PRIM_CHAR_IS_DIGIT 22
#define PRIM_CHAR_UPPERCASE 23
#define PRIM_CHAR_LOWERCASE 24
#define PRIM_STRING_EQ 25
#define PRIM_STRING_HASH_FNV 26
#define PRIM_STRING_AS_SYMBOL 27
#define PRIM_SYMBOL_EQ 28
#define PRIM_ERROR 29
#define PRIM_CLASS_SUPERCLASS 30
#define PRIM_CLASS_NAME 31
#define PRIM_CLASS_INCLUDES_SELECTOR 32
#define PRIM_SMALLTALK_GLOBALS 33
#define PRIM_METHOD_SOURCE_FOR_CLASS_SELECTOR 34
#define PRIM_READ_FD_COUNT 35
#define PRIM_WRITE_FD_STRING 36

// BlockClosure fields.
#define BLOCK_HOME_CONTEXT 0
#define BLOCK_HOME_RECEIVER 1
#define BLOCK_CM 2
#define BLOCK_COPIED_BASE 3

#define BLOCK_HOME_CONTEXT_OFS 24
#define BLOCK_HOME_RECEIVER_OFS 32
#define BLOCK_CM_OFS 40
#define BLOCK_COPIED_VALUES_OFS 48

// Context fields.
#define CONTEXT_SENDER 0
#define CONTEXT_IP 1
#define CONTEXT_METHOD 2
#define CONTEXT_RECEIVER 3
#define CONTEXT_HOME 4
#define CONTEXT_CLOSURE 5
#define CONTEXT_FLAGS 6
#define CONTEXT_NUM_ARGS 7
#define CONTEXT_NUM_TEMPS 8
#define CONTEXT_STACK_SIZE 9
#define CONTEXT_VAR_BASE 10

#define CONTEXT_RECEIVER_OFS 48
#define CONTEXT_HOME_OFS 56
#define CONTEXT_CLOSURE_OFS 64

// Bytecodes.
#define BC_PUSH_LITERAL 0
#define BC_PUSH_INST_VAR 1
#define BC_PUSH_TEMP 2
#define BC_PUSH_SELF 3
#define BC_STORE_INST_VAR 4
#define BC_STORE_TEMP 5
#define BC_SEND_MESSAGE 6
#define BC_RETURN 7
#define BC_RETURN_STACK_TOP 7
#define BC_JUMP 8
#define BC_JUMP_IF_TRUE 9
#define BC_JUMP_IF_FALSE 10
#define BC_POP 11
#define BC_DUPLICATE 12
#define BC_HALT 13
#define BC_PUSH_CLOSURE 14
#define BC_PUSH_ARG 15
#define BC_RETURN_NON_LOCAL 16
#define BC_PUSH_THIS_CONTEXT 17
#define BC_PUSH_GLOBAL 18

// Frame layout indices, relative to FP as uint64_t words.
#define FRAME_SAVED_IP 1
#define FRAME_SAVED_FP 0
#define FRAME_METHOD -1
#define FRAME_FLAGS -2
#define FRAME_CONTEXT -3
#define FRAME_RECEIVER -4
#define FRAME_TEMP0 -5

// Frame layout byte offsets.
#define FP_SAVED_IP_OFS 8
#define FP_METHOD_OFS -8
#define FP_FLAGS_OFS -16
#define FP_CONTEXT_OFS -24
#define FP_RECEIVER_OFS -32
#define FP_TEMP_BASE_OFS -40
#define FP_ARG0_OFS 16

#define FP_TEMP_BASE_WORDS 5
#define FP_ARG_BASE_WORDS 2

#define FRAME_FLAGS_HAS_CONTEXT_MASK 0x1
#define FRAME_FLAGS_BLOCK_CLOSURE_MASK 0x2
#define FRAME_FLAGS_NUM_ARGS_SHIFT 8
#define FRAME_FLAGS_NUM_ARGS_WIDTH 8
#define FRAME_FLAGS_IS_BLOCK_SHIFT 16
#define FRAME_FLAGS_IS_BLOCK_WIDTH 8

// GC and test heap defaults.
#define GC_FORWARD_TAG 1
#define STACK_WORDS 4096
#define OM_SIZE (4 * 1024 * 1024)

static inline void WRITE_U32(uint8_t *p, uint32_t v)
{
    p[0] = v & 0xFF;
    p[1] = (v >> 8) & 0xFF;
    p[2] = (v >> 16) & 0xFF;
    p[3] = (v >> 24) & 0xFF;
}

#endif
