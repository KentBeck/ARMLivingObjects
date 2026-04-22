#include "test_defs.h"

#include <unistd.h>

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
            (void)(msg);                                                        \
            passes++;                                                           \
        }                                                                       \
    } while (0)

typedef struct
{
    uint64_t om[10];
    uint64_t *class_class;
    uint64_t *class_table;
    uint64_t *test_class;
    uint64_t *symbol_class;
    uint64_t *symbol_table;
    uint64_t *context_class;
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

static uint64_t *make_bytes_object(SmokeWorld *world, uint64_t *klass, const char *text)
{
    uint64_t size = 0;
    while (text[size] != '\0')
    {
        size++;
    }

    uint64_t *object = om_alloc(world->om, (uint64_t)klass, FORMAT_BYTES, size);
    uint8_t *bytes = (uint8_t *)&OBJ_FIELD(object, 0);
    for (uint64_t index = 0; index < size; index++)
    {
        bytes[index] = (uint8_t)text[index];
    }
    return object;
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

static uint64_t *make_primitive_method(SmokeWorld *world, uint64_t primitive, uint64_t num_args)
{
    uint64_t *bytecodes = make_bytecodes(world, 1);
    uint8_t *bc = (uint8_t *)&OBJ_FIELD(bytecodes, 0);
    bc[0] = BC_HALT;

    uint64_t *method = make_method(world, bytecodes, NULL, num_args, 0);
    OBJ_FIELD(method, CM_PRIMITIVE) = tag_smallint((int64_t)primitive);
    return method;
}

static void install_method(SmokeWorld *world, uint64_t selector, uint64_t *method)
{
    uint64_t *method_dict = make_array(world, 2);
    OBJ_FIELD(method_dict, 0) = selector;
    OBJ_FIELD(method_dict, 1) = (uint64_t)method;
    OBJ_FIELD(world->test_class, CLASS_METHOD_DICT) = (uint64_t)method_dict;
}

static uint64_t run_method_with_txn(SmokeWorld *world, uint64_t *method, uint64_t receiver,
                                    const uint64_t *args, uint64_t arg_count, uint64_t *txn_log);
static uint64_t run_method_with_om(SmokeWorld *world, uint64_t *method, uint64_t receiver,
                                   const uint64_t *args, uint64_t arg_count, uint64_t *om,
                                   uint64_t *txn_log);

static uint64_t run_method(SmokeWorld *world, uint64_t *method, uint64_t receiver,
                           const uint64_t *args, uint64_t arg_count)
{
    return run_method_with_txn(world, method, receiver, args, arg_count, NULL);
}

static uint64_t run_method_with_txn(SmokeWorld *world, uint64_t *method, uint64_t receiver,
                                    const uint64_t *args, uint64_t arg_count, uint64_t *txn_log)
{
    return run_method_with_om(world, method, receiver, args, arg_count, world->om, txn_log);
}

static uint64_t run_method_with_om(SmokeWorld *world, uint64_t *method, uint64_t receiver,
                                   const uint64_t *args, uint64_t arg_count, uint64_t *om,
                                   uint64_t *txn_log)
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
    return interpret(&sp, &fp, (uint8_t *)&OBJ_FIELD(bytecodes, 0), world->class_table, om, txn_log);
}

static void init_world(SmokeWorld *world)
{
    static uint8_t om_buffer[OM_SIZE] __attribute__((aligned(8)));
    for (uint64_t index = 0; index < 10; index++)
    {
        world->om[index] = 0;
    }
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

    world->symbol_class = om_alloc(world->om, (uint64_t)world->class_class, FORMAT_FIELDS, 5);
    OBJ_FIELD(world->symbol_class, CLASS_SUPERCLASS) = tagged_nil();
    OBJ_FIELD(world->symbol_class, CLASS_METHOD_DICT) = tagged_nil();
    OBJ_FIELD(world->symbol_class, CLASS_INST_SIZE) = tag_smallint(0);
    OBJ_FIELD(world->symbol_class, CLASS_INST_FORMAT) = tag_smallint(FORMAT_BYTES);
    OBJ_FIELD(world->symbol_class, CLASS_INST_VARS) = tagged_nil();

    world->symbol_table = make_array(world, 16);
    for (uint64_t index = 0; index < OBJ_SIZE(world->symbol_table); index++)
    {
        OBJ_FIELD(world->symbol_table, index) = tagged_nil();
    }
    global_symbol_class = world->symbol_class;
    global_symbol_table = world->symbol_table;

    world->context_class = om_alloc(world->om, (uint64_t)world->class_class, FORMAT_FIELDS, 5);
    OBJ_FIELD(world->context_class, CLASS_SUPERCLASS) = tagged_nil();
    OBJ_FIELD(world->context_class, CLASS_METHOD_DICT) = tagged_nil();
    OBJ_FIELD(world->context_class, CLASS_INST_SIZE) = tag_smallint(CONTEXT_VAR_BASE);
    OBJ_FIELD(world->context_class, CLASS_INST_FORMAT) = tag_smallint(FORMAT_FIELDS);
    OBJ_FIELD(world->context_class, CLASS_INST_VARS) = tagged_nil();
    global_context_class = world->context_class;

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

static void test_push_global_halt(SmokeWorld *world)
{
    uint64_t *association = om_alloc(world->om, (uint64_t)world->class_class, FORMAT_FIELDS, 2);
    OBJ_FIELD(association, 0) = tag_smallint(1001);
    OBJ_FIELD(association, 1) = (uint64_t)world->test_class;

    uint64_t *literals = make_array(world, 1);
    OBJ_FIELD(literals, 0) = (uint64_t)association;
    uint64_t *bytecodes = make_bytecodes(world, 6);
    uint8_t *bc = (uint8_t *)&OBJ_FIELD(bytecodes, 0);
    bc[0] = BC_PUSH_GLOBAL;
    WRITE_U32(&bc[1], 0);
    bc[5] = BC_HALT;

    uint64_t *method = make_method(world, bytecodes, literals, 0, 0);
    CHECK_EQ(run_method(world, method, (uint64_t)world->receiver, NULL, 0),
             (uint64_t)world->test_class, "C interpreter: PUSH_GLOBAL + HALT");
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

static void test_zero_arg_send(SmokeWorld *world)
{
    uint64_t selector = tag_smallint(1000);

    uint64_t *callee_bytecodes = make_bytecodes(world, 2);
    uint8_t *cbc = (uint8_t *)&OBJ_FIELD(callee_bytecodes, 0);
    cbc[0] = BC_PUSH_SELF;
    cbc[1] = BC_RETURN;
    uint64_t *callee = make_method(world, callee_bytecodes, NULL, 0, 0);
    install_method(world, selector, callee);

    uint64_t *literals = make_array(world, 1);
    OBJ_FIELD(literals, 0) = selector;
    uint64_t *caller_bytecodes = make_bytecodes(world, 11);
    uint8_t *bc = (uint8_t *)&OBJ_FIELD(caller_bytecodes, 0);
    bc[0] = BC_PUSH_SELF;
    bc[1] = BC_SEND_MESSAGE;
    WRITE_U32(&bc[2], 0);
    WRITE_U32(&bc[6], 0);
    bc[10] = BC_RETURN;

    uint64_t *caller = make_method(world, caller_bytecodes, literals, 0, 0);
    CHECK_EQ(run_method(world, caller, (uint64_t)world->receiver, NULL, 0), (uint64_t)world->receiver,
             "C interpreter: SEND_MESSAGE 0-arg method");
}

static void test_one_arg_send(SmokeWorld *world)
{
    uint64_t selector = tag_smallint(1001);

    uint64_t *callee_bytecodes = make_bytecodes(world, 6);
    uint8_t *cbc = (uint8_t *)&OBJ_FIELD(callee_bytecodes, 0);
    cbc[0] = BC_PUSH_ARG;
    WRITE_U32(&cbc[1], 0);
    cbc[5] = BC_RETURN;
    uint64_t *callee = make_method(world, callee_bytecodes, NULL, 1, 0);
    install_method(world, selector, callee);

    uint64_t *literals = make_array(world, 2);
    OBJ_FIELD(literals, 0) = selector;
    OBJ_FIELD(literals, 1) = tag_smallint(77);
    uint64_t *caller_bytecodes = make_bytecodes(world, 16);
    uint8_t *bc = (uint8_t *)&OBJ_FIELD(caller_bytecodes, 0);
    uint64_t ip = 0;
    bc[ip++] = BC_PUSH_SELF;
    bc[ip++] = BC_PUSH_LITERAL;
    WRITE_U32(&bc[ip], 1);
    ip += 4;
    bc[ip++] = BC_SEND_MESSAGE;
    WRITE_U32(&bc[ip], 0);
    ip += 4;
    WRITE_U32(&bc[ip], 1);
    ip += 4;
    bc[ip++] = BC_RETURN;

    uint64_t *caller = make_method(world, caller_bytecodes, literals, 0, 0);
    CHECK_EQ(run_method(world, caller, (uint64_t)world->receiver, NULL, 0), tag_smallint(77),
             "C interpreter: SEND_MESSAGE 1-arg method");
}

static uint64_t run_smallint_send(SmokeWorld *world, uint64_t receiver, uint64_t arg,
                                  uint64_t selector)
{
    uint64_t *literals = make_array(world, 3);
    OBJ_FIELD(literals, 0) = receiver;
    OBJ_FIELD(literals, 1) = arg;
    OBJ_FIELD(literals, 2) = selector;

    uint64_t *bytecodes = make_bytecodes(world, 20);
    uint8_t *bc = (uint8_t *)&OBJ_FIELD(bytecodes, 0);
    bc[0] = BC_PUSH_LITERAL;
    WRITE_U32(&bc[1], 0);
    bc[5] = BC_PUSH_LITERAL;
    WRITE_U32(&bc[6], 1);
    bc[10] = BC_SEND_MESSAGE;
    WRITE_U32(&bc[11], 2);
    WRITE_U32(&bc[15], 1);
    bc[19] = BC_HALT;

    uint64_t *method = make_method(world, bytecodes, literals, 0, 0);
    return run_method(world, method, (uint64_t)world->receiver, NULL, 0);
}

static uint64_t run_unary_send(SmokeWorld *world, uint64_t receiver, uint64_t selector)
{
    uint64_t *literals = make_array(world, 2);
    OBJ_FIELD(literals, 0) = receiver;
    OBJ_FIELD(literals, 1) = selector;

    uint64_t *bytecodes = make_bytecodes(world, 15);
    uint8_t *bc = (uint8_t *)&OBJ_FIELD(bytecodes, 0);
    bc[0] = BC_PUSH_LITERAL;
    WRITE_U32(&bc[1], 0);
    bc[5] = BC_SEND_MESSAGE;
    WRITE_U32(&bc[6], 1);
    WRITE_U32(&bc[10], 0);
    bc[14] = BC_HALT;

    uint64_t *method = make_method(world, bytecodes, literals, 0, 0);
    return run_method(world, method, (uint64_t)world->receiver, NULL, 0);
}

static uint64_t run_binary_send(SmokeWorld *world, uint64_t receiver, uint64_t arg,
                                uint64_t selector)
{
    uint64_t *literals = make_array(world, 3);
    OBJ_FIELD(literals, 0) = receiver;
    OBJ_FIELD(literals, 1) = arg;
    OBJ_FIELD(literals, 2) = selector;

    uint64_t *bytecodes = make_bytecodes(world, 20);
    uint8_t *bc = (uint8_t *)&OBJ_FIELD(bytecodes, 0);
    bc[0] = BC_PUSH_LITERAL;
    WRITE_U32(&bc[1], 0);
    bc[5] = BC_PUSH_LITERAL;
    WRITE_U32(&bc[6], 1);
    bc[10] = BC_SEND_MESSAGE;
    WRITE_U32(&bc[11], 2);
    WRITE_U32(&bc[15], 1);
    bc[19] = BC_HALT;

    uint64_t *method = make_method(world, bytecodes, literals, 0, 0);
    return run_method(world, method, (uint64_t)world->receiver, NULL, 0);
}

static uint64_t run_binary_send_with_txn(SmokeWorld *world, uint64_t receiver, uint64_t arg,
                                         uint64_t selector, uint64_t *txn_log)
{
    uint64_t *literals = make_array(world, 3);
    OBJ_FIELD(literals, 0) = receiver;
    OBJ_FIELD(literals, 1) = arg;
    OBJ_FIELD(literals, 2) = selector;

    uint64_t *bytecodes = make_bytecodes(world, 20);
    uint8_t *bc = (uint8_t *)&OBJ_FIELD(bytecodes, 0);
    bc[0] = BC_PUSH_LITERAL;
    WRITE_U32(&bc[1], 0);
    bc[5] = BC_PUSH_LITERAL;
    WRITE_U32(&bc[6], 1);
    bc[10] = BC_SEND_MESSAGE;
    WRITE_U32(&bc[11], 2);
    WRITE_U32(&bc[15], 1);
    bc[19] = BC_HALT;

    uint64_t *method = make_method(world, bytecodes, literals, 0, 0);
    return run_method_with_txn(world, method, (uint64_t)world->receiver, NULL, 0, txn_log);
}

static uint64_t run_ternary_send_with_txn(SmokeWorld *world, uint64_t receiver, uint64_t arg0,
                                          uint64_t arg1, uint64_t selector, uint64_t *txn_log)
{
    uint64_t *literals = make_array(world, 4);
    OBJ_FIELD(literals, 0) = receiver;
    OBJ_FIELD(literals, 1) = arg0;
    OBJ_FIELD(literals, 2) = arg1;
    OBJ_FIELD(literals, 3) = selector;

    uint64_t *bytecodes = make_bytecodes(world, 25);
    uint8_t *bc = (uint8_t *)&OBJ_FIELD(bytecodes, 0);
    bc[0] = BC_PUSH_LITERAL;
    WRITE_U32(&bc[1], 0);
    bc[5] = BC_PUSH_LITERAL;
    WRITE_U32(&bc[6], 1);
    bc[10] = BC_PUSH_LITERAL;
    WRITE_U32(&bc[11], 2);
    bc[15] = BC_SEND_MESSAGE;
    WRITE_U32(&bc[16], 3);
    WRITE_U32(&bc[20], 2);
    bc[24] = BC_HALT;

    uint64_t *method = make_method(world, bytecodes, literals, 0, 0);
    return run_method_with_txn(world, method, (uint64_t)world->receiver, NULL, 0, txn_log);
}

static void test_smallint_primitives(SmokeWorld *world)
{
    uint64_t sel_plus = tag_smallint(2000);
    uint64_t sel_minus = tag_smallint(2001);
    uint64_t sel_lt = tag_smallint(2002);
    uint64_t sel_eq = tag_smallint(2003);
    uint64_t sel_mul = tag_smallint(2004);

    uint64_t *method_dict = make_array(world, 10);
    OBJ_FIELD(method_dict, 0) = sel_plus;
    OBJ_FIELD(method_dict, 1) = (uint64_t)make_primitive_method(world, PRIM_SMALLINT_ADD, 1);
    OBJ_FIELD(method_dict, 2) = sel_minus;
    OBJ_FIELD(method_dict, 3) = (uint64_t)make_primitive_method(world, PRIM_SMALLINT_SUB, 1);
    OBJ_FIELD(method_dict, 4) = sel_lt;
    OBJ_FIELD(method_dict, 5) = (uint64_t)make_primitive_method(world, PRIM_SMALLINT_LT, 1);
    OBJ_FIELD(method_dict, 6) = sel_eq;
    OBJ_FIELD(method_dict, 7) = (uint64_t)make_primitive_method(world, PRIM_SMALLINT_EQ, 1);
    OBJ_FIELD(method_dict, 8) = sel_mul;
    OBJ_FIELD(method_dict, 9) = (uint64_t)make_primitive_method(world, PRIM_SMALLINT_MUL, 1);

    uint64_t *smallint_class = (uint64_t *)OBJ_FIELD(world->class_table, CLASS_TABLE_SMALLINT);
    OBJ_FIELD(smallint_class, CLASS_METHOD_DICT) = (uint64_t)method_dict;

    CHECK_EQ(run_smallint_send(world, tag_smallint(3), tag_smallint(4), sel_plus), tag_smallint(7),
             "C interpreter: SmallInteger + primitive");
    CHECK_EQ(run_smallint_send(world, tag_smallint(10), tag_smallint(3), sel_minus), tag_smallint(7),
             "C interpreter: SmallInteger - primitive");
    CHECK_EQ(run_smallint_send(world, tag_smallint(3), tag_smallint(5), sel_lt), tagged_true(),
             "C interpreter: SmallInteger < primitive");
    CHECK_EQ(run_smallint_send(world, tag_smallint(5), tag_smallint(3), sel_lt), tagged_false(),
             "C interpreter: SmallInteger < false primitive");
    CHECK_EQ(run_smallint_send(world, tag_smallint(42), tag_smallint(42), sel_eq), tagged_true(),
             "C interpreter: SmallInteger = primitive");
    CHECK_EQ(run_smallint_send(world, tag_smallint(6), tag_smallint(7), sel_mul), tag_smallint(42),
             "C interpreter: SmallInteger * primitive");
}

static void test_identity_class_hash_primitives(SmokeWorld *world)
{
    uint64_t sel_identity = tag_smallint(3000);
    uint64_t sel_basic_class = tag_smallint(3001);
    uint64_t sel_hash = tag_smallint(3002);

    uint64_t *test_method_dict = make_array(world, 6);
    OBJ_FIELD(test_method_dict, 0) = sel_identity;
    OBJ_FIELD(test_method_dict, 1) = (uint64_t)make_primitive_method(world, PRIM_IDENTITY_EQ, 1);
    OBJ_FIELD(test_method_dict, 2) = sel_basic_class;
    OBJ_FIELD(test_method_dict, 3) = (uint64_t)make_primitive_method(world, PRIM_BASIC_CLASS, 0);
    OBJ_FIELD(test_method_dict, 4) = sel_hash;
    OBJ_FIELD(test_method_dict, 5) = (uint64_t)make_primitive_method(world, PRIM_HASH, 0);
    OBJ_FIELD(world->test_class, CLASS_METHOD_DICT) = (uint64_t)test_method_dict;

    uint64_t *smallint_method_dict = make_array(world, 4);
    OBJ_FIELD(smallint_method_dict, 0) = sel_basic_class;
    OBJ_FIELD(smallint_method_dict, 1) = (uint64_t)make_primitive_method(world, PRIM_BASIC_CLASS, 0);
    OBJ_FIELD(smallint_method_dict, 2) = sel_hash;
    OBJ_FIELD(smallint_method_dict, 3) = (uint64_t)make_primitive_method(world, PRIM_HASH, 0);
    uint64_t *smallint_class = (uint64_t *)OBJ_FIELD(world->class_table, CLASS_TABLE_SMALLINT);
    OBJ_FIELD(smallint_class, CLASS_METHOD_DICT) = (uint64_t)smallint_method_dict;

    uint64_t *other = om_alloc(world->om, (uint64_t)world->test_class, FORMAT_FIELDS, 2);

    CHECK_EQ(run_binary_send(world, (uint64_t)world->receiver, (uint64_t)world->receiver, sel_identity), tagged_true(),
             "C interpreter: == same object primitive");
    CHECK_EQ(run_binary_send(world, (uint64_t)world->receiver, (uint64_t)other, sel_identity), tagged_false(),
             "C interpreter: == different object primitive");
    CHECK_EQ(run_unary_send(world, (uint64_t)world->receiver, sel_basic_class), (uint64_t)world->test_class,
             "C interpreter: basicClass heap object primitive");
    CHECK_EQ(run_unary_send(world, tag_smallint(123), sel_basic_class),
             OBJ_FIELD(world->class_table, CLASS_TABLE_SMALLINT),
             "C interpreter: basicClass SmallInteger primitive");
    CHECK_EQ(run_unary_send(world, tag_smallint(123), sel_hash), tag_smallint(123),
             "C interpreter: hash SmallInteger primitive");

    uint64_t hash1 = run_unary_send(world, (uint64_t)world->receiver, sel_hash);
    uint64_t hash2 = run_unary_send(world, (uint64_t)world->receiver, sel_hash);
    CHECK_EQ(hash1 & TAG_MASK, TAG_SMALLINT, "C interpreter: hash heap object returns SmallInteger");
    CHECK_EQ(hash1, hash2, "C interpreter: hash heap object is stable");
}

static void test_character_primitives(SmokeWorld *world)
{
    uint64_t sel_value = tag_smallint(4000);
    uint64_t sel_as_character = tag_smallint(4001);
    uint64_t sel_is_letter = tag_smallint(4002);
    uint64_t sel_is_digit = tag_smallint(4003);
    uint64_t sel_uppercase = tag_smallint(4004);
    uint64_t sel_lowercase = tag_smallint(4005);
    uint64_t sel_print_char = tag_smallint(4006);

    uint64_t *character_method_dict = make_array(world, 12);
    OBJ_FIELD(character_method_dict, 0) = sel_value;
    OBJ_FIELD(character_method_dict, 1) = (uint64_t)make_primitive_method(world, PRIM_CHAR_VALUE, 0);
    OBJ_FIELD(character_method_dict, 2) = sel_is_letter;
    OBJ_FIELD(character_method_dict, 3) = (uint64_t)make_primitive_method(world, PRIM_CHAR_IS_LETTER, 0);
    OBJ_FIELD(character_method_dict, 4) = sel_is_digit;
    OBJ_FIELD(character_method_dict, 5) = (uint64_t)make_primitive_method(world, PRIM_CHAR_IS_DIGIT, 0);
    OBJ_FIELD(character_method_dict, 6) = sel_uppercase;
    OBJ_FIELD(character_method_dict, 7) = (uint64_t)make_primitive_method(world, PRIM_CHAR_UPPERCASE, 0);
    OBJ_FIELD(character_method_dict, 8) = sel_lowercase;
    OBJ_FIELD(character_method_dict, 9) = (uint64_t)make_primitive_method(world, PRIM_CHAR_LOWERCASE, 0);
    OBJ_FIELD(character_method_dict, 10) = sel_print_char;
    OBJ_FIELD(character_method_dict, 11) = (uint64_t)make_primitive_method(world, PRIM_PRINT_CHAR, 0);
    uint64_t *character_class = (uint64_t *)OBJ_FIELD(world->class_table, CLASS_TABLE_CHARACTER);
    OBJ_FIELD(character_class, CLASS_METHOD_DICT) = (uint64_t)character_method_dict;

    uint64_t *smallint_method_dict = make_array(world, 2);
    OBJ_FIELD(smallint_method_dict, 0) = sel_as_character;
    OBJ_FIELD(smallint_method_dict, 1) = (uint64_t)make_primitive_method(world, PRIM_AS_CHARACTER, 0);
    uint64_t *smallint_class = (uint64_t *)OBJ_FIELD(world->class_table, CLASS_TABLE_SMALLINT);
    OBJ_FIELD(smallint_class, CLASS_METHOD_DICT) = (uint64_t)smallint_method_dict;

    CHECK_EQ(run_unary_send(world, tag_character(65), sel_value), tag_smallint(65),
             "C interpreter: Character value primitive");
    CHECK_EQ(run_unary_send(world, tag_smallint(65), sel_as_character), tag_character(65),
             "C interpreter: SmallInteger asCharacter primitive");
    CHECK_EQ(run_unary_send(world, tag_character('A'), sel_is_letter), tagged_true(),
             "C interpreter: Character isLetter true primitive");
    CHECK_EQ(run_unary_send(world, tag_character('5'), sel_is_letter), tagged_false(),
             "C interpreter: Character isLetter false primitive");
    CHECK_EQ(run_unary_send(world, tag_character('5'), sel_is_digit), tagged_true(),
             "C interpreter: Character isDigit true primitive");
    CHECK_EQ(run_unary_send(world, tag_character('A'), sel_is_digit), tagged_false(),
             "C interpreter: Character isDigit false primitive");
    CHECK_EQ(run_unary_send(world, tag_character('a'), sel_uppercase), tag_character('A'),
             "C interpreter: Character asUppercase primitive");
    CHECK_EQ(run_unary_send(world, tag_character('A'), sel_lowercase), tag_character('a'),
             "C interpreter: Character asLowercase primitive");

    int pipe_fds[2];
    CHECK_EQ(pipe(pipe_fds), 0, "C interpreter: printChar pipe setup");
    int saved_stdout = dup(STDOUT_FILENO);
    CHECK_EQ(saved_stdout >= 0, 1, "C interpreter: printChar dup stdout");
    CHECK_EQ(dup2(pipe_fds[1], STDOUT_FILENO) >= 0, 1, "C interpreter: printChar redirect stdout");
    CHECK_EQ(run_unary_send(world, tag_character('.'), sel_print_char), tag_character('.'),
             "C interpreter: Character printChar returns self");
    CHECK_EQ(dup2(saved_stdout, STDOUT_FILENO) >= 0, 1, "C interpreter: printChar restore stdout");
    close(saved_stdout);
    close(pipe_fds[1]);
    char printed = 0;
    CHECK_EQ(read(pipe_fds[0], &printed, 1), 1, "C interpreter: printChar captured byte");
    close(pipe_fds[0]);
    CHECK_EQ((uint64_t)(uint8_t)printed, (uint64_t)'.', "C interpreter: Character printChar writes byte");
}

static void test_string_symbol_primitives(SmokeWorld *world)
{
    uint64_t sel_string_eq = tag_smallint(5000);
    uint64_t sel_string_hash = tag_smallint(5001);
    uint64_t sel_symbol_eq = tag_smallint(5002);
    uint64_t sel_as_symbol = tag_smallint(5003);

    uint64_t *string_class = om_alloc(world->om, (uint64_t)world->class_class, FORMAT_FIELDS, 5);
    OBJ_FIELD(string_class, CLASS_SUPERCLASS) = tagged_nil();
    OBJ_FIELD(string_class, CLASS_INST_SIZE) = tag_smallint(0);
    OBJ_FIELD(string_class, CLASS_INST_FORMAT) = tag_smallint(FORMAT_BYTES);
    OBJ_FIELD(string_class, CLASS_INST_VARS) = tagged_nil();

    uint64_t *symbol_class = om_alloc(world->om, (uint64_t)world->class_class, FORMAT_FIELDS, 5);
    OBJ_FIELD(symbol_class, CLASS_SUPERCLASS) = tagged_nil();
    OBJ_FIELD(symbol_class, CLASS_INST_SIZE) = tag_smallint(0);
    OBJ_FIELD(symbol_class, CLASS_INST_FORMAT) = tag_smallint(FORMAT_BYTES);
    OBJ_FIELD(symbol_class, CLASS_INST_VARS) = tagged_nil();

    uint64_t *string_method_dict = make_array(world, 6);
    OBJ_FIELD(string_method_dict, 0) = sel_string_eq;
    OBJ_FIELD(string_method_dict, 1) = (uint64_t)make_primitive_method(world, PRIM_STRING_EQ, 1);
    OBJ_FIELD(string_method_dict, 2) = sel_string_hash;
    OBJ_FIELD(string_method_dict, 3) = (uint64_t)make_primitive_method(world, PRIM_STRING_HASH_FNV, 0);
    OBJ_FIELD(string_method_dict, 4) = sel_as_symbol;
    OBJ_FIELD(string_method_dict, 5) = (uint64_t)make_primitive_method(world, PRIM_STRING_AS_SYMBOL, 0);
    OBJ_FIELD(string_class, CLASS_METHOD_DICT) = (uint64_t)string_method_dict;

    uint64_t *symbol_method_dict = make_array(world, 2);
    OBJ_FIELD(symbol_method_dict, 0) = sel_symbol_eq;
    OBJ_FIELD(symbol_method_dict, 1) = (uint64_t)make_primitive_method(world, PRIM_SYMBOL_EQ, 1);
    OBJ_FIELD(symbol_class, CLASS_METHOD_DICT) = (uint64_t)symbol_method_dict;

    uint64_t *hello1 = make_bytes_object(world, string_class, "hello");
    uint64_t *hello2 = make_bytes_object(world, string_class, "hello");
    uint64_t *world_string = make_bytes_object(world, string_class, "world");
    uint64_t *symbol1 = make_bytes_object(world, symbol_class, "name");
    uint64_t *symbol2 = make_bytes_object(world, symbol_class, "name");

    CHECK_EQ(run_binary_send(world, (uint64_t)hello1, (uint64_t)hello2, sel_string_eq), tagged_true(),
             "C interpreter: String = equal primitive");
    CHECK_EQ(run_binary_send(world, (uint64_t)hello1, (uint64_t)world_string, sel_string_eq), tagged_false(),
             "C interpreter: String = different primitive");
    CHECK_EQ(run_unary_send(world, (uint64_t)hello1, sel_string_hash),
             run_unary_send(world, (uint64_t)hello2, sel_string_hash),
             "C interpreter: String hash equal bytes primitive");
    CHECK_EQ(run_binary_send(world, (uint64_t)symbol1, (uint64_t)symbol1, sel_symbol_eq), tagged_true(),
             "C interpreter: Symbol = identical primitive");
    CHECK_EQ(run_binary_send(world, (uint64_t)symbol1, (uint64_t)symbol2, sel_symbol_eq), tagged_false(),
             "C interpreter: Symbol = distinct primitive");

    uint64_t as_symbol1 = run_unary_send(world, (uint64_t)hello1, sel_as_symbol);
    uint64_t as_symbol2 = run_unary_send(world, (uint64_t)hello2, sel_as_symbol);
    CHECK_EQ(as_symbol1, as_symbol2, "C interpreter: String asSymbol reuses interned symbol");
    CHECK_EQ(OBJ_CLASS((uint64_t *)as_symbol1), (uint64_t)world->symbol_class,
             "C interpreter: String asSymbol returns Symbol");
}

static void test_indexed_primitives(SmokeWorld *world)
{
    uint64_t sel_at = tag_smallint(6000);
    uint64_t sel_at_put = tag_smallint(6001);
    uint64_t sel_size = tag_smallint(6002);

    uint64_t *array_class = om_alloc(world->om, (uint64_t)world->class_class, FORMAT_FIELDS, 5);
    OBJ_FIELD(array_class, CLASS_SUPERCLASS) = tagged_nil();
    OBJ_FIELD(array_class, CLASS_INST_SIZE) = tag_smallint(0);
    OBJ_FIELD(array_class, CLASS_INST_FORMAT) = tag_smallint(FORMAT_INDEXABLE);
    OBJ_FIELD(array_class, CLASS_INST_VARS) = tagged_nil();

    uint64_t *bytes_class = om_alloc(world->om, (uint64_t)world->class_class, FORMAT_FIELDS, 5);
    OBJ_FIELD(bytes_class, CLASS_SUPERCLASS) = tagged_nil();
    OBJ_FIELD(bytes_class, CLASS_INST_SIZE) = tag_smallint(0);
    OBJ_FIELD(bytes_class, CLASS_INST_FORMAT) = tag_smallint(FORMAT_BYTES);
    OBJ_FIELD(bytes_class, CLASS_INST_VARS) = tagged_nil();

    uint64_t *indexed_method_dict = make_array(world, 6);
    OBJ_FIELD(indexed_method_dict, 0) = sel_at;
    OBJ_FIELD(indexed_method_dict, 1) = (uint64_t)make_primitive_method(world, PRIM_AT, 1);
    OBJ_FIELD(indexed_method_dict, 2) = sel_at_put;
    OBJ_FIELD(indexed_method_dict, 3) = (uint64_t)make_primitive_method(world, PRIM_AT_PUT, 2);
    OBJ_FIELD(indexed_method_dict, 4) = sel_size;
    OBJ_FIELD(indexed_method_dict, 5) = (uint64_t)make_primitive_method(world, PRIM_SIZE, 0);
    OBJ_FIELD(array_class, CLASS_METHOD_DICT) = (uint64_t)indexed_method_dict;
    OBJ_FIELD(bytes_class, CLASS_METHOD_DICT) = (uint64_t)indexed_method_dict;

    uint64_t *array = om_alloc(world->om, (uint64_t)array_class, FORMAT_INDEXABLE, 3);
    OBJ_FIELD(array, 0) = tag_smallint(10);
    OBJ_FIELD(array, 1) = tag_smallint(20);
    OBJ_FIELD(array, 2) = tag_smallint(30);

    uint64_t *bytes = make_bytes_object(world, bytes_class, "ABC");

    CHECK_EQ(run_unary_send(world, (uint64_t)array, sel_size), tag_smallint(3),
             "C interpreter: size primitive");
    CHECK_EQ(run_binary_send(world, (uint64_t)array, tag_smallint(2), sel_at), tag_smallint(20),
             "C interpreter: at: indexable primitive");
    CHECK_EQ(run_binary_send(world, (uint64_t)bytes, tag_smallint(2), sel_at), tag_smallint('B'),
             "C interpreter: at: bytes primitive");
    CHECK_EQ(run_ternary_send_with_txn(world, (uint64_t)array, tag_smallint(2), tag_smallint(99), sel_at_put, NULL),
             (uint64_t)array,
             "C interpreter: at:put: indexable returns receiver");
    CHECK_EQ(OBJ_FIELD(array, 1), tag_smallint(99),
             "C interpreter: at:put: indexable stores value");
    CHECK_EQ(run_ternary_send_with_txn(world, (uint64_t)bytes, tag_smallint(2), tag_smallint('Z'), sel_at_put, NULL),
             (uint64_t)bytes,
             "C interpreter: at:put: bytes returns receiver");
    CHECK_EQ(((uint8_t *)&OBJ_FIELD(bytes, 0))[1], 'Z',
             "C interpreter: at:put: bytes stores byte");

    uint64_t txn_log[32] = {0};
    CHECK_EQ(run_ternary_send_with_txn(world, (uint64_t)array, tag_smallint(1), tag_smallint(77), sel_at_put, txn_log),
             (uint64_t)array,
             "C interpreter: txn at:put: indexable returns receiver");
    CHECK_EQ(OBJ_FIELD(array, 0), tag_smallint(10),
             "C interpreter: txn at:put: indexable leaves object unchanged");
    CHECK_EQ(run_binary_send_with_txn(world, (uint64_t)array, tag_smallint(1), sel_at, txn_log), tag_smallint(77),
             "C interpreter: txn at: reads pending indexable value");

    uint64_t byte_txn_log[32] = {0};
    CHECK_EQ(run_ternary_send_with_txn(world, (uint64_t)bytes, tag_smallint(1), tag_smallint('Q'), sel_at_put, byte_txn_log),
             (uint64_t)bytes,
             "C interpreter: txn at:put: bytes returns receiver");
    CHECK_EQ(((uint8_t *)&OBJ_FIELD(bytes, 0))[0], 'A',
             "C interpreter: txn at:put: bytes leaves object unchanged");
    CHECK_EQ(run_binary_send_with_txn(world, (uint64_t)bytes, tag_smallint(1), sel_at, byte_txn_log), tag_smallint('Q'),
             "C interpreter: txn at: reads pending byte value");
}

static void test_perform_primitive(SmokeWorld *world)
{
    uint64_t sel_perform = tag_smallint(7000);
    uint64_t sel_answer = tag_smallint(7001);
    uint64_t sel_basic_class = tag_smallint(7002);

    uint64_t *answer_literals = make_array(world, 1);
    OBJ_FIELD(answer_literals, 0) = tag_smallint(4242);
    uint64_t *answer_bytecodes = make_bytecodes(world, 6);
    uint8_t *abc = (uint8_t *)&OBJ_FIELD(answer_bytecodes, 0);
    abc[0] = BC_PUSH_LITERAL;
    WRITE_U32(&abc[1], 0);
    abc[5] = BC_RETURN;
    uint64_t *answer_method = make_method(world, answer_bytecodes, answer_literals, 0, 0);

    uint64_t *method_dict = make_array(world, 6);
    OBJ_FIELD(method_dict, 0) = sel_perform;
    OBJ_FIELD(method_dict, 1) = (uint64_t)make_primitive_method(world, PRIM_PERFORM, 1);
    OBJ_FIELD(method_dict, 2) = sel_answer;
    OBJ_FIELD(method_dict, 3) = (uint64_t)answer_method;
    OBJ_FIELD(method_dict, 4) = sel_basic_class;
    OBJ_FIELD(method_dict, 5) = (uint64_t)make_primitive_method(world, PRIM_BASIC_CLASS, 0);
    OBJ_FIELD(world->test_class, CLASS_METHOD_DICT) = (uint64_t)method_dict;

    CHECK_EQ(run_binary_send(world, (uint64_t)world->receiver, sel_answer, sel_perform), tag_smallint(4242),
             "C interpreter: perform: sends normal target method");
    CHECK_EQ(run_binary_send(world, (uint64_t)world->receiver, sel_basic_class, sel_perform),
             (uint64_t)world->test_class,
             "C interpreter: perform: primitive target keeps receiver on stack");
}

static void test_inst_var_transaction_and_barrier(SmokeWorld *world)
{
    uint64_t *literals = make_array(world, 1);
    OBJ_FIELD(literals, 0) = tag_smallint(99);
    uint64_t *bytecodes = make_bytecodes(world, 16);
    uint8_t *bc = (uint8_t *)&OBJ_FIELD(bytecodes, 0);
    uint64_t ip = 0;
    bc[ip++] = BC_PUSH_LITERAL;
    WRITE_U32(&bc[ip], 0);
    ip += 4;
    bc[ip++] = BC_STORE_INST_VAR;
    WRITE_U32(&bc[ip], 0);
    ip += 4;
    bc[ip++] = BC_PUSH_INST_VAR;
    WRITE_U32(&bc[ip], 0);
    ip += 4;
    bc[ip++] = BC_HALT;

    uint64_t *method = make_method(world, bytecodes, literals, 0, 0);
    uint64_t txn_log[32] = {0};
    CHECK_EQ(run_method_with_txn(world, method, (uint64_t)world->receiver, NULL, 0, txn_log),
             tag_smallint(99),
             "C interpreter: txn inst var read sees pending write");
    CHECK_EQ(OBJ_FIELD(world->receiver, 0), tag_smallint(111),
             "C interpreter: txn inst var write leaves object unchanged");
    CHECK_EQ(txn_log[0], 1, "C interpreter: txn inst var write records log entry");

    static uint8_t young_a[4096] __attribute__((aligned(8)));
    static uint8_t young_b[4096] __attribute__((aligned(8)));
    static uint8_t tenured_space[4096] __attribute__((aligned(8)));
    uint64_t gc_ctx[10];
    gc_ctx_init(gc_ctx, young_a, young_b, sizeof(young_a));
    gc_ctx[GC_TENURED_START] = (uint64_t)tenured_space;
    gc_ctx[GC_TENURED_END] = (uint64_t)(tenured_space + sizeof(tenured_space));
    uint64_t remembered[32] = {0};
    gc_ctx[GC_REMEMBERED] = (uint64_t)remembered;

    uint64_t tenured_om[2];
    om_init(tenured_space, sizeof(tenured_space), tenured_om);
    uint64_t *tenured = om_alloc(tenured_om, (uint64_t)world->class_class, FORMAT_FIELDS, 1);
    uint64_t *young = om_alloc(gc_ctx, (uint64_t)world->class_class, FORMAT_FIELDS, 1);

    uint64_t *barrier_literals = om_alloc(gc_ctx, (uint64_t)world->class_class, FORMAT_INDEXABLE, 1);
    OBJ_FIELD(barrier_literals, 0) = (uint64_t)young;
    uint64_t *barrier_bytecodes = om_alloc(gc_ctx, (uint64_t)world->class_class, FORMAT_BYTES, 16);
    uint8_t *bbc = (uint8_t *)&OBJ_FIELD(barrier_bytecodes, 0);
    ip = 0;
    bbc[ip++] = BC_PUSH_LITERAL;
    WRITE_U32(&bbc[ip], 0);
    ip += 4;
    bbc[ip++] = BC_STORE_INST_VAR;
    WRITE_U32(&bbc[ip], 0);
    ip += 4;
    bbc[ip++] = BC_PUSH_INST_VAR;
    WRITE_U32(&bbc[ip], 0);
    ip += 4;
    bbc[ip++] = BC_HALT;
    uint64_t *barrier_method = om_alloc(gc_ctx, (uint64_t)world->class_class, FORMAT_FIELDS, 5);
    OBJ_FIELD(barrier_method, CM_PRIMITIVE) = tag_smallint(PRIM_NONE);
    OBJ_FIELD(barrier_method, CM_NUM_ARGS) = tag_smallint(0);
    OBJ_FIELD(barrier_method, CM_NUM_TEMPS) = tag_smallint(0);
    OBJ_FIELD(barrier_method, CM_LITERALS) = (uint64_t)barrier_literals;
    OBJ_FIELD(barrier_method, CM_BYTECODES) = (uint64_t)barrier_bytecodes;

    CHECK_EQ(run_method_with_om(world, barrier_method, (uint64_t)tenured, NULL, 0, gc_ctx, NULL),
             (uint64_t)young,
             "C interpreter: write barrier store leaves young value on top");
    CHECK_EQ(OBJ_FIELD(tenured, 0), (uint64_t)young,
             "C interpreter: write barrier stores young object");
    CHECK_EQ(remembered[0], 1, "C interpreter: write barrier records remembered set entry");
}

static void test_allocation_primitives(SmokeWorld *world)
{
    uint64_t sel_basic_new = tag_smallint(8000);
    uint64_t sel_basic_new_size = tag_smallint(8001);

    uint64_t *class_method_dict = make_array(world, 4);
    OBJ_FIELD(class_method_dict, 0) = sel_basic_new;
    OBJ_FIELD(class_method_dict, 1) = (uint64_t)make_primitive_method(world, PRIM_BASIC_NEW, 0);
    OBJ_FIELD(class_method_dict, 2) = sel_basic_new_size;
    OBJ_FIELD(class_method_dict, 3) = (uint64_t)make_primitive_method(world, PRIM_BASIC_NEW_SIZE, 1);
    OBJ_FIELD(world->class_class, CLASS_METHOD_DICT) = (uint64_t)class_method_dict;

    uint64_t object = run_unary_send(world, (uint64_t)world->test_class, sel_basic_new);
    CHECK_EQ(OBJ_CLASS((uint64_t *)object), (uint64_t)world->test_class,
             "C interpreter: basicNew object class");
    CHECK_EQ(OBJ_FORMAT((uint64_t *)object), FORMAT_FIELDS,
             "C interpreter: basicNew object format");
    CHECK_EQ(OBJ_SIZE((uint64_t *)object), 2,
             "C interpreter: basicNew object size");
    CHECK_EQ(OBJ_FIELD((uint64_t *)object, 0), tagged_nil(),
             "C interpreter: basicNew initializes fields to nil");

    uint64_t *array_class = om_alloc(world->om, (uint64_t)world->class_class, FORMAT_FIELDS, 5);
    OBJ_FIELD(array_class, CLASS_SUPERCLASS) = tagged_nil();
    OBJ_FIELD(array_class, CLASS_METHOD_DICT) = tagged_nil();
    OBJ_FIELD(array_class, CLASS_INST_SIZE) = tag_smallint(0);
    OBJ_FIELD(array_class, CLASS_INST_FORMAT) = tag_smallint(FORMAT_INDEXABLE);
    OBJ_FIELD(array_class, CLASS_INST_VARS) = tagged_nil();

    uint64_t array = run_binary_send(world, (uint64_t)array_class, tag_smallint(3), sel_basic_new_size);
    CHECK_EQ(OBJ_CLASS((uint64_t *)array), (uint64_t)array_class,
             "C interpreter: basicNew: array class");
    CHECK_EQ(OBJ_FORMAT((uint64_t *)array), FORMAT_INDEXABLE,
             "C interpreter: basicNew: array format");
    CHECK_EQ(OBJ_SIZE((uint64_t *)array), 3,
             "C interpreter: basicNew: array size");
    CHECK_EQ(OBJ_FIELD((uint64_t *)array, 2), tagged_nil(),
             "C interpreter: basicNew: initializes indexable slots to nil");

    uint64_t *bytes_class = om_alloc(world->om, (uint64_t)world->class_class, FORMAT_FIELDS, 5);
    OBJ_FIELD(bytes_class, CLASS_SUPERCLASS) = tagged_nil();
    OBJ_FIELD(bytes_class, CLASS_METHOD_DICT) = tagged_nil();
    OBJ_FIELD(bytes_class, CLASS_INST_SIZE) = tag_smallint(0);
    OBJ_FIELD(bytes_class, CLASS_INST_FORMAT) = tag_smallint(FORMAT_BYTES);
    OBJ_FIELD(bytes_class, CLASS_INST_VARS) = tagged_nil();

    uint64_t bytes = run_binary_send(world, (uint64_t)bytes_class, tag_smallint(5), sel_basic_new_size);
    CHECK_EQ(OBJ_CLASS((uint64_t *)bytes), (uint64_t)bytes_class,
             "C interpreter: basicNew: bytes class");
    CHECK_EQ(OBJ_FORMAT((uint64_t *)bytes), FORMAT_BYTES,
             "C interpreter: basicNew: bytes format");
    CHECK_EQ(OBJ_SIZE((uint64_t *)bytes), 5,
             "C interpreter: basicNew: bytes size");

    static uint8_t gc_a[4096] __attribute__((aligned(8)));
    static uint8_t gc_b[4096] __attribute__((aligned(8)));
    uint64_t gc_ctx[10];
    gc_ctx_init(gc_ctx, gc_a, gc_b, sizeof(gc_a));

    uint64_t *gc_inst_class = om_alloc(gc_ctx, (uint64_t)world->class_class, FORMAT_FIELDS, 5);
    OBJ_FIELD(gc_inst_class, CLASS_SUPERCLASS) = tagged_nil();
    OBJ_FIELD(gc_inst_class, CLASS_METHOD_DICT) = tagged_nil();
    OBJ_FIELD(gc_inst_class, CLASS_INST_SIZE) = tag_smallint(3);
    OBJ_FIELD(gc_inst_class, CLASS_INST_FORMAT) = tag_smallint(FORMAT_FIELDS);
    OBJ_FIELD(gc_inst_class, CLASS_INST_VARS) = tagged_nil();

    uint64_t *gc_lits = om_alloc(gc_ctx, (uint64_t)world->class_class, FORMAT_INDEXABLE, 2);
    OBJ_FIELD(gc_lits, 0) = (uint64_t)gc_inst_class;
    OBJ_FIELD(gc_lits, 1) = sel_basic_new;
    uint64_t *gc_bytecodes = om_alloc(gc_ctx, (uint64_t)world->class_class, FORMAT_BYTES, 15);
    uint8_t *gbc = (uint8_t *)&OBJ_FIELD(gc_bytecodes, 0);
    gbc[0] = BC_PUSH_LITERAL;
    WRITE_U32(&gbc[1], 0);
    gbc[5] = BC_SEND_MESSAGE;
    WRITE_U32(&gbc[6], 1);
    WRITE_U32(&gbc[10], 0);
    gbc[14] = BC_HALT;
    uint64_t *gc_method = om_alloc(gc_ctx, (uint64_t)world->class_class, FORMAT_FIELDS, 5);
    OBJ_FIELD(gc_method, CM_PRIMITIVE) = tag_smallint(PRIM_NONE);
    OBJ_FIELD(gc_method, CM_NUM_ARGS) = tag_smallint(0);
    OBJ_FIELD(gc_method, CM_NUM_TEMPS) = tag_smallint(0);
    OBJ_FIELD(gc_method, CM_LITERALS) = (uint64_t)gc_lits;
    OBJ_FIELD(gc_method, CM_BYTECODES) = (uint64_t)gc_bytecodes;

    uint64_t remaining = gc_ctx[GC_FROM_END] - gc_ctx[GC_FROM_FREE];
    if (remaining > 24)
    {
        uint64_t fill = (remaining - 24) / 8;
        if (fill > 3)
        {
            (void)om_alloc(gc_ctx, (uint64_t)world->class_class, FORMAT_FIELDS, fill - 3);
        }
    }

    uint64_t gc_object = run_method_with_om(world, gc_method, tag_smallint(0), NULL, 0, gc_ctx, NULL);
    CHECK_EQ(gc_object >= gc_ctx[GC_FROM_START], 1,
             "C interpreter: basicNew GC retry returns active-space object");
    CHECK_EQ(gc_object < gc_ctx[GC_FROM_END], 1,
             "C interpreter: basicNew GC retry result within active semispace");
    CHECK_EQ(OBJ_SIZE((uint64_t *)gc_object), 3,
             "C interpreter: basicNew GC retry preserves class shape");
    CHECK_EQ(OBJ_FIELD((uint64_t *)gc_object, 2), tagged_nil(),
             "C interpreter: basicNew GC retry initializes fields");
}

static void test_block_primitives(SmokeWorld *world)
{
    uint64_t sel_value = tag_smallint(9000);
    uint64_t sel_value_arg = tag_smallint(9001);
    uint64_t sel_cannot_return = cannot_return_selector_oop();
    uint64_t *block_class = (uint64_t *)OBJ_FIELD(world->class_table, CLASS_TABLE_BLOCK);
    uint64_t *cannot_return_bytecodes = make_bytecodes(world, 6);
    uint8_t *crb = (uint8_t *)&OBJ_FIELD(cannot_return_bytecodes, 0);
    crb[0] = BC_PUSH_ARG;
    WRITE_U32(&crb[1], 0);
    crb[5] = BC_RETURN;
    uint64_t *cannot_return = make_method(world, cannot_return_bytecodes, NULL, 1, 0);
    uint64_t *block_method_dict = make_array(world, 6);
    OBJ_FIELD(block_method_dict, 0) = sel_value;
    OBJ_FIELD(block_method_dict, 1) = (uint64_t)make_primitive_method(world, PRIM_BLOCK_VALUE, 0);
    OBJ_FIELD(block_method_dict, 2) = sel_value_arg;
    OBJ_FIELD(block_method_dict, 3) = (uint64_t)make_primitive_method(world, PRIM_BLOCK_VALUE_ARG, 1);
    OBJ_FIELD(block_method_dict, 4) = sel_cannot_return;
    OBJ_FIELD(block_method_dict, 5) = (uint64_t)cannot_return;
    OBJ_FIELD(block_class, CLASS_METHOD_DICT) = (uint64_t)block_method_dict;

    uint64_t *literal_body_literals = make_array(world, 1);
    OBJ_FIELD(literal_body_literals, 0) = tag_smallint(77);
    uint64_t *literal_body_bytecodes = make_bytecodes(world, 6);
    uint8_t *lbb = (uint8_t *)&OBJ_FIELD(literal_body_bytecodes, 0);
    lbb[0] = BC_PUSH_LITERAL;
    WRITE_U32(&lbb[1], 0);
    lbb[5] = BC_RETURN;
    uint64_t *literal_body = make_method(world, literal_body_bytecodes, literal_body_literals, 0, 0);

    uint64_t *caller_literals = make_array(world, 2);
    OBJ_FIELD(caller_literals, 0) = (uint64_t)literal_body;
    OBJ_FIELD(caller_literals, 1) = sel_value;
    uint64_t *caller_bytecodes = make_bytecodes(world, 15);
    uint8_t *cb = (uint8_t *)&OBJ_FIELD(caller_bytecodes, 0);
    cb[0] = BC_PUSH_CLOSURE;
    WRITE_U32(&cb[1], 0);
    cb[5] = BC_SEND_MESSAGE;
    WRITE_U32(&cb[6], 1);
    WRITE_U32(&cb[10], 0);
    cb[14] = BC_HALT;
    uint64_t *caller = make_method(world, caller_bytecodes, caller_literals, 0, 0);
    CHECK_EQ(run_method(world, caller, (uint64_t)world->receiver, NULL, 0), tag_smallint(77),
             "C interpreter: Block value returns literal");

    uint64_t *inspect_literals = make_array(world, 1);
    OBJ_FIELD(inspect_literals, 0) = (uint64_t)literal_body;
    uint64_t *inspect_bytecodes = make_bytecodes(world, 6);
    uint8_t *ib = (uint8_t *)&OBJ_FIELD(inspect_bytecodes, 0);
    ib[0] = BC_PUSH_CLOSURE;
    WRITE_U32(&ib[1], 0);
    ib[5] = BC_HALT;
    uint64_t *inspect = make_method(world, inspect_bytecodes, inspect_literals, 0, 0);
    uint64_t block = run_method(world, inspect, (uint64_t)world->receiver, NULL, 0);
    CHECK_EQ(OBJ_CLASS((uint64_t *)block), (uint64_t)block_class,
             "C interpreter: PUSH_CLOSURE returns Block object");
    CHECK_EQ(OBJ_FIELD((uint64_t *)block, BLOCK_HOME_RECEIVER), (uint64_t)world->receiver,
             "C interpreter: PUSH_CLOSURE records home receiver");
    CHECK_EQ(OBJ_FIELD((uint64_t *)block, BLOCK_CM), (uint64_t)literal_body,
             "C interpreter: PUSH_CLOSURE records block method");
    CHECK_EQ(OBJ_CLASS((uint64_t *)OBJ_FIELD((uint64_t *)block, BLOCK_HOME_CONTEXT)),
             (uint64_t)world->context_class,
             "C interpreter: PUSH_CLOSURE records home context");

    uint64_t *arg_body_bytecodes = make_bytecodes(world, 6);
    uint8_t *abb = (uint8_t *)&OBJ_FIELD(arg_body_bytecodes, 0);
    abb[0] = BC_PUSH_ARG;
    WRITE_U32(&abb[1], 0);
    abb[5] = BC_RETURN;
    uint64_t *arg_body = make_method(world, arg_body_bytecodes, NULL, 1, 0);

    uint64_t *arg_caller_literals = make_array(world, 3);
    OBJ_FIELD(arg_caller_literals, 0) = (uint64_t)arg_body;
    OBJ_FIELD(arg_caller_literals, 1) = tag_smallint(88);
    OBJ_FIELD(arg_caller_literals, 2) = sel_value_arg;
    uint64_t *arg_caller_bytecodes = make_bytecodes(world, 20);
    uint8_t *acb = (uint8_t *)&OBJ_FIELD(arg_caller_bytecodes, 0);
    acb[0] = BC_PUSH_CLOSURE;
    WRITE_U32(&acb[1], 0);
    acb[5] = BC_PUSH_LITERAL;
    WRITE_U32(&acb[6], 1);
    acb[10] = BC_SEND_MESSAGE;
    WRITE_U32(&acb[11], 2);
    WRITE_U32(&acb[15], 1);
    acb[19] = BC_HALT;
    uint64_t *arg_caller = make_method(world, arg_caller_bytecodes, arg_caller_literals, 0, 0);
    CHECK_EQ(run_method(world, arg_caller, (uint64_t)world->receiver, NULL, 0), tag_smallint(88),
             "C interpreter: Block value: passes argument");

    uint64_t *copied_body_bytecodes = make_bytecodes(world, 6);
    uint8_t *cbb = (uint8_t *)&OBJ_FIELD(copied_body_bytecodes, 0);
    cbb[0] = BC_PUSH_TEMP;
    WRITE_U32(&cbb[1], 0);
    cbb[5] = BC_RETURN;
    uint64_t *copied_body = make_method(world, copied_body_bytecodes, NULL, 0, 0);

    uint64_t *copied_caller_literals = make_array(world, 4);
    OBJ_FIELD(copied_caller_literals, 0) = tag_smallint(41);
    OBJ_FIELD(copied_caller_literals, 1) = (uint64_t)copied_body;
    OBJ_FIELD(copied_caller_literals, 2) = tag_smallint(99);
    OBJ_FIELD(copied_caller_literals, 3) = sel_value;
    uint64_t *copied_caller_bytecodes = make_bytecodes(world, 35);
    uint8_t *ccb = (uint8_t *)&OBJ_FIELD(copied_caller_bytecodes, 0);
    ccb[0] = BC_PUSH_LITERAL;
    WRITE_U32(&ccb[1], 0);
    ccb[5] = BC_STORE_TEMP;
    WRITE_U32(&ccb[6], 0);
    ccb[10] = BC_PUSH_CLOSURE;
    WRITE_U32(&ccb[11], 1);
    ccb[15] = BC_PUSH_LITERAL;
    WRITE_U32(&ccb[16], 2);
    ccb[20] = BC_STORE_TEMP;
    WRITE_U32(&ccb[21], 0);
    ccb[25] = BC_SEND_MESSAGE;
    WRITE_U32(&ccb[26], 3);
    WRITE_U32(&ccb[30], 0);
    ccb[34] = BC_HALT;
    uint64_t *copied_caller = make_method(world, copied_caller_bytecodes, copied_caller_literals, 0, 1);
    CHECK_EQ(run_method(world, copied_caller, (uint64_t)world->receiver, NULL, 0), tag_smallint(41),
             "C interpreter: Block copied temp keeps creation value");

    uint64_t *nlr_body_literals = make_array(world, 1);
    OBJ_FIELD(nlr_body_literals, 0) = tag_smallint(88);
    uint64_t *nlr_body_bytecodes = make_bytecodes(world, 6);
    uint8_t *nbb = (uint8_t *)&OBJ_FIELD(nlr_body_bytecodes, 0);
    nbb[0] = BC_PUSH_LITERAL;
    WRITE_U32(&nbb[1], 0);
    nbb[5] = BC_RETURN_NON_LOCAL;
    uint64_t *nlr_body = make_method(world, nlr_body_bytecodes, nlr_body_literals, 0, 0);

    uint64_t *nlr_caller_literals = make_array(world, 3);
    OBJ_FIELD(nlr_caller_literals, 0) = (uint64_t)nlr_body;
    OBJ_FIELD(nlr_caller_literals, 1) = sel_value;
    OBJ_FIELD(nlr_caller_literals, 2) = tag_smallint(99);
    uint64_t *nlr_caller_bytecodes = make_bytecodes(world, 20);
    uint8_t *ncb = (uint8_t *)&OBJ_FIELD(nlr_caller_bytecodes, 0);
    ncb[0] = BC_PUSH_CLOSURE;
    WRITE_U32(&ncb[1], 0);
    ncb[5] = BC_SEND_MESSAGE;
    WRITE_U32(&ncb[6], 1);
    WRITE_U32(&ncb[10], 0);
    ncb[14] = BC_PUSH_LITERAL;
    WRITE_U32(&ncb[15], 2);
    ncb[19] = BC_RETURN;
    uint64_t *nlr_caller = make_method(world, nlr_caller_bytecodes, nlr_caller_literals, 0, 0);
    CHECK_EQ(run_method(world, nlr_caller, (uint64_t)world->receiver, NULL, 0), tag_smallint(88),
             "C interpreter: Block non-local return unwinds to home");

    uint64_t *escaped_maker_literals = make_array(world, 1);
    OBJ_FIELD(escaped_maker_literals, 0) = (uint64_t)nlr_body;
    uint64_t *escaped_maker_bytecodes = make_bytecodes(world, 6);
    uint8_t *emb = (uint8_t *)&OBJ_FIELD(escaped_maker_bytecodes, 0);
    emb[0] = BC_PUSH_CLOSURE;
    WRITE_U32(&emb[1], 0);
    emb[5] = BC_RETURN;
    uint64_t *escaped_maker = make_method(world, escaped_maker_bytecodes, escaped_maker_literals, 0, 0);
    uint64_t escaped_block = run_method(world, escaped_maker, (uint64_t)world->receiver, NULL, 0);

    uint64_t *escaped_invoke_literals = make_array(world, 2);
    OBJ_FIELD(escaped_invoke_literals, 0) = escaped_block;
    OBJ_FIELD(escaped_invoke_literals, 1) = sel_value;
    uint64_t *escaped_invoke_bytecodes = make_bytecodes(world, 15);
    uint8_t *eib = (uint8_t *)&OBJ_FIELD(escaped_invoke_bytecodes, 0);
    eib[0] = BC_PUSH_LITERAL;
    WRITE_U32(&eib[1], 0);
    eib[5] = BC_SEND_MESSAGE;
    WRITE_U32(&eib[6], 1);
    WRITE_U32(&eib[10], 0);
    eib[14] = BC_HALT;
    uint64_t *escaped_invoke = make_method(world, escaped_invoke_bytecodes, escaped_invoke_literals, 0, 0);
    CHECK_EQ(run_method(world, escaped_invoke, (uint64_t)world->receiver, NULL, 0), tag_smallint(88),
             "C interpreter: escaped non-local return sends cannotReturn:");
}

static void test_this_context(SmokeWorld *world)
{
    uint64_t *bytecodes = make_bytecodes(world, 12);
    uint8_t *bc = (uint8_t *)&OBJ_FIELD(bytecodes, 0);
    bc[0] = BC_PUSH_LITERAL;
    WRITE_U32(&bc[1], 0);
    bc[5] = BC_STORE_TEMP;
    WRITE_U32(&bc[6], 0);
    bc[10] = BC_PUSH_THIS_CONTEXT;
    bc[11] = BC_HALT;
    uint64_t *literals = make_array(world, 1);
    OBJ_FIELD(literals, 0) = tag_smallint(5150);
    uint64_t arg = tag_smallint(123);
    uint64_t *method = make_method(world, bytecodes, literals, 1, 1);
    uint64_t context_oop = run_method(world, method, (uint64_t)world->receiver, &arg, 1);
    uint64_t *context = (uint64_t *)context_oop;

    CHECK_EQ(OBJ_CLASS(context), (uint64_t)world->context_class,
             "C interpreter: PUSH_THIS_CONTEXT returns Context");
    CHECK_EQ(OBJ_FIELD(context, CONTEXT_METHOD), (uint64_t)method,
             "C interpreter: context stores method");
    CHECK_EQ(OBJ_FIELD(context, CONTEXT_RECEIVER), (uint64_t)world->receiver,
             "C interpreter: context stores receiver");
    CHECK_EQ(OBJ_FIELD(context, CONTEXT_NUM_ARGS), tag_smallint(1),
             "C interpreter: context stores arg count");
    CHECK_EQ(OBJ_FIELD(context, CONTEXT_NUM_TEMPS), tag_smallint(1),
             "C interpreter: context stores temp count");
    CHECK_EQ(OBJ_FIELD(context, CONTEXT_VAR_BASE), arg,
             "C interpreter: context stores arg value");
    CHECK_EQ(OBJ_FIELD(context, CONTEXT_VAR_BASE + 1), tag_smallint(5150),
             "C interpreter: context stores temp value");
}

int main(void)
{
    setbuf(stdout, NULL);

    SmokeWorld world;
    init_world(&world);

    test_push_literal_halt(&world);
    test_push_global_halt(&world);
    test_self_inst_var_temp_return(&world);
    test_push_arg_duplicate_return(&world);
    test_jumps(&world);
    test_zero_arg_send(&world);
    test_one_arg_send(&world);
    test_smallint_primitives(&world);
    test_identity_class_hash_primitives(&world);
    test_character_primitives(&world);
    test_string_symbol_primitives(&world);
    test_indexed_primitives(&world);
    test_perform_primitive(&world);
    test_inst_var_transaction_and_barrier(&world);
    test_allocation_primitives(&world);
    test_block_primitives(&world);
    test_this_context(&world);

    printf("\n%d C interpreter smoke tests passed, %d failed\n", passes, failures);
    return failures == 0 ? 0 : 1;
}
