#include "bootstrap_compiler.h"

#include <ctype.h>
#include <stdio.h>
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

enum
{
    BC_CG_PUSH_LITERAL = 0,
    BC_CG_PUSH_INST_VAR = 1,
    BC_CG_PUSH_TEMP = 2,
    BC_CG_PUSH_SELF = 3,
    BC_CG_STORE_INST_VAR = 4,
    BC_CG_STORE_TEMP = 5,
    BC_CG_SEND_MESSAGE = 6,
    BC_CG_RETURN = 7,
    BC_CG_PUSH_CLOSURE = 14,
    BC_CG_POP = 11
};

typedef struct
{
    BParser parser;
    BMethodBody body;
    BCompiledBody *compiled;
    int saw_return;
} CgState;

static void cg_emit_byte(CgState *state, uint8_t value)
{
    state->compiled->bytecodes[state->compiled->bytecode_count++] = value;
}

static void cg_emit_u32(CgState *state, uint32_t value)
{
    cg_emit_byte(state, (uint8_t)(value & 0xFF));
    cg_emit_byte(state, (uint8_t)((value >> 8) & 0xFF));
    cg_emit_byte(state, (uint8_t)((value >> 16) & 0xFF));
    cg_emit_byte(state, (uint8_t)((value >> 24) & 0xFF));
}

static int cg_literal_index(CgState *state, BToken literal)
{
    for (int index = 0; index < state->compiled->literal_count; index++)
    {
        BToken existing = state->compiled->literals[index];
        if (existing.type == literal.type && existing.int_value == literal.int_value &&
            strcmp(existing.text, literal.text) == 0)
        {
            return index;
        }
    }

    int index = state->compiled->literal_count;
    state->compiled->literals[index] = literal;
    state->compiled->literal_count++;
    return index;
}

static int cg_emit_selector_send(CgState *state, const char *selector, uint32_t argc)
{
    BToken selector_token = make_token(BTOK_SYMBOL);
    strncpy(selector_token.text, selector, sizeof(selector_token.text) - 1);
    int selector_index = cg_literal_index(state, selector_token);
    cg_emit_byte(state, BC_CG_SEND_MESSAGE);
    cg_emit_u32(state, (uint32_t)selector_index);
    cg_emit_u32(state, argc);
    return 1;
}

static int cg_temp_index(BMethodBody *body, const char *name)
{
    for (int index = 0; index < body->temp_count; index++)
    {
        if (strcmp(body->temp_names[index], name) == 0)
        {
            return index;
        }
    }
    return -1;
}

static int cg_inst_var_index(CgState *state, const char *name)
{
    for (int index = 0; index < state->compiled->inst_var_count; index++)
    {
        if (strcmp(state->compiled->inst_var_names[index], name) == 0)
        {
            return index;
        }
    }

    if (state->compiled->inst_var_count >= 16)
    {
        return -1;
    }

    int index = state->compiled->inst_var_count;
    strncpy(state->compiled->inst_var_names[index], name,
            sizeof(state->compiled->inst_var_names[index]) - 1);
    state->compiled->inst_var_count++;
    return index;
}

static int cg_parse_expression(CgState *state);

static int cg_trim_bounds(const char *text, uint64_t *start, uint64_t *end)
{
    while (*start < *end && isspace((unsigned char)text[*start]))
    {
        (*start)++;
    }
    while (*end > *start && isspace((unsigned char)text[*end - 1]))
    {
        (*end)--;
    }
    return *end >= *start;
}

static int cg_build_implicit_return_source(const char *raw, char *out, size_t cap)
{
    uint64_t start = 0;
    uint64_t end = (uint64_t)strlen(raw);
    if (!cg_trim_bounds(raw, &start, &end))
    {
        return 0;
    }
    if (start == end)
    {
        return 0;
    }

    uint64_t insertion = start;
    if (raw[insertion] == '|')
    {
        uint64_t index = insertion + 1;
        while (index < end && raw[index] != '|')
        {
            index++;
        }
        if (index >= end)
        {
            return 0;
        }
        insertion = index + 1;
    }

    int written = snprintf(out, cap, "%.*s ^ %.*s",
                           (int)insertion, raw,
                           (int)(end - insertion), raw + insertion);
    return written > 0 && (size_t)written < cap;
}

static int cg_skip_block_literal(BParser *parser)
{
    int depth = 1;
    while (depth > 0)
    {
        BToken token = bp_next(parser);
        if (token.type == BTOK_EOF)
        {
            return 0;
        }
        if (token.type == BTOK_SPECIAL && strcmp(token.text, "[") == 0)
        {
            depth++;
            continue;
        }
        if (token.type == BTOK_SPECIAL && strcmp(token.text, "]") == 0)
        {
            depth--;
            continue;
        }
    }
    return 1;
}

