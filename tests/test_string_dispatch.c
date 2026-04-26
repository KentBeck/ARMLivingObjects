#include "test_defs.h"
#include "smalltalk_test_support.h"

static uint64_t *make_bytes_obj(uint64_t *om, uint64_t *class_class, uint64_t *string_class, const char *bytes, uint64_t n);

static void trap_string_eq_wrong_receiver(TestContext *ctx)
{
    uint64_t *om = ctx->om;
    uint64_t *class_class = ctx->class_class;
    uint64_t *class_table = ctx->class_table;
    uint64_t *stack = ctx->stack;
    uint64_t *sp;
    uint64_t *fp;
    uint64_t sel_eq = tag_smallint(320);

    uint64_t *prim_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 1);
    uint64_t *eq_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
    OBJ_FIELD(eq_cm, CM_PRIMITIVE) = tag_smallint(PRIM_STRING_EQ);
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
    uint64_t *arg_obj = make_bytes_obj(om, class_class, ctx->string_class, "a", 1);

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
    OBJ_FIELD(lits, 0) = (uint64_t)arg_obj;
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

static void trap_string_hash_wrong_receiver(TestContext *ctx)
{
    uint64_t *om = ctx->om;
    uint64_t *class_class = ctx->class_class;
    uint64_t *class_table = ctx->class_table;
    uint64_t *stack = ctx->stack;
    uint64_t *sp;
    uint64_t *fp;
    uint64_t sel_hash = tag_smallint(321);

    uint64_t *prim_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 1);
    uint64_t *hash_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
    OBJ_FIELD(hash_cm, CM_PRIMITIVE) = tag_smallint(PRIM_STRING_HASH_FNV);
    OBJ_FIELD(hash_cm, CM_NUM_ARGS) = tag_smallint(0);
    OBJ_FIELD(hash_cm, CM_NUM_TEMPS) = tag_smallint(0);
    OBJ_FIELD(hash_cm, CM_LITERALS) = tagged_nil();
    OBJ_FIELD(hash_cm, CM_BYTECODES) = (uint64_t)prim_bc;

    uint64_t *recv_class = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 4);
    OBJ_FIELD(recv_class, CLASS_SUPERCLASS) = tagged_nil();
    OBJ_FIELD(recv_class, CLASS_INST_SIZE) = tag_smallint(0);
    OBJ_FIELD(recv_class, CLASS_INST_FORMAT) = tag_smallint(FORMAT_FIELDS);
    uint64_t *recv_md = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 2);
    OBJ_FIELD(recv_md, 0) = sel_hash;
    OBJ_FIELD(recv_md, 1) = (uint64_t)hash_cm;
    OBJ_FIELD(recv_class, CLASS_METHOD_DICT) = (uint64_t)recv_md;

    uint64_t *recv_obj = om_alloc(om, (uint64_t)recv_class, FORMAT_FIELDS, 0);

    uint64_t *caller_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 16);
    uint8_t *p = (uint8_t *)&OBJ_FIELD(caller_bc, 0);
    p[0] = BC_PUSH_SELF;
    p[1] = BC_SEND_MESSAGE;
    WRITE_U32(&p[2], 0);
    WRITE_U32(&p[6], 0);
    p[10] = BC_HALT;

    uint64_t *lits = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 1);
    OBJ_FIELD(lits, 0) = sel_hash;

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

static void trap_string_as_symbol_wrong_receiver(TestContext *ctx)
{
    uint64_t *om = ctx->om;
    uint64_t *class_class = ctx->class_class;
    uint64_t *class_table = ctx->class_table;
    uint64_t *stack = ctx->stack;
    uint64_t *sp;
    uint64_t *fp;
    uint64_t sel_as_symbol = tag_smallint(322);

    uint64_t *prim_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 1);
    uint64_t *as_symbol_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
    OBJ_FIELD(as_symbol_cm, CM_PRIMITIVE) = tag_smallint(PRIM_STRING_AS_SYMBOL);
    OBJ_FIELD(as_symbol_cm, CM_NUM_ARGS) = tag_smallint(0);
    OBJ_FIELD(as_symbol_cm, CM_NUM_TEMPS) = tag_smallint(0);
    OBJ_FIELD(as_symbol_cm, CM_LITERALS) = tagged_nil();
    OBJ_FIELD(as_symbol_cm, CM_BYTECODES) = (uint64_t)prim_bc;

    uint64_t *recv_class = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 4);
    OBJ_FIELD(recv_class, CLASS_SUPERCLASS) = tagged_nil();
    OBJ_FIELD(recv_class, CLASS_INST_SIZE) = tag_smallint(0);
    OBJ_FIELD(recv_class, CLASS_INST_FORMAT) = tag_smallint(FORMAT_FIELDS);
    uint64_t *recv_md = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 2);
    OBJ_FIELD(recv_md, 0) = sel_as_symbol;
    OBJ_FIELD(recv_md, 1) = (uint64_t)as_symbol_cm;
    OBJ_FIELD(recv_class, CLASS_METHOD_DICT) = (uint64_t)recv_md;

    uint64_t *recv_obj = om_alloc(om, (uint64_t)recv_class, FORMAT_FIELDS, 0);

    uint64_t *caller_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 16);
    uint8_t *p = (uint8_t *)&OBJ_FIELD(caller_bc, 0);
    p[0] = BC_PUSH_SELF;
    p[1] = BC_SEND_MESSAGE;
    WRITE_U32(&p[2], 0);
    WRITE_U32(&p[6], 0);
    p[10] = BC_HALT;

    uint64_t *lits = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 1);
    OBJ_FIELD(lits, 0) = sel_as_symbol;

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

