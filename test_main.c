#include "test_defs.h"

void debug_mnu(uint64_t selector)
{
    fprintf(stderr, "MNU: selector=%lld (untagged=%lld)\n", selector, selector >> 2);
}

void debug_oom(void)
{
    fprintf(stderr, "OOM: out of object memory\n");
}

void debug_unknown_prim(uint64_t prim_index)
{
    fprintf(stderr, "UNKNOWN PRIM: %lld\n", prim_index);
}

int main()
{
    static uint8_t om_buffer[OM_SIZE] __attribute__((aligned(8)));
    uint64_t om[2];
    om_init(om_buffer, OM_SIZE, om);

    uint64_t *class_class = om_alloc(om, 0, FORMAT_FIELDS, 3);
    OBJ_CLASS(class_class) = (uint64_t)class_class;
    OBJ_FIELD(class_class, CLASS_SUPERCLASS) = tagged_nil();
    OBJ_FIELD(class_class, CLASS_METHOD_DICT) = tagged_nil();
    OBJ_FIELD(class_class, CLASS_INST_SIZE) = tag_smallint(3);

    uint64_t *smallint_class = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 3);
    OBJ_FIELD(smallint_class, CLASS_SUPERCLASS) = tagged_nil();
    OBJ_FIELD(smallint_class, CLASS_METHOD_DICT) = tagged_nil();
    OBJ_FIELD(smallint_class, CLASS_INST_SIZE) = tag_smallint(0);

    uint64_t *block_class = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 3);
    OBJ_FIELD(block_class, CLASS_SUPERCLASS) = tagged_nil();
    OBJ_FIELD(block_class, CLASS_METHOD_DICT) = tagged_nil();
    OBJ_FIELD(block_class, CLASS_INST_SIZE) = tag_smallint(2);

    uint64_t *test_class = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 3);
    OBJ_FIELD(test_class, CLASS_SUPERCLASS) = tagged_nil();
    OBJ_FIELD(test_class, CLASS_METHOD_DICT) = tagged_nil();
    OBJ_FIELD(test_class, CLASS_INST_SIZE) = tag_smallint(0);

    uint64_t *recv_obj = om_alloc(om, (uint64_t)test_class, FORMAT_FIELDS, 0);

    uint64_t *test_bytecodes = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 2);
    uint8_t *test_bc = (uint8_t *)&OBJ_FIELD(test_bytecodes, 0);
    test_bc[0] = BC_PUSH_SELF;
    test_bc[1] = BC_RETURN;
    uint64_t *test_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
    OBJ_FIELD(test_cm, CM_PRIMITIVE) = tag_smallint(0);
    OBJ_FIELD(test_cm, CM_NUM_ARGS) = tag_smallint(0);
    OBJ_FIELD(test_cm, CM_NUM_TEMPS) = tag_smallint(0);
    OBJ_FIELD(test_cm, CM_LITERALS) = tagged_nil();
    OBJ_FIELD(test_cm, CM_BYTECODES) = (uint64_t)test_bytecodes;

    TestContext ctx;
    ctx.om = om;
    ctx.class_class = class_class;
    ctx.smallint_class = smallint_class;
    ctx.block_class = block_class;
    ctx.test_class = test_class;
    ctx.receiver = (uint64_t)recv_obj;
    ctx.method = (uint64_t)test_cm;
    ctx.class_table[0] = (uint64_t)smallint_class;
    ctx.class_table[1] = (uint64_t)block_class;
    ctx.class_table[2] = 0;
    ctx.class_table[3] = 0;
    ctx.passes = 0;
    ctx.failures = 0;

    test_stack(&ctx);
    test_tagged(&ctx);
    test_object(&ctx);
    test_dispatch(&ctx);
    test_blocks(&ctx);
    test_factorial(&ctx);

    printf("\n%d passed, %d failed\n", ctx.passes, ctx.failures);
    return ctx.failures > 0 ? 1 : 0;
}
