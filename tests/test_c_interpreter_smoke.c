#include "test_defs.h"

static int failures = 0;
static int passes = 0;

#define CHECK_EQ(actual, expected, msg)                                          \
    do                                                                          \
    {                                                                           \
        uint64_t _actual = (actual);                                             \
        uint64_t _expected = (expected);                                         \
        if (_actual != _expected)                                                \
        {                                                                       \
            printf("FAIL: %s (expected %llu, got %llu)\n", msg, _expected, _actual); \
            failures++;                                                         \
        }                                                                       \
        else                                                                    \
        {                                                                       \
            printf("PASS: %s\n", msg);                                          \
            passes++;                                                           \
        }                                                                       \
    } while (0)

typedef struct
{
    uint64_t om[2];
    uint64_t *class_class;
    uint64_t *class_table;
    uint64_t *test_class;
    uint64_t *receiver;
    uint64_t stack[STACK_WORDS];
} SmokeWorld;

static uint64_t *make_array(SmokeWorld *world, uint64_t size)
{
    return om_alloc(world->om, (uint64_t)world->class_class, FORMAT_INDEXABLE, size);
}

static uint64_t *make_bytecodes(SmokeWorld *world, uint64_t size)
{
    return om_alloc(world->om, (uint64_t)world->class_class, FORMAT_BYTES, size);
}

static uint64_t *make_method(SmokeWorld *world, uint64_t *bytecodes, uint64_t *literals,
                             uint64_t num_args, uint64_t num_temps)
{
    uint64_t *method = om_alloc(world->om, (uint64_t)world->class_class, FORMAT_FIELDS, 5);
    OBJ_FIELD(method, CM_PRIMITIVE) = tag_smallint(PRIM_NONE);
    OBJ_FIELD(method, CM_NUM_ARGS) = tag_smallint((int64_t)num_args);
    OBJ_FIELD(method, CM_NUM_TEMPS) = tag_smallint((int64_t)num_temps);
    OBJ_FIELD(method, CM_LITERALS) = literals == NULL ? tagged_nil() : (uint64_t)literals;
    OBJ_FIELD(method, CM_BYTECODES) = (uint64_t)bytecodes;
    return method;
}

static uint64_t run_method(SmokeWorld *world, uint64_t *method, uint64_t receiver,
                           const uint64_t *args, uint64_t arg_count)
{
    uint64_t *bytecodes = (uint64_t *)OBJ_FIELD(method, CM_BYTECODES);
    uint64_t *sp = (uint64_t *)((uint8_t *)world->stack + STACK_WORDS * sizeof(uint64_t));
    uint64_t *fp = (uint64_t *)0xCAFE;

    stack_push(&sp, world->stack, receiver);
    for (uint64_t index = 0; index < arg_count; index++)
    {
        stack_push(&sp, world->stack, args[index]);
    }
    activate_method(&sp, &fp, 0, (uint64_t)method, arg_count, (uint64_t)untag_smallint(OBJ_FIELD(method, CM_NUM_TEMPS)));
    return interpret(&sp, &fp, (uint8_t *)&OBJ_FIELD(bytecodes, 0), world->class_table, world->om, NULL);
}

