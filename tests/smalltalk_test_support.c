#include "smalltalk_test_support.h"

Oop stt_selector_oop(Om om, const char *selector)
{
    return intern_cstring_symbol(om, selector);
}

void stt_md_append(Om om, ObjPtr class_class, ObjPtr klass, const char *selector, Oop method)
{
    Oop md_oop = OBJ_FIELD(klass, CLASS_METHOD_DICT);
    ObjPtr old_md = (md_oop != tagged_nil() && is_object_ptr(md_oop)) ? (ObjPtr)md_oop : NULL;
    uint64_t old_size = old_md != NULL ? OBJ_SIZE(old_md) : 0;
    ObjPtr new_md = om_alloc(om, (Oop)class_class, FORMAT_INDEXABLE, old_size + 2);

    for (uint64_t index = 0; index < old_size; index++)
    {
        OBJ_FIELD(new_md, index) = OBJ_FIELD(old_md, index);
    }
    OBJ_FIELD(new_md, old_size) = stt_selector_oop(om, selector);
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
