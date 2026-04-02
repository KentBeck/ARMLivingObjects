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
}
