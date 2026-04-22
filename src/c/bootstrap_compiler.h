#ifndef BOOTSTRAP_COMPILER_H
#define BOOTSTRAP_COMPILER_H

#include <stdint.h>

typedef enum
{
    BTOK_EOF = 0,
    BTOK_IDENTIFIER,
    BTOK_KEYWORD,
    BTOK_INTEGER,
    BTOK_CHARACTER,
    BTOK_STRING,
    BTOK_SYMBOL,
    BTOK_SELECTOR,
    BTOK_BLOCK_LITERAL,
    BTOK_LITERAL_ARRAY,
    BTOK_CLASS_REF,
    BTOK_SPECIAL
} BTokenType;

typedef struct
{
    BTokenType type;
    char text[128];
    int64_t int_value;
} BToken;

typedef struct
{
    const char *source;
    uint64_t index;
} BTokenizer;

void bt_init(BTokenizer *tokenizer, const char *source);
BToken bt_next(BTokenizer *tokenizer);

typedef enum
{
    BMETHOD_UNARY = 0,
    BMETHOD_BINARY,
    BMETHOD_KEYWORD
} BMethodKind;

typedef struct
{
    BMethodKind kind;
    char selector[128];
    int arg_count;
    char arg_names[8][32];
} BMethodHeader;

int bc_parse_method_header(const char *source, BMethodHeader *header);

typedef struct
{
    int temp_count;
    char temp_names[16][32];
    int assignment_count;
    int return_count;
    int literal_integer_count;
    int literal_character_count;
    int literal_string_count;
    int literal_symbol_count;
    int message_send_count;
} BMethodBody;

int bc_parse_method_body(const char *source, BMethodBody *body);

typedef struct
{
    uint8_t bytecodes[512];
    int bytecode_count;
    int literal_count;
    BToken literals[32];
} BCompiledBlock;

typedef struct
{
    uint8_t bytecodes[512];
    int bytecode_count;
    int temp_count;
    int literal_count;
    BToken literals[32];
    int inst_var_count;
    char inst_var_names[16][32];
    int block_count;
    BCompiledBlock blocks[16];
} BCompiledBody;

int bc_codegen_method_body(const char *source, BCompiledBody *compiled);

typedef struct
{
    char class_name[64];
    int class_side;
    char category[64];
    char method_source[4096];
} BMethodChunk;

int bc_parse_method_chunks(const char *source, BMethodChunk *chunks, int max_chunks, int *out_count);

typedef struct
{
    char class_name[64];
    int class_side;
    BMethodHeader header;
    int primitive_index;
    BCompiledBody body;
} BCompiledMethodDef;

int bc_compile_method_chunks(const BMethodChunk *chunks, int chunk_count,
                             BCompiledMethodDef *methods, int max_methods, int *out_count);

int bc_compile_source_methods(const char *source,
                              BCompiledMethodDef *methods, int max_methods, int *out_count);

typedef struct
{
    const char *class_name;
    uint64_t *klass;
} BClassBinding;

int bc_install_compiled_methods(uint64_t *om, uint64_t *class_class,
                                const BClassBinding *classes, int class_count,
                                const BCompiledMethodDef *methods, int method_count);

int bc_compile_and_install_source_methods(uint64_t *om, uint64_t *class_class,
                                          const BClassBinding *classes, int class_count,
                                          const char *source);

typedef enum
{
    BC_CLASS_FORMAT_FIELDS = 0,
    BC_CLASS_FORMAT_INDEXABLE = 1,
    BC_CLASS_FORMAT_BYTES = 2
} BClassFormat;

uint64_t *bc_define_class(uint64_t *om, uint64_t *class_class, uint64_t *string_class,
                          uint64_t *array_class, uint64_t *association_class,
                          const char *name, uint64_t *superclass,
                          const char **ivar_names, int ivar_count,
                          BClassFormat format);

uint64_t *bc_define_class_from_source(uint64_t *om, uint64_t *class_class,
                                      uint64_t *string_class, uint64_t *array_class,
                                      uint64_t *association_class,
                                      const BClassBinding *classes, int class_count,
                                      const char *source);

uint64_t *bc_compile_and_install_class_source(uint64_t *om, uint64_t *class_class,
                                              uint64_t *string_class, uint64_t *array_class,
                                              uint64_t *association_class,
                                              const BClassBinding *classes, int class_count,
                                              const char *source);

uint64_t *bc_compile_and_install_class_file(uint64_t *om, uint64_t *class_class,
                                            uint64_t *string_class, uint64_t *array_class,
                                            uint64_t *association_class,
                                            const BClassBinding *classes, int class_count,
                                            const char *path);

#endif
