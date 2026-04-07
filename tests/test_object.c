#include "test_defs.h"

void test_object(TestContext *ctx)
{
    uint64_t *om=ctx->om;
    uint64_t *class_class=ctx->class_class;
    uint64_t *smallint_class=ctx->smallint_class;
    uint64_t *block_class=ctx->block_class;
    uint64_t *test_class=ctx->test_class;
    uint64_t receiver=ctx->receiver;
    uint64_t method=ctx->method;
    uint64_t *class_table=ctx->class_table;
    uint64_t *stack=ctx->stack;
    (void)om;(void)class_class;(void)smallint_class;
    (void)block_class;(void)test_class;(void)receiver;
    (void)method;(void)class_table;(void)stack;
    uint64_t *iv_class = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 3);
    OBJ_FIELD(iv_class, CLASS_SUPERCLASS) = tagged_nil();
    OBJ_FIELD(iv_class, CLASS_METHOD_DICT) = tagged_nil();
    OBJ_FIELD(iv_class, CLASS_INST_SIZE) = tag_smallint(4);
    uint64_t *sp;
    uint64_t *fp;
    uint64_t ip;
    uint64_t fake_ip=0x1000;

    // --- Section 8: Object Memory (uses om + class_class from bootstrap above) ---

    // Allocate an object with 2 fields: size is correct
    uint64_t *obj2 = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 2);
    ASSERT_EQ(ctx, OBJ_SIZE(obj2), 2, "alloc 2 fields: size is 2");

    // Read class pointer from object (word 0)
    ASSERT_EQ(ctx, OBJ_CLASS(obj2), (uint64_t)class_class, "read class pointer from object");

    // Read format from object (word 1)
    ASSERT_EQ(ctx, OBJ_FORMAT(obj2), FORMAT_FIELDS, "read format from object");

    // Read size from object (word 2)
    ASSERT_EQ(ctx, OBJ_SIZE(obj2), 2, "read size from object");

    // Read field 0 from an object (at header + 3*W) — initialized to 0
    ASSERT_EQ(ctx, OBJ_FIELD(obj2, 0), 0, "field 0 initialized to 0");

    // Write field 1 of an object
    OBJ_FIELD(obj2, 1) = tag_smallint(0xBEEF);
    ASSERT_EQ(ctx, OBJ_FIELD(obj2, 1), tag_smallint(0xBEEF), "write and read field 1");

    // Object pointer has tag 00 (aligned)
    ASSERT_EQ(ctx, is_object_ptr((uint64_t)obj2), 1, "object pointer has tag 00");

    // Fields store tagged values
    OBJ_FIELD(obj2, 0) = tag_smallint(42);
    ASSERT_EQ(ctx, is_smallint(OBJ_FIELD(obj2, 0)), 1, "field stores tagged SmallInt");
    ASSERT_EQ(ctx, untag_smallint(OBJ_FIELD(obj2, 0)), 42, "field SmallInt value is 42");

    // Allocate a fields object (format 0)
    uint64_t *obj_f = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 3);
    ASSERT_EQ(ctx, OBJ_FORMAT(obj_f), FORMAT_FIELDS, "fields object: format 0");
    ASSERT_EQ(ctx, OBJ_SIZE(obj_f), 3, "fields object: size 3");

    // Allocate an indexable object (format 1)
    uint64_t *obj_i = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 5);
    ASSERT_EQ(ctx, OBJ_FORMAT(obj_i), FORMAT_INDEXABLE, "indexable object: format 1");
    ASSERT_EQ(ctx, OBJ_SIZE(obj_i), 5, "indexable object: size 5");
    OBJ_FIELD(obj_i, 3) = tag_smallint(99);
    ASSERT_EQ(ctx, untag_smallint(OBJ_FIELD(obj_i, 3)), 99, "indexable: store/read slot 3");

    // Allocate a bytes object (format 2)
    uint64_t *obj_b = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 10);
    ASSERT_EQ(ctx, OBJ_FORMAT(obj_b), FORMAT_BYTES, "bytes object: format 2");
    ASSERT_EQ(ctx, OBJ_SIZE(obj_b), 10, "bytes object: size 10 bytes");
    uint8_t *bytes = (uint8_t *)&OBJ_FIELD(obj_b, 0);
    bytes[0] = 0xAB;
    bytes[9] = 0xCD;
    ASSERT_EQ(ctx, bytes[0], 0xAB, "bytes object: write/read byte 0");
    ASSERT_EQ(ctx, bytes[9], 0xCD, "bytes object: write/read byte 9");

    // bc_push_inst_var with real object
    uint64_t *obj_recv = om_alloc(om, (uint64_t)iv_class, FORMAT_FIELDS, 4);
    OBJ_FIELD(obj_recv, 0) = tag_smallint(10);
    OBJ_FIELD(obj_recv, 1) = tag_smallint(20);
    OBJ_FIELD(obj_recv, 2) = tag_smallint(30);
    sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
    fp = 0;
    stack_push(&sp, stack, (uint64_t)obj_recv);
    activate_method(&sp, &fp, fake_ip, method, 0, 0);
    bc_push_inst_var(&sp, &fp, 1);
    ASSERT_EQ(ctx, stack_top(&sp), tag_smallint(20),
              "bc_push_inst_var with real object: field 1");

    // bc_store_inst_var with real object
    stack_push(&sp, stack, tag_smallint(99));
    bc_store_inst_var(&sp, &fp, 2);
    ASSERT_EQ(ctx, OBJ_FIELD(obj_recv, 2), tag_smallint(99),
              "bc_store_inst_var with real object: field 2");

    // --- Section 9: Class and Method Dictionary ---
    // (class_class already bootstrapped above)

    ASSERT_EQ(ctx, OBJ_CLASS(class_class), (uint64_t)class_class,
              "bootstrap: Class class is self-referential");

    // Create a method dictionary (Array) with one (selector, method) pair
    // Selector = tagged SmallInt 1 (symbol index for e.g. #foo)
    uint64_t sel_foo = tag_smallint(1);
    uint64_t *fake_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 1);
    // method dict: 2 slots (1 pair)
    uint64_t *md = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 2);
    OBJ_FIELD(md, 0) = sel_foo;
    OBJ_FIELD(md, 1) = (uint64_t)fake_cm;
    ASSERT_EQ(ctx, OBJ_FIELD(md, 0), sel_foo, "method dict: selector stored");
    ASSERT_EQ(ctx, OBJ_FIELD(md, 1), (uint64_t)fake_cm, "method dict: method stored");

    // Look up a selector in a method dictionary: found
    uint64_t found = md_lookup(md, sel_foo);
    ASSERT_EQ(ctx, found, (uint64_t)fake_cm, "md_lookup: found method for selector");

    // Look up a selector in a method dictionary: not found
    uint64_t sel_bar = tag_smallint(2);
    uint64_t not_found = md_lookup(md, sel_bar);
    ASSERT_EQ(ctx, not_found, 0, "md_lookup: not found returns 0");

    // Look up with superclass chain: found in superclass
    // Create a parent class with the method dict
    uint64_t *parent_class = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 3);
    OBJ_FIELD(parent_class, CLASS_SUPERCLASS) = tagged_nil();
    OBJ_FIELD(parent_class, CLASS_METHOD_DICT) = (uint64_t)md;
    OBJ_FIELD(parent_class, CLASS_INST_SIZE) = tag_smallint(0);
    // Create a child class with empty method dict, parent as superclass
    uint64_t *child_class = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 3);
    OBJ_FIELD(child_class, CLASS_SUPERCLASS) = (uint64_t)parent_class;
    OBJ_FIELD(child_class, CLASS_METHOD_DICT) = tagged_nil();
    OBJ_FIELD(child_class, CLASS_INST_SIZE) = tag_smallint(0);
    found = class_lookup(child_class, sel_foo);
    ASSERT_EQ(ctx, found, (uint64_t)fake_cm,
              "class_lookup: found in superclass");
    not_found = class_lookup(child_class, sel_bar);
    ASSERT_EQ(ctx, not_found, 0,
              "class_lookup: not found in chain returns 0");
    ASSERT_EQ(ctx, (uint64_t)oop_class(tagged_nil(), class_table),
              (uint64_t)ctx->undefined_object_class,
              "oop_class: nil resolves to UndefinedObject");

    // Create a CompiledMethod with bytecodes and literals
    // CM: 4 fields (num_args, num_temps, literal_count, bytecodes_ptr)
    // + literal_count literal fields
    // For a method with 1 arg, 2 temps, 1 literal:
    // fields: num_args=1, num_temps=2, literal_count=1, literal0, bytecodes
    uint64_t *bytecodes = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 4);
    uint8_t *bc = (uint8_t *)&OBJ_FIELD(bytecodes, 0);
    bc[0] = 3; // PUSH_SELF
    bc[1] = 7; // RETURN_STACK_TOP
    bc[2] = 0; // padding
    bc[3] = 0;

    uint64_t *cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
    OBJ_FIELD(cm, CM_PRIMITIVE) = tag_smallint(0);
    OBJ_FIELD(cm, CM_NUM_ARGS) = tag_smallint(1);
    OBJ_FIELD(cm, CM_NUM_TEMPS) = tag_smallint(2);
    uint64_t *_lits_1 = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 1);
    OBJ_FIELD(_lits_1, 0) = tag_smallint(42);
    OBJ_FIELD(cm, CM_LITERALS) = (uint64_t)_lits_1;
    OBJ_FIELD(cm, CM_BYTECODES) = (uint64_t)bytecodes;

    // Read num_args and num_temps from a CompiledMethod
    ASSERT_EQ(ctx, untag_smallint(OBJ_FIELD(cm, CM_NUM_ARGS)), 1,
              "CompiledMethod: num_args = 1");
    ASSERT_EQ(ctx, untag_smallint(OBJ_FIELD(cm, CM_NUM_TEMPS)), 2,
              "CompiledMethod: num_temps = 2");
    ASSERT_EQ(ctx, OBJ_FIELD(_lits_1, 0), tag_smallint(42),
              "CompiledMethod: literal 0 = 42");
    ASSERT_EQ(ctx, OBJ_FIELD(cm, CM_BYTECODES), (uint64_t)bytecodes,
              "CompiledMethod: bytecodes pointer");

    // Look up class from receiver's header, then find method
    // Create an instance of parent_class, look up sel_foo
    uint64_t *instance = om_alloc(om, (uint64_t)parent_class, FORMAT_FIELDS, 0);
    uint64_t *recv_class = (uint64_t *)OBJ_CLASS(instance);
    found = class_lookup(recv_class, sel_foo);
    ASSERT_EQ(ctx, found, (uint64_t)fake_cm,
              "lookup from receiver: class -> method dict -> method");


    ctx->smallint_class=smallint_class;
}
