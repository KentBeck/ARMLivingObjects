#include "test_defs.h"

static int read_file(const char *path, char *buf, size_t cap)
{
    FILE *f = fopen(path, "rb");
    if (!f)
    {
        return 0;
    }
    size_t n = fread(buf, 1, cap - 1, f);
    fclose(f);
    buf[n] = '\0';
    return 1;
}

void test_smalltalk_sources(TestContext *ctx)
{
    char class_src[4096];
    char string_src[8192];

    ASSERT_EQ(ctx, read_file("smalltalk/Class.st", class_src, sizeof(class_src)), 1,
              "smalltalk/Class.st exists");
    ASSERT_EQ(ctx, strstr(class_src, "new\n    ^ self basicNew") != NULL, 1,
              "Class>>new delegates to basicNew");
    ASSERT_EQ(ctx, strstr(class_src, "new: size\n    ^ self basicNew: size") != NULL, 1,
              "Class>>new: delegates to basicNew:");

    ASSERT_EQ(ctx, read_file("smalltalk/String.st", string_src, sizeof(string_src)), 1,
              "smalltalk/String.st exists");
    ASSERT_EQ(ctx, strstr(string_src, ", aString") != NULL, 1,
              "String>>, method exists");
    ASSERT_EQ(ctx, strstr(string_src, "result := String new: self size + aString size.") != NULL, 1,
              "String>>, allocates result with String new:");
    ASSERT_EQ(ctx, strstr(string_src, "result at: i put: (self at: i).") != NULL, 1,
              "String>>, copies receiver bytes");
    ASSERT_EQ(ctx, strstr(string_src, "result at: (offset + i) put: (aString at: i).") != NULL, 1,
              "String>>, copies argument bytes");
}

