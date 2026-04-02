#ifndef BOOTSTRAP_COMPILER_H
#define BOOTSTRAP_COMPILER_H

#include <stdint.h>

typedef enum
{
    BTOK_EOF = 0,
    BTOK_IDENTIFIER,
    BTOK_KEYWORD,
    BTOK_INTEGER,
    BTOK_STRING,
    BTOK_SYMBOL,
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
    int literal_string_count;
    int literal_symbol_count;
    int message_send_count;
} BMethodBody;

int bc_parse_method_body(const char *source, BMethodBody *body);

#endif
