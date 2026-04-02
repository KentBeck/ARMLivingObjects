#include "bootstrap_compiler.h"

#include <ctype.h>
#include <string.h>

static char peek(BTokenizer *tokenizer)
{
    return tokenizer->source[tokenizer->index];
}

static char advance(BTokenizer *tokenizer)
{
    char character = tokenizer->source[tokenizer->index];
    if (character != '\0')
    {
        tokenizer->index++;
    }
    return character;
}

static void skip_whitespace(BTokenizer *tokenizer)
{
    while (1)
    {
        char character = peek(tokenizer);
        if (character == ' ' || character == '\t' || character == '\n' || character == '\r')
        {
            advance(tokenizer);
            continue;
        }
        break;
    }
}

static BToken make_token(BTokenType type)
{
    BToken token;
    token.type = type;
    token.text[0] = '\0';
    token.int_value = 0;
    return token;
}

void bt_init(BTokenizer *tokenizer, const char *source)
{
    tokenizer->source = source;
    tokenizer->index = 0;
}

BToken bt_next(BTokenizer *tokenizer)
{
    skip_whitespace(tokenizer);

    char character = peek(tokenizer);
    if (character == '\0')
    {
        return make_token(BTOK_EOF);
    }

    if (isdigit((unsigned char)character))
    {
        BToken token = make_token(BTOK_INTEGER);
        int64_t value = 0;
        int text_index = 0;
        while (isdigit((unsigned char)peek(tokenizer)))
        {
            char digit = advance(tokenizer);
            if (text_index < (int)sizeof(token.text) - 1)
            {
                token.text[text_index++] = digit;
            }
            value = (value * 10) + (digit - '0');
        }
        token.text[text_index] = '\0';
        token.int_value = value;
        return token;
    }

    if (isalpha((unsigned char)character) || character == '_')
    {
        BToken token = make_token(BTOK_IDENTIFIER);
        int text_index = 0;
        while (isalnum((unsigned char)peek(tokenizer)) || peek(tokenizer) == '_')
        {
            char c = advance(tokenizer);
            if (text_index < (int)sizeof(token.text) - 1)
            {
                token.text[text_index++] = c;
            }
        }
        if (peek(tokenizer) == ':')
        {
            token.type = BTOK_KEYWORD;
            if (text_index < (int)sizeof(token.text) - 1)
            {
                token.text[text_index++] = advance(tokenizer);
            }
            else
            {
                advance(tokenizer);
            }
        }
        token.text[text_index] = '\0';
        return token;
    }

    if (character == '\'')
    {
        BToken token = make_token(BTOK_STRING);
        int text_index = 0;
        advance(tokenizer); // opening quote
        while (peek(tokenizer) != '\0')
        {
            char c = advance(tokenizer);
            if (c == '\'')
            {
                if (peek(tokenizer) == '\'')
                {
                    c = advance(tokenizer);
                }
                else
                {
                    break;
                }
            }
            if (text_index < (int)sizeof(token.text) - 1)
            {
                token.text[text_index++] = c;
            }
        }
        token.text[text_index] = '\0';
        return token;
    }

    if (character == '#')
    {
        BToken token = make_token(BTOK_SYMBOL);
        int text_index = 0;
        advance(tokenizer); // '#'
        while (isalnum((unsigned char)peek(tokenizer)) || peek(tokenizer) == '_' || peek(tokenizer) == ':')
        {
            char c = advance(tokenizer);
            if (text_index < (int)sizeof(token.text) - 1)
            {
                token.text[text_index++] = c;
            }
        }
        token.text[text_index] = '\0';
        return token;
    }

    {
        BToken token = make_token(BTOK_SPECIAL);
        token.text[0] = advance(tokenizer);
        token.text[1] = '\0';
        return token;
    }
}

