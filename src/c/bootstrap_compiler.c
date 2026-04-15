#include "bootstrap_compiler.h"
#include "primitives.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

extern uint64_t *om_alloc(uint64_t *free_ptr_var, uint64_t class_ptr, uint64_t format, uint64_t size);
extern uint64_t tag_smallint(int64_t value);
extern uint64_t tag_character(uint64_t code_point);
extern uint64_t tagged_nil(void);
extern uint64_t tagged_true(void);
extern uint64_t tagged_false(void);

enum
{
    BC_FORMAT_INDEXABLE = 1,
    BC_FORMAT_BYTES = 2,
    BC_CLASS_SUPERCLASS = 0,
    BC_CLASS_METHOD_DICT = 1,
    BC_CLASS_INST_SIZE = 2,
    BC_CLASS_INST_FORMAT = 3,
    BC_CLASS_INST_VARS = 4,
    BC_CM_PRIMITIVE = 0,
    BC_CM_NUM_ARGS = 1,
    BC_CM_NUM_TEMPS = 2,
    BC_CM_LITERALS = 3,
    BC_CM_BYTECODES = 4,
    BC_ASSOC_KEY = 0,
    BC_ASSOC_VALUE = 1,
    BC_DICT_ASSOCIATIONS = 0,
    BC_DICT_TALLY = 1
};

#define BC_OBJ_FIELD(obj, n) ((obj)[3 + (n)])
#define BC_OBJ_SIZE(obj) ((obj)[2])
#define BC_OBJ_CLASS(obj) ((obj)[0])

static const BClassBinding *bc_find_class_binding(const BClassBinding *classes, int class_count,
                                                  const char *class_name);
