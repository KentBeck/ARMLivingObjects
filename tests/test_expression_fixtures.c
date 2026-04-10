#include "test_defs.h"
#include "bootstrap_compiler.h"
#include "primitives.h"
#include <ctype.h>

typedef enum
{
    FIXTURE_EXPECT_SMALLINT = 0,
    FIXTURE_EXPECT_TRUE,
    FIXTURE_EXPECT_FALSE
} FixtureExpectedKind;

typedef struct
{
    char name[64];
    char superclass_name[64];
} FixtureClassDecl;

typedef struct
{
    char name[64];
    char receiver_class_name[64];
    char expression[256];
    char setup_source[4096];
    FixtureExpectedKind expected_kind;
    int64_t expected_smallint;
    int class_count;
    FixtureClassDecl classes[8];
} ExpressionFixtureSpec;

static int read_file(const char *path, char *buf, size_t cap)
{
    FILE *f = fopen(path, "rb");
    if (!f)
    {
        return 0;
    }
    size_t n = fread(buf, 1, cap - 1, f);
    fclose(f);
    buf[n] = '\0';
    return 1;
}

static void trim_in_place(char *text)
{
    size_t len = strlen(text);
    size_t start = 0;
    while (start < len && isspace((unsigned char)text[start]))
    {
        start++;
    }
    size_t end = len;
    while (end > start && isspace((unsigned char)text[end - 1]))
    {
        end--;
    }
    if (start > 0)
    {
        memmove(text, text + start, end - start);
    }
    text[end - start] = '\0';
}

static int parse_expected_value(const char *text, FixtureExpectedKind *kind, int64_t *smallint_value)
{
    if (strcmp(text, "true") == 0)
    {
        *kind = FIXTURE_EXPECT_TRUE;
        *smallint_value = 0;
        return 1;
    }
    if (strcmp(text, "false") == 0)
    {
        *kind = FIXTURE_EXPECT_FALSE;
        *smallint_value = 0;
        return 1;
    }

    char *end = NULL;
    long long parsed = strtoll(text, &end, 10);
    if (end == text || *end != '\0')
    {
        return 0;
    }
    *kind = FIXTURE_EXPECT_SMALLINT;
    *smallint_value = (int64_t)parsed;
    return 1;
}

static int load_expression_fixture_specs(const char *path, ExpressionFixtureSpec *specs, int max_specs, int *out_count)
{
    FILE *file = fopen(path, "rb");
    if (!file)
    {
        return 0;
    }

    char line[1024];
    int count = 0;
    ExpressionFixtureSpec *current = NULL;
    int in_setup = 0;

    while (fgets(line, sizeof(line), file) != NULL)
    {
        char raw_line[1024];
        strncpy(raw_line, line, sizeof(raw_line) - 1);
        raw_line[sizeof(raw_line) - 1] = '\0';

        trim_in_place(line);
        if (!in_setup && (line[0] == '\0' || line[0] == '#'))
        {
            continue;
        }

        if (strncmp(line, "===", 3) == 0)
        {
            if (count >= max_specs)
            {
                fclose(file);
                return 0;
            }
            current = &specs[count];
            memset(current, 0, sizeof(*current));
            strncpy(current->name, line + 3, sizeof(current->name) - 1);
            trim_in_place(current->name);
            in_setup = 0;
            count++;
            continue;
        }

        if (current == NULL)
        {
            fclose(file);
            return 0;
        }

        if (in_setup)
        {
            if (strncmp(line, "expression:", 11) == 0)
            {
                strncpy(current->expression, line + 11, sizeof(current->expression) - 1);
                trim_in_place(current->expression);
                in_setup = 0;
                continue;
            }

            size_t setup_len = strlen(current->setup_source);
            size_t raw_len = strlen(raw_line);
            if (setup_len + raw_len >= sizeof(current->setup_source))
            {
                fclose(file);
                return 0;
            }
            memcpy(current->setup_source + setup_len, raw_line, raw_len + 1);
            continue;
        }

        if (strncmp(line, "class:", 6) == 0)
        {
            if (current->class_count >= 8)
            {
                fclose(file);
                return 0;
            }

            char class_decl[128];
            memset(class_decl, 0, sizeof(class_decl));
            strncpy(class_decl, line + 6, sizeof(class_decl) - 1);
            trim_in_place(class_decl);

            FixtureClassDecl *decl = &current->classes[current->class_count++];
            char *lt = strstr(class_decl, "<");
            if (lt != NULL)
            {
                *lt = '\0';
                strncpy(decl->name, class_decl, sizeof(decl->name) - 1);
                trim_in_place(decl->name);
                strncpy(decl->superclass_name, lt + 1, sizeof(decl->superclass_name) - 1);
                trim_in_place(decl->superclass_name);
            }
            else
            {
                strncpy(decl->name, class_decl, sizeof(decl->name) - 1);
                trim_in_place(decl->name);
                strncpy(decl->superclass_name, "Object", sizeof(decl->superclass_name) - 1);
            }
            continue;
        }

        if (strncmp(line, "receiver-class:", 15) == 0)
        {
            strncpy(current->receiver_class_name, line + 15, sizeof(current->receiver_class_name) - 1);
            trim_in_place(current->receiver_class_name);
            continue;
        }

        if (strncmp(line, "setup:", 6) == 0)
        {
            current->setup_source[0] = '\0';
            in_setup = 1;
            continue;
        }

        if (strncmp(line, "expression:", 11) == 0)
        {
            strncpy(current->expression, line + 11, sizeof(current->expression) - 1);
            trim_in_place(current->expression);
            continue;
        }

        if (strncmp(line, "expected:", 9) == 0)
        {
            char expected_text[64];
            memset(expected_text, 0, sizeof(expected_text));
            strncpy(expected_text, line + 9, sizeof(expected_text) - 1);
            trim_in_place(expected_text);
            if (!parse_expected_value(expected_text, &current->expected_kind, &current->expected_smallint))
            {
                fclose(file);
                return 0;
            }
            continue;
        }

        fclose(file);
        return 0;
    }

    fclose(file);
    if (count == 0)
    {
        return 0;
    }

    for (int index = 0; index < count; index++)
    {
        ExpressionFixtureSpec *spec = &specs[index];
        if (spec->name[0] == '\0' || spec->receiver_class_name[0] == '\0' ||
            spec->expression[0] == '\0' || spec->class_count == 0)
        {
            return 0;
        }
    }

    *out_count = count;
    return 1;
}

