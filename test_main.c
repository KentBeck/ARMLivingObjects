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

    uint64_t *class_class = om_alloc(om, 0, FORMAT_FIELDS, 4);
    OBJ_CLASS(class_class) = (uint64_t)class_class;
    OBJ_FIELD(class_class, CLASS_SUPERCLASS) = tagged_nil();
    OBJ_FIELD(class_class, CLASS_METHOD_DICT) = tagged_nil();
    OBJ_FIELD(class_class, CLASS_INST_SIZE) = tag_smallint(4);
    OBJ_FIELD(class_class, CLASS_INST_FORMAT) = tag_smallint(FORMAT_FIELDS);

    uint64_t *smallint_class = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 4);
    OBJ_FIELD(smallint_class, CLASS_SUPERCLASS) = tagged_nil();
    OBJ_FIELD(smallint_class, CLASS_METHOD_DICT) = tagged_nil();
    OBJ_FIELD(smallint_class, CLASS_INST_SIZE) = tag_smallint(0);
    OBJ_FIELD(smallint_class, CLASS_INST_FORMAT) = tag_smallint(FORMAT_FIELDS);

    uint64_t *block_class = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 4);
    OBJ_FIELD(block_class, CLASS_SUPERCLASS) = tagged_nil();
    OBJ_FIELD(block_class, CLASS_METHOD_DICT) = tagged_nil();
    OBJ_FIELD(block_class, CLASS_INST_SIZE) = tag_smallint(2);
    OBJ_FIELD(block_class, CLASS_INST_FORMAT) = tag_smallint(FORMAT_FIELDS);

    uint64_t *string_class = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 4);
    OBJ_FIELD(string_class, CLASS_SUPERCLASS) = tagged_nil();
    OBJ_FIELD(string_class, CLASS_METHOD_DICT) = tagged_nil();
    OBJ_FIELD(string_class, CLASS_INST_SIZE) = tag_smallint(0);
    OBJ_FIELD(string_class, CLASS_INST_FORMAT) = tag_smallint(FORMAT_BYTES);

    uint64_t *symbol_class = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 4);
    OBJ_FIELD(symbol_class, CLASS_SUPERCLASS) = (uint64_t)string_class;
    OBJ_FIELD(symbol_class, CLASS_METHOD_DICT) = tagged_nil();
    OBJ_FIELD(symbol_class, CLASS_INST_SIZE) = tag_smallint(0);
    OBJ_FIELD(symbol_class, CLASS_INST_FORMAT) = tag_smallint(FORMAT_BYTES);

    uint64_t *symbol_table_obj = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 20); // size 20 for now
    for (int i = 0; i < 20; i++)
    {
        OBJ_FIELD(symbol_table_obj, i) = tagged_nil();
    }

    // Define selectors for String and Symbol primitives
    uint64_t sel_eq_string = tag_smallint(100);
    uint64_t sel_hash_string = tag_smallint(101);
    uint64_t sel_asSymbol_string = tag_smallint(102);
    uint64_t sel_eq_symbol = tag_smallint(103);
    uint64_t sel_size = tag_smallint(72); // Re-use existing size selector
    uint64_t sel_at = tag_smallint(90);   // Re-use existing at: selector
    uint64_t sel_at_put = tag_smallint(91); // Re-use existing at:put: selector

    // Dummy bytecodes for primitives (never executed)
    uint64_t *prim_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 2);

    // Helper macro to create a CompiledMethod for a primitive
#define MAKE_PRIM_CM(name, prim_num, num_args)                               \
    uint64_t *name = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5); \
    OBJ_FIELD(name, CM_PRIMITIVE) = tag_smallint(prim_num);                  \
    OBJ_FIELD(name, CM_NUM_ARGS) = tag_smallint(num_args);                   \
    OBJ_FIELD(name, CM_NUM_TEMPS) = tag_smallint(0);                         \
    OBJ_FIELD(name, CM_LITERALS) = tagged_nil();                             \
    OBJ_FIELD(name, CM_BYTECODES) = (uint64_t)prim_bc;

    // String primitives
    MAKE_PRIM_CM(cm_string_eq, PRIM_STRING_EQ, 1)
    MAKE_PRIM_CM(cm_string_hash_fnv, PRIM_STRING_HASH_FNV, 0)
    MAKE_PRIM_CM(cm_string_asSymbol, PRIM_STRING_AS_SYMBOL, 0)
    MAKE_PRIM_CM(cm_string_size, PRIM_SIZE, 0)
    MAKE_PRIM_CM(cm_string_at, PRIM_AT, 1)
    MAKE_PRIM_CM(cm_string_at_put, PRIM_AT_PUT, 2)

    // Symbol primitives
    MAKE_PRIM_CM(cm_symbol_eq, PRIM_SYMBOL_EQ, 1)

