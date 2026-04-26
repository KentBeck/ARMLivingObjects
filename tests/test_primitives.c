#include "test_defs.h"
#include "primitives.h"

#include <unistd.h>

static uint64_t *make_test_string(uint64_t *om, uint64_t *string_class, const char *text)
{
    uint64_t size = (uint64_t)strlen(text);
    uint64_t *obj = om_alloc(om, (uint64_t)string_class, FORMAT_BYTES, size);
    uint8_t *data = (uint8_t *)&OBJ_FIELD(obj, 0);
    memcpy(data, text, size);
    return obj;
}

static int byte_object_equals_cstring(uint64_t value, const char *text)
{
    if (!is_object_ptr(value))
    {
        return 0;
    }
    return OBJ_FORMAT((uint64_t *)value) == FORMAT_BYTES &&
           OBJ_SIZE((uint64_t *)value) == (uint64_t)strlen(text) &&
           memcmp(&OBJ_FIELD((uint64_t *)value, 0), text, strlen(text)) == 0;
}

void test_primitives(TestContext *ctx)
{
    uint64_t *saved_smalltalk_dictionary = global_smalltalk_dictionary;
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

    {
        uint64_t *dictionary = om_alloc(ctx->om, (uint64_t)ctx->class_class, FORMAT_FIELDS, 2);
        uint64_t *associations = om_alloc(ctx->om, (uint64_t)ctx->class_class, FORMAT_INDEXABLE, 1);
        uint64_t *association = om_alloc(ctx->om, (uint64_t)ctx->class_class, FORMAT_FIELDS, 2);

        OBJ_FIELD(dictionary, 0) = (uint64_t)associations;
        OBJ_FIELD(dictionary, 1) = tag_smallint(1);
        OBJ_FIELD(association, 0) = intern_cstring_symbol(ctx->om, "String");
        OBJ_FIELD(association, 1) = (uint64_t)ctx->string_class;
        OBJ_FIELD(associations, 0) = (uint64_t)association;
        global_smalltalk_dictionary = dictionary;
    }

    {
        int read_pipe[2];
        uint64_t read_bytes;

        ASSERT_EQ(ctx, pipe(read_pipe), 0, "PRIM_READ_FD_COUNT: pipe setup succeeds");
        ASSERT_EQ(ctx, write(read_pipe[1], "ping", 4), 4, "PRIM_READ_FD_COUNT: pipe write succeeds");
        close(read_pipe[1]);
        read_bytes = prim_read_fd_count(tag_smallint(read_pipe[0]), tag_smallint(8), ctx->om);
        close(read_pipe[0]);
        ASSERT_EQ(ctx, byte_object_equals_cstring(read_bytes, "ping"), 1,
                  "PRIM_READ_FD_COUNT: reads bytes from fd");
    }

    {
        int write_pipe[2];
        char buffer[8] = {0};
        ssize_t bytes_read;
        uint64_t write_count;

        ASSERT_EQ(ctx, pipe(write_pipe), 0, "PRIM_WRITE_FD_STRING: pipe setup succeeds");
        write_count = prim_write_fd_string(tag_smallint(write_pipe[1]), (uint64_t)s1);
        close(write_pipe[1]);
        bytes_read = read(write_pipe[0], buffer, sizeof(buffer));
        close(write_pipe[0]);
        ASSERT_EQ(ctx, write_count, tag_smallint(5), "PRIM_WRITE_FD_STRING: reports bytes written");
        ASSERT_EQ(ctx, bytes_read, 5, "PRIM_WRITE_FD_STRING: pipe captures all bytes");
        ASSERT_EQ(ctx, memcmp(buffer, "hello", 5) == 0, 1,
                  "PRIM_WRITE_FD_STRING: writes string bytes to fd");
    }

    global_smalltalk_dictionary = saved_smalltalk_dictionary;
}