static uint64_t selector_oop(uint64_t *om, const char *selector)
{
    return intern_cstring_symbol(om, selector);
}

static void md_append(uint64_t *om, uint64_t *class_class, uint64_t *klass, const char *selector, uint64_t method)
{
    uint64_t md_val = OBJ_FIELD(klass, CLASS_METHOD_DICT);
    uint64_t *old_md = (md_val != tagged_nil() && (md_val & 3) == 0) ? (uint64_t *)md_val : NULL;
    uint64_t old_size = old_md ? OBJ_SIZE(old_md) : 0;
    uint64_t *new_md = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, old_size + 2);

    for (uint64_t index = 0; index < old_size; index++)
    {
        OBJ_FIELD(new_md, index) = OBJ_FIELD(old_md, index);
    }
    OBJ_FIELD(new_md, old_size) = selector_oop(om, selector);
    OBJ_FIELD(new_md, old_size + 1) = method;
    OBJ_FIELD(klass, CLASS_METHOD_DICT) = (uint64_t)new_md;
}

static uint64_t *make_primitive_cm(uint64_t *om, uint64_t *class_class, int prim, int num_args)
{
    uint64_t *prim_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 1);
    ((uint8_t *)&OBJ_FIELD(prim_bc, 0))[0] = BC_HALT;

    uint64_t *cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
    OBJ_FIELD(cm, CM_PRIMITIVE) = tag_smallint(prim);
    OBJ_FIELD(cm, CM_NUM_ARGS) = tag_smallint(num_args);
    OBJ_FIELD(cm, CM_NUM_TEMPS) = tag_smallint(0);
    OBJ_FIELD(cm, CM_LITERALS) = tagged_nil();
    OBJ_FIELD(cm, CM_BYTECODES) = (uint64_t)prim_bc;
    return cm;
}

