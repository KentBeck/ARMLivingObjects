#include "test_defs.h"
#include "bootstrap_compiler.h"

static void assert_tok(TestContext *ctx, BTokenizer *tokenizer, BTokenType type, const char *text)
{
    BToken token = bt_next(tokenizer);
    ASSERT_EQ(ctx, token.type, type, "token type matches");
    ASSERT_EQ(ctx, strcmp(token.text, text), 0, "token text matches");
}

void test_bootstrap_compiler(TestContext *ctx)
{
    {
        BTokenizer tokenizer;
        bt_init(&tokenizer, "at: index put: value ^ index + 1");

        assert_tok(ctx, &tokenizer, BTOK_KEYWORD, "at:");
        assert_tok(ctx, &tokenizer, BTOK_IDENTIFIER, "index");
        assert_tok(ctx, &tokenizer, BTOK_KEYWORD, "put:");
        assert_tok(ctx, &tokenizer, BTOK_IDENTIFIER, "value");
        assert_tok(ctx, &tokenizer, BTOK_SPECIAL, "^");
        assert_tok(ctx, &tokenizer, BTOK_IDENTIFIER, "index");
        assert_tok(ctx, &tokenizer, BTOK_SPECIAL, "+");

        BToken int_token = bt_next(&tokenizer);
        ASSERT_EQ(ctx, int_token.type, BTOK_INTEGER, "integer token type");
        ASSERT_EQ(ctx, int_token.int_value, 1, "integer token value");

        BToken eof = bt_next(&tokenizer);
        ASSERT_EQ(ctx, eof.type, BTOK_EOF, "tokenizer reaches EOF");
    }

    {
        BTokenizer tokenizer;
        bt_init(&tokenizer, "#Array 'hello''s' ()[]");

        assert_tok(ctx, &tokenizer, BTOK_SYMBOL, "Array");
        assert_tok(ctx, &tokenizer, BTOK_STRING, "hello's");
        assert_tok(ctx, &tokenizer, BTOK_SPECIAL, "(");
        assert_tok(ctx, &tokenizer, BTOK_SPECIAL, ")");
        assert_tok(ctx, &tokenizer, BTOK_SPECIAL, "[");
        assert_tok(ctx, &tokenizer, BTOK_SPECIAL, "]");

        BToken eof = bt_next(&tokenizer);
        ASSERT_EQ(ctx, eof.type, BTOK_EOF, "tokenizer EOF after specials");
    }

    {
        BTokenizer tokenizer;
        bt_init(&tokenizer, "$A");

        BToken token = bt_next(&tokenizer);
        ASSERT_EQ(ctx, token.type, BTOK_CHARACTER, "character token type");
        ASSERT_EQ(ctx, token.int_value, 65, "character token value");
        ASSERT_EQ(ctx, strcmp(token.text, "A"), 0, "character token text");

        BToken eof = bt_next(&tokenizer);
        ASSERT_EQ(ctx, eof.type, BTOK_EOF, "tokenizer EOF after character");
    }

    {
        BTokenizer tokenizer;
        bt_init(&tokenizer, "#(1 #foo)");

        assert_tok(ctx, &tokenizer, BTOK_SPECIAL, "#(");
        assert_tok(ctx, &tokenizer, BTOK_INTEGER, "1");
        assert_tok(ctx, &tokenizer, BTOK_SYMBOL, "foo");
        assert_tok(ctx, &tokenizer, BTOK_SPECIAL, ")");

        BToken eof = bt_next(&tokenizer);
        ASSERT_EQ(ctx, eof.type, BTOK_EOF, "tokenizer EOF after literal array");
    }

    {
        BTokenizer tokenizer;
        bt_init(&tokenizer, "x := 1");

        assert_tok(ctx, &tokenizer, BTOK_IDENTIFIER, "x");
        assert_tok(ctx, &tokenizer, BTOK_SPECIAL, ":=");
        assert_tok(ctx, &tokenizer, BTOK_INTEGER, "1");

        BToken eof = bt_next(&tokenizer);
        ASSERT_EQ(ctx, eof.type, BTOK_EOF, "tokenizer EOF after assignment");
    }

    {
        BMethodHeader header;
        ASSERT_EQ(ctx, bc_parse_method_header("size", &header), 1,
                  "parse unary method header");
        ASSERT_EQ(ctx, header.kind, BMETHOD_UNARY,
                  "unary header kind");
        ASSERT_EQ(ctx, strcmp(header.selector, "size"), 0,
                  "unary selector");
        ASSERT_EQ(ctx, header.arg_count, 0,
                  "unary arg count");
    }

    {
        BMethodHeader header;
        ASSERT_EQ(ctx, bc_parse_method_header("+ other", &header), 1,
                  "parse binary method header");
        ASSERT_EQ(ctx, header.kind, BMETHOD_BINARY,
                  "binary header kind");
        ASSERT_EQ(ctx, strcmp(header.selector, "+"), 0,
                  "binary selector");
        ASSERT_EQ(ctx, header.arg_count, 1,
                  "binary arg count");
        ASSERT_EQ(ctx, strcmp(header.arg_names[0], "other"), 0,
                  "binary arg name");
    }

    {
        BMethodHeader header;
        ASSERT_EQ(ctx, bc_parse_method_header("at: index put: value", &header), 1,
                  "parse keyword method header");
        ASSERT_EQ(ctx, header.kind, BMETHOD_KEYWORD,
                  "keyword header kind");
        ASSERT_EQ(ctx, strcmp(header.selector, "at:put:"), 0,
                  "keyword selector");
        ASSERT_EQ(ctx, header.arg_count, 2,
                  "keyword arg count");
        ASSERT_EQ(ctx, strcmp(header.arg_names[0], "index"), 0,
                  "keyword first arg");
        ASSERT_EQ(ctx, strcmp(header.arg_names[1], "value"), 0,
                  "keyword second arg");
    }

    {
        BMethodHeader header;
        ASSERT_EQ(ctx, bc_parse_method_header("at: put:", &header), 0,
                  "reject invalid keyword method header");
    }

    {
        BMethodBody body;
        ASSERT_EQ(ctx, bc_parse_method_body("| x y | x := 1. y := x + 2. ^ y", &body), 1,
                  "parse method body with temps and assignments");
        ASSERT_EQ(ctx, body.temp_count, 2, "temp count");
        ASSERT_EQ(ctx, strcmp(body.temp_names[0], "x"), 0, "first temp");
        ASSERT_EQ(ctx, strcmp(body.temp_names[1], "y"), 0, "second temp");
        ASSERT_EQ(ctx, body.assignment_count, 2, "assignment count");
        ASSERT_EQ(ctx, body.return_count, 1, "return count");
        ASSERT_EQ(ctx, body.literal_integer_count, 2, "integer literal count");
        ASSERT_EQ(ctx, body.literal_character_count, 0, "character literal count");
        ASSERT_EQ(ctx, body.literal_string_count, 0, "string literal count");
        ASSERT_EQ(ctx, body.literal_symbol_count, 0, "symbol literal count");
        ASSERT_EQ(ctx, body.message_send_count, 1, "message send count");
    }

    {
        BMethodBody body;
        ASSERT_EQ(ctx, bc_parse_method_body("^ self at: 1 put: 'a'", &body), 1,
                  "parse method body with keyword message");
        ASSERT_EQ(ctx, body.temp_count, 0, "no temps");
        ASSERT_EQ(ctx, body.assignment_count, 0, "no assignments");
        ASSERT_EQ(ctx, body.return_count, 1, "single return");
        ASSERT_EQ(ctx, body.literal_integer_count, 1, "integer literal in keyword arg");
        ASSERT_EQ(ctx, body.literal_character_count, 0, "no character literals");
        ASSERT_EQ(ctx, body.literal_string_count, 1, "string literal in keyword arg");
        ASSERT_EQ(ctx, body.literal_symbol_count, 0, "no symbol literals");
        ASSERT_EQ(ctx, body.message_send_count, 1, "single keyword send");
    }

    {
        BMethodBody body;
        ASSERT_EQ(ctx, bc_parse_method_body("^ #foo", &body), 1,
                  "parse method body with symbol return");
        ASSERT_EQ(ctx, body.return_count, 1, "single return for symbol");
        ASSERT_EQ(ctx, body.literal_character_count, 0, "no character literals");
        ASSERT_EQ(ctx, body.literal_symbol_count, 1, "symbol literal count");
        ASSERT_EQ(ctx, body.message_send_count, 0, "no message sends");
    }

    {
        BMethodBody body;
        ASSERT_EQ(ctx, bc_parse_method_body("^ $A", &body), 1,
                  "parse method body with character return");
        ASSERT_EQ(ctx, body.return_count, 1, "single return for character");
        ASSERT_EQ(ctx, body.literal_character_count, 1, "character literal count");
    }

    {
        BMethodBody body;
        ASSERT_EQ(ctx, bc_parse_method_body("^ [ :x | x + 1 ] value: 2", &body), 1,
                  "parse method body with block and keyword send");
        ASSERT_EQ(ctx, body.return_count, 1, "block body return count");
        ASSERT_EQ(ctx, body.assignment_count, 0, "block body assignment count");
        ASSERT_EQ(ctx, body.literal_integer_count, 2, "block literal integers");
        ASSERT_EQ(ctx, body.message_send_count, 2, "block and outer message sends");
    }

    {
        BMethodBody body;
        ASSERT_EQ(ctx, bc_parse_method_body("^ [ :x | x + 1", &body), 0,
                  "reject unterminated block body");
    }

    {
        BMethodBody body;
        ASSERT_EQ(ctx, bc_parse_method_body("^ (1 + 2) = 3", &body), 1,
                  "parse parenthesized expression");
        ASSERT_EQ(ctx, body.return_count, 1, "parenthesized return count");
        ASSERT_EQ(ctx, body.literal_integer_count, 3, "parenthesized integer literals");
        ASSERT_EQ(ctx, body.message_send_count, 2, "parenthesized message sends");
    }

    {
        BMethodBody body;
        ASSERT_EQ(ctx, bc_parse_method_body("^ stream nextPut: $A; nextPut: $B; contents", &body), 1,
                  "parse cascade expression");
        ASSERT_EQ(ctx, body.return_count, 1, "cascade return count");
        ASSERT_EQ(ctx, body.literal_character_count, 2, "cascade character literals");
        ASSERT_EQ(ctx, body.message_send_count, 3, "cascade message sends");
    }

    {
        BMethodBody body;
        ASSERT_EQ(ctx, bc_parse_method_body("^ #(1 'a' #foo $B)", &body), 1,
                  "parse literal array expression");
        ASSERT_EQ(ctx, body.return_count, 1, "literal array return count");
        ASSERT_EQ(ctx, body.literal_integer_count, 1, "literal array integer count");
        ASSERT_EQ(ctx, body.literal_string_count, 1, "literal array string count");
        ASSERT_EQ(ctx, body.literal_symbol_count, 1, "literal array symbol count");
        ASSERT_EQ(ctx, body.literal_character_count, 1, "literal array character count");
    }

    {
        BMethodBody body;
        ASSERT_EQ(ctx, bc_parse_method_body("^ #(1 #foo", &body), 0,
                  "reject unterminated literal array");
    }
}
