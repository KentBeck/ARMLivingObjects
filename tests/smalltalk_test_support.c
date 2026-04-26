#include "smalltalk_test_support.h"
#include <ctype.h>

Oop stt_selector_oop(Om om, const char *selector)
{
    return intern_cstring_symbol(om, selector);
}

void stt_trim_in_place(char *text)
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

int stt_parse_expected_value(const char *text, int *kind_out, int64_t *smallint_value)
{
    char *end = NULL;
    long long parsed;

    if (strcmp(text, "true") == 0)
    {
        *kind_out = 1;
        *smallint_value = 0;
        return 1;
    }
    if (strcmp(text, "false") == 0)
    {
        *kind_out = 2;
        *smallint_value = 0;
        return 1;
    }

    parsed = strtoll(text, &end, 10);
    if (end == text || *end != '\0')
    {
        return 0;
    }
    *kind_out = 0;
    *smallint_value = (int64_t)parsed;
    return 1;
}

void stt_md_append(Om om, ObjPtr class_class, ObjPtr klass, const char *selector, Oop method)
{
    stt_md_append_oop(om, class_class, klass, stt_selector_oop(om, selector), method);
}

void stt_md_append_oop(Om om, ObjPtr class_class, ObjPtr klass, Oop selector, Oop method)
{
    Oop md_oop = OBJ_FIELD(klass, CLASS_METHOD_DICT);
    ObjPtr old_md = (md_oop != tagged_nil() && is_object_ptr(md_oop)) ? (ObjPtr)md_oop : NULL;
    uint64_t old_size = old_md != NULL ? OBJ_SIZE(old_md) : 0;
    ObjPtr new_md = om_alloc(om, (Oop)class_class, FORMAT_INDEXABLE, old_size + 2);

    for (uint64_t index = 0; index < old_size; index++)
    {
        OBJ_FIELD(new_md, index) = OBJ_FIELD(old_md, index);
    }
    OBJ_FIELD(new_md, old_size) = selector;
    OBJ_FIELD(new_md, old_size + 1) = method;
    OBJ_FIELD(klass, CLASS_METHOD_DICT) = (Oop)new_md;
}

ObjPtr stt_make_primitive_cm(Om om, ObjPtr class_class, int prim, int num_args)
{
    ObjPtr prim_bc = om_alloc(om, (Oop)class_class, FORMAT_BYTES, 1);
    ((uint8_t *)&OBJ_FIELD(prim_bc, 0))[0] = BC_HALT;

    ObjPtr cm = om_alloc(om, (Oop)class_class, FORMAT_FIELDS, 6);
    OBJ_FIELD(cm, CM_PRIMITIVE) = tag_smallint(prim);
    OBJ_FIELD(cm, CM_NUM_ARGS) = tag_smallint(num_args);
    OBJ_FIELD(cm, CM_NUM_TEMPS) = tag_smallint(0);
    OBJ_FIELD(cm, CM_LITERALS) = tagged_nil();
    OBJ_FIELD(cm, CM_BYTECODES) = (Oop)prim_bc;
    OBJ_FIELD(cm, CM_SOURCE) = tagged_nil();
    return cm;
}

ObjPtr stt_make_byte_string(Om om, ObjPtr string_class, const char *text)
{
    uint64_t size = (uint64_t)strlen(text);
    ObjPtr obj = om_alloc(om, (Oop)string_class, FORMAT_BYTES, size);
    memcpy((uint8_t *)&OBJ_FIELD(obj, 0), text, size);
    return obj;
}

