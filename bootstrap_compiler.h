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

#endif

