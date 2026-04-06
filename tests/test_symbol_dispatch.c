#include "test_defs.h"

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
    uint64_t *symbol_class = ctx->symbol_class;

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
}