ObjPtr stt_make_class_with_ivars(Om om, ObjPtr class_class, ObjPtr string_class,
                                 ObjPtr superclass, const char **ivars, uint64_t ivar_count)
{
    ObjPtr klass = om_alloc(om, (Oop)class_class, FORMAT_FIELDS, 5);
    OBJ_FIELD(klass, CLASS_SUPERCLASS) = superclass != NULL ? (Oop)superclass : tagged_nil();
    OBJ_FIELD(klass, CLASS_METHOD_DICT) = tagged_nil();
    OBJ_FIELD(klass, CLASS_INST_SIZE) = tag_smallint((int64_t)ivar_count);
    OBJ_FIELD(klass, CLASS_INST_FORMAT) = tag_smallint(FORMAT_FIELDS);
    if (ivar_count == 0)
    {
        OBJ_FIELD(klass, CLASS_INST_VARS) = tagged_nil();
        return klass;
    }

    ObjPtr ivar_array = om_alloc(om, (Oop)class_class, FORMAT_INDEXABLE, ivar_count);
    for (uint64_t index = 0; index < ivar_count; index++)
    {
        OBJ_FIELD(ivar_array, index) = (Oop)stt_make_byte_string(om, string_class, ivars[index]);
    }
    OBJ_FIELD(klass, CLASS_INST_VARS) = (Oop)ivar_array;
    return klass;
}

void stt_smalltalk_at_put(Om om, ObjPtr array_class, ObjPtr association_class,
                          const char *name, Oop value)
{
    Oop key = intern_cstring_symbol(om, name);
    Oop associations_oop = OBJ_FIELD(global_smalltalk_dictionary, 0);
    uint64_t tally = OBJ_FIELD(global_smalltalk_dictionary, 1) == tagged_nil()
                         ? 0
                         : (uint64_t)untag_smallint(OBJ_FIELD(global_smalltalk_dictionary, 1));

    if (associations_oop == tagged_nil())
    {
        ObjPtr associations = om_alloc(om, (Oop)array_class, FORMAT_INDEXABLE, 8);
        for (uint64_t index = 0; index < 8; index++)
        {
            OBJ_FIELD(associations, index) = tagged_nil();
        }
        OBJ_FIELD(global_smalltalk_dictionary, 0) = (Oop)associations;
        associations_oop = (Oop)associations;
        OBJ_FIELD(global_smalltalk_dictionary, 1) = tag_smallint(0);
    }

    ObjPtr associations = (ObjPtr)associations_oop;
    for (uint64_t index = 0; index < tally; index++)
    {
        Oop assoc_oop = OBJ_FIELD(associations, index);
        if (!is_object_ptr(assoc_oop))
        {
            continue;
        }
        ObjPtr assoc = (ObjPtr)assoc_oop;
        if (OBJ_FIELD(assoc, 0) == key)
        {
            OBJ_FIELD(assoc, 1) = value;
            return;
        }
    }

    if (tally >= OBJ_SIZE(associations))
    {
        uint64_t new_size = OBJ_SIZE(associations) * 2;
        ObjPtr grown = om_alloc(om, (Oop)array_class, FORMAT_INDEXABLE, new_size);
        for (uint64_t index = 0; index < new_size; index++)
        {
            OBJ_FIELD(grown, index) = index < OBJ_SIZE(associations)
                                          ? OBJ_FIELD(associations, index)
                                          : tagged_nil();
        }
        OBJ_FIELD(global_smalltalk_dictionary, 0) = (Oop)grown;
        associations = grown;
    }

    ObjPtr assoc = om_alloc(om, (Oop)association_class, FORMAT_FIELDS, 2);
    OBJ_FIELD(assoc, 0) = key;
    OBJ_FIELD(assoc, 1) = value;
    OBJ_FIELD(associations, tally) = (Oop)assoc;
    OBJ_FIELD(global_smalltalk_dictionary, 1) = tag_smallint((int64_t)(tally + 1));
}

