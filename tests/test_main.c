#include "test_defs.h"
#include <sys/wait.h>
#include <unistd.h>

static uint64_t *make_byte_string(uint64_t *om, uint64_t *string_class, const char *text)
{
    size_t len = strlen(text);
    uint64_t *string = om_alloc(om, (uint64_t)string_class, FORMAT_BYTES, (uint64_t)len);
    if (len > 0)
    {
        memcpy(&OBJ_FIELD(string, 0), text, len);
    }
    return string;
}

static uint64_t debug_oop_class(uint64_t oop, uint64_t *class_table)
{
    if ((oop & 1) == 0)
    {
        return oop ? ((uint64_t *)oop)[0] : 0;
    }
    if ((oop & 3) == 1)
    {
        return class_table ? class_table[3] : 0;
    }
    if ((oop & 0x0F) == 0x0F)
    {
        return class_table ? class_table[7] : 0;
    }
    if (oop == tagged_nil())
    {
        return class_table ? class_table[8] : 0;
    }
    if (oop == tagged_true())
    {
        return class_table ? class_table[5] : 0;
    }
    if (oop == tagged_false())
    {
        return class_table ? class_table[6] : 0;
    }
    return 0;
}

static uint64_t debug_method_selector(uint64_t klass, uint64_t method)
{
    while (klass != 0 && klass != tagged_nil())
    {
        uint64_t md_value = ((uint64_t *)klass)[4];
        if (md_value != tagged_nil() && (md_value & 3) == 0)
        {
            uint64_t *md = (uint64_t *)md_value;
            uint64_t size = md[2];
            for (uint64_t index = 0; index + 1 < size; index += 2)
            {
                if (md[3 + index + 1] == method)
                {
                    return md[3 + index];
                }
            }
        }
        klass = ((uint64_t *)klass)[3];
    }
    return 0;
}

static void debug_print_oop(FILE *stream, uint64_t oop)
{
    if ((oop & 3) == 1)
    {
        fprintf(stream, "%lld", (long long)(oop >> 2));
        return;
    }
    if ((oop & 0x0F) == 0x0F)
    {
        unsigned char ch = (unsigned char)(oop >> 4);
        if (ch >= 32 && ch < 127)
            fprintf(stream, "$%c", ch);
        else
            fprintf(stream, "$<%u>", (unsigned)ch);
        return;
    }
    if (oop == tagged_nil())
    {
        fputs("nil", stream);
        return;
    }
    if (oop == tagged_true())
    {
        fputs("true", stream);
        return;
    }
    if (oop == tagged_false())
    {
        fputs("false", stream);
        return;
    }
    if ((oop & 3) == 0 && oop != 0)
    {
        uint64_t *obj = (uint64_t *)oop;
        if (obj[1] == FORMAT_BYTES)
        {
            uint64_t size = obj[2];
            uint8_t *bytes = (uint8_t *)&obj[3];
            fputc('\'', stream);
            for (uint64_t i = 0; i < size; i++)
            {
                unsigned char ch = bytes[i];
                if (ch >= 32 && ch < 127 && ch != '\'')
                    fputc((int)ch, stream);
                else
                    fputc('.', stream);
            }
            fputc('\'', stream);
            return;
        }
        fprintf(stream, "<oop %p>", (void *)obj);
        return;
    }
    fprintf(stream, "<tagged %llu>", (unsigned long long)oop);
}

void debug_mnu(uint64_t selector)
{
    fprintf(stderr, "MNU: selector=");
    debug_print_oop(stderr, selector);
    fprintf(stderr, " raw=%lld\n", selector);
}

void debug_oom(void)
{
    fprintf(stderr, "OOM: out of object memory\n");
}

void debug_unknown_prim(uint64_t prim_index)
{
    fprintf(stderr, "UNKNOWN PRIM: %lld\n", prim_index);
}

void debug_error(uint64_t message, uint64_t *fp, uint64_t *class_table)
{
    fprintf(stderr, "ERROR: ");
    debug_print_oop(stderr, message);
    fprintf(stderr, "\nSTACK TRACE:\n");

    int depth = 0;
    while (fp != NULL && fp != (uint64_t *)0xCAFE)
    {
        uint64_t receiver = fp[-4];
        uint64_t method = fp[-1];
        uint64_t klass = debug_oop_class(receiver, class_table);
        uint64_t selector = debug_method_selector(klass, method);

        fprintf(stderr, "  #%d receiver=", depth);
        debug_print_oop(stderr, receiver);
        if (selector != 0)
            fprintf(stderr, " selector=%lld", (long long)(selector >> 2));
        fprintf(stderr, " method=%p\n", (void *)method);

        fp = (uint64_t *)fp[0];
        depth++;
    }
}