static uint64_t *make_class_with_ivars(uint64_t *om, uint64_t *class_class, uint64_t *string_class,
                                       uint64_t *superclass, const char **ivars, uint64_t ivar_count)
{
    uint64_t *klass = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
    OBJ_FIELD(klass, CLASS_SUPERCLASS) = superclass ? (uint64_t)superclass : tagged_nil();
    OBJ_FIELD(klass, CLASS_METHOD_DICT) = tagged_nil();
    OBJ_FIELD(klass, CLASS_INST_SIZE) = tag_smallint((int64_t)ivar_count);
    OBJ_FIELD(klass, CLASS_INST_FORMAT) = tag_smallint(FORMAT_FIELDS);
    if (ivar_count == 0)
    {
        OBJ_FIELD(klass, CLASS_INST_VARS) = tagged_nil();
        return klass;
    }

    uint64_t *ivar_array = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, ivar_count);
    for (uint64_t index = 0; index < ivar_count; index++)
    {
        uint64_t size = (uint64_t)strlen(ivars[index]);
        uint64_t *name = om_alloc(om, (uint64_t)string_class, FORMAT_BYTES, size);
        memcpy((uint8_t *)&OBJ_FIELD(name, 0), ivars[index], size);
        OBJ_FIELD(ivar_array, index) = (uint64_t)name;
    }
    OBJ_FIELD(klass, CLASS_INST_VARS) = (uint64_t)ivar_array;
    return klass;
}

static uint64_t *make_runtime_class(uint64_t *om, uint64_t *class_class, uint64_t *string_class,
                                    uint64_t *superclass, const char **ivars, uint64_t ivar_count)
{
    uint64_t *meta = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 5);
    OBJ_FIELD(meta, CLASS_SUPERCLASS) = tagged_nil();
    OBJ_FIELD(meta, CLASS_METHOD_DICT) = tagged_nil();
    OBJ_FIELD(meta, CLASS_INST_SIZE) = tag_smallint(0);
    OBJ_FIELD(meta, CLASS_INST_FORMAT) = tag_smallint(FORMAT_FIELDS);
    OBJ_FIELD(meta, CLASS_INST_VARS) = tagged_nil();

    uint64_t *klass = make_class_with_ivars(om, class_class, string_class, superclass, ivars, ivar_count);
    OBJ_CLASS(klass) = (uint64_t)meta;
    return klass;
}

static uint64_t *find_bound_class(const BClassBinding *bindings, int binding_count, const char *name)
{
    for (int index = 0; index < binding_count; index++)
    {
        if (bindings[index].class_name != NULL && bindings[index].klass != NULL &&
            strcmp(bindings[index].class_name, name) == 0)
        {
            return bindings[index].klass;
        }
    }
    return NULL;
}

static uint64_t *smalltalk_lookup_class(const char *name)
{
    if (global_smalltalk_dictionary == NULL || !is_object_ptr((uint64_t)global_smalltalk_dictionary))
    {
        return NULL;
    }

    uint64_t key = lookup_cstring_symbol(name);
    if (key == tagged_nil())
    {
        return NULL;
    }

    uint64_t associations_oop = OBJ_FIELD(global_smalltalk_dictionary, 0);
    uint64_t tally_oop = OBJ_FIELD(global_smalltalk_dictionary, 1);
    if (!is_object_ptr(associations_oop) || tally_oop == tagged_nil())
    {
        return NULL;
    }

    uint64_t *associations = (uint64_t *)associations_oop;
    uint64_t tally = (uint64_t)untag_smallint(tally_oop);
    for (uint64_t index = 0; index < tally; index++)
    {
        uint64_t assoc_oop = OBJ_FIELD(associations, index);
        if (!is_object_ptr(assoc_oop))
        {
            continue;
        }
        uint64_t *assoc = (uint64_t *)assoc_oop;
        if (OBJ_FIELD(assoc, 0) == key)
        {
            return is_object_ptr(OBJ_FIELD(assoc, 1)) ? (uint64_t *)OBJ_FIELD(assoc, 1) : NULL;
        }
    }
    return NULL;
}

