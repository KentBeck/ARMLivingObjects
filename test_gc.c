#include "test_defs.h"

void test_gc(TestContext *ctx)
{
    uint64_t *om = ctx->om;
    uint64_t *class_class = ctx->class_class;
    (void)om;

    // --- gc_copy_object: copy a single object to to-space ---
    {
        // Allocate a to-space buffer
        static uint8_t to_buf[4096] __attribute__((aligned(8)));
        uint64_t to_space[2];
        om_init(to_buf, 4096, to_space);

        // Create an object with 2 fields in the main heap
        uint64_t *obj = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 2);
        OBJ_FIELD(obj, 0) = tag_smallint(42);
        OBJ_FIELD(obj, 1) = tag_smallint(99);

        // Copy it
        uint64_t *copy = gc_copy_object(obj, to_space);

        // New copy has same header and fields
        ASSERT_EQ(ctx, OBJ_CLASS(copy), (uint64_t)class_class,
                  "gc_copy: class preserved");
        ASSERT_EQ(ctx, OBJ_FORMAT(copy), FORMAT_FIELDS,
                  "gc_copy: format preserved");
        ASSERT_EQ(ctx, OBJ_SIZE(copy), 2,
                  "gc_copy: size preserved");
        ASSERT_EQ(ctx, OBJ_FIELD(copy, 0), tag_smallint(42),
                  "gc_copy: field 0 preserved");
        ASSERT_EQ(ctx, OBJ_FIELD(copy, 1), tag_smallint(99),
                  "gc_copy: field 1 preserved");

        // Old object has forwarding pointer
        ASSERT_EQ(ctx, gc_is_forwarded(obj), 1,
                  "gc_copy: old obj is forwarded");
        ASSERT_EQ(ctx, (uint64_t)gc_forwarding_ptr(obj), (uint64_t)copy,
                  "gc_copy: forwarding ptr points to copy");
    }

    // --- gc_copy_object: copying a forwarded object returns existing copy ---
    {
        static uint8_t to_buf2[4096] __attribute__((aligned(8)));
        uint64_t to_space2[2];
        om_init(to_buf2, 4096, to_space2);

        uint64_t *obj2 = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 1);
        OBJ_FIELD(obj2, 0) = tag_smallint(7);

        uint64_t *copy1 = gc_copy_object(obj2, to_space2);
        uint64_t *copy2 = gc_copy_object(obj2, to_space2);

        // Second copy returns same pointer (doesn't copy again)
        ASSERT_EQ(ctx, (uint64_t)copy1, (uint64_t)copy2,
                  "gc_copy: forwarded obj returns existing copy");
    }
}

