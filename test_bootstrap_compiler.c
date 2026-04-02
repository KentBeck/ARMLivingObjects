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
}