static uint64_t *make_bytes_obj(uint64_t *om, uint64_t *class_class, uint64_t *string_class, const char *bytes, uint64_t n)
{
    uint64_t *obj = om_alloc(om, (uint64_t)string_class, FORMAT_BYTES, n);
    uint8_t *dst = (uint8_t *)&OBJ_FIELD(obj, 0);
    memcpy(dst, bytes, (size_t)n);
    return obj;
}

void test_string_dispatch(TestContext *ctx)
{
    uint64_t *om = ctx->om;
    uint64_t *class_class = ctx->class_class;
    uint64_t *smallint_class = ctx->smallint_class;
    uint64_t *string_class = ctx->string_class;
    uint64_t *class_table = ctx->class_table;
    uint64_t *stack = ctx->stack;
    uint64_t *sp;
    uint64_t *fp;
    uint64_t result;

    uint64_t sel_lt = tag_smallint(310);
    uint64_t sel_plus = tag_smallint(311);
    uint64_t sel_size = tag_smallint(72);
    uint64_t sel_at = tag_smallint(90);
    uint64_t sel_at_put = tag_smallint(91);
    uint64_t sel_basicNewSize = tag_smallint(71);
    uint64_t sel_new_size = tag_smallint(312);
    uint64_t sel_comma = tag_smallint(313);

    stt_md_append_oop(om, class_class, smallint_class, sel_lt,
                      (uint64_t)stt_make_primitive_cm(om, class_class, PRIM_SMALLINT_LT, 1));
    stt_md_append_oop(om, class_class, smallint_class, sel_plus,
                      (uint64_t)stt_make_primitive_cm(om, class_class, PRIM_SMALLINT_ADD, 1));
    stt_install_class_new_size_methods(om, class_class, sel_basicNewSize, sel_new_size);

    {
        result = stt_send_class_new_size(ctx, class_table, om, class_class,
                                         (uint64_t)string_class, sel_new_size, 5);

        ASSERT_EQ(ctx, result & 3, 0, "dispatch new:: result is object ptr");
        ASSERT_EQ(ctx, OBJ_CLASS((uint64_t *)result), (uint64_t)string_class,
                  "dispatch new:: class is String");
        ASSERT_EQ(ctx, OBJ_FORMAT((uint64_t *)result), FORMAT_BYTES,
                  "dispatch new:: format is bytes");
        ASSERT_EQ(ctx, OBJ_SIZE((uint64_t *)result), 5,
                  "dispatch new:: size is 5");
    }

    {
        uint64_t sel_eq_string = tag_smallint(100);
        uint64_t *eq_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 6);
        uint8_t *eqb = (uint8_t *)&OBJ_FIELD(eq_bc, 0);
        eqb[0] = BC_PUSH_LITERAL;
        WRITE_U32(&eqb[1], 0);
        eqb[5] = BC_RETURN;

        uint64_t *eq_lits = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 1);
        OBJ_FIELD(eq_lits, 0) = tagged_false();

        uint64_t *eq_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(eq_cm, CM_PRIMITIVE) = tag_smallint(PRIM_STRING_EQ);
        OBJ_FIELD(eq_cm, CM_NUM_ARGS) = tag_smallint(1);
        OBJ_FIELD(eq_cm, CM_NUM_TEMPS) = tag_smallint(0);
        OBJ_FIELD(eq_cm, CM_LITERALS) = (uint64_t)eq_lits;
        OBJ_FIELD(eq_cm, CM_BYTECODES) = (uint64_t)eq_bc;

        uint64_t *eq_class = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 4);
        OBJ_FIELD(eq_class, CLASS_SUPERCLASS) = tagged_nil();
        OBJ_FIELD(eq_class, CLASS_INST_SIZE) = tag_smallint(0);
        OBJ_FIELD(eq_class, CLASS_INST_FORMAT) = tag_smallint(FORMAT_BYTES);
        uint64_t *eq_md = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 2);
        OBJ_FIELD(eq_md, 0) = sel_eq_string;
        OBJ_FIELD(eq_md, 1) = (uint64_t)eq_cm;
        OBJ_FIELD(eq_class, CLASS_METHOD_DICT) = (uint64_t)eq_md;

        uint64_t *recv = make_bytes_obj(om, class_class, eq_class, "alpha", 5);

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
        OBJ_FIELD(lits, 1) = sel_eq_string;

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
                  "primitive failure: String>>= wrong arg falls through to method body");
        ASSERT_EQ(ctx, (uint64_t)run_trap_test(ctx, trap_string_eq_wrong_receiver),
                  (uint64_t)SIGTRAP,
                  "primitive trap: String>>= wrong receiver traps");
        ASSERT_EQ(ctx, (uint64_t)run_trap_test(ctx, trap_string_hash_wrong_receiver),
                  (uint64_t)SIGTRAP,
                  "primitive trap: String>>hash wrong receiver traps");
        ASSERT_EQ(ctx, (uint64_t)run_trap_test(ctx, trap_string_as_symbol_wrong_receiver),
                  (uint64_t)SIGTRAP,
                  "primitive trap: String>>asSymbol wrong receiver traps");
    }

    {
        uint64_t *comma_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 220);
        uint8_t *p = (uint8_t *)&OBJ_FIELD(comma_bc, 0);
        int i = 0;

