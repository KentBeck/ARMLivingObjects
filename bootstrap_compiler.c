#include "bootstrap_compiler.h"

#include <ctype.h>
#include <string.h>

static char peek(BTokenizer *tokenizer)
{
    return tokenizer->source[tokenizer->index];
}

static int is_binary_selector_char(char character)
{
    return character == '+' || character == '-' || character == '*' || character == '/' ||
           character == '<' || character == '>' || character == '=' || character == '~' ||
           character == '&' || character == '|' || character == '@' || character == '%' ||
           character == ',' || character == '?';
}

typedef struct
{
    BTokenizer tokenizer;
    int has_buffered;
    BToken buffered;
} BParser;

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

    if (character == '$')
    {
        BToken token = make_token(BTOK_CHARACTER);
        advance(tokenizer); // '$'
        char value = advance(tokenizer);
        if (value == '\0')
        {
            return make_token(BTOK_EOF);
        }
        token.text[0] = value;
        token.text[1] = '\0';
        token.int_value = (unsigned char)value;
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
        if (tokenizer->source[tokenizer->index + 1] == '(')
        {
            BToken token = make_token(BTOK_SPECIAL);
            advance(tokenizer); // '#'
            advance(tokenizer); // '('
            token.text[0] = '#';
            token.text[1] = '(';
            token.text[2] = '\0';
            return token;
        }

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
        if (character == ':' && tokenizer->source[tokenizer->index + 1] == '=')
        {
            advance(tokenizer);
            advance(tokenizer);
            token.text[0] = ':';
            token.text[1] = '=';
            token.text[2] = '\0';
            return token;
        }
        token.text[0] = advance(tokenizer);
        token.text[1] = '\0';
        return token;
    }
}

static void bp_init(BParser *parser, const char *source)
{
    bt_init(&parser->tokenizer, source);
    parser->has_buffered = 0;
}

static BToken bp_next(BParser *parser)
{
    if (parser->has_buffered)
    {
        parser->has_buffered = 0;
        return parser->buffered;
    }
    return bt_next(&parser->tokenizer);
}

static void bp_unread(BParser *parser, BToken token)
{
    parser->buffered = token;
    parser->has_buffered = 1;
}

static void count_literal(BMethodBody *body, BToken token)
{
    if (token.type == BTOK_INTEGER)
    {
        body->literal_integer_count++;
    }
    else if (token.type == BTOK_CHARACTER)
    {
        body->literal_character_count++;
    }
    else if (token.type == BTOK_STRING)
    {
        body->literal_string_count++;
    }
    else if (token.type == BTOK_SYMBOL)
    {
        body->literal_symbol_count++;
    }
}

static int parse_expression_from_first(BParser *parser, BMethodBody *body, BToken first);

static int parse_literal_array(BParser *parser, BMethodBody *body)
{
    while (1)
    {
        BToken token = bp_next(parser);
        if (token.type == BTOK_EOF)
        {
            return 0;
        }
        if (token.type == BTOK_SPECIAL && strcmp(token.text, ")") == 0)
        {
            return 1;
        }
        if (token.type == BTOK_SPECIAL && strcmp(token.text, "#(") == 0)
        {
            if (!parse_literal_array(parser, body))
            {
                return 0;
            }
            continue;
        }
        if (token.type == BTOK_INTEGER || token.type == BTOK_CHARACTER || token.type == BTOK_STRING ||
            token.type == BTOK_SYMBOL)
        {
            count_literal(body, token);
            continue;
        }
        return 0;
    }
}

static int parse_statements_until(BParser *parser, BMethodBody *body, const char *terminator)
{
    BToken token = bp_next(parser);

    while (1)
    {
        if (token.type == BTOK_EOF)
        {
            return terminator == NULL;
        }

        if (terminator != NULL && token.type == BTOK_SPECIAL && strcmp(token.text, terminator) == 0)
        {
            return 1;
        }

        if (token.type == BTOK_SPECIAL && strcmp(token.text, ".") == 0)
        {
            token = bp_next(parser);
            continue;
        }

        if (token.type == BTOK_SPECIAL && strcmp(token.text, "^") == 0)
        {
            body->return_count++;
            token = bp_next(parser);
            if (token.type == BTOK_EOF)
            {
                return 0;
            }
        }

        if (token.type == BTOK_IDENTIFIER)
        {
            BToken maybe_assign = bp_next(parser);
            if (maybe_assign.type == BTOK_SPECIAL && strcmp(maybe_assign.text, ":=") == 0)
            {
                body->assignment_count++;
                BToken assigned_value_start = bp_next(parser);
                if (assigned_value_start.type == BTOK_EOF)
                {
                    return 0;
                }
                if (!parse_expression_from_first(parser, body, assigned_value_start))
                {
                    return 0;
                }
            }
            else
            {
                bp_unread(parser, maybe_assign);
                if (!parse_expression_from_first(parser, body, token))
                {
                    return 0;
                }
            }
        }
        else
        {
            if (!parse_expression_from_first(parser, body, token))
            {
                return 0;
            }
        }

        token = bp_next(parser);
    }
}

