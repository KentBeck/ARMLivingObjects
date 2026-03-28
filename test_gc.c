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

    // --- gc_collect: copy reachable objects, update pointers ---
    {
        // Set up a small from-space and to-space
        static uint8_t from_buf[8192] __attribute__((aligned(8)));
        static uint8_t to_buf3[8192] __attribute__((aligned(8)));
        uint64_t from[2], to[2];
        om_init(from_buf, 8192, from);
        om_init(to_buf3, 8192, to);

        // Allocate in from-space: A -> B, and unreferenced C
        uint64_t *objB = om_alloc(from, (uint64_t)class_class, FORMAT_FIELDS, 1);
        OBJ_FIELD(objB, 0) = tag_smallint(77);

        uint64_t *objA = om_alloc(from, (uint64_t)class_class, FORMAT_FIELDS, 1);
        OBJ_FIELD(objA, 0) = (uint64_t)objB; // A points to B

        uint64_t *objC = om_alloc(from, (uint64_t)class_class, FORMAT_FIELDS, 1);
        OBJ_FIELD(objC, 0) = tag_smallint(55);
        (void)objC;

        // Roots: just objA
        uint64_t roots[1];
        roots[0] = (uint64_t)objA;

        gc_collect(roots, 1, from, to,
                   (uint64_t)from_buf, (uint64_t)(from_buf + 8192));

        // roots[0] should now point to the copy of A in to-space
        uint64_t *newA = (uint64_t *)roots[0];
        ASSERT_EQ(ctx, OBJ_FORMAT(newA), FORMAT_FIELDS,
                  "gc_collect: A format preserved");

        // newA's field 0 should point to the copy of B (not old B)
        uint64_t *newB = (uint64_t *)OBJ_FIELD(newA, 0);
        ASSERT_EQ(ctx, OBJ_FIELD(newB, 0), tag_smallint(77),
                  "gc_collect: B field preserved");

        // Both should be in to-space
        ASSERT_EQ(ctx, (uint64_t)newA >= (uint64_t)to_buf3, 1,
                  "gc_collect: A is in to-space");
        ASSERT_EQ(ctx, (uint64_t)newB >= (uint64_t)to_buf3, 1,
                  "gc_collect: B is in to-space");
    }

    // --- gc_scan_roots: walk stack frames, collect object pointers ---
    {
        uint64_t *om = ctx->om;
        uint64_t *class_class = ctx->class_class;
        uint64_t *stack = ctx->stack;
        uint64_t *sp, *fp;

        // Create two objects
        uint64_t *objX = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 1);
        OBJ_FIELD(objX, 0) = tag_smallint(1);
        uint64_t *objY = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 1);
        OBJ_FIELD(objY, 0) = tag_smallint(2);

        // Method with 0 args, 1 temp
        uint64_t *tcm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        uint64_t *tbc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 2);
        ((uint8_t *)&OBJ_FIELD(tbc, 0))[0] = BC_HALT;
        OBJ_FIELD(tcm, CM_PRIMITIVE) = tag_smallint(0);
        OBJ_FIELD(tcm, CM_NUM_ARGS) = tag_smallint(0);
        OBJ_FIELD(tcm, CM_NUM_TEMPS) = tag_smallint(1);
        OBJ_FIELD(tcm, CM_LITERALS) = tagged_nil();
        OBJ_FIELD(tcm, CM_BYTECODES) = (uint64_t)tbc;

        // Build a stack frame: receiver=objX, temp0=objY
        sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
        fp = (uint64_t *)0xCAFE;
        stack_push(&sp, stack, (uint64_t)objX); // receiver
        activate_method(&sp, &fp, 0, (uint64_t)tcm, 0, 1);
        // Store objY into temp 0
        frame_store_temp(fp, 0, (uint64_t)objY);

        // Scan roots from stack
        uint64_t root_buf[32];
        uint64_t num_found = gc_scan_stack(fp, root_buf, 32);

        // Should find: receiver (objX), temp0 (objY), method (tcm)
        // At minimum, receiver and temp must be found
        ASSERT_EQ(ctx, num_found >= 2, 1,
                  "gc_scan_stack: found at least 2 roots");

        // Check that objX and objY are in the root buffer
        int found_x = 0, found_y = 0;
        for (uint64_t i = 0; i < num_found; i++)
        {
            if (root_buf[i] == (uint64_t)objX)
                found_x = 1;
            if (root_buf[i] == (uint64_t)objY)
                found_y = 1;
        }
        ASSERT_EQ(ctx, found_x, 1, "gc_scan_stack: found objX");
        ASSERT_EQ(ctx, found_y, 1, "gc_scan_stack: found objY");
    }
}