static void init_world(SmokeWorld *world)
{
    static uint8_t om_buffer[OM_SIZE] __attribute__((aligned(8)));
    om_init(om_buffer, OM_SIZE, world->om);

    world->class_class = om_alloc(world->om, 0, FORMAT_FIELDS, 5);
    OBJ_CLASS(world->class_class) = (uint64_t)world->class_class;
    OBJ_FIELD(world->class_class, CLASS_SUPERCLASS) = tagged_nil();
    OBJ_FIELD(world->class_class, CLASS_METHOD_DICT) = tagged_nil();
    OBJ_FIELD(world->class_class, CLASS_INST_SIZE) = tag_smallint(5);
    OBJ_FIELD(world->class_class, CLASS_INST_FORMAT) = tag_smallint(FORMAT_FIELDS);
    OBJ_FIELD(world->class_class, CLASS_INST_VARS) = tagged_nil();

    uint64_t *smallint_class = om_alloc(world->om, (uint64_t)world->class_class, FORMAT_FIELDS, 5);
    uint64_t *block_class = om_alloc(world->om, (uint64_t)world->class_class, FORMAT_FIELDS, 5);
    uint64_t *true_class = om_alloc(world->om, (uint64_t)world->class_class, FORMAT_FIELDS, 5);
    uint64_t *false_class = om_alloc(world->om, (uint64_t)world->class_class, FORMAT_FIELDS, 5);
    uint64_t *character_class = om_alloc(world->om, (uint64_t)world->class_class, FORMAT_FIELDS, 5);
    uint64_t *undefined_object_class = om_alloc(world->om, (uint64_t)world->class_class, FORMAT_FIELDS, 5);

    world->class_table = make_array(world, 6);
    OBJ_FIELD(world->class_table, CLASS_TABLE_SMALLINT) = (uint64_t)smallint_class;
    OBJ_FIELD(world->class_table, CLASS_TABLE_BLOCK) = (uint64_t)block_class;
    OBJ_FIELD(world->class_table, CLASS_TABLE_TRUE) = (uint64_t)true_class;
    OBJ_FIELD(world->class_table, CLASS_TABLE_FALSE) = (uint64_t)false_class;
    OBJ_FIELD(world->class_table, CLASS_TABLE_CHARACTER) = (uint64_t)character_class;
    OBJ_FIELD(world->class_table, CLASS_TABLE_UNDEFINED_OBJECT) = (uint64_t)undefined_object_class;

    world->test_class = om_alloc(world->om, (uint64_t)world->class_class, FORMAT_FIELDS, 5);
    OBJ_FIELD(world->test_class, CLASS_SUPERCLASS) = tagged_nil();
    OBJ_FIELD(world->test_class, CLASS_METHOD_DICT) = tagged_nil();
    OBJ_FIELD(world->test_class, CLASS_INST_SIZE) = tag_smallint(2);
    OBJ_FIELD(world->test_class, CLASS_INST_FORMAT) = tag_smallint(FORMAT_FIELDS);
    OBJ_FIELD(world->test_class, CLASS_INST_VARS) = tagged_nil();

    world->receiver = om_alloc(world->om, (uint64_t)world->test_class, FORMAT_FIELDS, 2);
    OBJ_FIELD(world->receiver, 0) = tag_smallint(111);
    OBJ_FIELD(world->receiver, 1) = tag_smallint(222);
}

static void test_push_literal_halt(SmokeWorld *world)
{
    uint64_t *literals = make_array(world, 1);
    OBJ_FIELD(literals, 0) = tag_smallint(42);
    uint64_t *bytecodes = make_bytecodes(world, 6);
    uint8_t *bc = (uint8_t *)&OBJ_FIELD(bytecodes, 0);
    bc[0] = BC_PUSH_LITERAL;
    WRITE_U32(&bc[1], 0);
    bc[5] = BC_HALT;

    uint64_t *method = make_method(world, bytecodes, literals, 0, 0);
    CHECK_EQ(run_method(world, method, (uint64_t)world->receiver, NULL, 0), tag_smallint(42),
             "C interpreter: PUSH_LITERAL + HALT");
}

static void test_self_inst_var_temp_return(SmokeWorld *world)
{
    uint64_t *bytecodes = make_bytecodes(world, 24);
    uint8_t *bc = (uint8_t *)&OBJ_FIELD(bytecodes, 0);
    uint64_t ip = 0;
    bc[ip++] = BC_PUSH_INST_VAR;
    WRITE_U32(&bc[ip], 1);
    ip += 4;
    bc[ip++] = BC_STORE_TEMP;
    WRITE_U32(&bc[ip], 0);
    ip += 4;
    bc[ip++] = BC_PUSH_SELF;
    bc[ip++] = BC_POP;
    bc[ip++] = BC_PUSH_TEMP;
    WRITE_U32(&bc[ip], 0);
    ip += 4;
    bc[ip++] = BC_RETURN;

    uint64_t *method = make_method(world, bytecodes, NULL, 0, 1);
    CHECK_EQ(run_method(world, method, (uint64_t)world->receiver, NULL, 0), tag_smallint(222),
             "C interpreter: inst var/temp/self/pop/return");
}