static int parse_block(BParser *parser, BMethodBody *body)
{
    BToken token = bp_next(parser);

    if (token.type == BTOK_SPECIAL && strcmp(token.text, ":") == 0)
    {
        while (1)
        {
            BToken arg = bp_next(parser);
            if (arg.type != BTOK_IDENTIFIER)
            {
                return 0;
            }

            BToken next = bp_next(parser);
            if (next.type == BTOK_SPECIAL && strcmp(next.text, ":") == 0)
            {
                continue;
            }
            if (next.type == BTOK_SPECIAL && strcmp(next.text, "|") == 0)
            {
                break;
            }
            return 0;
        }
    }
    else
    {
        bp_unread(parser, token);
    }

    return parse_statements_until(parser, body, "]");
}

static int parse_primary(BParser *parser, BMethodBody *body)
{
    BToken token = bp_next(parser);
    if (token.type == BTOK_EOF)
    {
        return 0;
    }
    if (token.type == BTOK_SPECIAL && (strcmp(token.text, ".") == 0 || strcmp(token.text, ")") == 0 ||
                                        strcmp(token.text, "]") == 0))
    {
        bp_unread(parser, token);
        return 0;
    }

    count_literal(body, token);

    if (token.type == BTOK_SPECIAL && strcmp(token.text, "(") == 0)
    {
        BToken first = bp_next(parser);
        if (first.type == BTOK_EOF)
        {
            return 0;
        }
        if (!parse_expression_from_first(parser, body, first))
        {
            return 0;
        }
        BToken close = bp_next(parser);
        if (close.type != BTOK_SPECIAL || strcmp(close.text, ")") != 0)
        {
            return 0;
        }
    }
    else if (token.type == BTOK_SPECIAL && strcmp(token.text, "[") == 0)
    {
        if (!parse_block(parser, body))
        {
            return 0;
        }
    }
    else if (token.type == BTOK_SPECIAL && strcmp(token.text, "#(") == 0)
    {
        if (!parse_literal_array(parser, body))
        {
            return 0;
        }
    }
    return 1;
}

static int parse_expression_from_first(BParser *parser, BMethodBody *body, BToken first)
{
    if (first.type == BTOK_SPECIAL && strcmp(first.text, "[") == 0)
    {
        if (!parse_block(parser, body))
        {
            return 0;
        }
    }
    else if (first.type == BTOK_SPECIAL && strcmp(first.text, "#(") == 0)
    {
        if (!parse_literal_array(parser, body))
        {
            return 0;
        }
    }
    else if (first.type == BTOK_SPECIAL && strcmp(first.text, "(") == 0)
    {
        bp_unread(parser, first);
        if (!parse_primary(parser, body))
        {
            return 0;
        }
    }

    count_literal(body, first);

    while (1)
    {
        BToken token = bp_next(parser);

        if (token.type == BTOK_EOF)
        {
            return 1;
        }
        if (token.type == BTOK_SPECIAL &&
            (strcmp(token.text, ".") == 0 || strcmp(token.text, ")") == 0 || strcmp(token.text, "]") == 0))
        {
            bp_unread(parser, token);
            return 1;
        }

        if (token.type == BTOK_SPECIAL && strcmp(token.text, ";") == 0)
        {
            continue;
        }

        if (token.type == BTOK_IDENTIFIER)
        {
            body->message_send_count++;
            continue;
        }

        if (token.type == BTOK_SPECIAL && is_binary_selector_char(token.text[0]) && token.text[1] == '\0')
        {
            body->message_send_count++;
            if (!parse_primary(parser, body))
            {
                return 0;
            }
            continue;
        }

        if (token.type == BTOK_KEYWORD)
        {
            body->message_send_count++;
            if (!parse_primary(parser, body))
            {
                return 0;
            }

            while (1)
            {
                BToken maybe_next_keyword = bp_next(parser);
                if (maybe_next_keyword.type != BTOK_KEYWORD)
                {
                    bp_unread(parser, maybe_next_keyword);
                    break;
                }
                if (!parse_primary(parser, body))
                {
                    return 0;
                }
            }
            continue;
        }

        return 0;
    }
}