#undef MAKE_PRIM_CM

    // Method dictionary for String
    uint64_t *string_md = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 12); // 6 pairs
    OBJ_FIELD(string_md, 0) = sel_eq_string;
    OBJ_FIELD(string_md, 1) = (uint64_t)cm_string_eq;
    OBJ_FIELD(string_md, 2) = sel_hash_string;
    OBJ_FIELD(string_md, 3) = (uint64_t)cm_string_hash_fnv;
    OBJ_FIELD(string_md, 4) = sel_asSymbol_string;
    OBJ_FIELD(string_md, 5) = (uint64_t)cm_string_asSymbol;
    OBJ_FIELD(string_md, 6) = sel_size;
    OBJ_FIELD(string_md, 7) = (uint64_t)cm_string_size;
    OBJ_FIELD(string_md, 8) = sel_at;
    OBJ_FIELD(string_md, 9) = (uint64_t)cm_string_at;
    OBJ_FIELD(string_md, 10) = sel_at_put;
    OBJ_FIELD(string_md, 11) = (uint64_t)cm_string_at_put;
    OBJ_FIELD(string_class, CLASS_METHOD_DICT) = (uint64_t)string_md;

    // Method dictionary for Symbol
    uint64_t *symbol_md = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 2); // 1 pair
    OBJ_FIELD(symbol_md, 0) = sel_eq_symbol;
    OBJ_FIELD(symbol_md, 1) = (uint64_t)cm_symbol_eq;
    OBJ_FIELD(symbol_class, CLASS_METHOD_DICT) = (uint64_t)symbol_md;

    uint64_t *test_class = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 4);
    OBJ_FIELD(test_class, CLASS_SUPERCLASS) = tagged_nil();
    OBJ_FIELD(test_class, CLASS_METHOD_DICT) = tagged_nil();
    OBJ_FIELD(test_class, CLASS_INST_SIZE) = tag_smallint(0);
    OBJ_FIELD(test_class, CLASS_INST_FORMAT) = tag_smallint(FORMAT_FIELDS);

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
    // Character class — for Character immediates
    uint64_t *character_class = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 4);
    OBJ_FIELD(character_class, CLASS_SUPERCLASS) = tagged_nil();
    OBJ_FIELD(character_class, CLASS_METHOD_DICT) = tagged_nil();
    OBJ_FIELD(character_class, CLASS_INST_SIZE) = tag_smallint(0);
    OBJ_FIELD(character_class, CLASS_INST_FORMAT) = tag_smallint(FORMAT_FIELDS);

    ctx.om = om;
    ctx.class_class = class_class;
    ctx.smallint_class = smallint_class;
    ctx.block_class = block_class;
    ctx.character_class = character_class;
    ctx.string_class = string_class;
    ctx.symbol_class = symbol_class;
    ctx.symbol_table = symbol_table_obj;
    global_symbol_table = symbol_table_obj; // Initialize the global symbol table
    ctx.test_class = test_class;
    ctx.receiver = (uint64_t)recv_obj;
    ctx.method = (uint64_t)test_cm;
    uint64_t *class_table_obj = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 5);
    OBJ_FIELD(class_table_obj, 0) = (uint64_t)smallint_class;
    OBJ_FIELD(class_table_obj, 1) = (uint64_t)block_class;
    OBJ_FIELD(class_table_obj, 2) = 0; // True class (not yet)
    OBJ_FIELD(class_table_obj, 3) = 0; // False class (not yet)
    OBJ_FIELD(class_table_obj, 4) = (uint64_t)character_class;
    ctx.class_table = class_table_obj;
    ctx.passes = 0;
    ctx.failures = 0;

    test_stack(&ctx);
    test_tagged(&ctx);
    test_object(&ctx);
    test_dispatch(&ctx);
    test_blocks(&ctx);
    test_factorial(&ctx);
    test_transaction(&ctx);
    test_gc(&ctx);
    test_persist(&ctx);
    test_primitives(&ctx);
    test_smalltalk_sources(&ctx);
    test_string_dispatch(&ctx);
    test_array_dispatch(&ctx);
    test_symbol_dispatch(&ctx);
    test_bootstrap_compiler(&ctx);

    printf("\n%d passed, %d failed\n", ctx.passes, ctx.failures);
    return ctx.failures > 0 ? 1 : 0;
}
