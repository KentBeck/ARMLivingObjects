#include "test_defs.h"
#include "bootstrap_compiler.h"
#include "primitives.h"
#include <string.h>

static uint64_t *make_test_string(uint64_t *om, uint64_t *string_class, const char *text)
{
    uint64_t size = (uint64_t)strlen(text);
    uint64_t *obj = om_alloc(om, (uint64_t)string_class, FORMAT_BYTES, size);
    uint8_t *data = (uint8_t *)&OBJ_FIELD(obj, 0);
    memcpy(data, text, size);
    return obj;
}

static void smalltalk_at_put(uint64_t *om, uint64_t *array_class, uint64_t *association_class,
                             const char *name, uint64_t value)
{
    uint64_t key = intern_cstring_symbol(om, name);
    uint64_t associations_oop = OBJ_FIELD(global_smalltalk_dictionary, 0);
    uint64_t tally = OBJ_FIELD(global_smalltalk_dictionary, 1) == tagged_nil()
                         ? 0
                         : (uint64_t)untag_smallint(OBJ_FIELD(global_smalltalk_dictionary, 1));

    if (associations_oop == tagged_nil())
    {
        uint64_t *associations = om_alloc(om, (uint64_t)array_class, FORMAT_INDEXABLE, 8);
        for (uint64_t index = 0; index < 8; index++)
        {
            OBJ_FIELD(associations, index) = tagged_nil();
        }
        OBJ_FIELD(global_smalltalk_dictionary, 0) = (uint64_t)associations;
        OBJ_FIELD(global_smalltalk_dictionary, 1) = tag_smallint(0);
        associations_oop = (uint64_t)associations;
    }

    uint64_t *associations = (uint64_t *)associations_oop;
    for (uint64_t index = 0; index < tally; index++)
    {
        uint64_t assoc_oop = OBJ_FIELD(associations, index);
        if (!is_object_ptr(assoc_oop))
        {
            continue;
        }
        uint64_t *assoc = (uint64_t *)assoc_oop;
        if (OBJ_FIELD(assoc, 0) == key)
        {
            OBJ_FIELD(assoc, 1) = value;
            return;
        }
    }

    uint64_t *assoc = om_alloc(om, (uint64_t)association_class, FORMAT_FIELDS, 2);
    OBJ_FIELD(assoc, 0) = key;
    OBJ_FIELD(assoc, 1) = value;
    OBJ_FIELD(associations, tally) = (uint64_t)assoc;
    OBJ_FIELD(global_smalltalk_dictionary, 1) = tag_smallint((int64_t)(tally + 1));
}

