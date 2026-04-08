#include "test_defs.h"

static uint64_t *make_test_string(uint64_t *om, uint64_t *string_class, const char *text)
{
    uint64_t size = (uint64_t)strlen(text);
    uint64_t *obj = om_alloc(om, (uint64_t)string_class, FORMAT_BYTES, size);
    uint8_t *data = (uint8_t *)&OBJ_FIELD(obj, 0);
    memcpy(data, text, size);
    return obj;
}

void test_primitives(TestContext *ctx)
{
    uint64_t *s1 = make_test_string(ctx->om, ctx->string_class, "hello");
    uint64_t *s2 = make_test_string(ctx->om, ctx->string_class, "hello");
    uint64_t *s3 = make_test_string(ctx->om, ctx->string_class, "world");

    ASSERT_EQ(ctx, prim_string_eq((uint64_t)s1, (uint64_t)s2), tagged_true(), "PRIM_STRING_EQ: equal strings");
    ASSERT_EQ(ctx, prim_string_eq((uint64_t)s1, (uint64_t)s3), tagged_false(), "PRIM_STRING_EQ: different strings");
    ASSERT_EQ(ctx, prim_string_eq(tag_smallint(1), (uint64_t)s2), tagged_false(), "PRIM_STRING_EQ: non-object input");

    uint64_t h1 = prim_string_hash_fnv((uint64_t)s1);
    uint64_t h2 = prim_string_hash_fnv((uint64_t)s2);
    uint64_t h3 = prim_string_hash_fnv((uint64_t)s3);
    ASSERT_EQ(ctx, h1, h2, "PRIM_STRING_HASH_FNV: same bytes same hash");
    ASSERT_EQ(ctx, h1 == h3, 0, "PRIM_STRING_HASH_FNV: different bytes different hash");

    uint64_t sym1 = prim_string_as_symbol((uint64_t)s1);
    uint64_t sym2 = prim_string_as_symbol((uint64_t)s2);
    uint64_t sym3 = prim_string_as_symbol((uint64_t)s3);
    ASSERT_EQ(ctx, OBJ_CLASS((uint64_t *)sym1), (uint64_t)ctx->symbol_class,
              "PRIM_STRING_AS_SYMBOL: first insert returns Symbol");
    ASSERT_EQ(ctx, sym2, sym1, "PRIM_STRING_AS_SYMBOL: interned identity reused");
    ASSERT_EQ(ctx, OBJ_CLASS((uint64_t *)sym3), (uint64_t)ctx->symbol_class,
              "PRIM_STRING_AS_SYMBOL: second distinct symbol is Symbol");

    ASSERT_EQ(ctx, prim_symbol_eq(sym1, sym2), tagged_true(), "PRIM_SYMBOL_EQ: identical symbols true");
    ASSERT_EQ(ctx, prim_symbol_eq(sym1, sym3), tagged_false(), "PRIM_SYMBOL_EQ: distinct symbols false");
}