void stt_install_class_new_size_methods(Om om, ObjPtr class_class, Oop sel_basic_new_size, Oop sel_new_size)
{
    ObjPtr prim_bc = om_alloc(om, (Oop)class_class, FORMAT_BYTES, 1);
    ((uint8_t *)&OBJ_FIELD(prim_bc, 0))[0] = BC_HALT;

    ObjPtr cm_basic_new_size = stt_make_primitive_cm(om, class_class, PRIM_BASIC_NEW_SIZE, 1);
    OBJ_FIELD(cm_basic_new_size, CM_BYTECODES) = (Oop)prim_bc;
    stt_md_append_oop(om, class_class, class_class, sel_basic_new_size, (Oop)cm_basic_new_size);

    ObjPtr bc = om_alloc(om, (Oop)class_class, FORMAT_BYTES, 20);
    uint8_t *p = (uint8_t *)&OBJ_FIELD(bc, 0);
    p[0] = BC_PUSH_SELF;
    p[1] = BC_PUSH_ARG;
    WRITE_U32(&p[2], 0);
    p[6] = BC_SEND_MESSAGE;
    WRITE_U32(&p[7], 0);
    WRITE_U32(&p[11], 1);
    p[15] = BC_RETURN;

    ObjPtr lits = om_alloc(om, (Oop)class_class, FORMAT_INDEXABLE, 1);
    OBJ_FIELD(lits, 0) = sel_basic_new_size;

    ObjPtr cm_new_size = om_alloc(om, (Oop)class_class, FORMAT_FIELDS, 6);
    OBJ_FIELD(cm_new_size, CM_PRIMITIVE) = tag_smallint(0);
    OBJ_FIELD(cm_new_size, CM_NUM_ARGS) = tag_smallint(1);
    OBJ_FIELD(cm_new_size, CM_NUM_TEMPS) = tag_smallint(0);
    OBJ_FIELD(cm_new_size, CM_LITERALS) = (Oop)lits;
    OBJ_FIELD(cm_new_size, CM_BYTECODES) = (Oop)bc;
    OBJ_FIELD(cm_new_size, CM_SOURCE) = tagged_nil();
    stt_md_append_oop(om, class_class, class_class, sel_new_size, (Oop)cm_new_size);
}

Oop stt_send_class_new_size(TestContext *ctx, ObjPtr class_table, Om om, ObjPtr class_class,
                            Oop class_receiver, Oop sel_new_size, int64_t requested_size)
{
    ObjPtr caller_bc = om_alloc(om, (Oop)class_class, FORMAT_BYTES, 20);
    uint8_t *p = (uint8_t *)&OBJ_FIELD(caller_bc, 0);
    ObjPtr lits;
    ObjPtr caller_cm;
    Oop *sp;
    Oop *fp;

    p[0] = BC_PUSH_SELF;
    p[1] = BC_PUSH_LITERAL;
    WRITE_U32(&p[2], 1);
    p[6] = BC_SEND_MESSAGE;
    WRITE_U32(&p[7], 0);
    WRITE_U32(&p[11], 1);
    p[15] = BC_HALT;

    lits = om_alloc(om, (Oop)class_class, FORMAT_INDEXABLE, 2);
    OBJ_FIELD(lits, 0) = sel_new_size;
    OBJ_FIELD(lits, 1) = tag_smallint(requested_size);

    caller_cm = om_alloc(om, (Oop)class_class, FORMAT_FIELDS, 6);
    OBJ_FIELD(caller_cm, CM_PRIMITIVE) = tag_smallint(0);
    OBJ_FIELD(caller_cm, CM_NUM_ARGS) = tag_smallint(0);
    OBJ_FIELD(caller_cm, CM_NUM_TEMPS) = tag_smallint(0);
    OBJ_FIELD(caller_cm, CM_LITERALS) = (Oop)lits;
    OBJ_FIELD(caller_cm, CM_BYTECODES) = (Oop)caller_bc;
    OBJ_FIELD(caller_cm, CM_SOURCE) = tagged_nil();

    sp = (Oop *)((uint8_t *)ctx->stack + STACK_WORDS * sizeof(Oop));
    fp = (Oop *)0xCAFE;
    stack_push(&sp, ctx->stack, class_receiver);
    activate_method(&sp, &fp, 0, (Oop)caller_cm, 0, 0);
    return interpret(&sp, &fp, (uint8_t *)&OBJ_FIELD(caller_bc, 0), class_table, om, NULL);
}