static uint64_t *bc_lookup_smalltalk_global(const char *name);
static uint64_t *bc_lookup_class_named(const BClassBinding *classes, int class_count, const char *class_name);
static const BClassBinding *bc_active_class_bindings = NULL;
static int bc_active_class_count = 0;

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
        if (isalnum((unsigned char)peek(tokenizer)) || peek(tokenizer) == '_' || peek(tokenizer) == ':')
        {
            while (isalnum((unsigned char)peek(tokenizer)) || peek(tokenizer) == '_' || peek(tokenizer) == ':')
            {
                char c = advance(tokenizer);
                if (text_index < (int)sizeof(token.text) - 1)
                {
                    token.text[text_index++] = c;
                }
            }
        }
        else
        {
            while (is_binary_selector_char(peek(tokenizer)))
            {
                char c = advance(tokenizer);
                if (text_index < (int)sizeof(token.text) - 1)
                {
                    token.text[text_index++] = c;
                }
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
    BC_CG_PUSH_THIS_CONTEXT = 17,
    BC_CG_PUSH_ARG = 15,
    BC_CG_STORE_INST_VAR = 4,
    BC_CG_STORE_TEMP = 5,
    BC_CG_SEND_MESSAGE = 6,
    BC_CG_RETURN = 7,
    BC_CG_RETURN_NON_LOCAL = 16,
    BC_CG_PUSH_GLOBAL = 18,
    BC_CG_PUSH_CLOSURE = 14,
    BC_CG_POP = 11
};

struct CgState_;
typedef struct CgState_
{
    BParser parser;
    BMethodBody body;
    BCompiledBody *compiled;
    const BMethodHeader *header;
    int saw_return;
    int in_block;
    uint64_t *target_class;
    struct CgState_ *outer_state; // outer scope's state (NULL for top-level methods)
    int local_arg_count;          // args owned by THIS scope (not enclosing)
    int closure_offset;           // slot offset where this scope's own args/temps begin
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
    BToken selector_token = make_token(BTOK_SELECTOR);
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

// Slot offset for this scope's own temps within its frame. For top-level
// methods, temps live at offset 0 in the separate temp slot space. For
// blocks, temps live AFTER all copied outer args/temps in the unified
// closure slot space, plus the block's own args.
static int cg_local_temp_slot(const CgState *state, int temp_index)
{
    if (!state->in_block)
    {
        return temp_index;
    }
    return state->closure_offset + state->local_arg_count + temp_index;
}

// Look up an identifier in any enclosing scope's temps (walking up the chain).
// Returns the slot index in the current block frame, or -1.
static int cg_outer_temp_slot(CgState *state, const char *name)
{
    CgState *outer = state->outer_state;
    while (outer != NULL)
    {
        for (int index = 0; index < outer->body.temp_count; index++)
        {
            if (strcmp(outer->body.temp_names[index], name) == 0)
            {
                return outer->closure_offset + outer->local_arg_count + index;
            }
        }
        outer = outer->outer_state;
    }
    return -1;
}

// Look up an identifier in any enclosing scope's args (walking up the chain).
// Returns the slot index in the current block frame, or -1.
static int cg_outer_arg_slot(CgState *state, const char *name)
{
    CgState *outer = state->outer_state;
    while (outer != NULL)
    {
        if (outer->header != NULL)
        {
            for (int arg_index = 0; arg_index < outer->local_arg_count; arg_index++)
            {
                int header_index = outer->header->arg_count - outer->local_arg_count + arg_index;
                if (strcmp(outer->header->arg_names[header_index], name) == 0)
                {
                    return outer->closure_offset + arg_index;
                }
            }
        }
        outer = outer->outer_state;
    }
    return -1;
}

static int cg_arg_index(const BMethodHeader *header, const char *name)
{
    if (header == NULL)
    {
        return -1;
    }

    for (int index = 0; index < header->arg_count; index++)
    {
        if (strcmp(header->arg_names[index], name) == 0)
        {
            return index;
        }
    }

    return -1;
}

static int bc_name_matches_string_oop(uint64_t oop, const char *name)
{
    if ((oop & 3) != 0 || oop == 0)
    {
        return 0;
    }

    uint64_t *string = (uint64_t *)oop;
    size_t len = strlen(name);
    if (BC_OBJ_SIZE(string) != len)
    {
        return 0;
    }

    return memcmp((const void *)&BC_OBJ_FIELD(string, 0), name, len) == 0;
}

static int bc_class_total_inst_var_count(uint64_t *klass)
{
    if (klass == NULL || ((uint64_t)klass & 3) != 0)
    {
        return 0;
    }

    int total = 0;
    uint64_t super_oop = BC_OBJ_FIELD(klass, BC_CLASS_SUPERCLASS);
    if ((super_oop & 3) == 0 && super_oop != 0)
    {
        total += bc_class_total_inst_var_count((uint64_t *)super_oop);
    }

    if (BC_OBJ_SIZE(klass) > BC_CLASS_INST_VARS)
    {
        uint64_t ivars_oop = BC_OBJ_FIELD(klass, BC_CLASS_INST_VARS);
        if ((ivars_oop & 3) == 0 && ivars_oop != 0)
        {
            total += (int)BC_OBJ_SIZE((uint64_t *)ivars_oop);
        }
    }

    return total;
}

static int bc_class_inst_var_index(uint64_t *klass, const char *name)
{
    if (klass == NULL || ((uint64_t)klass & 3) != 0)
    {
        return -1;
    }

    int inherited_count = 0;
    uint64_t super_oop = BC_OBJ_FIELD(klass, BC_CLASS_SUPERCLASS);
    if ((super_oop & 3) == 0 && super_oop != 0)
    {
        int super_index = bc_class_inst_var_index((uint64_t *)super_oop, name);
        if (super_index >= 0)
        {
            return super_index;
        }
        inherited_count = bc_class_total_inst_var_count((uint64_t *)super_oop);
    }

    if (BC_OBJ_SIZE(klass) <= BC_CLASS_INST_VARS)
    {
        return -1;
    }

    uint64_t ivars_oop = BC_OBJ_FIELD(klass, BC_CLASS_INST_VARS);
    if ((ivars_oop & 3) != 0 || ivars_oop == 0)
    {
        return -1;
    }

    uint64_t *ivars = (uint64_t *)ivars_oop;
    for (uint64_t index = 0; index < BC_OBJ_SIZE(ivars); index++)
    {
        if (bc_name_matches_string_oop(BC_OBJ_FIELD(ivars, index), name))
        {
            return inherited_count + (int)index;
        }
    }

    return -1;
}

static int cg_inst_var_index(CgState *state, const char *name)
{
    if (state->target_class != NULL)
    {
        return bc_class_inst_var_index(state->target_class, name);
    }

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

static int cg_capture_literal_array(CgState *state, uint64_t start, char *out, size_t cap)
{
    int depth = 1;
    while (depth > 0)
    {
        BToken token = bp_next(&state->parser);
        if (token.type == BTOK_EOF)
        {
            return 0;
        }
        if (token.type == BTOK_SPECIAL && strcmp(token.text, "#(") == 0)
        {
            depth++;
            continue;
        }
        if (token.type == BTOK_SPECIAL && strcmp(token.text, ")") == 0)
        {
            depth--;
            continue;
        }
    }

    uint64_t end = state->parser.tokenizer.index;
    if (end <= start)
    {
        return 0;
    }
    uint64_t length = end - start;
    if (length >= cap)
    {
        return 0;
    }

    memcpy(out, state->parser.tokenizer.source + start, (size_t)length);
    out[length] = '\0';
    return 1;
}

static int bc_codegen_body(const char *source, BCompiledBody *compiled, int in_block,
                           const BMethodHeader *header, uint64_t *target_class);
static int bc_codegen_body_with_outer_state(const char *source, BCompiledBody *compiled, int in_block,
                                            const BMethodHeader *header, uint64_t *target_class,
                                            CgState *outer_state, int closure_offset);

static int cg_compile_and_store_block(CgState *state, const char *raw_source, int block_index)
{
    BCompiledBody compiled_block;
    char implicit_source[4096];

    int outer_arg_count = state->header != NULL ? state->header->arg_count : 0;
    int closure_offset = state->closure_offset + outer_arg_count + state->body.temp_count;

    if (!bc_codegen_body_with_outer_state(raw_source, &compiled_block, 1, state->header,
                                          state->target_class, state, closure_offset))
    {
        if (!cg_build_implicit_return_source(raw_source, implicit_source, sizeof(implicit_source)))
        {
            return 0;
        }
        if (!bc_codegen_body_with_outer_state(implicit_source, &compiled_block, 1, state->header,
                                              state->target_class, state, closure_offset))
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

static int cg_emit_primary_token(CgState *state, BToken token)
{
    if (token.type == BTOK_IDENTIFIER)
    {
        if (strcmp(token.text, "self") == 0)
        {
            cg_emit_byte(state, BC_CG_PUSH_SELF);
            return 1;
        }
        if (strcmp(token.text, "thisContext") == 0)
        {
            cg_emit_byte(state, BC_CG_PUSH_THIS_CONTEXT);
            return 1;
        }
        if (strcmp(token.text, "nil") == 0 || strcmp(token.text, "true") == 0 ||
            strcmp(token.text, "false") == 0)
        {
            int index = cg_literal_index(state, token);
            cg_emit_byte(state, BC_CG_PUSH_LITERAL);
            cg_emit_u32(state, (uint32_t)index);
            return 1;
        }

        int index = cg_temp_index(&state->body, token.text);
        if (index >= 0)
        {
            cg_emit_byte(state, BC_CG_PUSH_TEMP);
            cg_emit_u32(state, (uint32_t)cg_local_temp_slot(state, index));
            return 1;
        }

        index = cg_arg_index(state->header, token.text);
        if (index >= 0)
        {
            cg_emit_byte(state, state->in_block ? BC_CG_PUSH_TEMP : BC_CG_PUSH_ARG);
            // In a block, the same `state->header` is the outer method's header.
            // The block's "args" lookup actually finds the OUTER method's args,
            // which are copied at slot 0..arg_count-1 of the OUTERMOST frame
            // (and accessed via PUSH_TEMP at the same slot offset within nested
            // closures because the chain copies args first).
            int slot = state->in_block ? index : index;
            cg_emit_u32(state, (uint32_t)slot);
            return 1;
        }

        // In a nested block, look up identifiers in any enclosing block's
        // args/temps and emit PUSH_TEMP at the right copied-slot offset.
        index = cg_outer_arg_slot(state, token.text);
        if (index >= 0)
        {
            cg_emit_byte(state, BC_CG_PUSH_TEMP);
            cg_emit_u32(state, (uint32_t)index);
            return 1;
        }
        index = cg_outer_temp_slot(state, token.text);
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

        BToken global_token = make_token(BTOK_CLASS_REF);
        strncpy(global_token.text, token.text, sizeof(global_token.text) - 1);
        int global_index = cg_literal_index(state, global_token);
        cg_emit_byte(state, BC_CG_PUSH_GLOBAL);
        cg_emit_u32(state, (uint32_t)global_index);
        return 1;
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

    if (token.type == BTOK_SPECIAL && strcmp(token.text, "#(") == 0)
    {
        BToken literal_array = make_token(BTOK_LITERAL_ARRAY);
        uint64_t start = state->parser.tokenizer.index >= 2 ? state->parser.tokenizer.index - 2 : 0;
        if (!cg_capture_literal_array(state, start, literal_array.text, sizeof(literal_array.text)))
        {
            return 0;
        }
        int index = cg_literal_index(state, literal_array);
        cg_emit_byte(state, BC_CG_PUSH_LITERAL);
        cg_emit_u32(state, (uint32_t)index);
        return 1;
    }

    if (token.type == BTOK_SPECIAL && strcmp(token.text, "[") == 0)
    {
        char block_source[4096];
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

        BToken block_literal = make_token(BTOK_BLOCK_LITERAL);
        block_literal.int_value = block_index;
        snprintf(block_literal.text, sizeof(block_literal.text), "block:%d", block_index);
        int index = cg_literal_index(state, block_literal);
        cg_emit_byte(state, BC_CG_PUSH_CLOSURE);
        cg_emit_u32(state, (uint32_t)index);
        return 1;
    }

    return 0;
}

static int cg_parse_primary(CgState *state)
{
    BToken token = bp_next(&state->parser);
    return cg_emit_primary_token(state, token);
}

static int cg_parse_unary_expression(CgState *state)
{
    if (!cg_parse_primary(state))
    {
        return 0;
    }

    while (1)
    {
        BToken token = bp_next(&state->parser);
        if (token.type != BTOK_IDENTIFIER)
        {
            bp_unread(&state->parser, token);
            return 1;
        }

        if (!cg_emit_selector_send(state, token.text, 0))
        {
            return 0;
        }
    }
}

static int cg_parse_binary_expression(CgState *state)
{
    if (!cg_parse_unary_expression(state))
    {
        return 0;
    }

    while (1)
    {
        BToken token = bp_next(&state->parser);
        if (!(token.type == BTOK_SPECIAL && is_binary_selector_char(token.text[0]) && token.text[1] == '\0'))
        {
            bp_unread(&state->parser, token);
            return 1;
        }

        char selector[4];
        selector[0] = token.text[0];
        selector[1] = '\0';

        BToken maybe_second = bp_next(&state->parser);
        if (maybe_second.type == BTOK_SPECIAL && is_binary_selector_char(maybe_second.text[0]) &&
            maybe_second.text[1] == '\0')
        {
            selector[1] = maybe_second.text[0];
            selector[2] = '\0';
        }
        else
        {
            bp_unread(&state->parser, maybe_second);
        }

        if (!cg_parse_unary_expression(state))
        {
            return 0;
        }
        if (!cg_emit_selector_send(state, selector, 1))
        {
            return 0;
        }
    }
}

static int cg_parse_expression_continuation(CgState *state)
{
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
            char selector[4];
            selector[0] = token.text[0];
            selector[1] = '\0';

            BToken maybe_second = bp_next(&state->parser);
            if (maybe_second.type == BTOK_SPECIAL && is_binary_selector_char(maybe_second.text[0]) &&
                maybe_second.text[1] == '\0')
            {
                selector[1] = maybe_second.text[0];
                selector[2] = '\0';
            }
            else
            {
                bp_unread(&state->parser, maybe_second);
            }

            if (!cg_parse_unary_expression(state))
            {
                return 0;
            }
            if (!cg_emit_selector_send(state, selector, 1))
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
                if (!cg_parse_binary_expression(state))
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

static int cg_parse_expression(CgState *state)
{
    if (!cg_parse_binary_expression(state))
    {
        return 0;
    }

    return cg_parse_expression_continuation(state);
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
            cg_emit_byte(state, state->in_block ? BC_CG_RETURN_NON_LOCAL : BC_CG_RETURN);
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
                    cg_emit_u32(state, (uint32_t)cg_local_temp_slot(state, index));
                }
                else if ((index = cg_outer_temp_slot(state, token.text)) >= 0)
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

            if (!cg_emit_primary_token(state, token))
            {
                return 0;
            }
            if (!cg_parse_expression_continuation(state))
            {
                return 0;
            }

            BToken separator = bp_next(&state->parser);
            if (separator.type == BTOK_EOF)
            {
                cg_emit_byte(state, BC_CG_RETURN);
                state->saw_return = 1;
                return 1;
            }
            if (separator.type != BTOK_SPECIAL || strcmp(separator.text, ".") != 0)
            {
                return 0;
            }
            cg_emit_byte(state, BC_CG_POP);
            continue;
        }

        bp_unread(&state->parser, token);
        if (!cg_parse_expression(state))
        {
            return 0;
        }

        BToken separator = bp_next(&state->parser);
        if (separator.type == BTOK_EOF)
        {
            cg_emit_byte(state, BC_CG_RETURN);
            state->saw_return = 1;
            return 1;
        }
        if (separator.type != BTOK_SPECIAL || strcmp(separator.text, ".") != 0)
        {
            return 0;
        }
        cg_emit_byte(state, BC_CG_POP);
    }
}

static int bc_codegen_body_with_outer_state(const char *source, BCompiledBody *compiled, int in_block,
                                            const BMethodHeader *header, uint64_t *target_class,
                                            CgState *outer_state, int closure_offset)
{
    CgState state;
    memset(compiled, 0, sizeof(*compiled));
    memset(&state, 0, sizeof(state));
    state.compiled = compiled;
    state.header = header;
    state.in_block = in_block;
    state.target_class = target_class;
    state.outer_state = outer_state;
    state.closure_offset = closure_offset;
    // Top-level methods own their args; blocks have 0 args (no [:x|] support yet).
    state.local_arg_count = (in_block || header == NULL) ? 0 : header->arg_count;
    bp_init(&state.parser, source);

    if (!cg_parse_temp_decls(&state))
    {
        return 0;
    }

    if (!cg_parse_statements(&state))
    {
        return 0;
    }

    compiled->temp_count = state.body.temp_count;

    return state.saw_return;
}

static int bc_codegen_body(const char *source, BCompiledBody *compiled, int in_block,
                           const BMethodHeader *header, uint64_t *target_class)
{
    return bc_codegen_body_with_outer_state(source, compiled, in_block, header, target_class, NULL, 0);
}

int bc_codegen_method_body(const char *source, BCompiledBody *compiled)
{
    return bc_codegen_body(source, compiled, 0, NULL, NULL);
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
        char selector[3] = {0};
        selector[0] = first.text[0];

        BToken second = bt_next(&tokenizer);
        BToken arg = second;
        if (second.type == BTOK_SPECIAL && is_binary_selector_char(second.text[0]))
        {
            selector[1] = second.text[0];
            arg = bt_next(&tokenizer);
        }

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
        strncpy(header->selector, selector, sizeof(header->selector) - 1);
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

static int starts_with(const char *text, uint64_t index, const char *prefix)
{
    uint64_t offset = 0;
    while (prefix[offset] != '\0')
    {
        if (text[index + offset] != prefix[offset])
        {
            return 0;
        }
        offset++;
    }
    return 1;
}

int bc_parse_method_chunks(const char *source, BMethodChunk *chunks, int max_chunks, int *out_count)
{
    uint64_t index = 0;
    char current_class[64] = {0};
    int current_class_side = 0;
    char current_category[64] = {0};
    *out_count = 0;

    while (source[index] != '\0')
    {
        if (source[index] == '!')
        {
            uint64_t segment_start = ++index;
            while (source[index] != '\0' && source[index] != '!')
            {
                index++;
            }
            if (source[index] != '!')
            {
                return 0;
            }
            uint64_t segment_end = index++;

            while (segment_start < segment_end &&
                   (source[segment_start] == ' ' || source[segment_start] == '\t' ||
                    source[segment_start] == '\n' || source[segment_start] == '\r'))
            {
                segment_start++;
            }
            while (segment_end > segment_start &&
                   (source[segment_end - 1] == ' ' || source[segment_end - 1] == '\t' ||
                    source[segment_end - 1] == '\n' || source[segment_end - 1] == '\r'))
            {
                segment_end--;
            }
            if (segment_end <= segment_start)
            {
                continue;
            }

            char segment[4096];
            uint64_t segment_len = segment_end - segment_start;
            if (segment_len >= sizeof(segment))
            {
                return 0;
            }
            memcpy(segment, source + segment_start, (size_t)segment_len);
            segment[segment_len] = '\0';

            if (strstr(segment, "methodsFor:") != NULL)
            {
                uint64_t pos = 0;
                int class_len = 0;
                while ((isalnum((unsigned char)segment[pos]) || segment[pos] == '_') &&
                       class_len < (int)sizeof(current_class) - 1)
                {
                    current_class[class_len++] = segment[pos++];
                }
                current_class[class_len] = '\0';
                while (segment[pos] == ' ' || segment[pos] == '\t')
                {
                    pos++;
                }

                current_class_side = 0;
                if (starts_with(segment, pos, "class"))
                {
                    current_class_side = 1;
                    pos += 5;
                    while (segment[pos] == ' ' || segment[pos] == '\t')
                    {
                        pos++;
                    }
                }
                if (!starts_with(segment, pos, "methodsFor:"))
                {
                    return 0;
                }
                pos += 11;
                while (segment[pos] == ' ' || segment[pos] == '\t')
                {
                    pos++;
                }
                if (segment[pos] != '\'')
                {
                    return 0;
                }
                pos++;

                int category_len = 0;
                while (segment[pos] != '\0' && segment[pos] != '\'' &&
                       category_len < (int)sizeof(current_category) - 1)
                {
                    current_category[category_len++] = segment[pos++];
                }
                current_category[category_len] = '\0';
                if (segment[pos] != '\'')
                {
                    return 0;
                }
            }

            continue;
        }

        if (current_class[0] == '\0')
        {
            index++;
            continue;
        }

        uint64_t method_start = index;
        while (source[index] != '\0' && source[index] != '!')
        {
            index++;
        }
        if (source[index] != '!')
        {
            break;
        }
        uint64_t method_end = index;

        while (method_start < method_end &&
               (source[method_start] == ' ' || source[method_start] == '\t' ||
                source[method_start] == '\n' || source[method_start] == '\r'))
        {
            method_start++;
        }
        while (method_end > method_start &&
               (source[method_end - 1] == ' ' || source[method_end - 1] == '\t' ||
                source[method_end - 1] == '\n' || source[method_end - 1] == '\r'))
        {
            method_end--;
        }
        if (method_end <= method_start)
        {
            // Preserve '!' so the next iteration can parse a header chunk.
            continue;
        }

        // Consume method terminator '!'.
        index++;

        if (*out_count >= max_chunks)
        {
            return 0;
        }
        BMethodChunk *chunk = &chunks[*out_count];
        memset(chunk, 0, sizeof(*chunk));
        strncpy(chunk->class_name, current_class, sizeof(chunk->class_name) - 1);
        chunk->class_side = current_class_side;
        strncpy(chunk->category, current_category, sizeof(chunk->category) - 1);
        uint64_t method_len = method_end - method_start;
        if (method_len >= sizeof(chunk->method_source))
        {
            return 0;
        }
        memcpy(chunk->method_source, source + method_start, (size_t)method_len);
        chunk->method_source[method_len] = '\0';
        (*out_count)++;
    }

    return 1;
}

int bc_compile_method_chunks(const BMethodChunk *chunks, int chunk_count,
                             BCompiledMethodDef *methods, int max_methods, int *out_count)
{
    *out_count = 0;

    for (int index = 0; index < chunk_count; index++)
    {
        if (*out_count >= max_methods)
        {
            return 0;
        }

        const char *method_source = chunks[index].method_source;
        const char *newline = strchr(method_source, '\n');
        char header_source[256];
        const char *body_source = "";

        if (newline != NULL)
        {
            uint64_t header_len = (uint64_t)(newline - method_source);
            if (header_len >= sizeof(header_source))
            {
                return 0;
            }
            memcpy(header_source, method_source, (size_t)header_len);
            header_source[header_len] = '\0';
            body_source = newline + 1;
        }
        else
        {
            uint64_t header_len = strlen(method_source);
            if (header_len >= sizeof(header_source))
            {
                return 0;
            }
            memcpy(header_source, method_source, (size_t)header_len + 1);
        }

        BCompiledMethodDef *method = &methods[*out_count];
        memset(method, 0, sizeof(*method));
        strncpy(method->class_name, chunks[index].class_name, sizeof(method->class_name) - 1);
        method->class_side = chunks[index].class_side;
        method->primitive_index = -1;

        if (!bc_parse_method_header(header_source, &method->header))
        {
            return 0;
        }

        while (*body_source == ' ' || *body_source == '\t' || *body_source == '\n' || *body_source == '\r')
        {
            body_source++;
        }

        int primitive_index = -1;
        if (sscanf(body_source, "<primitive: %d>", &primitive_index) == 1)
        {
            method->primitive_index = primitive_index;

            const char *primitive_end = strchr(body_source, '>');
            if (primitive_end != NULL)
            {
                body_source = primitive_end + 1;
                while (*body_source == ' ' || *body_source == '\t' || *body_source == '\n' || *body_source == '\r')
                {
                    body_source++;
                }
                if (*body_source != '\0' && !bc_codegen_body(body_source, &method->body, 0, &method->header, NULL))
                {
                    return 0;
                }
            }
        }
        else if (!bc_codegen_body(body_source, &method->body, 0, &method->header, NULL))
        {
            return 0;
        }

        (*out_count)++;
    }

    return 1;
}

int bc_compile_source_methods(const char *source,
                              BCompiledMethodDef *methods, int max_methods, int *out_count)
{
    BMethodChunk chunks[128];
    int chunk_count = 0;

    if (max_methods <= 0 || max_methods > (int)(sizeof(chunks) / sizeof(chunks[0])))
    {
        return 0;
    }

    if (!bc_parse_method_chunks(source, chunks, max_methods, &chunk_count))
    {
        return 0;
    }

    return bc_compile_method_chunks(chunks, chunk_count, methods, max_methods, out_count);
}

static const BClassBinding *bc_find_class_binding(const BClassBinding *classes, int class_count,
                                                  const char *class_name)
{
    for (int index = 0; index < class_count; index++)
    {
        if (classes[index].class_name != NULL && classes[index].klass != NULL &&
            strcmp(classes[index].class_name, class_name) == 0)
        {
            return &classes[index];
        }
    }
    return NULL;
}

static uint64_t *bc_lookup_smalltalk_global(const char *name)
{
    if (global_smalltalk_dictionary == NULL || !is_object_ptr((uint64_t)global_smalltalk_dictionary) ||
        name == NULL)
    {
        return NULL;
    }

    uint64_t key_oop = lookup_cstring_symbol(name);
    if (key_oop == tagged_nil())
    {
        return NULL;
    }

    uint64_t associations_oop = BC_OBJ_FIELD(global_smalltalk_dictionary, BC_DICT_ASSOCIATIONS);
    uint64_t tally_oop = BC_OBJ_FIELD(global_smalltalk_dictionary, BC_DICT_TALLY);
    if (!is_object_ptr(associations_oop) || tally_oop == tagged_nil())
    {
        return NULL;
    }

    uint64_t *associations = (uint64_t *)associations_oop;
    uint64_t tally = (uint64_t)untag_smallint(tally_oop);
    for (uint64_t index = 0; index < tally; index++)
    {
        uint64_t association_oop = BC_OBJ_FIELD(associations, index);
        if (!is_object_ptr(association_oop))
        {
            continue;
        }
        uint64_t *association = (uint64_t *)association_oop;
        if (BC_OBJ_FIELD(association, BC_ASSOC_KEY) == key_oop)
        {
            uint64_t value = BC_OBJ_FIELD(association, BC_ASSOC_VALUE);
            return is_object_ptr(value) ? (uint64_t *)value : NULL;
        }
    }

    return NULL;
}

static uint64_t *bc_lookup_class_named(const BClassBinding *classes, int class_count, const char *class_name)
{
    const BClassBinding *binding = bc_find_class_binding(classes, class_count, class_name);
    if (binding != NULL)
    {
        return binding->klass;
    }
    return bc_lookup_smalltalk_global(class_name);
}

static uint64_t *bc_make_byte_string(uint64_t *om, uint64_t *string_class, const char *text)
{
    size_t len = strlen(text);
    uint64_t *string = om_alloc(om, (uint64_t)string_class, BC_FORMAT_BYTES, (uint64_t)len);
    if (string == NULL)
    {
        return NULL;
    }
    if (len > 0)
    {
        memcpy(&BC_OBJ_FIELD(string, 0), text, len);
    }
    return string;
}

static int bc_global_association_oop(uint64_t *om, uint64_t array_class, uint64_t association_class,
                                     const char *name, uint64_t value, uint64_t *out_oop)
{
    if (global_smalltalk_dictionary == NULL || !is_object_ptr((uint64_t)global_smalltalk_dictionary) ||
        array_class == 0 || association_class == 0)
    {
        return 0;
    }

    uint64_t key_oop = intern_cstring_symbol(om, name);
    if (key_oop == tagged_nil())
    {
        return 0;
    }

    uint64_t associations_oop = BC_OBJ_FIELD(global_smalltalk_dictionary, BC_DICT_ASSOCIATIONS);
    uint64_t tally_oop = BC_OBJ_FIELD(global_smalltalk_dictionary, BC_DICT_TALLY);
    uint64_t tally = tally_oop == tagged_nil() ? 0 : (uint64_t)untag_smallint(tally_oop);

    if (associations_oop == tagged_nil())
    {
        uint64_t *new_associations = om_alloc(om, array_class, FORMAT_INDEXABLE, 8);
        if (new_associations == NULL)
        {
            return 0;
        }
        for (uint64_t index = 0; index < 8; index++)
        {
            BC_OBJ_FIELD(new_associations, index) = tagged_nil();
        }
        BC_OBJ_FIELD(global_smalltalk_dictionary, BC_DICT_ASSOCIATIONS) = (uint64_t)new_associations;
        BC_OBJ_FIELD(global_smalltalk_dictionary, BC_DICT_TALLY) = tag_smallint(0);
        associations_oop = (uint64_t)new_associations;
    }

    if (!is_object_ptr(associations_oop))
    {
        return 0;
    }
    uint64_t *associations = (uint64_t *)associations_oop;

    for (uint64_t index = 0; index < tally; index++)
    {
        uint64_t association_oop = BC_OBJ_FIELD(associations, index);
        if (!is_object_ptr(association_oop))
        {
            continue;
        }

        uint64_t *association = (uint64_t *)association_oop;
        if (BC_OBJ_FIELD(association, BC_ASSOC_KEY) == key_oop)
        {
            BC_OBJ_FIELD(association, BC_ASSOC_VALUE) = value;
            *out_oop = association_oop;
            return 1;
        }
    }

    if (tally >= BC_OBJ_SIZE(associations))
    {
        uint64_t new_size = BC_OBJ_SIZE(associations) == 0 ? 8 : BC_OBJ_SIZE(associations) * 2;
        uint64_t *grown = om_alloc(om, array_class, FORMAT_INDEXABLE, new_size);
        if (grown == NULL)
        {
            return 0;
        }
        for (uint64_t index = 0; index < new_size; index++)
        {
            BC_OBJ_FIELD(grown, index) = index < BC_OBJ_SIZE(associations)
                                             ? BC_OBJ_FIELD(associations, index)
                                             : tagged_nil();
        }
        BC_OBJ_FIELD(global_smalltalk_dictionary, BC_DICT_ASSOCIATIONS) = (uint64_t)grown;
        associations = grown;
    }

    uint64_t *association = om_alloc(om, association_class, FORMAT_FIELDS, 2);
    if (association == NULL)
    {
        return 0;
    }
    BC_OBJ_FIELD(association, BC_ASSOC_KEY) = key_oop;
    BC_OBJ_FIELD(association, BC_ASSOC_VALUE) = value;
    BC_OBJ_FIELD(associations, tally) = (uint64_t)association;
    BC_OBJ_FIELD(global_smalltalk_dictionary, BC_DICT_TALLY) = tag_smallint((int64_t)(tally + 1));

    *out_oop = (uint64_t)association;
    return 1;
}

static int bc_literal_token_to_oop(uint64_t *om, uint64_t *string_class, uint64_t *array_class,
                                   uint64_t *association_class, const BToken *token, uint64_t *out_oop);

static int bc_materialize_literal_array_oop(uint64_t *om, uint64_t *string_class, uint64_t *array_class,
                                            uint64_t *association_class, const char *source, uint64_t *out_oop)
{
    BTokenizer tokenizer;
    bt_init(&tokenizer, source);

    BToken open = bt_next(&tokenizer);
    if (open.type != BTOK_SPECIAL || strcmp(open.text, "#(") != 0)
    {
        return 0;
    }

    BToken items[16];
    int item_count = 0;
    while (1)
    {
        BToken token = bt_next(&tokenizer);
        if (token.type == BTOK_EOF)
        {
            return 0;
        }
        if (token.type == BTOK_SPECIAL && strcmp(token.text, ")") == 0)
        {
            break;
        }
        if (item_count >= 16)
        {
            return 0;
        }

        if (token.type == BTOK_SPECIAL && strcmp(token.text, "#(") == 0)
        {
            uint64_t nested_start = tokenizer.index >= 2 ? tokenizer.index - 2 : 0;
            int depth = 1;
            while (depth > 0)
            {
                BToken nested = bt_next(&tokenizer);
                if (nested.type == BTOK_EOF)
                {
                    return 0;
                }
                if (nested.type == BTOK_SPECIAL && strcmp(nested.text, "#(") == 0)
                {
                    depth++;
                }
                else if (nested.type == BTOK_SPECIAL && strcmp(nested.text, ")") == 0)
                {
                    depth--;
                }
            }

            BToken literal_array = make_token(BTOK_LITERAL_ARRAY);
            uint64_t nested_len = tokenizer.index - nested_start;
            if (nested_len >= sizeof(literal_array.text))
            {
                return 0;
            }
            memcpy(literal_array.text, source + nested_start, (size_t)nested_len);
            literal_array.text[nested_len] = '\0';
            items[item_count++] = literal_array;
            continue;
        }

        items[item_count++] = token;
    }

    uint64_t *array = om_alloc(om, (uint64_t)array_class, BC_FORMAT_INDEXABLE, (uint64_t)item_count);
    if (array == NULL)
    {
        return 0;
    }

    for (int index = 0; index < item_count; index++)
    {
        uint64_t element = 0;
        if (items[index].type == BTOK_LITERAL_ARRAY)
        {
            if (!bc_materialize_literal_array_oop(om, string_class, array_class, association_class,
                                                  items[index].text, &element))
            {
                return 0;
            }
        }
        else if (!bc_literal_token_to_oop(om, string_class, array_class, association_class,
                                          &items[index], &element))
        {
            return 0;
        }
        BC_OBJ_FIELD(array, index) = element;
    }

    *out_oop = (uint64_t)array;
    return 1;
}

static int bc_literal_token_to_oop(uint64_t *om, uint64_t *string_class, uint64_t *array_class,
                                   uint64_t *association_class, const BToken *token, uint64_t *out_oop)
{
    if (token->type == BTOK_INTEGER)
    {
        *out_oop = tag_smallint(token->int_value);
        return 1;
    }
    if (token->type == BTOK_CHARACTER)
    {
        *out_oop = tag_character((uint64_t)token->int_value);
        return 1;
    }
    if (token->type == BTOK_STRING)
    {
        if (string_class == NULL)
        {
            return 0;
        }
        *out_oop = (uint64_t)bc_make_byte_string(om, string_class, token->text);
        return *out_oop != 0;
    }
    if (token->type == BTOK_SYMBOL)
    {
        *out_oop = intern_cstring_symbol(om, token->text);
        return *out_oop != tagged_nil();
    }
    if (token->type == BTOK_SELECTOR)
    {
        *out_oop = intern_cstring_symbol(om, token->text);
        return *out_oop != tagged_nil();
    }
    if (token->type == BTOK_CLASS_REF)
    {
        uint64_t *klass = bc_lookup_class_named(bc_active_class_bindings, bc_active_class_count, token->text);
        if (klass == NULL || association_class == NULL)
        {
            return 0;
        }
        return bc_global_association_oop(om, (uint64_t)array_class, (uint64_t)association_class, token->text,
                                         (uint64_t)klass, out_oop);
    }
    if (token->type == BTOK_IDENTIFIER)
    {
        if (strcmp(token->text, "nil") == 0)
        {
            *out_oop = tagged_nil();
            return 1;
        }
        if (strcmp(token->text, "true") == 0)
        {
            *out_oop = tagged_true();
            return 1;
        }
        if (strcmp(token->text, "false") == 0)
        {
            *out_oop = tagged_false();
            return 1;
        }
    }
    return 0;
}

static uint64_t *bc_build_block_literal_array(uint64_t *om, uint64_t *class_class, uint64_t *string_class,
                                              uint64_t *array_class,
                                              uint64_t *association_class,
                                              const BCompiledBlock *block);
static uint64_t *bc_build_literal_array(uint64_t *om, uint64_t *class_class, uint64_t *string_class,
                                        uint64_t *array_class, uint64_t *association_class,
                                        const BCompiledBody *body);

static uint64_t *bc_materialize_compiled_block(uint64_t *om, uint64_t *class_class, uint64_t *string_class,
                                               uint64_t *array_class,
                                               uint64_t *association_class,
                                               const BCompiledBlock *block)
{
    uint64_t *literals = bc_build_block_literal_array(om, class_class, string_class, array_class,
                                                      association_class, block);
    if (block->literal_count > 0 && literals == NULL)
    {
        return NULL;
    }

    uint64_t bytecode_size = block->bytecode_count > 0 ? (uint64_t)block->bytecode_count : 1;
    uint64_t *bytecodes = om_alloc(om, (uint64_t)class_class, BC_FORMAT_BYTES, bytecode_size);
    if (bytecodes == NULL)
    {
        return NULL;
    }
    if (block->bytecode_count > 0)
    {
        memcpy(&BC_OBJ_FIELD(bytecodes, 0), block->bytecodes, (size_t)block->bytecode_count);
    }
    else
    {
        ((uint8_t *)&BC_OBJ_FIELD(bytecodes, 0))[0] = 0;
    }

    uint64_t *compiled_method = om_alloc(om, (uint64_t)class_class, 0, 5);
    if (compiled_method == NULL)
    {
        return NULL;
    }
    BC_OBJ_FIELD(compiled_method, BC_CM_PRIMITIVE) = tag_smallint(0);
    BC_OBJ_FIELD(compiled_method, BC_CM_NUM_ARGS) = tag_smallint(0);
    BC_OBJ_FIELD(compiled_method, BC_CM_NUM_TEMPS) = tag_smallint(0);
    BC_OBJ_FIELD(compiled_method, BC_CM_LITERALS) = literals ? (uint64_t)literals : tagged_nil();
    BC_OBJ_FIELD(compiled_method, BC_CM_BYTECODES) = (uint64_t)bytecodes;
    return compiled_method;
}

static uint64_t *bc_build_block_literal_array(uint64_t *om, uint64_t *class_class, uint64_t *string_class,
                                              uint64_t *array_class,
                                              uint64_t *association_class,
                                              const BCompiledBlock *block)
{
    if (block->literal_count == 0)
    {
        return NULL;
    }

    uint64_t *literals = om_alloc(om, (uint64_t)class_class, BC_FORMAT_INDEXABLE, (uint64_t)block->literal_count);
    if (literals == NULL)
    {
        return NULL;
    }
    for (int index = 0; index < block->literal_count; index++)
    {
        uint64_t literal_oop = 0;
        if (block->literals[index].type == BTOK_BLOCK_LITERAL)
        {
            return NULL;
        }
        if (!bc_literal_token_to_oop(om, string_class, array_class, association_class,
                                     &block->literals[index], &literal_oop))
        {
            return NULL;
        }
        BC_OBJ_FIELD(literals, index) = literal_oop;
    }
    return literals;
}

static uint64_t *bc_build_literal_array(uint64_t *om, uint64_t *class_class, uint64_t *string_class,
                                        uint64_t *array_class, uint64_t *association_class,
                                        const BCompiledBody *body)
{
    if (body->literal_count == 0)
    {
        return NULL;
    }

    uint64_t *literals = om_alloc(om, (uint64_t)class_class, BC_FORMAT_INDEXABLE, (uint64_t)body->literal_count);
    for (int index = 0; index < body->literal_count; index++)
    {
        uint64_t literal_oop = 0;
        if (body->literals[index].type == BTOK_BLOCK_LITERAL)
        {
            int block_index = (int)body->literals[index].int_value;
            if (block_index < 0 || block_index >= body->block_count)
            {
                return NULL;
            }
            uint64_t *compiled_block = bc_materialize_compiled_block(om, class_class, string_class,
                                                                     array_class,
                                                                     association_class, &body->blocks[block_index]);
            if (compiled_block == NULL)
            {
                return NULL;
            }
            literal_oop = (uint64_t)compiled_block;
        }
        else if (body->literals[index].type == BTOK_LITERAL_ARRAY)
        {
            if (array_class == NULL ||
                !bc_materialize_literal_array_oop(om, string_class, array_class, association_class,
                                                  body->literals[index].text, &literal_oop))
            {
                return NULL;
            }
        }
        else if (!bc_literal_token_to_oop(om, string_class, array_class, association_class,
                                          &body->literals[index], &literal_oop))
        {
            return NULL;
        }
        BC_OBJ_FIELD(literals, index) = literal_oop;
    }
    return literals;
}

static uint64_t *bc_append_method_dict(uint64_t *om, uint64_t *class_class, uint64_t *klass,
                                       uint64_t selector_oop, uint64_t method_oop)
{
    uint64_t md_value = BC_OBJ_FIELD(klass, BC_CLASS_METHOD_DICT);
    uint64_t *old_md = (md_value != tagged_nil() && (md_value & 3) == 0) ? (uint64_t *)md_value : NULL;
    uint64_t old_size = old_md ? BC_OBJ_SIZE(old_md) : 0;
    uint64_t *new_md = om_alloc(om, (uint64_t)class_class, BC_FORMAT_INDEXABLE, old_size + 2);

    for (uint64_t index = 0; index < old_size; index++)
    {
        BC_OBJ_FIELD(new_md, index) = BC_OBJ_FIELD(old_md, index);
    }
    BC_OBJ_FIELD(new_md, old_size) = selector_oop;
    BC_OBJ_FIELD(new_md, old_size + 1) = method_oop;
    BC_OBJ_FIELD(klass, BC_CLASS_METHOD_DICT) = (uint64_t)new_md;
    return new_md;
}

static uint64_t *bc_target_class_for_method(uint64_t *klass, const BCompiledMethodDef *method)
{
    if (!method->class_side)
    {
        return klass;
    }
    if (((uint64_t)klass & 3) != 0)
    {
        return NULL;
    }
    return (uint64_t *)BC_OBJ_CLASS(klass);
}

static uint64_t *bc_materialize_compiled_method(uint64_t *om, uint64_t *class_class,
                                                uint64_t *string_class, uint64_t *array_class,
                                                uint64_t *association_class,
                                                const BCompiledMethodDef *method)
{
    uint64_t *literals = bc_build_literal_array(om, class_class, string_class, array_class,
                                                association_class, &method->body);
    if (method->body.literal_count > 0 && literals == NULL)
    {
        return NULL;
    }

    uint64_t bytecode_size = method->body.bytecode_count > 0 ? (uint64_t)method->body.bytecode_count : 1;
    uint64_t *bytecodes = om_alloc(om, (uint64_t)class_class, BC_FORMAT_BYTES, bytecode_size);
    if (method->body.bytecode_count > 0)
    {
        memcpy(&BC_OBJ_FIELD(bytecodes, 0), method->body.bytecodes, (size_t)method->body.bytecode_count);
    }
    else
    {
        ((uint8_t *)&BC_OBJ_FIELD(bytecodes, 0))[0] = 0;
    }

    uint64_t *compiled_method = om_alloc(om, (uint64_t)class_class, 0, 5);
    BC_OBJ_FIELD(compiled_method, BC_CM_PRIMITIVE) =
        tag_smallint(method->primitive_index >= 0 ? method->primitive_index : 0);
    BC_OBJ_FIELD(compiled_method, BC_CM_NUM_ARGS) = tag_smallint(method->header.arg_count);
    BC_OBJ_FIELD(compiled_method, BC_CM_NUM_TEMPS) = tag_smallint(method->body.temp_count);
    BC_OBJ_FIELD(compiled_method, BC_CM_LITERALS) = literals ? (uint64_t)literals : tagged_nil();
    BC_OBJ_FIELD(compiled_method, BC_CM_BYTECODES) = (uint64_t)bytecodes;
    return compiled_method;
}

int bc_install_compiled_methods(uint64_t *om, uint64_t *class_class,
                                const BClassBinding *classes, int class_count,
                                const BCompiledMethodDef *methods, int method_count)
{
    const BClassBinding *saved_bindings = bc_active_class_bindings;
    int saved_binding_count = bc_active_class_count;
    uint64_t *string_class = bc_lookup_class_named(classes, class_count, "String");
    uint64_t *array_class = bc_lookup_class_named(classes, class_count, "Array");
    uint64_t *association_class = bc_lookup_class_named(classes, class_count, "Association");
    bc_active_class_bindings = classes;
    bc_active_class_count = class_count;

    for (int index = 0; index < method_count; index++)
    {
        const BCompiledMethodDef *method = &methods[index];
        uint64_t *klass = bc_lookup_class_named(classes, class_count, method->class_name);
        if (klass == NULL)
        {
            bc_active_class_bindings = saved_bindings;
            bc_active_class_count = saved_binding_count;
            return 0;
        }

        uint64_t *target_class = bc_target_class_for_method(klass, method);
        if (target_class == NULL)
        {
            bc_active_class_bindings = saved_bindings;
            bc_active_class_count = saved_binding_count;
            return 0;
        }

        uint64_t selector_oop = intern_cstring_symbol(om, method->header.selector);
        if (selector_oop == tagged_nil())
        {
            bc_active_class_bindings = saved_bindings;
            bc_active_class_count = saved_binding_count;
            return 0;
        }
        uint64_t *compiled_method = bc_materialize_compiled_method(om, class_class, string_class,
                                                                   array_class, association_class, method);
        if (compiled_method == NULL)
        {
            bc_active_class_bindings = saved_bindings;
            bc_active_class_count = saved_binding_count;
            return 0;
        }

        if (bc_append_method_dict(om, class_class, target_class, selector_oop, (uint64_t)compiled_method) == NULL)
        {
            bc_active_class_bindings = saved_bindings;
            bc_active_class_count = saved_binding_count;
            return 0;
        }
    }

    bc_active_class_bindings = saved_bindings;
    bc_active_class_count = saved_binding_count;
    return 1;
}

int bc_compile_and_install_source_methods(uint64_t *om, uint64_t *class_class,
                                          const BClassBinding *classes, int class_count,
                                          const char *source)
{
    BMethodChunk chunks[128];
    int chunk_count = 0;
    const BClassBinding *saved_bindings = bc_active_class_bindings;
    int saved_binding_count = bc_active_class_count;
    uint64_t *string_class = bc_lookup_class_named(classes, class_count, "String");
    uint64_t *array_class = bc_lookup_class_named(classes, class_count, "Array");
    uint64_t *association_class = bc_lookup_class_named(classes, class_count, "Association");
    bc_active_class_bindings = classes;
    bc_active_class_count = class_count;

    if (!bc_parse_method_chunks(source, chunks, 128, &chunk_count))
    {
        bc_active_class_bindings = saved_bindings;
        bc_active_class_count = saved_binding_count;
        return 0;
    }

    for (int index = 0; index < chunk_count; index++)
    {
        const BMethodChunk *chunk = &chunks[index];
        uint64_t *klass = bc_lookup_class_named(classes, class_count, chunk->class_name);
        if (klass == NULL)
        {
            bc_active_class_bindings = saved_bindings;
            bc_active_class_count = saved_binding_count;
            return 0;
        }

        BCompiledMethodDef method;
        memset(&method, 0, sizeof(method));
        strncpy(method.class_name, chunk->class_name, sizeof(method.class_name) - 1);
        method.class_side = chunk->class_side;
        method.primitive_index = -1;

        uint64_t *target_class = bc_target_class_for_method(klass, &method);
        if (target_class == NULL)
        {
            bc_active_class_bindings = saved_bindings;
            bc_active_class_count = saved_binding_count;
            return 0;
        }

        const char *method_source = chunk->method_source;
        const char *newline = strchr(method_source, '\n');
        char header_source[256];
        const char *body_source = "";

        if (newline != NULL)
        {
            uint64_t header_len = (uint64_t)(newline - method_source);
            if (header_len >= sizeof(header_source))
            {
                return 0;
            }
            memcpy(header_source, method_source, (size_t)header_len);
            header_source[header_len] = '\0';
            body_source = newline + 1;
        }
        else
        {
            uint64_t header_len = strlen(method_source);
            if (header_len >= sizeof(header_source))
            {
                return 0;
            }
            memcpy(header_source, method_source, (size_t)header_len + 1);
        }

        if (!bc_parse_method_header(header_source, &method.header))
        {
            bc_active_class_bindings = saved_bindings;
            bc_active_class_count = saved_binding_count;
            return 0;
        }

        while (*body_source == ' ' || *body_source == '\t' || *body_source == '\n' || *body_source == '\r')
        {
            body_source++;
        }

        int primitive_index = -1;
        if (sscanf(body_source, "<primitive: %d>", &primitive_index) == 1)
        {
            method.primitive_index = primitive_index;

            const char *primitive_end = strchr(body_source, '>');
            if (primitive_end != NULL)
            {
                body_source = primitive_end + 1;
                while (*body_source == ' ' || *body_source == '\t' || *body_source == '\n' || *body_source == '\r')
                {
                    body_source++;
                }
                if (*body_source != '\0' && !bc_codegen_body(body_source, &method.body, 0, &method.header, target_class))
                {
                    bc_active_class_bindings = saved_bindings;
                    bc_active_class_count = saved_binding_count;
                    return 0;
                }
            }
        }
        else if (!bc_codegen_body(body_source, &method.body, 0, &method.header, target_class))
        {
            bc_active_class_bindings = saved_bindings;
            bc_active_class_count = saved_binding_count;
            return 0;
        }

        uint64_t selector_oop = intern_cstring_symbol(om, method.header.selector);
        if (selector_oop == tagged_nil())
        {
            bc_active_class_bindings = saved_bindings;
            bc_active_class_count = saved_binding_count;
            return 0;
        }
        uint64_t *compiled_method = bc_materialize_compiled_method(om, class_class, string_class,
                                                                   array_class, association_class, &method);
        if (compiled_method == NULL)
        {
            bc_active_class_bindings = saved_bindings;
            bc_active_class_count = saved_binding_count;
            return 0;
        }

        if (bc_append_method_dict(om, class_class, target_class, selector_oop, (uint64_t)compiled_method) == NULL)
        {
            bc_active_class_bindings = saved_bindings;
            bc_active_class_count = saved_binding_count;
            return 0;
        }
    }

    bc_active_class_bindings = saved_bindings;
    bc_active_class_count = saved_binding_count;
    return 1;
}

uint64_t *bc_define_class(uint64_t *om, uint64_t *class_class, uint64_t *string_class,
                          uint64_t *array_class, uint64_t *association_class,
                          const char *name, uint64_t *superclass,
                          const char **ivar_names, int ivar_count,
                          BClassFormat format)
{
    // Metaclass holds class-side methods. Use class_class as its class
    // because we don't have a proper Metaclass object in the boot image.
    uint64_t *metaclass = om_alloc(om, (uint64_t)class_class, 0, 5);
    if (metaclass == NULL)
    {
        return NULL;
    }
    BC_OBJ_FIELD(metaclass, BC_CLASS_SUPERCLASS) = tagged_nil();
    BC_OBJ_FIELD(metaclass, BC_CLASS_METHOD_DICT) = tagged_nil();
    BC_OBJ_FIELD(metaclass, BC_CLASS_INST_SIZE) = tag_smallint(0);
    BC_OBJ_FIELD(metaclass, BC_CLASS_INST_FORMAT) = tag_smallint(0);
    BC_OBJ_FIELD(metaclass, BC_CLASS_INST_VARS) = tagged_nil();

    uint64_t *klass = om_alloc(om, (uint64_t)metaclass, 0, 5);
    if (klass == NULL)
    {
        return NULL;
    }
    BC_OBJ_FIELD(klass, BC_CLASS_SUPERCLASS) =
        superclass != NULL ? (uint64_t)superclass : tagged_nil();
    BC_OBJ_FIELD(klass, BC_CLASS_METHOD_DICT) = tagged_nil();
    BC_OBJ_FIELD(klass, BC_CLASS_INST_SIZE) = tag_smallint((int64_t)ivar_count);
    BC_OBJ_FIELD(klass, BC_CLASS_INST_FORMAT) = tag_smallint((int64_t)format);

    if (ivar_count <= 0)
    {
        BC_OBJ_FIELD(klass, BC_CLASS_INST_VARS) = tagged_nil();
    }
    else
    {
        uint64_t *ivar_array = om_alloc(om, (uint64_t)class_class, BC_FORMAT_INDEXABLE,
                                        (uint64_t)ivar_count);
        if (ivar_array == NULL)
        {
            return NULL;
        }
        for (int index = 0; index < ivar_count; index++)
        {
            uint64_t *ivar_string = bc_make_byte_string(om, string_class, ivar_names[index]);
            if (ivar_string == NULL)
            {
                return NULL;
            }
            BC_OBJ_FIELD(ivar_array, (uint64_t)index) = (uint64_t)ivar_string;
        }
        BC_OBJ_FIELD(klass, BC_CLASS_INST_VARS) = (uint64_t)ivar_array;
    }

    if (global_smalltalk_dictionary != NULL && array_class != NULL && association_class != NULL)
    {
        uint64_t association_oop = 0;
        if (!bc_global_association_oop(om, (uint64_t)array_class, (uint64_t)association_class,
                                       name, (uint64_t)klass, &association_oop))
        {
            return NULL;
        }
    }

    return klass;
}