static int cg_compile_and_store_block(CgState *state, const char *raw_source, int block_index)
{
    BCompiledBody compiled_block;
    char implicit_source[1024];

    if (!bc_codegen_method_body(raw_source, &compiled_block))
    {
        if (!cg_build_implicit_return_source(raw_source, implicit_source, sizeof(implicit_source)))
        {
            return 0;
        }
        if (!bc_codegen_method_body(implicit_source, &compiled_block))
        {
            return 0;
        }
    }

    state->compiled->blocks[block_index].bytecode_count = compiled_block.bytecode_count;
    memcpy(state->compiled->blocks[block_index].bytecodes, compiled_block.bytecodes,
           sizeof(state->compiled->blocks[block_index].bytecodes));
    state->compiled->blocks[block_index].literal_count = compiled_block.literal_count;
    memcpy(state->compiled->blocks[block_index].literals, compiled_block.literals,
           sizeof(state->compiled->blocks[block_index].literals));

    return 1;
}

static int cg_parse_primary(CgState *state)
{
    BToken token = bp_next(&state->parser);

    if (token.type == BTOK_IDENTIFIER)
    {
        if (strcmp(token.text, "self") == 0)
        {
            cg_emit_byte(state, BC_CG_PUSH_SELF);
            return 1;
        }

        int index = cg_temp_index(&state->body, token.text);
        if (index >= 0)
        {
            cg_emit_byte(state, BC_CG_PUSH_TEMP);
            cg_emit_u32(state, (uint32_t)index);
            return 1;
        }

        index = cg_inst_var_index(state, token.text);
        if (index >= 0)
        {
            cg_emit_byte(state, BC_CG_PUSH_INST_VAR);
            cg_emit_u32(state, (uint32_t)index);
            return 1;
        }

        return 0;
    }

    if (token.type == BTOK_INTEGER || token.type == BTOK_CHARACTER || token.type == BTOK_STRING ||
        token.type == BTOK_SYMBOL)
    {
        int index = cg_literal_index(state, token);
        cg_emit_byte(state, BC_CG_PUSH_LITERAL);
        cg_emit_u32(state, (uint32_t)index);
        return 1;
    }

    if (token.type == BTOK_SPECIAL && strcmp(token.text, "(") == 0)
    {
        if (!cg_parse_expression(state))
        {
            return 0;
        }
        BToken close = bp_next(&state->parser);
        return close.type == BTOK_SPECIAL && strcmp(close.text, ")") == 0;
    }

    if (token.type == BTOK_SPECIAL && strcmp(token.text, "[") == 0)
    {
        char block_source[1024];
        uint64_t start = state->parser.tokenizer.index;

        if (!cg_skip_block_literal(&state->parser))
        {
            return 0;
        }

        if (state->compiled->block_count >= 16)
        {
            return 0;
        }

        uint64_t end = state->parser.tokenizer.index;
        if (end == 0 || end <= start)
        {
            return 0;
        }
        uint64_t close = end - 1;
        uint64_t length = close - start;
        if (length >= sizeof(block_source))
        {
            return 0;
        }
        memcpy(block_source, state->parser.tokenizer.source + start, (size_t)length);
        block_source[length] = '\0';

        int block_index = state->compiled->block_count;
        if (!cg_compile_and_store_block(state, block_source, block_index))
        {
            return 0;
        }
        state->compiled->block_count++;

        BToken block_literal = make_token(BTOK_SYMBOL);
        snprintf(block_literal.text, sizeof(block_literal.text), "__block%d", block_index);
        int index = cg_literal_index(state, block_literal);
        cg_emit_byte(state, BC_CG_PUSH_CLOSURE);
        cg_emit_u32(state, (uint32_t)index);
        return 1;
    }

    return 0;
}

