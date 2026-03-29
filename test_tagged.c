#include "test_defs.h"

void test_tagged(TestContext *ctx)
{
    uint64_t *om = ctx->om;
    uint64_t *class_class = ctx->class_class;
    uint64_t *smallint_class = ctx->smallint_class;
    uint64_t *block_class = ctx->block_class;
    uint64_t *test_class = ctx->test_class;
    uint64_t receiver = ctx->receiver;
    uint64_t method = ctx->method;
    uint64_t *class_table = ctx->class_table;
    uint64_t *stack = ctx->stack;
    (void)om;
    (void)class_class;
    (void)smallint_class;
    (void)block_class;
    (void)test_class;
    (void)receiver;
    (void)method;
    (void)class_table;
    (void)stack;
    uint64_t *sp;
    uint64_t ip;

    // --- Section 7: Tagged Pointers ---

    // Test: encode SmallInteger 0 and decode it back
    uint64_t tagged = tag_smallint(0);
    ASSERT_EQ(ctx, untag_smallint(tagged), 0, "SmallInt 0: encode/decode roundtrip");

    // Test: encode SmallInteger 42 and decode it back
    tagged = tag_smallint(42);
    ASSERT_EQ(ctx, untag_smallint(tagged), 42, "SmallInt 42: encode/decode roundtrip");
    ASSERT_EQ(ctx, tagged, (42ULL << 2) | 1, "SmallInt 42: raw tagged value is 169");

    // Test: encode SmallInteger -1 and decode it back
    tagged = tag_smallint(-1);
    ASSERT_EQ(ctx, (int64_t)untag_smallint(tagged), -1, "SmallInt -1: encode/decode roundtrip");

    // Test: detect tag: SmallInteger has bits 1:0 == 01
    ASSERT_EQ(ctx, get_tag(tag_smallint(42)), 1, "tag of SmallInt is 01");
    ASSERT_EQ(ctx, is_smallint(tag_smallint(42)), 1, "is_smallint(SmallInt 42)");

    // Test: detect tag: object pointer has bits 1:0 == 00
    uint64_t aligned_ptr = 0x1000; // 8-byte aligned, tag 00
    ASSERT_EQ(ctx, get_tag(aligned_ptr), 0, "tag of object pointer is 00");
    ASSERT_EQ(ctx, is_object_ptr(aligned_ptr), 1, "is_object_ptr(0x1000)");
    ASSERT_EQ(ctx, is_smallint(aligned_ptr), 0, "is_smallint(obj ptr) is 0");

    // Test: detect tag: immediate float has bits 1:0 == 10
    uint64_t fake_float = 0x42 | 2; // tag 10
    ASSERT_EQ(ctx, get_tag(fake_float), 2, "tag of immediate float is 10");
    ASSERT_EQ(ctx, is_immediate_float(fake_float), 1, "is_immediate_float");

    // Test: detect tag: special object has bits 1:0 == 11
    ASSERT_EQ(ctx, get_tag(tagged_nil()), 3, "tag of nil is 11");
    ASSERT_EQ(ctx, is_special(tagged_nil()), 1, "is_special(nil)");

    // Test: nil is the tagged value 0x03
    ASSERT_EQ(ctx, tagged_nil(), 0x03, "nil == 0x03");

    // Test: true is the tagged value 0x07
    ASSERT_EQ(ctx, tagged_true(), 0x07, "true == 0x07");

    // Test: false is the tagged value 0x0B
    ASSERT_EQ(ctx, tagged_false(), 0x0B, "false == 0x0B");

    // Test: is_nil check
    ASSERT_EQ(ctx, is_nil(tagged_nil()), 1, "is_nil(nil) == 1");
    ASSERT_EQ(ctx, is_nil(tagged_true()), 0, "is_nil(true) == 0");
    ASSERT_EQ(ctx, is_nil(tag_smallint(0)), 0, "is_nil(SmallInt 0) == 0");

    // Test: is_boolean check
    ASSERT_EQ(ctx, is_boolean(tagged_true()), 1, "is_boolean(true) == 1");
    ASSERT_EQ(ctx, is_boolean(tagged_false()), 1, "is_boolean(false) == 1");
    ASSERT_EQ(ctx, is_boolean(tagged_nil()), 0, "is_boolean(nil) == 0");
    ASSERT_EQ(ctx, is_boolean(tag_smallint(7)), 0, "is_boolean(SmallInt 7) == 0");

    // Test: SmallInteger addition
    uint64_t a = tag_smallint(3);
    uint64_t b = tag_smallint(4);
    uint64_t sum = smallint_add(a, b);
    ASSERT_EQ(ctx, untag_smallint(sum), 7, "3 + 4 = 7");
    ASSERT_EQ(ctx, is_smallint(sum), 1, "sum is tagged SmallInt");

    // Test: SmallInteger subtraction
    uint64_t diff = smallint_sub(tag_smallint(10), tag_smallint(3));
    ASSERT_EQ(ctx, untag_smallint(diff), 7, "10 - 3 = 7");
    ASSERT_EQ(ctx, is_smallint(diff), 1, "diff is tagged SmallInt");

    // Test: SmallInteger less-than
    ASSERT_EQ(ctx, smallint_less_than(tag_smallint(3), tag_smallint(5)),
              tagged_true(), "3 < 5 is true");
    ASSERT_EQ(ctx, smallint_less_than(tag_smallint(5), tag_smallint(3)),
              tagged_false(), "5 < 3 is false");
    ASSERT_EQ(ctx, smallint_less_than(tag_smallint(3), tag_smallint(3)),
              tagged_false(), "3 < 3 is false");

    // Test: SmallInteger equality
    ASSERT_EQ(ctx, smallint_equal(tag_smallint(42), tag_smallint(42)),
              tagged_true(), "42 = 42 is true");
    ASSERT_EQ(ctx, smallint_equal(tag_smallint(42), tag_smallint(43)),
              tagged_false(), "42 = 43 is false");

    // --- Character immediates ---

    // Test: tag_character($A = 65) has low 4 bits = 0x0F
    uint64_t charA = tag_character(65);
    ASSERT_EQ(ctx, charA & 0x0F, 0x0F, "Character $A: low 4 bits = 0x0F");

    // Test: tag_character / untag_character roundtrip
    ASSERT_EQ(ctx, untag_character(charA), 65, "Character $A: roundtrip = 65");

    // Test: is_character detects character immediates
    ASSERT_EQ(ctx, is_character(charA), 1, "is_character($A) == 1");
    ASSERT_EQ(ctx, is_character(tag_smallint(65)), 0, "is_character(SmallInt 65) == 0");
    ASSERT_EQ(ctx, is_character(tagged_nil()), 0, "is_character(nil) == 0");
    ASSERT_EQ(ctx, is_character(tagged_true()), 0, "is_character(true) == 0");

    // Test: Character encoding is (codePoint << 4) | 0x0F
    ASSERT_EQ(ctx, charA, (65ULL << 4) | 0x0F, "Character $A: raw value = (65<<4)|0x0F");

    // Test: zero character
    uint64_t charNul = tag_character(0);
    ASSERT_EQ(ctx, untag_character(charNul), 0, "Character NUL: roundtrip = 0");
    ASSERT_EQ(ctx, is_character(charNul), 1, "is_character(NUL) == 1");

    // Test: is_special still works (Characters are special too — tag 11)
    ASSERT_EQ(ctx, is_special(charA), 1, "is_special($A) == 1");

    ctx->smallint_class = smallint_class;
}