void test_persist(TestContext *ctx)
{
    uint64_t *om = ctx->om;
    uint64_t *class_class = ctx->class_class;

    // --- Page bookkeeping: count, ownership, fullness ---
    {
        static uint8_t page_buf[8192] __attribute__((aligned(8)));
        uint64_t page_om[2];
        uint64_t *first;
        uint64_t *last = NULL;
        uint64_t first_page;
        uint64_t last_page;

        om_init(page_buf, sizeof(page_buf), page_om);
        ASSERT_EQ(ctx, om_page_bytes(), (uint64_t)OM_PAGE_BYTES,
                  "pages: page size constant exposed");
        ASSERT_EQ(ctx, om_page_count(page_om), (uint64_t)2,
                  "pages: 8192-byte heap reports two 4KB pages");
        ASSERT_EQ(ctx, om_page_start(page_om, 0), (uint64_t)page_buf,
                  "pages: first page starts at heap base");
        ASSERT_EQ(ctx, om_page_start(page_om, 1), (uint64_t)(page_buf + OM_PAGE_BYTES),
                  "pages: second page start follows first by page size");
        ASSERT_EQ(ctx, om_page_used_bytes(page_om, 0), (uint64_t)0,
                  "pages: empty first page has zero used bytes");
        ASSERT_EQ(ctx, om_page_used_bytes(page_om, 1), (uint64_t)0,
                  "pages: empty second page has zero used bytes");

        first = om_alloc(page_om, (uint64_t)class_class, FORMAT_FIELDS, 1);
        ASSERT_EQ(ctx, first != NULL, 1, "pages: first object allocated");
        first_page = om_page_id_for_address(page_om, (uint64_t)first);
        ASSERT_EQ(ctx, first_page, (uint64_t)0,
                  "pages: first object belongs to first page");
        ASSERT_EQ(ctx, om_page_used_bytes(page_om, 0), (uint64_t)(4 * WORD_BYTES),
                  "pages: first page used bytes track first allocation");
        ASSERT_EQ(ctx, om_object_spans_pages(page_om, first), (uint64_t)0,
                  "pages: small object stays within one page");

        while ((last = om_alloc(page_om, (uint64_t)class_class, FORMAT_FIELDS, 8)) != NULL)
        {
            if (om_page_id_for_address(page_om, (uint64_t)last) == 1)
            {
                break;
            }
        }

        ASSERT_EQ(ctx, last != NULL, 1,
                  "pages: allocation eventually reaches second page");
        last_page = om_page_id_for_address(page_om, (uint64_t)last);
        ASSERT_EQ(ctx, last_page, (uint64_t)1,
                  "pages: later object belongs to second page");
        ASSERT_EQ(ctx, om_page_used_bytes(page_om, 0), (uint64_t)OM_PAGE_BYTES,
                  "pages: first page reports full once allocation crosses boundary");
        ASSERT_EQ(ctx, om_page_used_bytes(page_om, 1) > 0, (uint64_t)1,
                  "pages: second page reports non-zero used bytes after allocation");
    }

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
        uint64_t *my_class = om_alloc(s2, (uint64_t)class_class, FORMAT_FIELDS, 4);
        OBJ_FIELD(my_class, CLASS_SUPERCLASS) = tagged_nil();
        OBJ_FIELD(my_class, CLASS_INST_SIZE) = tag_smallint(1);
        OBJ_FIELD(my_class, CLASS_METHOD_DICT) = tagged_nil();
        OBJ_FIELD(my_class, CLASS_INST_FORMAT) = tag_smallint(FORMAT_FIELDS);

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

    // --- Raw zero words survive save/load without becoming heap_base ---
    {
        static uint8_t srcz[8192] __attribute__((aligned(8)));
        uint64_t sz[2];
        om_init(srcz, 8192, sz);

        uint64_t *first = om_alloc(sz, (uint64_t)class_class, FORMAT_FIELDS, 1);
        OBJ_FIELD(first, 0) = tag_smallint(123);

        uint64_t *holder = om_alloc(sz, (uint64_t)class_class, FORMAT_FIELDS, 2);
        OBJ_FIELD(holder, 0) = 0;
        OBJ_FIELD(holder, 1) = (uint64_t)first;

        uint64_t usedz = sz[0] - (uint64_t)srcz;
        static uint8_t imgz[8192];
        memcpy(imgz, srcz, usedz);
        image_pointers_to_offsets(imgz, usedz, (uint64_t)srcz);

        static uint8_t dstz[8192] __attribute__((aligned(8)));
        memcpy(dstz, imgz, usedz);
        image_offsets_to_pointers(dstz, usedz, (uint64_t)dstz);

        uint64_t off_first = (uint64_t)first - (uint64_t)srcz;
        uint64_t off_holder = (uint64_t)holder - (uint64_t)srcz;
        uint64_t *new_first = (uint64_t *)((uint64_t)dstz + off_first);
        uint64_t *new_holder = (uint64_t *)((uint64_t)dstz + off_holder);

        ASSERT_EQ(ctx, OBJ_FIELD(new_holder, 0), 0,
                  "persist zero: raw zero stays zero after reload");
        ASSERT_EQ(ctx, OBJ_FIELD(new_holder, 1), (uint64_t)new_first,
                  "persist zero: first object pointer still relocates correctly");
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

    // --- Reload an image and execute a selector from the loaded heap ---
    {
        static uint8_t src8[32768] __attribute__((aligned(8)));
        uint64_t s8[2];
        om_init(src8, sizeof(src8), s8);

        uint64_t *image_class = om_alloc(s8, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(image_class, CLASS_SUPERCLASS) = tagged_nil();
        OBJ_FIELD(image_class, CLASS_METHOD_DICT) = tagged_nil();
        OBJ_FIELD(image_class, CLASS_INST_SIZE) = tag_smallint(1);
        OBJ_FIELD(image_class, CLASS_INST_FORMAT) = tag_smallint(FORMAT_FIELDS);

        uint64_t *image_ivars = om_alloc(s8, (uint64_t)class_class, FORMAT_INDEXABLE, 1);
        OBJ_FIELD(image_ivars, 0) = (uint64_t)make_test_string(s8, ctx->string_class, "slot0");
        OBJ_FIELD(image_class, CLASS_INST_VARS) = (uint64_t)image_ivars;

        const char *association_ivars[] = {"key", "value"};
        const char *dictionary_ivars[] = {"associations", "tally"};
        uint64_t *image_symbol_table = om_alloc(s8, (uint64_t)class_class, FORMAT_INDEXABLE, 64);
        for (int i = 0; i < 64; i++)
        {
            OBJ_FIELD(image_symbol_table, i) = tagged_nil();
        }
        uint64_t *saved_global_symbol_table = global_symbol_table;
        uint64_t *saved_global_symbol_class = global_symbol_class;
        uint64_t *saved_global_smalltalk_dictionary = global_smalltalk_dictionary;
        global_symbol_table = image_symbol_table;
        global_symbol_class = ctx->symbol_class;
        uint64_t *array_class = om_alloc(s8, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(array_class, CLASS_SUPERCLASS) = tagged_nil();
        OBJ_FIELD(array_class, CLASS_METHOD_DICT) = tagged_nil();
        OBJ_FIELD(array_class, CLASS_INST_SIZE) = tag_smallint(0);
        OBJ_FIELD(array_class, CLASS_INST_FORMAT) = tag_smallint(FORMAT_INDEXABLE);
        OBJ_FIELD(array_class, CLASS_INST_VARS) = tagged_nil();
        uint64_t *association_class = om_alloc(s8, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(association_class, CLASS_SUPERCLASS) = tagged_nil();
        OBJ_FIELD(association_class, CLASS_METHOD_DICT) = tagged_nil();
        OBJ_FIELD(association_class, CLASS_INST_SIZE) = tag_smallint(2);
        OBJ_FIELD(association_class, CLASS_INST_FORMAT) = tag_smallint(FORMAT_FIELDS);
        uint64_t *association_ivars_obj = om_alloc(s8, (uint64_t)class_class, FORMAT_INDEXABLE, 2);
        OBJ_FIELD(association_ivars_obj, 0) = (uint64_t)make_test_string(s8, ctx->string_class, association_ivars[0]);
        OBJ_FIELD(association_ivars_obj, 1) = (uint64_t)make_test_string(s8, ctx->string_class, association_ivars[1]);
        OBJ_FIELD(association_class, CLASS_INST_VARS) = (uint64_t)association_ivars_obj;
        uint64_t *dictionary_class = om_alloc(s8, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(dictionary_class, CLASS_SUPERCLASS) = tagged_nil();
        OBJ_FIELD(dictionary_class, CLASS_METHOD_DICT) = tagged_nil();
        OBJ_FIELD(dictionary_class, CLASS_INST_SIZE) = tag_smallint(2);
        OBJ_FIELD(dictionary_class, CLASS_INST_FORMAT) = tag_smallint(FORMAT_FIELDS);
        uint64_t *dictionary_ivars_obj = om_alloc(s8, (uint64_t)class_class, FORMAT_INDEXABLE, 2);
        OBJ_FIELD(dictionary_ivars_obj, 0) = (uint64_t)make_test_string(s8, ctx->string_class, dictionary_ivars[0]);
        OBJ_FIELD(dictionary_ivars_obj, 1) = (uint64_t)make_test_string(s8, ctx->string_class, dictionary_ivars[1]);
        OBJ_FIELD(dictionary_class, CLASS_INST_VARS) = (uint64_t)dictionary_ivars_obj;
        global_smalltalk_dictionary = om_alloc(s8, (uint64_t)dictionary_class, FORMAT_FIELDS, 2);
        OBJ_FIELD(global_smalltalk_dictionary, 0) = tagged_nil();
        OBJ_FIELD(global_smalltalk_dictionary, 1) = tag_smallint(0);
        smalltalk_at_put(s8, array_class, association_class, "String", (uint64_t)ctx->string_class);
        smalltalk_at_put(s8, array_class, association_class, "Array", (uint64_t)array_class);
        smalltalk_at_put(s8, array_class, association_class, "Association", (uint64_t)association_class);
        smalltalk_at_put(s8, array_class, association_class, "Dictionary", (uint64_t)dictionary_class);
        smalltalk_at_put(s8, array_class, association_class, "ImageThing", (uint64_t)image_class);
        const char *source =
            "!ImageThing methodsFor: 'testing'!\n"
            "answer\n"
            "    ^ slot0\n"
            "!\n";
        ASSERT_EQ(ctx,
                  bc_compile_and_install_source_methods(s8, class_class, NULL, 0, source),
                  1,
                  "persist image-exec: source methods install");
        uint64_t answer_selector = intern_cstring_symbol(s8, "answer");
        global_symbol_table = saved_global_symbol_table;
        global_symbol_class = saved_global_symbol_class;
        global_smalltalk_dictionary = saved_global_smalltalk_dictionary;

        uint64_t *receiver = om_alloc(s8, (uint64_t)image_class, FORMAT_FIELDS, 1);
        OBJ_FIELD(receiver, 0) = tag_smallint(42);

        // Root manifest for future image entrypoints: receiver, class, selector.
        uint64_t *roots = om_alloc(s8, (uint64_t)class_class, FORMAT_INDEXABLE, 3);
        OBJ_FIELD(roots, 0) = (uint64_t)receiver;
        OBJ_FIELD(roots, 1) = (uint64_t)image_class;
        OBJ_FIELD(roots, 2) = answer_selector;

        uint64_t used8 = s8[0] - (uint64_t)src8;
        uint64_t roots_off = (uint64_t)roots - (uint64_t)src8;

        static uint8_t img8[32768];
        memcpy(img8, src8, used8);
        image_pointers_to_offsets(img8, used8, (uint64_t)src8);

        static uint8_t ld8[32768] __attribute__((aligned(8)));
        memcpy(ld8, img8, used8);
        image_offsets_to_pointers(ld8, used8, (uint64_t)ld8);

        uint64_t *loaded_roots = (uint64_t *)((uint64_t)ld8 + roots_off);
        uint64_t *loaded_receiver = (uint64_t *)OBJ_FIELD(loaded_roots, 0);
        uint64_t *loaded_class = (uint64_t *)OBJ_FIELD(loaded_roots, 1);
        uint64_t selector = OBJ_FIELD(loaded_roots, 2);

        ASSERT_EQ(ctx, OBJ_FIELD(loaded_class, CLASS_INST_VARS) != 0, 1,
                  "persist image-exec: class keeps ivar-name array");
        uint64_t *loaded_ivars = (uint64_t *)OBJ_FIELD(loaded_class, CLASS_INST_VARS);
        ASSERT_EQ(ctx, OBJ_SIZE(loaded_ivars), 1,
                  "persist image-exec: ivar-name array size preserved");
        uint64_t *loaded_slot0 = (uint64_t *)OBJ_FIELD(loaded_ivars, 0);
        ASSERT_EQ(ctx, OBJ_SIZE(loaded_slot0), 5,
                  "persist image-exec: ivar-name string size preserved");
        ASSERT_EQ(ctx, memcmp((uint8_t *)&OBJ_FIELD(loaded_slot0, 0), "slot0", 5) == 0, 1,
                  "persist image-exec: ivar-name string bytes preserved");

        uint64_t method_oop = class_lookup(loaded_class, selector);
        ASSERT_EQ(ctx, method_oop != 0, 1,
                  "persist image-exec: method lookup succeeds after reload");

        uint64_t *compiled_method = (uint64_t *)method_oop;
        uint64_t *bytecodes = (uint64_t *)OBJ_FIELD(compiled_method, CM_BYTECODES);
        uint64_t stack[STACK_WORDS];
        uint64_t *sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
        uint64_t *fp = (uint64_t *)0xCAFE;
        stack_push(&sp, stack, (uint64_t)loaded_receiver);
        activate_method(&sp, &fp, 0, (uint64_t)compiled_method, 0, 0);

        uint64_t result = interpret(&sp, &fp, (uint8_t *)&OBJ_FIELD(bytecodes, 0), ctx->class_table, s8, NULL);
        ASSERT_EQ(ctx, result, tag_smallint(42),
                  "persist image-exec: loaded image method returns expected value");
    }
}