int bc_parse_method_body(const char *source, BMethodBody *body)
{
    BParser parser;
    memset(body, 0, sizeof(*body));
    bp_init(&parser, source);

    BToken token = bp_next(&parser);

    if (token.type == BTOK_SPECIAL && strcmp(token.text, "|") == 0)
    {
        while (1)
        {
            BToken temp = bp_next(&parser);
            if (temp.type == BTOK_SPECIAL && strcmp(temp.text, "|") == 0)
            {
                token = bp_next(&parser);
                break;
            }
            if (temp.type != BTOK_IDENTIFIER || body->temp_count >= 16)
            {
                return 0;
            }

            strncpy(body->temp_names[body->temp_count], temp.text,
                    sizeof(body->temp_names[body->temp_count]) - 1);
            body->temp_count++;
        }
    }

    bp_unread(&parser, token);
    return parse_statements_until(&parser, body, NULL);
}

int bc_codegen_method_body(const char *source, BCompiledBody *compiled)
{
    enum
    {
        BC_PUSH_LITERAL = 0,
        BC_PUSH_SELF = 3,
        BC_RETURN = 7
    };

    BTokenizer tokenizer;
    memset(compiled, 0, sizeof(*compiled));
    bt_init(&tokenizer, source);

    BToken ret = bt_next(&tokenizer);
    if (ret.type != BTOK_SPECIAL || strcmp(ret.text, "^") != 0)
    {
        return 0;
    }

    BToken value = bt_next(&tokenizer);
    BToken eof = bt_next(&tokenizer);
    if (eof.type != BTOK_EOF)
    {
        return 0;
    }

    if (value.type == BTOK_IDENTIFIER && strcmp(value.text, "self") == 0)
    {
        compiled->bytecodes[compiled->bytecode_count++] = BC_PUSH_SELF;
        compiled->bytecodes[compiled->bytecode_count++] = BC_RETURN;
        return 1;
    }

    if (value.type == BTOK_INTEGER || value.type == BTOK_CHARACTER || value.type == BTOK_STRING ||
        value.type == BTOK_SYMBOL)
    {
        compiled->literals[0] = value;
        compiled->literal_count = 1;
        compiled->bytecodes[compiled->bytecode_count++] = BC_PUSH_LITERAL;
        compiled->bytecodes[compiled->bytecode_count++] = 0;
        compiled->bytecodes[compiled->bytecode_count++] = BC_RETURN;
        return 1;
    }

    return 0;
}

int bc_parse_method_header(const char *source, BMethodHeader *header)
{
    BTokenizer tokenizer;
    bt_init(&tokenizer, source);

    memset(header, 0, sizeof(*header));

    BToken first = bt_next(&tokenizer);
    if (first.type == BTOK_EOF)
    {
        return 0;
    }

    if (first.type == BTOK_IDENTIFIER)
    {
        BToken maybe_eof = bt_next(&tokenizer);
        if (maybe_eof.type != BTOK_EOF)
        {
            return 0;
        }
        header->kind = BMETHOD_UNARY;
        strncpy(header->selector, first.text, sizeof(header->selector) - 1);
        return 1;
    }

    if (first.type == BTOK_SPECIAL && is_binary_selector_char(first.text[0]))
    {
        BToken arg = bt_next(&tokenizer);
        if (arg.type != BTOK_IDENTIFIER)
        {
            return 0;
        }
        BToken maybe_eof = bt_next(&tokenizer);
        if (maybe_eof.type != BTOK_EOF)
        {
            return 0;
        }
        header->kind = BMETHOD_BINARY;
        strncpy(header->selector, first.text, sizeof(header->selector) - 1);
        header->arg_count = 1;
        strncpy(header->arg_names[0], arg.text, sizeof(header->arg_names[0]) - 1);
        return 1;
    }

    if (first.type == BTOK_KEYWORD)
    {
        header->kind = BMETHOD_KEYWORD;
        header->selector[0] = '\0';

        BToken current = first;
        while (1)
        {
            if (header->arg_count >= 8)
            {
                return 0;
            }

            strncat(header->selector, current.text,
                    sizeof(header->selector) - strlen(header->selector) - 1);

            BToken arg = bt_next(&tokenizer);
            if (arg.type != BTOK_IDENTIFIER)
            {
                return 0;
            }
            strncpy(header->arg_names[header->arg_count], arg.text,
                    sizeof(header->arg_names[header->arg_count]) - 1);
            header->arg_count++;

            BToken next = bt_next(&tokenizer);
            if (next.type == BTOK_EOF)
            {
                break;
            }
            if (next.type != BTOK_KEYWORD)
            {
                return 0;
            }
            current = next;
        }
        return 1;
    }

    return 0;
}