static void smalltalk_at_put(uint64_t *om, uint64_t *array_class, uint64_t *association_class,
                             const char *name, uint64_t value)
{
    uint64_t key = intern_cstring_symbol(om, name);
    uint64_t associations_oop = OBJ_FIELD(global_smalltalk_dictionary, 0);
    uint64_t tally = OBJ_FIELD(global_smalltalk_dictionary, 1) == tagged_nil()
                         ? 0
                         : (uint64_t)untag_smallint(OBJ_FIELD(global_smalltalk_dictionary, 1));

    if (associations_oop == tagged_nil())
    {
        uint64_t *associations = om_alloc(om, (uint64_t)array_class, FORMAT_INDEXABLE, 8);
        for (uint64_t index = 0; index < 8; index++)
        {
            OBJ_FIELD(associations, index) = tagged_nil();
        }
        OBJ_FIELD(global_smalltalk_dictionary, 0) = (uint64_t)associations;
        associations_oop = (uint64_t)associations;
        OBJ_FIELD(global_smalltalk_dictionary, 1) = tag_smallint(0);
    }

    uint64_t *associations = (uint64_t *)associations_oop;
    for (uint64_t index = 0; index < tally; index++)
    {
        uint64_t assoc_oop = OBJ_FIELD(associations, index);
        if (!is_object_ptr(assoc_oop))
        {
            continue;
        }
        uint64_t *assoc = (uint64_t *)assoc_oop;
        if (OBJ_FIELD(assoc, 0) == key)
        {
            OBJ_FIELD(assoc, 1) = value;
            return;
        }
    }

    if (tally >= OBJ_SIZE(associations))
    {
        uint64_t new_size = OBJ_SIZE(associations) * 2;
        uint64_t *grown = om_alloc(om, (uint64_t)array_class, FORMAT_INDEXABLE, new_size);
        for (uint64_t index = 0; index < new_size; index++)
        {
            OBJ_FIELD(grown, index) = index < OBJ_SIZE(associations)
                                          ? OBJ_FIELD(associations, index)
                                          : tagged_nil();
        }
        OBJ_FIELD(global_smalltalk_dictionary, 0) = (uint64_t)grown;
        associations = grown;
    }

    uint64_t *assoc = om_alloc(om, (uint64_t)association_class, FORMAT_FIELDS, 2);
    OBJ_FIELD(assoc, 0) = key;
    OBJ_FIELD(assoc, 1) = value;
    OBJ_FIELD(associations, tally) = (uint64_t)assoc;
    OBJ_FIELD(global_smalltalk_dictionary, 1) = tag_smallint((int64_t)(tally + 1));
}