#define EMIT(op) p[i++] = (uint8_t)(op)
#define EMIT_U32(v)       \
    do                    \
    {                     \
        WRITE_U32(&p[i], (v)); \
        i += 4;           \
    } while (0)

        EMIT(BC_PUSH_LITERAL);
        EMIT_U32(0);
        EMIT(BC_PUSH_LITERAL);
        EMIT_U32(1);
        EMIT(BC_SEND_MESSAGE);
        EMIT_U32(2);
        EMIT_U32(1);
        EMIT(BC_STORE_TEMP);
        EMIT_U32(0);

        EMIT(BC_PUSH_TEMP);
        EMIT_U32(0);
        EMIT(BC_PUSH_LITERAL);
        EMIT_U32(3);
        EMIT(BC_PUSH_SELF);
        EMIT(BC_PUSH_LITERAL);
        EMIT_U32(3);
        EMIT(BC_SEND_MESSAGE);
        EMIT_U32(4);
        EMIT_U32(1);
        EMIT(BC_SEND_MESSAGE);
        EMIT_U32(5);
        EMIT_U32(2);
        EMIT(BC_POP);

        EMIT(BC_PUSH_TEMP);
        EMIT_U32(0);
        EMIT(BC_PUSH_LITERAL);
        EMIT_U32(6);
        EMIT(BC_PUSH_SELF);
        EMIT(BC_PUSH_LITERAL);
        EMIT_U32(6);
        EMIT(BC_SEND_MESSAGE);
        EMIT_U32(4);
        EMIT_U32(1);
        EMIT(BC_SEND_MESSAGE);
        EMIT_U32(5);
        EMIT_U32(2);
        EMIT(BC_POP);

        EMIT(BC_PUSH_TEMP);
        EMIT_U32(0);
        EMIT(BC_PUSH_LITERAL);
        EMIT_U32(7);
        EMIT(BC_PUSH_ARG);
        EMIT_U32(0);
        EMIT(BC_PUSH_LITERAL);
        EMIT_U32(3);
        EMIT(BC_SEND_MESSAGE);
        EMIT_U32(4);
        EMIT_U32(1);
        EMIT(BC_SEND_MESSAGE);
        EMIT_U32(5);
        EMIT_U32(2);
        EMIT(BC_POP);

        EMIT(BC_PUSH_TEMP);
        EMIT_U32(0);
        EMIT(BC_PUSH_LITERAL);
        EMIT_U32(8);
        EMIT(BC_PUSH_ARG);
        EMIT_U32(0);
        EMIT(BC_PUSH_LITERAL);
        EMIT_U32(6);
        EMIT(BC_SEND_MESSAGE);
        EMIT_U32(4);
        EMIT_U32(1);
        EMIT(BC_SEND_MESSAGE);
        EMIT_U32(5);
        EMIT_U32(2);
        EMIT(BC_POP);

        EMIT(BC_PUSH_TEMP);
        EMIT_U32(0);
        EMIT(BC_PUSH_LITERAL);
        EMIT_U32(1);
        EMIT(BC_PUSH_ARG);
        EMIT_U32(0);
        EMIT(BC_PUSH_LITERAL);
        EMIT_U32(7);
        EMIT(BC_SEND_MESSAGE);
        EMIT_U32(4);
        EMIT_U32(1);
        EMIT(BC_SEND_MESSAGE);
        EMIT_U32(5);
        EMIT_U32(2);
        EMIT(BC_POP);

        EMIT(BC_PUSH_TEMP);
        EMIT_U32(0);
        EMIT(BC_RETURN);