int run_trap_test(TestContext *ctx, TrapTestFn fn)
{
    pid_t pid = fork();
    if (pid == 0)
    {
        fn(ctx);
        _exit(0);
    }
    if (pid < 0)
    {
        return -1;
    }

    int status = 0;
    if (waitpid(pid, &status, 0) < 0)
    {
        return -1;
    }
    if (WIFSIGNALED(status))
    {
        return WTERMSIG(status);
    }
    return 0;
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
    OBJ_FIELD(block_class, CLASS_INST_SIZE) = tag_smallint(3);
    OBJ_FIELD(block_class, CLASS_INST_FORMAT) = tag_smallint(FORMAT_FIELDS);

    uint64_t *undefined_object_class = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 4);
    OBJ_FIELD(undefined_object_class, CLASS_SUPERCLASS) = tagged_nil();
    OBJ_FIELD(undefined_object_class, CLASS_METHOD_DICT) = tagged_nil();
    OBJ_FIELD(undefined_object_class, CLASS_INST_SIZE) = tag_smallint(0);
    OBJ_FIELD(undefined_object_class, CLASS_INST_FORMAT) = tag_smallint(FORMAT_FIELDS);

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

    uint64_t *context_class = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
    OBJ_FIELD(context_class, CLASS_SUPERCLASS) = tagged_nil();
    OBJ_FIELD(context_class, CLASS_METHOD_DICT) = tagged_nil();
    OBJ_FIELD(context_class, CLASS_INST_SIZE) = tag_smallint(CONTEXT_VAR_BASE);
    OBJ_FIELD(context_class, CLASS_INST_FORMAT) = tag_smallint(FORMAT_FIELDS);
    uint64_t *context_ivars = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, CONTEXT_VAR_BASE);
    OBJ_FIELD(context_ivars, 0) = (uint64_t)make_byte_string(om, string_class, "sender");
    OBJ_FIELD(context_ivars, 1) = (uint64_t)make_byte_string(om, string_class, "ip");
    OBJ_FIELD(context_ivars, 2) = (uint64_t)make_byte_string(om, string_class, "method");
    OBJ_FIELD(context_ivars, 3) = (uint64_t)make_byte_string(om, string_class, "receiver");
    OBJ_FIELD(context_ivars, 4) = (uint64_t)make_byte_string(om, string_class, "home");
    OBJ_FIELD(context_ivars, 5) = (uint64_t)make_byte_string(om, string_class, "closure");
    OBJ_FIELD(context_ivars, 6) = (uint64_t)make_byte_string(om, string_class, "flags");
    OBJ_FIELD(context_ivars, 7) = (uint64_t)make_byte_string(om, string_class, "numArgs");
    OBJ_FIELD(context_ivars, 8) = (uint64_t)make_byte_string(om, string_class, "numTemps");
    OBJ_FIELD(context_class, CLASS_INST_VARS) = (uint64_t)context_ivars;

    uint64_t *symbol_table_obj = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 256);
    for (int i = 0; i < 256; i++)
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
    ctx.undefined_object_class = undefined_object_class;
    ctx.character_class = character_class;
    ctx.string_class = string_class;
    ctx.symbol_class = symbol_class;
    ctx.context_class = context_class;
    ctx.symbol_table = symbol_table_obj;
    global_symbol_table = symbol_table_obj; // Initialize the global symbol table
    global_symbol_class = symbol_class;
    global_context_class = context_class;
    ctx.test_class = test_class;
    ctx.receiver = (uint64_t)recv_obj;
    ctx.method = (uint64_t)test_cm;
    uint64_t *class_table_obj = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, 6);
    OBJ_FIELD(class_table_obj, 0) = (uint64_t)smallint_class;
    OBJ_FIELD(class_table_obj, 1) = (uint64_t)block_class;
    OBJ_FIELD(class_table_obj, 2) = 0; // True class (not yet)
    OBJ_FIELD(class_table_obj, 3) = 0; // False class (not yet)
    OBJ_FIELD(class_table_obj, 4) = (uint64_t)character_class;
    OBJ_FIELD(class_table_obj, 5) = (uint64_t)undefined_object_class;
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
    test_smalltalk_expressions(&ctx);
    test_expression_fixtures(&ctx);

    printf("\n%d passed, %d failed\n", ctx.passes, ctx.failures);
    return ctx.failures > 0 ? 1 : 0;
}