void test_expression_fixtures(TestContext *ctx)
{
    static ExpressionFixtureSpec specs[16];
    static uint8_t fixture_om_buffer[262144] __attribute__((aligned(8)));
    char fixture_src[8192];
    char true_src[1024];
    char false_src[1024];
    int spec_count = 0;
    uint64_t fixture_om[2];
    uint64_t fixture_stack[STACK_WORDS];
    uint64_t *saved_symbol_table = global_symbol_table;
    uint64_t *saved_symbol_class = global_symbol_class;
    uint64_t *saved_context_class = global_context_class;
    uint64_t *saved_smalltalk_dictionary = global_smalltalk_dictionary;

    ASSERT_EQ(ctx, read_file("tests/ExpressionFixtureSpecs.txt", fixture_src, sizeof(fixture_src)), 1,
              "expression fixture specs file loads");
    ASSERT_EQ(ctx, load_expression_fixture_specs("tests/ExpressionFixtureSpecs.txt", specs, 16, &spec_count), 1,
              "expression fixture specs parse");

    ASSERT_EQ(ctx, read_file("src/smalltalk/True.st", true_src, sizeof(true_src)), 1,
              "fixture True source loads");
    ASSERT_EQ(ctx, read_file("src/smalltalk/False.st", false_src, sizeof(false_src)), 1,
              "fixture False source loads");

    om_init(fixture_om_buffer, sizeof(fixture_om_buffer), fixture_om);

    uint64_t *class_class = om_alloc(fixture_om, 0, FORMAT_FIELDS, 5);
    OBJ_CLASS(class_class) = (uint64_t)class_class;
    OBJ_FIELD(class_class, CLASS_SUPERCLASS) = tagged_nil();
    OBJ_FIELD(class_class, CLASS_METHOD_DICT) = tagged_nil();
    OBJ_FIELD(class_class, CLASS_INST_SIZE) = tag_smallint(5);
    OBJ_FIELD(class_class, CLASS_INST_FORMAT) = tag_smallint(FORMAT_FIELDS);
    OBJ_FIELD(class_class, CLASS_INST_VARS) = tagged_nil();

    uint64_t *string_class = make_runtime_class(fixture_om, class_class, class_class, NULL, NULL, 0);
    OBJ_FIELD(string_class, CLASS_INST_FORMAT) = tag_smallint(FORMAT_BYTES);
    uint64_t *symbol_class = make_runtime_class(fixture_om, class_class, string_class, string_class, NULL, 0);
    OBJ_FIELD(symbol_class, CLASS_INST_FORMAT) = tag_smallint(FORMAT_BYTES);
    uint64_t *smallint_class = make_runtime_class(fixture_om, class_class, string_class, NULL, NULL, 0);
    uint64_t *block_class = make_runtime_class(fixture_om, class_class, string_class, NULL, NULL, 0);
    uint64_t *undefined_object_class = make_runtime_class(fixture_om, class_class, string_class, NULL, NULL, 0);
    uint64_t *character_class = make_runtime_class(fixture_om, class_class, string_class, NULL, NULL, 0);
    uint64_t *object_class = make_runtime_class(fixture_om, class_class, string_class, NULL, NULL, 0);
    uint64_t *true_class = make_runtime_class(fixture_om, class_class, string_class, object_class, NULL, 0);
    uint64_t *false_class = make_runtime_class(fixture_om, class_class, string_class, object_class, NULL, 0);
    uint64_t *array_class = make_runtime_class(fixture_om, class_class, string_class, object_class, NULL, 0);
    OBJ_FIELD(array_class, CLASS_INST_FORMAT) = tag_smallint(FORMAT_INDEXABLE);
    const char *association_ivars[] = {"key", "value"};
    const char *dictionary_ivars[] = {"associations", "tally"};
    uint64_t *association_class = make_runtime_class(fixture_om, class_class, string_class,
                                                     object_class, association_ivars, 2);
    uint64_t *dictionary_class = make_runtime_class(fixture_om, class_class, string_class,
                                                    object_class, dictionary_ivars, 2);

    uint64_t *symbol_table = om_alloc(fixture_om, (uint64_t)class_class, FORMAT_INDEXABLE, 256);
    for (uint64_t index = 0; index < OBJ_SIZE(symbol_table); index++)
    {
        OBJ_FIELD(symbol_table, index) = tagged_nil();
    }
    global_symbol_table = symbol_table;
    global_symbol_class = symbol_class;
    global_context_class = NULL;
    global_smalltalk_dictionary = om_alloc(fixture_om, (uint64_t)dictionary_class, FORMAT_FIELDS, 2);
    OBJ_FIELD(global_smalltalk_dictionary, 0) = tagged_nil();
    OBJ_FIELD(global_smalltalk_dictionary, 1) = tag_smallint(0);
    smalltalk_at_put(fixture_om, array_class, association_class, "Object", (uint64_t)object_class);
    smalltalk_at_put(fixture_om, array_class, association_class, "True", (uint64_t)true_class);
    smalltalk_at_put(fixture_om, array_class, association_class, "False", (uint64_t)false_class);
    smalltalk_at_put(fixture_om, array_class, association_class, "String", (uint64_t)string_class);
    smalltalk_at_put(fixture_om, array_class, association_class, "Array", (uint64_t)array_class);
    smalltalk_at_put(fixture_om, array_class, association_class, "Association", (uint64_t)association_class);
    smalltalk_at_put(fixture_om, array_class, association_class, "Dictionary", (uint64_t)dictionary_class);

    uint64_t *class_table = om_alloc(fixture_om, (uint64_t)class_class, FORMAT_INDEXABLE, 6);
    OBJ_FIELD(class_table, 0) = (uint64_t)smallint_class;
    OBJ_FIELD(class_table, 1) = (uint64_t)block_class;
    OBJ_FIELD(class_table, 2) = (uint64_t)true_class;
    OBJ_FIELD(class_table, 3) = (uint64_t)false_class;
    OBJ_FIELD(class_table, 4) = (uint64_t)character_class;
    OBJ_FIELD(class_table, 5) = (uint64_t)undefined_object_class;

    md_append(fixture_om, class_class, smallint_class, "+",
              (uint64_t)make_primitive_cm(fixture_om, class_class, PRIM_SMALLINT_ADD, 1));
    md_append(fixture_om, class_class, smallint_class, "-",
              (uint64_t)make_primitive_cm(fixture_om, class_class, PRIM_SMALLINT_SUB, 1));
    md_append(fixture_om, class_class, smallint_class, "*",
              (uint64_t)make_primitive_cm(fixture_om, class_class, PRIM_SMALLINT_MUL, 1));
    md_append(fixture_om, class_class, smallint_class, "<",
              (uint64_t)make_primitive_cm(fixture_om, class_class, PRIM_SMALLINT_LT, 1));
    md_append(fixture_om, class_class, smallint_class, "=",
              (uint64_t)make_primitive_cm(fixture_om, class_class, PRIM_SMALLINT_EQ, 1));
    md_append(fixture_om, class_class, block_class, "value",
              (uint64_t)make_primitive_cm(fixture_om, class_class, PRIM_BLOCK_VALUE, 0));

    ASSERT_EQ(ctx,
              bc_compile_and_install_source_methods(fixture_om, class_class, NULL, 0, true_src),
              1,
              "fixture True methods install");
    ASSERT_EQ(ctx,
              bc_compile_and_install_source_methods(fixture_om, class_class, NULL, 0, false_src),
              1,
              "fixture False methods install");

    for (int fixture_index = 0; fixture_index < spec_count; fixture_index++)
    {
        ExpressionFixtureSpec *fixture = &specs[fixture_index];
        BCompiledMethodDef setup_methods[8];
        int setup_method_count = 0;
        BCompiledMethodDef expression_methods[2];
        int expression_method_count = 0;
        for (int class_index = 0; class_index < fixture->class_count; class_index++)
        {
            FixtureClassDecl *decl = &fixture->classes[class_index];
            uint64_t *superclass = smalltalk_lookup_class(decl->superclass_name);
            ASSERT_EQ(ctx, superclass != NULL, 1, fixture->name);

            uint64_t *klass = make_runtime_class(fixture_om, class_class, string_class,
                                                 superclass, NULL, 0);
            smalltalk_at_put(fixture_om, array_class, association_class, decl->name, (uint64_t)klass);
        }

        int setup_parse_ok = bc_compile_source_methods(fixture->setup_source, setup_methods, 8, &setup_method_count);
        int setup_ok = 0;
        if (setup_parse_ok)
        {
            setup_ok = bc_install_compiled_methods(fixture_om, class_class, NULL, 0,
                                                   setup_methods, setup_method_count);
        }
        ASSERT_EQ(ctx, setup_ok, 1, fixture->name);

        uint64_t *receiver_class = smalltalk_lookup_class(fixture->receiver_class_name);
        ASSERT_EQ(ctx, receiver_class != NULL, 1, fixture->name);
        ASSERT_EQ(ctx, class_lookup(receiver_class, selector_oop(fixture_om, "factorial:")) != 0, 1,
                  "fixture helper method installed");

        char selector[32];
        char source[1024];
        snprintf(selector, sizeof(selector), "fixtureExpr%d", fixture_index + 1);
        snprintf(source, sizeof(source),
                 "!%s methodsFor: 'expression fixtures'!\n"
                 "%s\n"
                 "    ^ %s\n"
                 "!\n",
                 fixture->receiver_class_name, selector, fixture->expression);

        ASSERT_EQ(ctx,
                  bc_compile_source_methods(source, expression_methods, 2, &expression_method_count),
                  1,
                  "fixture expression source parses");
        ASSERT_EQ(ctx,
                  bc_install_compiled_methods(fixture_om, class_class, NULL, 0,
                                              expression_methods, expression_method_count),
                  1,
                  fixture->name);

        uint64_t *instance_md = (uint64_t *)OBJ_FIELD(receiver_class, CLASS_METHOD_DICT);
        uint64_t method_oop = md_lookup(instance_md, selector_oop(fixture_om, selector));
        ASSERT_EQ(ctx, method_oop != 0, 1, "fixture expression method installed");
        ASSERT_EQ(ctx, class_lookup(receiver_class, selector_oop(fixture_om, "factorial:")) != 0, 1,
                  "fixture helper method survives expression install");

        uint64_t *receiver = om_alloc(fixture_om, (uint64_t)receiver_class, FORMAT_FIELDS, 0);
        uint64_t *compiled_method = (uint64_t *)method_oop;
        uint64_t *bytecodes = (uint64_t *)OBJ_FIELD(compiled_method, CM_BYTECODES);

        uint64_t *sp = (uint64_t *)((uint8_t *)fixture_stack + STACK_WORDS * sizeof(uint64_t));
        uint64_t *fp = (uint64_t *)0xCAFE;
        stack_push(&sp, fixture_stack, (uint64_t)receiver);
        activate_method(&sp, &fp, 0, (uint64_t)compiled_method, 0, 0);

        uint64_t result = interpret(&sp, &fp, (uint8_t *)&OBJ_FIELD(bytecodes, 0), class_table, fixture_om, NULL);

        if (fixture->expected_kind == FIXTURE_EXPECT_SMALLINT)
        {
            ASSERT_EQ(ctx, result, tag_smallint(fixture->expected_smallint), fixture->name);
        }
        else if (fixture->expected_kind == FIXTURE_EXPECT_TRUE)
        {
            ASSERT_EQ(ctx, result, tagged_true(), fixture->name);
        }
        else
        {
            ASSERT_EQ(ctx, result, tagged_false(), fixture->name);
        }
    }

    global_symbol_table = saved_symbol_table;
    global_symbol_class = saved_symbol_class;
    global_context_class = saved_context_class;
    global_smalltalk_dictionary = saved_smalltalk_dictionary;
}
