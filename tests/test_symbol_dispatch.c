#include "test_defs.h"

static uint64_t *make_bytes_obj(uint64_t *om, uint64_t *klass, const char *bytes, uint64_t n);

static void trap_symbol_eq_wrong_receiver(TestContext *ctx)
{
    uint64_t *om = ctx->om;
    uint64_t *class_class = ctx->class_class;
    uint64_t *class_table = ctx->class_table;
    uint64_t *stack = ctx->stack;
    uint64_t *sp;
    uint64_t *fp;
    uint64_t sel_eq = tag_smallint(330);

    uint64_t *prim_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 1);
    uint64_t *eq_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
    OBJ_FIELD(eq_cm, CM_PRIMITIVE) = tag_smallint(PRIM_SYMBOL_EQ);
    OBJ_FIELD(eq_cm, CM_NUM_ARGS) = tag_smallint(1);
    OBJ_FIELD(eq_cm, CM_NUM_TEMPS) = tag_smallint(0);
    OBJ_FIELD(eq_cm, CM_LITERALS) = tagged_nil();
    OBJ_FIELD(eq_cm, CM_BYTECODES) = (uint64_t)prim_bc;

    uint64_t *recv_class = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 4);
    OBJ_FIELD(recv_class, CLASS_SUPERCLASS) = tagged_nil();
    OBJ_FIELD(recv_class, CLASS_INST_SIZE) = tag_smallint(0);
    OBJ_FIELD(recv_class, CLASS_INST_FORMAT) = tag_smallint(FORMAT_FIELDS);
    uint64_t *recv_md = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 2);
    OBJ_FIELD(recv_md, 0) = sel_eq;
    OBJ_FIELD(recv_md, 1) = (uint64_t)eq_cm;
    OBJ_FIELD(recv_class, CLASS_METHOD_DICT) = (uint64_t)recv_md;

    uint64_t *recv_obj = om_alloc(om, (uint64_t)recv_class, FORMAT_FIELDS, 0);
    uint64_t arg = (uint64_t)make_bytes_obj(om, ctx->symbol_class, "alpha", 5);

    uint64_t *caller_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 20);
    uint8_t *p = (uint8_t *)&OBJ_FIELD(caller_bc, 0);
    p[0] = BC_PUSH_SELF;
    p[1] = BC_PUSH_LITERAL;
    WRITE_U32(&p[2], 0);
    p[6] = BC_SEND_MESSAGE;
    WRITE_U32(&p[7], 1);
    WRITE_U32(&p[11], 1);
    p[15] = BC_HALT;

    uint64_t *lits = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 2);
    OBJ_FIELD(lits, 0) = arg;
    OBJ_FIELD(lits, 1) = sel_eq;

    uint64_t *caller_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
    OBJ_FIELD(caller_cm, CM_PRIMITIVE) = tag_smallint(0);
    OBJ_FIELD(caller_cm, CM_NUM_ARGS) = tag_smallint(0);
    OBJ_FIELD(caller_cm, CM_NUM_TEMPS) = tag_smallint(0);
    OBJ_FIELD(caller_cm, CM_LITERALS) = (uint64_t)lits;
    OBJ_FIELD(caller_cm, CM_BYTECODES) = (uint64_t)caller_bc;

    sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
    fp = (uint64_t *)0xCAFE;
    stack_push(&sp, stack, (uint64_t)recv_obj);
    activate_method(&sp, &fp, 0, (uint64_t)caller_cm, 0, 0);
    (void)interpret(&sp, &fp, (uint8_t *)&OBJ_FIELD(caller_bc, 0), class_table, om, NULL);
}

static uint64_t *make_bytes_obj(uint64_t *om, uint64_t *klass, const char *bytes, uint64_t n)
{
    uint64_t *obj = om_alloc(om, (uint64_t)klass, FORMAT_BYTES, n);
    uint8_t *dst = (uint8_t *)&OBJ_FIELD(obj, 0);
    memcpy(dst, bytes, (size_t)n);
    return obj;
}

static uint64_t run_symbol_eq_send(TestContext *ctx, uint64_t receiver, uint64_t argument)
{
    uint64_t *om = ctx->om;
    uint64_t *class_class = ctx->class_class;
    uint64_t *stack = ctx->stack;
    uint64_t *class_table = ctx->class_table;
    uint64_t *sp;
    uint64_t *fp;

    uint64_t sel_eq_symbol = tag_smallint(103);

    uint64_t *caller_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 20);
    uint8_t *p = (uint8_t *)&OBJ_FIELD(caller_bc, 0);
    p[0] = BC_PUSH_SELF;
    p[1] = BC_PUSH_LITERAL;
    WRITE_U32(&p[2], 1);
    p[6] = BC_SEND_MESSAGE;
    WRITE_U32(&p[7], 0);
    WRITE_U32(&p[11], 1);
    p[15] = BC_HALT;

    uint64_t *lits = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 2);
    OBJ_FIELD(lits, 0) = sel_eq_symbol;
    OBJ_FIELD(lits, 1) = argument;

    uint64_t *caller_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
    OBJ_FIELD(caller_cm, CM_PRIMITIVE) = tag_smallint(0);
    OBJ_FIELD(caller_cm, CM_NUM_ARGS) = tag_smallint(0);
    OBJ_FIELD(caller_cm, CM_NUM_TEMPS) = tag_smallint(0);
    OBJ_FIELD(caller_cm, CM_LITERALS) = (uint64_t)lits;
    OBJ_FIELD(caller_cm, CM_BYTECODES) = (uint64_t)caller_bc;

    sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
    fp = (uint64_t *)0xCAFE;
    stack_push(&sp, stack, receiver);
    activate_method(&sp, &fp, 0, (uint64_t)caller_cm, 0, 0);

    return interpret(&sp, &fp, (uint8_t *)&OBJ_FIELD(caller_bc, 0), class_table, om, NULL);
}

