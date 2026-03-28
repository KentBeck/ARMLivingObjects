#include "test_defs.h"
#include <string.h>

void test_persist(TestContext *ctx)
{
    uint64_t *om = ctx->om;
    uint64_t *class_class = ctx->class_class;

    // --- Serialize heap to buffer, deserialize, verify ---
    {
        // Source heap
        static uint8_t src_buf[8192] __attribute__((aligned(8)));
        uint64_t src[2];
        om_init(src_buf, 8192, src);

        // Create objects: A -> B, B has a SmallInt
        uint64_t *objB = om_alloc(src, (uint64_t)class_class, FORMAT_FIELDS, 1);
        OBJ_FIELD(objB, 0) = tag_smallint(42);

        uint64_t *objA = om_alloc(src, (uint64_t)class_class, FORMAT_FIELDS, 2);
        OBJ_FIELD(objA, 0) = (uint64_t)objB;
        OBJ_FIELD(objA, 1) = tagged_nil();

        uint64_t heap_used = src[0] - (uint64_t)src_buf;

        // Serialize: convert pointers to offsets
        static uint8_t image[8192];
        memcpy(image, src_buf, heap_used);
        image_pointers_to_offsets(image, heap_used, (uint64_t)src_buf);

        // Deserialize into a new region
        static uint8_t dst_buf[8192] __attribute__((aligned(8)));
        memcpy(dst_buf, image, heap_used);
        image_offsets_to_pointers(dst_buf, heap_used, (uint64_t)dst_buf);

        // Set up dst om
        uint64_t dst[2];
        dst[0] = (uint64_t)dst_buf + heap_used;
        dst[1] = (uint64_t)dst_buf + 8192;

        // Find the objects in dst: same offsets as in src
        uint64_t offB = (uint64_t)objB - (uint64_t)src_buf;
        uint64_t offA = (uint64_t)objA - (uint64_t)src_buf;
        uint64_t *newB = (uint64_t *)((uint64_t)dst_buf + offB);
        uint64_t *newA = (uint64_t *)((uint64_t)dst_buf + offA);

        ASSERT_EQ(ctx, OBJ_FIELD(newB, 0), tag_smallint(42),
                  "persist: B field 0 = 42");
        ASSERT_EQ(ctx, OBJ_FIELD(newA, 0), (uint64_t)newB,
                  "persist: A field 0 points to new B");
        ASSERT_EQ(ctx, OBJ_FIELD(newA, 1), tagged_nil(),
                  "persist: A field 1 = nil");

        // Class pointers should also be relocated
        // (class_class is outside src_buf, so it stays as-is)
        ASSERT_EQ(ctx, OBJ_CLASS(newA), (uint64_t)class_class,
                  "persist: A class preserved (external)");
        ASSERT_EQ(ctx, OBJ_FORMAT(newA), FORMAT_FIELDS,
                  "persist: A format preserved");
        ASSERT_EQ(ctx, OBJ_SIZE(newA), 2,
                  "persist: A size preserved");
    }

    // --- Serialize heap where class is also in the heap ---
    {
        static uint8_t src2[16384] __attribute__((aligned(8)));
        uint64_t s2[2];
        om_init(src2, 16384, s2);

        // Create a class in this heap
        uint64_t *my_class = om_alloc(s2, (uint64_t)class_class, FORMAT_FIELDS, 3);
        OBJ_FIELD(my_class, CLASS_SUPERCLASS) = tagged_nil();
        OBJ_FIELD(my_class, CLASS_INST_SIZE) = tag_smallint(1);
        OBJ_FIELD(my_class, CLASS_METHOD_DICT) = tagged_nil();

        // Instance of that class
        uint64_t *inst = om_alloc(s2, (uint64_t)my_class, FORMAT_FIELDS, 1);
        OBJ_FIELD(inst, 0) = tag_smallint(99);

        uint64_t used = s2[0] - (uint64_t)src2;

        static uint8_t img2[16384];
        memcpy(img2, src2, used);
        image_pointers_to_offsets(img2, used, (uint64_t)src2);

        static uint8_t dst2[16384] __attribute__((aligned(8)));
        memcpy(dst2, img2, used);
        image_offsets_to_pointers(dst2, used, (uint64_t)dst2);

        uint64_t off_class = (uint64_t)my_class - (uint64_t)src2;
        uint64_t off_inst = (uint64_t)inst - (uint64_t)src2;
        uint64_t *nc = (uint64_t *)((uint64_t)dst2 + off_class);
        uint64_t *ni = (uint64_t *)((uint64_t)dst2 + off_inst);

        ASSERT_EQ(ctx, OBJ_CLASS(ni), (uint64_t)nc,
                  "persist heap-class: instance class -> relocated class");
        ASSERT_EQ(ctx, OBJ_FIELD(ni, 0), tag_smallint(99),
                  "persist heap-class: instance field preserved");
        ASSERT_EQ(ctx, OBJ_FIELD(nc, CLASS_INST_SIZE), tag_smallint(1),
                  "persist heap-class: class inst_size preserved");
    }

    // --- Transaction log replay on loaded image ---
    {
        static uint8_t src3[8192] __attribute__((aligned(8)));
        uint64_t s3[2];
        om_init(src3, 8192, s3);

        uint64_t *obj = om_alloc(s3, (uint64_t)class_class, FORMAT_FIELDS, 2);
        OBJ_FIELD(obj, 0) = tag_smallint(10);
        OBJ_FIELD(obj, 1) = tag_smallint(20);

        uint64_t heap_used = s3[0] - (uint64_t)src3;

        // Serialize image
        static uint8_t img3[8192];
        memcpy(img3, src3, heap_used);
        image_pointers_to_offsets(img3, heap_used, (uint64_t)src3);

        // Create a transaction log entry: obj.field1 = 99
        // In the log, obj is identified by its offset in the image
        uint64_t obj_offset = (uint64_t)obj - (uint64_t)src3;
        // Log format: [count, (obj_offset, field_idx, value), ...]
        uint64_t log[1 + 3];
        log[0] = 1;
        log[1] = obj_offset;       // object (as offset)
        log[2] = 1;                // field index
        log[3] = tag_smallint(99); // new value (tagged, no relocation needed)

        // Load image into new buffer
        static uint8_t dst3[8192] __attribute__((aligned(8)));
        memcpy(dst3, img3, heap_used);
        image_offsets_to_pointers(dst3, heap_used, (uint64_t)dst3);

        // Replay log: convert obj_offset to pointer, apply
        uint64_t *loaded_obj = (uint64_t *)((uint64_t)dst3 + obj_offset);
        for (uint64_t i = 0; i < log[0]; i++)
        {
            uint64_t off = log[1 + i * 3];
            uint64_t field = log[2 + i * 3];
            uint64_t val = log[3 + i * 3];
            uint64_t *target = (uint64_t *)((uint64_t)dst3 + off);
            OBJ_FIELD(target, field) = val;
        }

        ASSERT_EQ(ctx, OBJ_FIELD(loaded_obj, 0), tag_smallint(10),
                  "persist log-replay: field 0 unchanged");
        ASSERT_EQ(ctx, OBJ_FIELD(loaded_obj, 1), tag_smallint(99),
                  "persist log-replay: field 1 updated to 99");
    }

    // --- Full cycle: save image to file, load, replay log, verify ---
    {
        static uint8_t src4[8192] __attribute__((aligned(8)));
        uint64_t s4[2];
        om_init(src4, 8192, s4);

        uint64_t *p = om_alloc(s4, (uint64_t)class_class, FORMAT_FIELDS, 1);
        OBJ_FIELD(p, 0) = tag_smallint(7);

        uint64_t used4 = s4[0] - (uint64_t)src4;

        // Save to file
        static uint8_t img4[8192];
        memcpy(img4, src4, used4);
        image_pointers_to_offsets(img4, used4, (uint64_t)src4);

        FILE *f = fopen("/tmp/arlo_test.image", "wb");
        fwrite(&used4, sizeof(used4), 1, f);
        fwrite(img4, 1, used4, f);
        fclose(f);

        // Modify via transaction
        uint64_t p_off = (uint64_t)p - (uint64_t)src4;
        FILE *lf = fopen("/tmp/arlo_test.log", "wb");
        uint64_t log_entry[4] = {1, p_off, 0, tag_smallint(42)};
        fwrite(log_entry, sizeof(uint64_t), 4, lf);
        fclose(lf);

        // Load image from file
        f = fopen("/tmp/arlo_test.image", "rb");
        uint64_t file_used;
        fread(&file_used, sizeof(file_used), 1, f);
        static uint8_t loaded[8192] __attribute__((aligned(8)));
        fread(loaded, 1, file_used, f);
        fclose(f);
        image_offsets_to_pointers(loaded, file_used, (uint64_t)loaded);

        // Replay log from file
        lf = fopen("/tmp/arlo_test.log", "rb");
        uint64_t lbuf[4];
        fread(lbuf, sizeof(uint64_t), 4, lf);
        fclose(lf);

        for (uint64_t i = 0; i < lbuf[0]; i++)
        {
            uint64_t *t = (uint64_t *)((uint64_t)loaded + lbuf[1 + i * 3]);
            OBJ_FIELD(t, lbuf[2 + i * 3]) = lbuf[3 + i * 3];
        }

        uint64_t *loaded_p = (uint64_t *)((uint64_t)loaded + p_off);
        ASSERT_EQ(ctx, OBJ_FIELD(loaded_p, 0), tag_smallint(42),
                  "persist file: loaded + replayed = 42");
    }
}
