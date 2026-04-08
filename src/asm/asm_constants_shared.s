// Shared assembly constants

.equ WORD_BYTES, 8

// Object header layout
.equ OBJ_HEADER_WORDS, 3
.equ OBJ_CLASS_OFS, 0
.equ OBJ_FORMAT_OFS, 8
.equ OBJ_SIZE_OFS, 16
.equ OBJ_FIELDS_OFS, 24

// Object formats
.equ FORMAT_FIELDS, 0
.equ FORMAT_INDEXABLE, 1
.equ FORMAT_BYTES, 2

// Tagging
.equ TAG_MASK, 0x3
.equ TAG_OBJECT, 0
.equ TAG_SMALLINT, 1
.equ TAG_FLOAT, 2
.equ TAG_SPECIAL, 3
.equ SMALLINT_SHIFT, 2

.equ TAGGED_NIL, 0x03
.equ TAGGED_TRUE, 0x07
.equ TAGGED_FALSE, 0x0B

.equ CHAR_TAG_MASK, 0x0F
.equ CHAR_TAG_VALUE, 0x0F
.equ CHAR_SHIFT, 4

// Class field offsets (as object fields)
.equ CLASS_SUPERCLASS_OFS, 24
.equ CLASS_METHOD_DICT_OFS, 32
.equ CLASS_INST_SIZE_OFS, 40
.equ CLASS_INST_FORMAT_OFS, 48

// CompiledMethod field offsets
.equ CM_NUM_TEMPS_OFS, 40
.equ CM_LITERALS_OFS, 48
.equ CM_BYTECODES_OFS, 56

// Class table object field offsets
.equ CLASS_TABLE_SMALLINT_OFS, 24
.equ CLASS_TABLE_BLOCK_OFS, 32
.equ CLASS_TABLE_TRUE_OFS, 40
.equ CLASS_TABLE_FALSE_OFS, 48
.equ CLASS_TABLE_CHARACTER_OFS, 56
.equ CLASS_TABLE_UNDEFINED_OBJECT_OFS, 64

// Frame layout offsets
.equ FP_SAVED_IP_OFS, 8
.equ FP_METHOD_OFS, -8
.equ FP_FLAGS_OFS, -16
.equ FP_CONTEXT_OFS, -24
.equ FP_RECEIVER_OFS, -32
.equ FP_TEMP_BASE_OFS, -40
.equ FP_ARG0_OFS, 16

// Frame index constants (words from FP)
.equ FP_TEMP_BASE_WORDS, 5
.equ FP_ARG_BASE_WORDS, 2

// Frame flags bit layout
.equ FRAME_FLAGS_HAS_CONTEXT_MASK, 0xFF
.equ FRAME_FLAGS_NUM_ARGS_SHIFT, 8
.equ FRAME_FLAGS_NUM_ARGS_WIDTH, 8
.equ FRAME_FLAGS_IS_BLOCK_SHIFT, 16
.equ FRAME_FLAGS_IS_BLOCK_WIDTH, 8

// Block object fields
.equ BLOCK_HOME_CONTEXT_OFS, 24
.equ BLOCK_HOME_RECEIVER_OFS, 32
.equ BLOCK_CM_OFS, 40
.equ BLOCK_COPIED_VALUES_OFS, 48

// CompiledMethod primitive field
.equ CM_PRIMITIVE_OFS, 24
