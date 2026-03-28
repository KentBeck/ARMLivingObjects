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

    // --- Circular reference: A -> B -> A ---
    {
        static uint8_t fb4[8192] __attribute__((aligned(8)));
        static uint8_t tb4[8192] __attribute__((aligned(8)));
        uint64_t from4[2], to4[2];
        om_init(fb4, 8192, from4);
        om_init(tb4, 8192, to4);

        uint64_t *cA = om_alloc(from4, (uint64_t)class_class, FORMAT_FIELDS, 1);
        uint64_t *cB = om_alloc(from4, (uint64_t)class_class, FORMAT_FIELDS, 1);
        OBJ_FIELD(cA, 0) = (uint64_t)cB;
        OBJ_FIELD(cB, 0) = (uint64_t)cA; // cycle

        uint64_t roots4[1];
        roots4[0] = (uint64_t)cA;
        gc_collect(roots4, 1, from4, to4,
                   (uint64_t)fb4, (uint64_t)(fb4 + 8192));

        uint64_t *nA = (uint64_t *)roots4[0];
        uint64_t *nB = (uint64_t *)OBJ_FIELD(nA, 0);
        ASSERT_EQ(ctx, (uint64_t)OBJ_FIELD(nB, 0), (uint64_t)nA,
                  "gc cycle: B->A updated to new A");
        ASSERT_EQ(ctx, (uint64_t)nA >= (uint64_t)tb4, 1,
                  "gc cycle: A in to-space");
        ASSERT_EQ(ctx, (uint64_t)nB >= (uint64_t)tb4, 1,
                  "gc cycle: B in to-space");
    }

    // --- Self-referencing object ---
    {
        static uint8_t fb5[8192] __attribute__((aligned(8)));
        static uint8_t tb5[8192] __attribute__((aligned(8)));
        uint64_t from5[2], to5[2];
        om_init(fb5, 8192, from5);
        om_init(tb5, 8192, to5);

        uint64_t *self_ref = om_alloc(from5, (uint64_t)class_class, FORMAT_FIELDS, 2);
        OBJ_FIELD(self_ref, 0) = (uint64_t)self_ref; // points to self
        OBJ_FIELD(self_ref, 1) = tag_smallint(88);

        uint64_t roots5[1];
        roots5[0] = (uint64_t)self_ref;
        gc_collect(roots5, 1, from5, to5,
                   (uint64_t)fb5, (uint64_t)(fb5 + 8192));

        uint64_t *ns = (uint64_t *)roots5[0];
        ASSERT_EQ(ctx, (uint64_t)OBJ_FIELD(ns, 0), (uint64_t)ns,
                  "gc self-ref: field 0 points to self");
        ASSERT_EQ(ctx, OBJ_FIELD(ns, 1), tag_smallint(88),
                  "gc self-ref: field 1 preserved");
    }

    // --- Deep chain: A -> B -> C -> D -> E ---
    {
        static uint8_t fb6[16384] __attribute__((aligned(8)));
        static uint8_t tb6[16384] __attribute__((aligned(8)));
        uint64_t from6[2], to6[2];
        om_init(fb6, 16384, from6);
        om_init(tb6, 16384, to6);

        uint64_t *chain[5];
        for (int i = 0; i < 5; i++)
            chain[i] = om_alloc(from6, (uint64_t)class_class, FORMAT_FIELDS, 2);
        for (int i = 0; i < 4; i++)
            OBJ_FIELD(chain[i], 0) = (uint64_t)chain[i + 1];
        OBJ_FIELD(chain[4], 0) = tag_smallint(0); // end
        for (int i = 0; i < 5; i++)
            OBJ_FIELD(chain[i], 1) = tag_smallint(i * 10);

        uint64_t roots6[1];
        roots6[0] = (uint64_t)chain[0];
        gc_collect(roots6, 1, from6, to6,
                   (uint64_t)fb6, (uint64_t)(fb6 + 16384));

        // Walk the chain in to-space
        uint64_t *cur = (uint64_t *)roots6[0];
        for (int i = 0; i < 5; i++)
        {
            ASSERT_EQ(ctx, (uint64_t)cur >= (uint64_t)tb6, 1,
                      "gc chain: obj in to-space");
            ASSERT_EQ(ctx, OBJ_FIELD(cur, 1), tag_smallint(i * 10),
                      "gc chain: data preserved");
            if (i < 4)
                cur = (uint64_t *)OBJ_FIELD(cur, 0);
        }
    }

    // --- Mixed formats: fields + indexable + bytes all survive ---
    {
        static uint8_t fb7[16384] __attribute__((aligned(8)));
        static uint8_t tb7[16384] __attribute__((aligned(8)));
        uint64_t from7[2], to7[2];
        om_init(fb7, 16384, from7);
        om_init(tb7, 16384, to7);

        uint64_t *fobj = om_alloc(from7, (uint64_t)class_class, FORMAT_FIELDS, 2);
        uint64_t *iobj = om_alloc(from7, (uint64_t)class_class, FORMAT_INDEXABLE, 3);
        uint64_t *bobj = om_alloc(from7, (uint64_t)class_class, FORMAT_BYTES, 8);

        OBJ_FIELD(fobj, 0) = (uint64_t)iobj;
        OBJ_FIELD(fobj, 1) = (uint64_t)bobj;
        OBJ_FIELD(iobj, 0) = tag_smallint(11);
        OBJ_FIELD(iobj, 1) = tag_smallint(22);
        OBJ_FIELD(iobj, 2) = tag_smallint(33);
        uint8_t *bdata = (uint8_t *)&OBJ_FIELD(bobj, 0);
        bdata[0] = 0xAA;
        bdata[7] = 0xBB;

        uint64_t roots7[1];
        roots7[0] = (uint64_t)fobj;
        gc_collect(roots7, 1, from7, to7,
                   (uint64_t)fb7, (uint64_t)(fb7 + 16384));

        uint64_t *nf = (uint64_t *)roots7[0];
        uint64_t *ni = (uint64_t *)OBJ_FIELD(nf, 0);
        uint64_t *nb = (uint64_t *)OBJ_FIELD(nf, 1);

        ASSERT_EQ(ctx, OBJ_FORMAT(nf), FORMAT_FIELDS, "gc mixed: fields format");
        ASSERT_EQ(ctx, OBJ_FORMAT(ni), FORMAT_INDEXABLE, "gc mixed: indexable format");
        ASSERT_EQ(ctx, OBJ_FORMAT(nb), FORMAT_BYTES, "gc mixed: bytes format");
        ASSERT_EQ(ctx, OBJ_FIELD(ni, 1), tag_smallint(22), "gc mixed: indexable[1]");
        uint8_t *nbd = (uint8_t *)&OBJ_FIELD(nb, 0);
        ASSERT_EQ(ctx, nbd[0], 0xAA, "gc mixed: byte 0");
        ASSERT_EQ(ctx, nbd[7], 0xBB, "gc mixed: byte 7");
    }

    // --- Multiple roots ---
    {
        static uint8_t fb8[8192] __attribute__((aligned(8)));
        static uint8_t tb8[8192] __attribute__((aligned(8)));
        uint64_t from8[2], to8[2];
        om_init(fb8, 8192, from8);
        om_init(tb8, 8192, to8);

        uint64_t *r1 = om_alloc(from8, (uint64_t)class_class, FORMAT_FIELDS, 1);
        uint64_t *r2 = om_alloc(from8, (uint64_t)class_class, FORMAT_FIELDS, 1);
        OBJ_FIELD(r1, 0) = tag_smallint(111);
        OBJ_FIELD(r2, 0) = tag_smallint(222);

        uint64_t roots8[2];
        roots8[0] = (uint64_t)r1;
        roots8[1] = (uint64_t)r2;
        gc_collect(roots8, 2, from8, to8,
                   (uint64_t)fb8, (uint64_t)(fb8 + 8192));

        uint64_t *nr1 = (uint64_t *)roots8[0];
        uint64_t *nr2 = (uint64_t *)roots8[1];
        ASSERT_EQ(ctx, OBJ_FIELD(nr1, 0), tag_smallint(111),
                  "gc multi-root: r1 preserved");
        ASSERT_EQ(ctx, OBJ_FIELD(nr2, 0), tag_smallint(222),
                  "gc multi-root: r2 preserved");
        ASSERT_EQ(ctx, (uint64_t)nr1 != (uint64_t)nr2, 1,
                  "gc multi-root: distinct copies");
    }

    // --- Shared reference: A -> C, B -> C (C copied once) ---
    {
        static uint8_t fb9[8192] __attribute__((aligned(8)));
        static uint8_t tb9[8192] __attribute__((aligned(8)));
        uint64_t from9[2], to9[2];
        om_init(fb9, 8192, from9);
        om_init(tb9, 8192, to9);

        uint64_t *shared = om_alloc(from9, (uint64_t)class_class, FORMAT_FIELDS, 1);
        OBJ_FIELD(shared, 0) = tag_smallint(999);
        uint64_t *sa = om_alloc(from9, (uint64_t)class_class, FORMAT_FIELDS, 1);
        uint64_t *sb = om_alloc(from9, (uint64_t)class_class, FORMAT_FIELDS, 1);
        OBJ_FIELD(sa, 0) = (uint64_t)shared;
        OBJ_FIELD(sb, 0) = (uint64_t)shared;

        uint64_t roots9[2];
        roots9[0] = (uint64_t)sa;
        roots9[1] = (uint64_t)sb;
        gc_collect(roots9, 2, from9, to9,
                   (uint64_t)fb9, (uint64_t)(fb9 + 8192));

        uint64_t *nsa = (uint64_t *)roots9[0];
        uint64_t *nsb = (uint64_t *)roots9[1];
        uint64_t *nshared_a = (uint64_t *)OBJ_FIELD(nsa, 0);
        uint64_t *nshared_b = (uint64_t *)OBJ_FIELD(nsb, 0);
        ASSERT_EQ(ctx, (uint64_t)nshared_a, (uint64_t)nshared_b,
                  "gc shared: both point to same copy");
        ASSERT_EQ(ctx, OBJ_FIELD(nshared_a, 0), tag_smallint(999),
                  "gc shared: data preserved");
    }

    // --- FORMAT_BYTES fields must NOT be scanned as pointers ---
    {
        static uint8_t fb_bytes[16384] __attribute__((aligned(8)));
        static uint8_t tb_bytes[16384] __attribute__((aligned(8)));
        uint64_t from_b[2], to_b[2];
        om_init(fb_bytes, 16384, from_b);
        om_init(tb_bytes, 16384, to_b);

        // Create a bytes object whose raw data looks like a from-space pointer
        uint64_t *bobj2 = om_alloc(from_b, (uint64_t)class_class, FORMAT_BYTES, 16);
        // Write a value that looks like a valid from-space pointer (aligned, in range)
        uint64_t fake_ptr = (uint64_t)fb_bytes + 64; // in from-space range, tag 00
        uint8_t *bd2 = (uint8_t *)&OBJ_FIELD(bobj2, 0);
        memcpy(bd2, &fake_ptr, 8);
        bd2[8] = 0x42;
        bd2[9] = 0x43;

        uint64_t roots_b[1];
        roots_b[0] = (uint64_t)bobj2;

        // Should NOT crash or corrupt — bytes data must be copied verbatim
        gc_collect(roots_b, 1, from_b, to_b,
                   (uint64_t)fb_bytes, (uint64_t)(fb_bytes + 16384));

        uint64_t *nb2 = (uint64_t *)roots_b[0];
        ASSERT_EQ(ctx, OBJ_FORMAT(nb2), FORMAT_BYTES,
                  "gc bytes-no-scan: format preserved");
        uint8_t *nbd2 = (uint8_t *)&OBJ_FIELD(nb2, 0);
        uint64_t read_back;
        memcpy(&read_back, nbd2, 8);
        ASSERT_EQ(ctx, read_back, fake_ptr,
                  "gc bytes-no-scan: raw bytes preserved verbatim");
        ASSERT_EQ(ctx, nbd2[8], 0x42,
                  "gc bytes-no-scan: byte 8 preserved");
    }

    // --- Bytes object sizing: small bytes obj next to fields obj ---
    {
        static uint8_t fb_sz[8192] __attribute__((aligned(8)));
        static uint8_t tb_sz[8192] __attribute__((aligned(8)));
        uint64_t from_sz[2], to_sz[2];
        om_init(fb_sz, 8192, from_sz);
        om_init(tb_sz, 8192, to_sz);

        // 3-byte object (rounds to 1 word = 8 bytes data)
        uint64_t *small_bytes = om_alloc(from_sz, (uint64_t)class_class, FORMAT_BYTES, 3);
        uint8_t *sbd = (uint8_t *)&OBJ_FIELD(small_bytes, 0);
        sbd[0] = 0xDE;
        sbd[1] = 0xAD;
        sbd[2] = 0xBE;

        // Fields object right after
        uint64_t *after = om_alloc(from_sz, (uint64_t)class_class, FORMAT_FIELDS, 1);
        OBJ_FIELD(after, 0) = tag_smallint(777);

        // Point fields obj to bytes obj
        uint64_t *holder = om_alloc(from_sz, (uint64_t)class_class, FORMAT_FIELDS, 2);
        OBJ_FIELD(holder, 0) = (uint64_t)small_bytes;
        OBJ_FIELD(holder, 1) = (uint64_t)after;

        uint64_t roots_sz[1];
        roots_sz[0] = (uint64_t)holder;
        gc_collect(roots_sz, 1, from_sz, to_sz,
                   (uint64_t)fb_sz, (uint64_t)(fb_sz + 8192));

        uint64_t *nh = (uint64_t *)roots_sz[0];
        uint64_t *nsb = (uint64_t *)OBJ_FIELD(nh, 0);
        uint64_t *na = (uint64_t *)OBJ_FIELD(nh, 1);
        uint8_t *nsbd = (uint8_t *)&OBJ_FIELD(nsb, 0);

        ASSERT_EQ(ctx, nsbd[0], 0xDE, "gc bytes-size: byte 0");
        ASSERT_EQ(ctx, nsbd[1], 0xAD, "gc bytes-size: byte 1");
        ASSERT_EQ(ctx, nsbd[2], 0xBE, "gc bytes-size: byte 2");
        ASSERT_EQ(ctx, OBJ_SIZE(nsb), 3, "gc bytes-size: size is 3");
        ASSERT_EQ(ctx, OBJ_FIELD(na, 0), tag_smallint(777),
                  "gc bytes-size: adjacent obj intact");
    }

    // --- Tagged values in roots are ignored (not dereferenced) ---
    {
        static uint8_t fb10[4096] __attribute__((aligned(8)));
        static uint8_t tb10[4096] __attribute__((aligned(8)));
        uint64_t from10[2], to10[2];
        om_init(fb10, 4096, from10);
        om_init(tb10, 4096, to10);

        uint64_t roots10[3];
        roots10[0] = tag_smallint(42); // SmallInt — skip
        roots10[1] = tagged_nil();     // nil — skip
        roots10[2] = tag_smallint(99); // SmallInt — skip

        // Should not crash
        gc_collect(roots10, 3, from10, to10,
                   (uint64_t)fb10, (uint64_t)(fb10 + 4096));

        ASSERT_EQ(ctx, roots10[0], tag_smallint(42),
                  "gc tagged roots: SmallInt unchanged");
        ASSERT_EQ(ctx, roots10[1], tagged_nil(),
                  "gc tagged roots: nil unchanged");
    }

    // --- Full class hierarchy survives GC, message send works after ---
    {
        static uint8_t fb_cls[65536] __attribute__((aligned(8)));
        static uint8_t tb_cls[65536] __attribute__((aligned(8)));
        uint64_t from_cls[2], to_cls[2];
        om_init(fb_cls, 65536, from_cls);
        om_init(tb_cls, 65536, to_cls);

        // Build a class with a method that returns instvar 0
        uint64_t *cc = ctx->class_class; // class_class is NOT in from-space

        uint64_t *my_class = om_alloc(from_cls, (uint64_t)cc, FORMAT_FIELDS, 3);
        OBJ_FIELD(my_class, CLASS_SUPERCLASS) = tagged_nil();
        OBJ_FIELD(my_class, CLASS_INST_SIZE) = tag_smallint(1);

        // Method: PUSH_INST_VAR 0, RETURN_STACK_TOP
        uint64_t *my_bc = om_alloc(from_cls, (uint64_t)cc, FORMAT_BYTES, 10);
        uint8_t *bc = (uint8_t *)&OBJ_FIELD(my_bc, 0);
        bc[0] = BC_PUSH_INST_VAR;
        WRITE_U32(bc + 1, 0);
        bc[5] = BC_RETURN;

        uint64_t *my_cm = om_alloc(from_cls, (uint64_t)cc, FORMAT_FIELDS, 5);
        OBJ_FIELD(my_cm, CM_PRIMITIVE) = tag_smallint(0);
        OBJ_FIELD(my_cm, CM_NUM_ARGS) = tag_smallint(0);
        OBJ_FIELD(my_cm, CM_NUM_TEMPS) = tag_smallint(0);
        OBJ_FIELD(my_cm, CM_LITERALS) = tagged_nil();
        OBJ_FIELD(my_cm, CM_BYTECODES) = (uint64_t)my_bc;

        uint64_t sel = tag_smallint(50);
        uint64_t *my_md = om_alloc(from_cls, (uint64_t)cc, FORMAT_INDEXABLE, 2);
        OBJ_FIELD(my_md, 0) = sel;
        OBJ_FIELD(my_md, 1) = (uint64_t)my_cm;
        OBJ_FIELD(my_class, CLASS_METHOD_DICT) = (uint64_t)my_md;

        // Instance with field 0 = 42
        uint64_t *inst = om_alloc(from_cls, (uint64_t)my_class, FORMAT_FIELDS, 1);
        OBJ_FIELD(inst, 0) = tag_smallint(42);

        // Also allocate garbage to ensure from-space is messy
        for (int i = 0; i < 20; i++)
            om_alloc(from_cls, (uint64_t)cc, FORMAT_FIELDS, 3);

        // GC with inst as root
        uint64_t roots_cls[1];
        roots_cls[0] = (uint64_t)inst;
        gc_collect(roots_cls, 1, from_cls, to_cls,
                   (uint64_t)fb_cls, (uint64_t)(fb_cls + 65536));

        uint64_t *ninst = (uint64_t *)roots_cls[0];

        // Verify basic structure survived
        ASSERT_EQ(ctx, OBJ_FIELD(ninst, 0), tag_smallint(42),
                  "gc class: instance field preserved");

        // The class pointer should be in to-space
        uint64_t *nclass = (uint64_t *)OBJ_CLASS(ninst);
        ASSERT_EQ(ctx, (uint64_t)nclass >= (uint64_t)tb_cls, 1,
                  "gc class: class in to-space");

        // Method dict should be in to-space
        uint64_t *nmd = (uint64_t *)OBJ_FIELD(nclass, CLASS_METHOD_DICT);
        ASSERT_EQ(ctx, (uint64_t)nmd >= (uint64_t)tb_cls, 1,
                  "gc class: method dict in to-space");

        // Compiled method and bytecodes should be in to-space
        uint64_t *ncm = (uint64_t *)OBJ_FIELD(nmd, 1);
        ASSERT_EQ(ctx, (uint64_t)ncm >= (uint64_t)tb_cls, 1,
                  "gc class: compiled method in to-space");
        uint64_t *nbc = (uint64_t *)OBJ_FIELD(ncm, CM_BYTECODES);
        ASSERT_EQ(ctx, (uint64_t)nbc >= (uint64_t)tb_cls, 1,
                  "gc class: bytecodes in to-space");

        // Now actually use it: send the message via interpret
        uint64_t *stack = ctx->stack;
        uint64_t *class_table = ctx->class_table;
        uint64_t *sp, *fp;

        // Build a caller method that does: PUSH_SELF, SEND sel 0, HALT
        uint64_t *caller_bc = om_alloc(om, (uint64_t)cc, FORMAT_BYTES, 20);
        uint8_t *cbc = (uint8_t *)&OBJ_FIELD(caller_bc, 0);
        cbc[0] = BC_PUSH_SELF;
        cbc[1] = BC_SEND_MESSAGE;
        WRITE_U32(cbc + 2, 0); // selector lit index 0
        WRITE_U32(cbc + 6, 0); // 0 args
        cbc[10] = BC_HALT;

        uint64_t *caller_lits = om_alloc(om, (uint64_t)cc, FORMAT_INDEXABLE, 1);
        OBJ_FIELD(caller_lits, 0) = sel;

        uint64_t *caller_cm = om_alloc(om, (uint64_t)cc, FORMAT_FIELDS, 5);
        OBJ_FIELD(caller_cm, CM_PRIMITIVE) = tag_smallint(0);
        OBJ_FIELD(caller_cm, CM_NUM_ARGS) = tag_smallint(0);
        OBJ_FIELD(caller_cm, CM_NUM_TEMPS) = tag_smallint(0);
        OBJ_FIELD(caller_cm, CM_LITERALS) = (uint64_t)caller_lits;
        OBJ_FIELD(caller_cm, CM_BYTECODES) = (uint64_t)caller_bc;

        sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
        fp = (uint64_t *)0xCAFE;
        stack_push(&sp, stack, (uint64_t)ninst);

        activate_method(&sp, &fp, 0, (uint64_t)caller_cm, 0, 0);
        uint64_t result = interpret(&sp, &fp,
                                    (uint8_t *)&OBJ_FIELD(caller_bc, 0),
                                    class_table, om, NULL);

        ASSERT_EQ(ctx, result, tag_smallint(42),
                  "gc class: message send after GC returns 42");
    }

    // --- Stress: allocate 100 objects, keep every 3rd, GC, verify survivors ---
    {
        static uint8_t fb_st[131072] __attribute__((aligned(8)));
        static uint8_t tb_st[131072] __attribute__((aligned(8)));
        uint64_t from_st[2], to_st[2];
        om_init(fb_st, 131072, from_st);
        om_init(tb_st, 131072, to_st);

        uint64_t *cc = ctx->class_class;
#define STRESS_N 100
#define KEEP_EVERY 3
        uint64_t *all[STRESS_N];
        uint64_t roots_st[STRESS_N / KEEP_EVERY + 1];
        int nroots = 0;

        for (int i = 0; i < STRESS_N; i++)
        {
            all[i] = om_alloc(from_st, (uint64_t)cc, FORMAT_FIELDS, 2);
            OBJ_FIELD(all[i], 0) = tag_smallint(i);
            OBJ_FIELD(all[i], 1) = tag_smallint(i * 10);
            if (i % KEEP_EVERY == 0)
                roots_st[nroots++] = (uint64_t)all[i];
        }

        gc_collect(roots_st, nroots, from_st, to_st,
                   (uint64_t)fb_st, (uint64_t)(fb_st + 131072));

        int stress_ok = 1;
        for (int i = 0; i < nroots; i++)
        {
            uint64_t *obj = (uint64_t *)roots_st[i];
            int orig_idx = i * KEEP_EVERY;
            if (OBJ_FIELD(obj, 0) != tag_smallint(orig_idx))
                stress_ok = 0;
            if (OBJ_FIELD(obj, 1) != tag_smallint(orig_idx * 10))
                stress_ok = 0;
            if ((uint64_t)obj < (uint64_t)tb_st)
                stress_ok = 0;
        }
        ASSERT_EQ(ctx, stress_ok, 1,
                  "gc stress: all 34 survivors have correct data in to-space");

        // Verify to-space used less memory than from-space
        // (only ~34 objects survived out of 100)
        uint64_t from_used = from_st[0] - (uint64_t)fb_st;
        uint64_t to_used = to_st[0] - (uint64_t)tb_st;
        ASSERT_EQ(ctx, to_used < from_used, 1,
                  "gc stress: to-space smaller than from-space");
    }

    // --- Alloc-collect-alloc cycle ---
    {
        // Small nursery: 2048 bytes = room for ~25 4-word objects
        static uint8_t space_a[2048] __attribute__((aligned(8)));
        static uint8_t space_b[2048] __attribute__((aligned(8)));
        uint64_t sa[2], sb[2];
        om_init(space_a, 2048, sa);
        om_init(space_b, 2048, sb);

        uint64_t *cc = ctx->class_class;

        // Allocate until full, keep only the last one
        uint64_t *last = NULL;
        int alloc_count = 0;
        while (1)
        {
            uint64_t *obj = om_alloc(sa, (uint64_t)cc, FORMAT_FIELDS, 1);
            if (!obj)
                break;
            OBJ_FIELD(obj, 0) = tag_smallint(alloc_count);
            last = obj;
            alloc_count++;
        }

        ASSERT_EQ(ctx, alloc_count > 10, 1,
                  "gc alloc-cycle: allocated many objects");
        ASSERT_EQ(ctx, OBJ_FIELD(last, 0), tag_smallint(alloc_count - 1),
                  "gc alloc-cycle: last obj has correct value");

        // GC: only keep 'last'
        uint64_t roots_ac[1];
        roots_ac[0] = (uint64_t)last;
        gc_collect(roots_ac, 1, sa, sb,
                   (uint64_t)space_a, (uint64_t)(space_a + 2048));

        uint64_t *survived = (uint64_t *)roots_ac[0];
        ASSERT_EQ(ctx, OBJ_FIELD(survived, 0), tag_smallint(alloc_count - 1),
                  "gc alloc-cycle: survivor has correct value");
        ASSERT_EQ(ctx, (uint64_t)survived >= (uint64_t)space_b, 1,
                  "gc alloc-cycle: survivor in to-space");

        // Now allocate more in to-space (sb) — should succeed
        uint64_t *after_gc = om_alloc(sb, (uint64_t)cc, FORMAT_FIELDS, 1);
        ASSERT_EQ(ctx, after_gc != NULL, 1,
                  "gc alloc-cycle: can allocate after GC");
        OBJ_FIELD(after_gc, 0) = tag_smallint(9999);
        ASSERT_EQ(ctx, OBJ_FIELD(after_gc, 0), tag_smallint(9999),
                  "gc alloc-cycle: new obj has correct value");

        // Do another full cycle: fill sb, collect back to sa
        om_init(space_a, 2048, sa); // reset sa as new to-space
        int alloc_count2 = 0;
        uint64_t *last2 = survived; // keep the original survivor
        while (1)
        {
            uint64_t *obj = om_alloc(sb, (uint64_t)cc, FORMAT_FIELDS, 1);
            if (!obj)
                break;
            OBJ_FIELD(obj, 0) = tag_smallint(alloc_count2 + 1000);
            alloc_count2++;
        }

        uint64_t roots_ac2[1];
        roots_ac2[0] = (uint64_t)last2;
        gc_collect(roots_ac2, 1, sb, sa,
                   (uint64_t)space_b, (uint64_t)(space_b + 2048));

        uint64_t *survived2 = (uint64_t *)roots_ac2[0];
        ASSERT_EQ(ctx, OBJ_FIELD(survived2, 0), tag_smallint(alloc_count - 1),
                  "gc alloc-cycle: double-GC survivor preserved");
        ASSERT_EQ(ctx, (uint64_t)survived2 >= (uint64_t)space_a, 1,
                  "gc alloc-cycle: back in space_a after double GC");
    }
}