#undef EMIT
#undef EMIT_U32

        uint64_t *comma_lits = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 9);
        OBJ_FIELD(comma_lits, 0) = (uint64_t)string_class;
        OBJ_FIELD(comma_lits, 1) = tag_smallint(5);
        OBJ_FIELD(comma_lits, 2) = sel_new_size;
        OBJ_FIELD(comma_lits, 3) = tag_smallint(1);
        OBJ_FIELD(comma_lits, 4) = sel_at;
        OBJ_FIELD(comma_lits, 5) = sel_at_put;
        OBJ_FIELD(comma_lits, 6) = tag_smallint(2);
        OBJ_FIELD(comma_lits, 7) = tag_smallint(3);
        OBJ_FIELD(comma_lits, 8) = tag_smallint(4);

        uint64_t *cm_comma = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(cm_comma, CM_PRIMITIVE) = tag_smallint(0);
        OBJ_FIELD(cm_comma, CM_NUM_ARGS) = tag_smallint(1);
        OBJ_FIELD(cm_comma, CM_NUM_TEMPS) = tag_smallint(1);
        OBJ_FIELD(cm_comma, CM_LITERALS) = (uint64_t)comma_lits;
        OBJ_FIELD(cm_comma, CM_BYTECODES) = (uint64_t)comma_bc;
        stt_md_append_oop(om, class_class, string_class, sel_comma, (uint64_t)cm_comma);

        uint64_t *s1 = make_bytes_obj(om, class_class, string_class, "he", 2);
        uint64_t *s2 = make_bytes_obj(om, class_class, string_class, "llo", 3);

        uint64_t *caller_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 20);
        p = (uint8_t *)&OBJ_FIELD(caller_bc, 0);
        p[0] = BC_PUSH_SELF;
        p[1] = BC_PUSH_LITERAL;
        WRITE_U32(&p[2], 1);
        p[6] = BC_SEND_MESSAGE;
        WRITE_U32(&p[7], 0);
        WRITE_U32(&p[11], 1);
        p[15] = BC_HALT;

        uint64_t *caller_lits = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 2);
        OBJ_FIELD(caller_lits, 0) = sel_comma;
        OBJ_FIELD(caller_lits, 1) = (uint64_t)s2;

        uint64_t *caller_cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(caller_cm, CM_PRIMITIVE) = tag_smallint(0);
        OBJ_FIELD(caller_cm, CM_NUM_ARGS) = tag_smallint(0);
        OBJ_FIELD(caller_cm, CM_NUM_TEMPS) = tag_smallint(0);
        OBJ_FIELD(caller_cm, CM_LITERALS) = (uint64_t)caller_lits;
        OBJ_FIELD(caller_cm, CM_BYTECODES) = (uint64_t)caller_bc;

        sp = (uint64_t *)((uint8_t *)stack + STACK_WORDS * sizeof(uint64_t));
        fp = (uint64_t *)0xCAFE;
        stack_push(&sp, stack, (uint64_t)s1);
        activate_method(&sp, &fp, 0, (uint64_t)caller_cm, 0, 0);
        result = interpret(&sp, &fp, (uint8_t *)&OBJ_FIELD(caller_bc, 0), class_table, om, NULL);

        ASSERT_EQ(ctx, result & 3, 0, "dispatch comma: result is object ptr");
        uint64_t *r = (uint64_t *)result;
        ASSERT_EQ(ctx, OBJ_CLASS(r), (uint64_t)string_class, "dispatch comma: class is String");
        ASSERT_EQ(ctx, OBJ_FORMAT(r), FORMAT_BYTES, "dispatch comma: format is bytes");
        ASSERT_EQ(ctx, OBJ_SIZE(r), 5, "dispatch comma: size is 5");

        uint8_t *rb = (uint8_t *)&OBJ_FIELD(r, 0);
        ASSERT_EQ(ctx, rb[0], (uint8_t)'h', "dispatch comma: byte 1");
        ASSERT_EQ(ctx, rb[1], (uint8_t)'e', "dispatch comma: byte 2");
        ASSERT_EQ(ctx, rb[2], (uint8_t)'l', "dispatch comma: byte 3");
        ASSERT_EQ(ctx, rb[3], (uint8_t)'l', "dispatch comma: byte 4");
        ASSERT_EQ(ctx, rb[4], (uint8_t)'o', "dispatch comma: byte 5");
    }
}
