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

    // --- Append-only log: multiple commits ---
    {
        static uint8_t src5[8192] __attribute__((aligned(8)));
        uint64_t s5[2];
        om_init(src5, 8192, s5);

        uint64_t *obj5 = om_alloc(s5, (uint64_t)class_class, FORMAT_FIELDS, 3);
        OBJ_FIELD(obj5, 0) = tag_smallint(1);
        OBJ_FIELD(obj5, 1) = tag_smallint(2);
        OBJ_FIELD(obj5, 2) = tag_smallint(3);

        uint64_t used5 = s5[0] - (uint64_t)src5;
        uint64_t off5 = (uint64_t)obj5 - (uint64_t)src5;

        // Save image
        static uint8_t img5[8192];
        memcpy(img5, src5, used5);
        image_pointers_to_offsets(img5, used5, (uint64_t)src5);

        FILE *f5 = fopen("/tmp/arlo_multi.image", "wb");
        fwrite(&used5, sizeof(used5), 1, f5);
        fwrite(img5, 1, used5, f5);
        fclose(f5);

        // Append 3 separate commits to the log file
        FILE *lf5 = fopen("/tmp/arlo_multi.log", "wb");
        // Commit 1: field 0 = 10
        uint64_t e1[3] = {off5, 0, tag_smallint(10)};
        fwrite(e1, sizeof(uint64_t), 3, lf5);
        // Commit 2: field 1 = 20
        uint64_t e2[3] = {off5, 1, tag_smallint(20)};
        fwrite(e2, sizeof(uint64_t), 3, lf5);
        // Commit 3: field 2 = 30
        uint64_t e3[3] = {off5, 2, tag_smallint(30)};
        fwrite(e3, sizeof(uint64_t), 3, lf5);
        fclose(lf5);

        // Load image
        f5 = fopen("/tmp/arlo_multi.image", "rb");
        uint64_t fu5;
        fread(&fu5, sizeof(fu5), 1, f5);
        static uint8_t ld5[8192] __attribute__((aligned(8)));
        fread(ld5, 1, fu5, f5);
        fclose(f5);
        image_offsets_to_pointers(ld5, fu5, (uint64_t)ld5);

        // Replay log: read all triples
        lf5 = fopen("/tmp/arlo_multi.log", "rb");
        uint64_t triple[3];
        while (fread(triple, sizeof(uint64_t), 3, lf5) == 3)
        {
            uint64_t *t = (uint64_t *)((uint64_t)ld5 + triple[0]);
            OBJ_FIELD(t, triple[1]) = triple[2];
        }
        fclose(lf5);

        uint64_t *r5 = (uint64_t *)((uint64_t)ld5 + off5);
        ASSERT_EQ(ctx, OBJ_FIELD(r5, 0), tag_smallint(10),
                  "persist multi-log: field 0 = 10");
        ASSERT_EQ(ctx, OBJ_FIELD(r5, 1), tag_smallint(20),
                  "persist multi-log: field 1 = 20");
        ASSERT_EQ(ctx, OBJ_FIELD(r5, 2), tag_smallint(30),
                  "persist multi-log: field 2 = 30");
    }

    // --- Log with pointer values: offset conversion ---
    {
        static uint8_t src6[8192] __attribute__((aligned(8)));
        uint64_t s6[2];
        om_init(src6, 8192, s6);

        uint64_t *target6 = om_alloc(s6, (uint64_t)class_class, FORMAT_FIELDS, 1);
        OBJ_FIELD(target6, 0) = tag_smallint(555);

        uint64_t *holder6 = om_alloc(s6, (uint64_t)class_class, FORMAT_FIELDS, 1);
        OBJ_FIELD(holder6, 0) = tagged_nil();

        uint64_t used6 = s6[0] - (uint64_t)src6;
        uint64_t off_target = (uint64_t)target6 - (uint64_t)src6;
        uint64_t off_holder = (uint64_t)holder6 - (uint64_t)src6;

        // Save image
        static uint8_t img6[8192];
        memcpy(img6, src6, used6);
        image_pointers_to_offsets(img6, used6, (uint64_t)src6);

        // Log: holder.field0 = target (pointer value as offset)
        // In the log file, pointer values are stored as offsets
        uint64_t log_entry6[3] = {off_holder, 0, off_target};
        // Mark offset values: set bit 2 to distinguish from SmallInt
        // Actually, offsets are 8-byte aligned (low 3 bits = 0).
        // SmallInt has bit 0 set. So offset IS distinguishable.
        // The log replay needs to convert offset values to pointers.

        // Load image
        static uint8_t ld6[8192] __attribute__((aligned(8)));
        memcpy(ld6, img6, used6);
        image_offsets_to_pointers(ld6, used6, (uint64_t)ld6);

        // Replay log: convert value if it looks like an offset
        uint64_t *t6 = (uint64_t *)((uint64_t)ld6 + log_entry6[0]);
        uint64_t val6 = log_entry6[2];
        // If val has tag 00 and < used6, it's an offset → convert to pointer
        if ((val6 & 3) == 0 && val6 < used6)
            val6 = (uint64_t)ld6 + val6;
        OBJ_FIELD(t6, log_entry6[1]) = val6;

        uint64_t *loaded_holder = (uint64_t *)((uint64_t)ld6 + off_holder);
        uint64_t *loaded_target = (uint64_t *)((uint64_t)ld6 + off_target);

        ASSERT_EQ(ctx, OBJ_FIELD(loaded_holder, 0), (uint64_t)loaded_target,
                  "persist ptr-log: holder points to loaded target");
        ASSERT_EQ(ctx, OBJ_FIELD(loaded_target, 0), tag_smallint(555),
                  "persist ptr-log: target data preserved");
    }

    // --- Checkpoint: bake log into new image ---
    {
        static uint8_t src7[8192] __attribute__((aligned(8)));
        uint64_t s7[2];
        om_init(src7, 8192, s7);

        uint64_t *obj7 = om_alloc(s7, (uint64_t)class_class, FORMAT_FIELDS, 2);
        OBJ_FIELD(obj7, 0) = tag_smallint(1);
        OBJ_FIELD(obj7, 1) = tag_smallint(2);

        uint64_t used7 = s7[0] - (uint64_t)src7;
        uint64_t off7 = (uint64_t)obj7 - (uint64_t)src7;

        // Save initial image
        static uint8_t img7[8192];
        memcpy(img7, src7, used7);
        image_pointers_to_offsets(img7, used7, (uint64_t)src7);

        // Apply changes in memory (simulating log replay)
        static uint8_t live7[8192] __attribute__((aligned(8)));
        memcpy(live7, img7, used7);
        image_offsets_to_pointers(live7, used7, (uint64_t)live7);

        uint64_t *live_obj = (uint64_t *)((uint64_t)live7 + off7);
        OBJ_FIELD(live_obj, 0) = tag_smallint(100);
        OBJ_FIELD(live_obj, 1) = tag_smallint(200);

        // Checkpoint: save the modified heap as a new image
        static uint8_t chk[8192];
        memcpy(chk, live7, used7);
        image_pointers_to_offsets(chk, used7, (uint64_t)live7);

        // Load from checkpoint (no log needed)
        static uint8_t ld7[8192] __attribute__((aligned(8)));
        memcpy(ld7, chk, used7);
        image_offsets_to_pointers(ld7, used7, (uint64_t)ld7);

        uint64_t *r7 = (uint64_t *)((uint64_t)ld7 + off7);
        ASSERT_EQ(ctx, OBJ_FIELD(r7, 0), tag_smallint(100),
                  "persist checkpoint: field 0 = 100");
        ASSERT_EQ(ctx, OBJ_FIELD(r7, 1), tag_smallint(200),
                  "persist checkpoint: field 1 = 200");
    }
}