static int cg_parse_expression(CgState *state)
{
    if (!cg_parse_primary(state))
    {
        return 0;
    }

    while (1)
    {
        BToken token = bp_next(&state->parser);
        if (token.type == BTOK_EOF)
        {
            bp_unread(&state->parser, token);
            return 1;
        }

        if (token.type == BTOK_SPECIAL &&
            (strcmp(token.text, ".") == 0 || strcmp(token.text, ")") == 0 || strcmp(token.text, "]") == 0))
        {
            bp_unread(&state->parser, token);
            return 1;
        }

        if (token.type == BTOK_SPECIAL && strcmp(token.text, ";") == 0)
        {
            continue;
        }

        if (token.type == BTOK_IDENTIFIER)
        {
            if (!cg_emit_selector_send(state, token.text, 0))
            {
                return 0;
            }
            continue;
        }

        if (token.type == BTOK_SPECIAL && is_binary_selector_char(token.text[0]) && token.text[1] == '\0')
        {
            if (!cg_parse_primary(state))
            {
                return 0;
            }
            if (!cg_emit_selector_send(state, token.text, 1))
            {
                return 0;
            }
            continue;
        }

        if (token.type == BTOK_KEYWORD)
        {
            char selector[128];
            selector[0] = '\0';
            uint32_t argc = 0;
            BToken current = token;

            while (1)
            {
                strncat(selector, current.text, sizeof(selector) - strlen(selector) - 1);
                if (!cg_parse_primary(state))
                {
                    return 0;
                }
                argc++;
                BToken maybe_next = bp_next(&state->parser);
                if (maybe_next.type != BTOK_KEYWORD)
                {
                    bp_unread(&state->parser, maybe_next);
                    break;
                }
                current = maybe_next;
            }

            if (!cg_emit_selector_send(state, selector, argc))
            {
                return 0;
            }
            continue;
        }

        return 0;
    }
}

static int cg_parse_temp_decls(CgState *state)
{
    BToken token = bp_next(&state->parser);
    if (token.type == BTOK_SPECIAL && strcmp(token.text, "|") == 0)
    {
        while (1)
        {
            BToken temp = bp_next(&state->parser);
            if (temp.type == BTOK_SPECIAL && strcmp(temp.text, "|") == 0)
            {
                return 1;
            }
            if (temp.type != BTOK_IDENTIFIER || state->body.temp_count >= 16)
            {
                return 0;
            }
            strncpy(state->body.temp_names[state->body.temp_count], temp.text,
                    sizeof(state->body.temp_names[state->body.temp_count]) - 1);
            state->body.temp_count++;
        }
    }

    bp_unread(&state->parser, token);
    return 1;
}

static int cg_parse_statements(CgState *state)
{
    while (1)
    {
        BToken token = bp_next(&state->parser);
        if (token.type == BTOK_EOF)
        {
            return 1;
        }
        if (token.type == BTOK_SPECIAL && strcmp(token.text, ".") == 0)
        {
            continue;
        }
        if (token.type == BTOK_SPECIAL && strcmp(token.text, "^") == 0)
        {
            if (!cg_parse_expression(state))
            {
                return 0;
            }
            cg_emit_byte(state, BC_CG_RETURN);
            state->saw_return = 1;

            BToken eof = bp_next(&state->parser);
            return eof.type == BTOK_EOF;
        }

        if (token.type == BTOK_IDENTIFIER)
        {
            BToken maybe_assign = bp_next(&state->parser);
            if (maybe_assign.type == BTOK_SPECIAL && strcmp(maybe_assign.text, ":=") == 0)
            {
                int index = cg_temp_index(&state->body, token.text);
                if (index >= 0)
                {
                    if (!cg_parse_expression(state))
                    {
                        return 0;
                    }
                    cg_emit_byte(state, BC_CG_STORE_TEMP);
                    cg_emit_u32(state, (uint32_t)index);
                }
                else
                {
                    index = cg_inst_var_index(state, token.text);
                    if (index < 0)
                    {
                        return 0;
                    }
                    if (!cg_parse_expression(state))
                    {
                        return 0;
                    }
                    cg_emit_byte(state, BC_CG_STORE_INST_VAR);
                    cg_emit_u32(state, (uint32_t)index);
                }

                BToken separator = bp_next(&state->parser);
                if (separator.type == BTOK_EOF)
                {
                    return 1;
                }
                if (separator.type != BTOK_SPECIAL || strcmp(separator.text, ".") != 0)
                {
                    return 0;
                }
                continue;
            }
            bp_unread(&state->parser, maybe_assign);
        }

        bp_unread(&state->parser, token);
        if (!cg_parse_expression(state))
        {
            return 0;
        }
        cg_emit_byte(state, BC_CG_POP);

        BToken separator = bp_next(&state->parser);
        if (separator.type == BTOK_EOF)
        {
            return 1;
        }
        if (separator.type != BTOK_SPECIAL || strcmp(separator.text, ".") != 0)
        {
            return 0;
        }
    }
}

int bc_codegen_method_body(const char *source, BCompiledBody *compiled)
{
    CgState state;
    memset(compiled, 0, sizeof(*compiled));
    memset(&state, 0, sizeof(state));
    state.compiled = compiled;
    bp_init(&state.parser, source);

    if (!cg_parse_temp_decls(&state))
    {
        return 0;
    }

    if (!cg_parse_statements(&state))
    {
        return 0;
    }

    return state.saw_return;
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