static void test_push_arg_duplicate_return(SmokeWorld *world)
{
    uint64_t *bytecodes = make_bytecodes(world, 9);
    uint8_t *bc = (uint8_t *)&OBJ_FIELD(bytecodes, 0);
    bc[0] = BC_PUSH_ARG;
    WRITE_U32(&bc[1], 0);
    bc[5] = BC_DUPLICATE;
    bc[6] = BC_POP;
    bc[7] = BC_RETURN;

    uint64_t arg = tag_smallint(77);
    uint64_t *method = make_method(world, bytecodes, NULL, 1, 0);
    CHECK_EQ(run_method(world, method, (uint64_t)world->receiver, &arg, 1), tag_smallint(77),
             "C interpreter: PUSH_ARG + DUPLICATE + POP + RETURN");
}

static void test_jumps(SmokeWorld *world)
{
    uint64_t *literals = make_array(world, 3);
    OBJ_FIELD(literals, 0) = tagged_true();
    OBJ_FIELD(literals, 1) = tag_smallint(1);
    OBJ_FIELD(literals, 2) = tag_smallint(99);

    uint64_t *bytecodes = make_bytecodes(world, 28);
    uint8_t *bc = (uint8_t *)&OBJ_FIELD(bytecodes, 0);
    uint64_t ip = 0;
    bc[ip++] = BC_PUSH_LITERAL;
    WRITE_U32(&bc[ip], 0);
    ip += 4;
    bc[ip++] = BC_JUMP_IF_TRUE;
    WRITE_U32(&bc[ip], 20);
    ip += 4;
    bc[ip++] = BC_PUSH_LITERAL;
    WRITE_U32(&bc[ip], 1);
    ip += 4;
    bc[ip++] = BC_JUMP;
    WRITE_U32(&bc[ip], 27);
    ip += 4;
    bc[ip++] = BC_PUSH_LITERAL;
    WRITE_U32(&bc[ip], 2);
    ip += 4;
    bc[ip++] = BC_RETURN;

    uint64_t *method = make_method(world, bytecodes, literals, 0, 0);
    CHECK_EQ(run_method(world, method, (uint64_t)world->receiver, NULL, 0), tag_smallint(99),
             "C interpreter: JUMP_IF_TRUE absolute target");

    uint64_t *false_literals = make_array(world, 2);
    OBJ_FIELD(false_literals, 0) = tagged_false();
    OBJ_FIELD(false_literals, 1) = tag_smallint(33);
    uint64_t *false_bytecodes = make_bytecodes(world, 17);
    uint8_t *fbc = (uint8_t *)&OBJ_FIELD(false_bytecodes, 0);
    fbc[0] = BC_PUSH_LITERAL;
    WRITE_U32(&fbc[1], 0);
    fbc[5] = BC_JUMP_IF_FALSE;
    WRITE_U32(&fbc[6], 11);
    fbc[10] = BC_HALT;
    fbc[11] = BC_PUSH_LITERAL;
    WRITE_U32(&fbc[12], 1);
    fbc[16] = BC_RETURN;

    uint64_t *false_method = make_method(world, false_bytecodes, false_literals, 0, 0);
    CHECK_EQ(run_method(world, false_method, (uint64_t)world->receiver, NULL, 0), tag_smallint(33),
             "C interpreter: JUMP_IF_FALSE absolute target");
}

int main(void)
{
    setbuf(stdout, NULL);

    SmokeWorld world;
    init_world(&world);

    test_push_literal_halt(&world);
    test_self_inst_var_temp_return(&world);
    test_push_arg_duplicate_return(&world);
    test_jumps(&world);

    printf("\n%d C interpreter smoke tests passed, %d failed\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