void test_symbol_dispatch(TestContext *ctx)
{
    uint64_t *om = ctx->om;
    uint64_t *class_class = ctx->class_class;
    uint64_t *symbol_class = ctx->symbol_class;
    uint64_t *class_table = ctx->class_table;
    uint64_t *stack = ctx->stack;
    uint64_t *sp;
    uint64_t *fp;
    uint64_t result;

    uint64_t sym1 = (uint64_t)make_bytes_obj(om, symbol_class, "alpha", 5);
    uint64_t sym2 = sym1;
    uint64_t sym3 = (uint64_t)make_bytes_obj(om, symbol_class, "beta", 4);

    uint64_t *same_bytes_non_interned = make_bytes_obj(om, symbol_class, "alpha", 5);

    ASSERT_EQ(ctx, run_symbol_eq_send(ctx, sym1, sym2), tagged_true(),
              "dispatch Symbol>>=: interned same bytes are identical");
    ASSERT_EQ(ctx, run_symbol_eq_send(ctx, sym1, sym3), tagged_false(),
              "dispatch Symbol>>=: different symbols are not identical");
    ASSERT_EQ(ctx, run_symbol_eq_send(ctx, sym1, (uint64_t)same_bytes_non_interned), tagged_false(),
              "dispatch Symbol>>=: same bytes but distinct object is false");

    {
        uint64_t sel_eq_symbol = tag_smallint(103);

        uint64_t *eq_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 6);
        uint8_t *eqb = (uint8_t *)&OBJ_FIELD(eq_bc, 0);
        eqb[0] = BC_PUSH_LITERAL;
        WRITE_U32(&eqb[1], 0);
        eqb[5] = BC_RETURN;

        uint64_t *eq_lits = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 1);
        OBJ_FIELD(eq_lits, 0) = tagged_false();

        uint64_t *eq_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(eq_cm, CM_PRIMITIVE) = tag_smallint(PRIM_SYMBOL_EQ);
        OBJ_FIELD(eq_cm, CM_NUM_ARGS) = tag_smallint(1);
        OBJ_FIELD(eq_cm, CM_NUM_TEMPS) = tag_smallint(0);
        OBJ_FIELD(eq_cm, CM_LITERALS) = (uint64_t)eq_lits;
        OBJ_FIELD(eq_cm, CM_BYTECODES) = (uint64_t)eq_bc;

        uint64_t *eq_class = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 4);
        OBJ_FIELD(eq_class, CLASS_SUPERCLASS) = tagged_nil();
        OBJ_FIELD(eq_class, CLASS_INST_SIZE) = tag_smallint(0);
        OBJ_FIELD(eq_class, CLASS_INST_FORMAT) = tag_smallint(FORMAT_BYTES);
        uint64_t *eq_md = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 2);
        OBJ_FIELD(eq_md, 0) = sel_eq_symbol;
        OBJ_FIELD(eq_md, 1) = (uint64_t)eq_cm;
        OBJ_FIELD(eq_class, CLASS_METHOD_DICT) = (uint64_t)eq_md;

        uint64_t *recv = make_bytes_obj(om, eq_class, "alpha", 5);

        uint64_t *caller_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 20);
        uint8_t *p = (uint8_t *)&OBJ_FIELD(caller_bc, 0);
        p[0] = BC_PUSH_SELF;
        p[1] = BC_PUSH_LITERAL;
        WRITE_U32(&p[2], 0);
        p[6] = BC_SEND_MESSAGE;
        WRITE_U32(&p[7], 1);
        WRITE_U32(&p[11], 1);
        p[15] = BC_HALT;

        uint64_t *lits = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 2);
        OBJ_FIELD(lits, 0) = tagged_nil();
        OBJ_FIELD(lits, 1) = sel_eq_symbol;

        uint64_t *caller_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(caller_cm, CM_PRIMITIVE) = tag_smallint(0);
        OBJ_FIELD(caller_cm, CM_NUM_ARGS) = tag_smallint(0);
        OBJ_FIELD(caller_cm, CM_NUM_TEMPS) = tag_smallint(0);
        OBJ_FIELD(caller_cm, CM_LITERALS) = (uint64_t)lits;
        OBJ_FIELD(caller_cm, CM_BYTECODES) = (uint64_t)caller_bc;

        sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
        fp = (uint64_t *)0xCAFE;
        stack_push(&sp, stack, (uint64_t)recv);
        activate_method(&sp, &fp, 0, (uint64_t)caller_cm, 0, 0);
        result = interpret(&sp, &fp, (uint8_t *)&OBJ_FIELD(caller_bc, 0), class_table, om, NULL);

        ASSERT_EQ(ctx, result, tagged_false(),
                  "primitive failure: Symbol>>= wrong arg falls through to method body");
        ASSERT_EQ(ctx, (uint64_t)run_trap_test(ctx, trap_symbol_eq_wrong_receiver),
                  (uint64_t)SIGTRAP,
                  "primitive trap: Symbol>>= wrong receiver traps");
    }
}
