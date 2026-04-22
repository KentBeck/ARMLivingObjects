#include "vm_defs.h"

#include <stddef.h>
#include <stdint.h>

uint64_t oop_class(uint64_t oop, uint64_t *class_table)
{
    if ((oop & TAG_MASK) == TAG_OBJECT)
    {
        return OBJ_CLASS((uint64_t *)oop);
    }

    if ((oop & TAG_MASK) == TAG_SMALLINT)
    {
        return OBJ_FIELD(class_table, CLASS_TABLE_SMALLINT);
    }

    if ((oop & CHAR_TAG_MASK) == CHAR_TAG_VALUE)
    {
        return OBJ_FIELD(class_table, CLASS_TABLE_CHARACTER);
    }

    if (oop == TAGGED_NIL)
    {
        return OBJ_FIELD(class_table, CLASS_TABLE_UNDEFINED_OBJECT);
    }

    if (oop == TAGGED_TRUE)
    {
        return OBJ_FIELD(class_table, CLASS_TABLE_TRUE);
    }

    if (oop == TAGGED_FALSE)
    {
        return OBJ_FIELD(class_table, CLASS_TABLE_FALSE);
    }

    return 0;
}

uint64_t md_lookup(uint64_t *method_dict, uint64_t selector)
{
    uint64_t size = OBJ_SIZE(method_dict);

    for (uint64_t index = 0; index < size; index += 2)
    {
        if (OBJ_FIELD(method_dict, index) == selector)
        {
            return OBJ_FIELD(method_dict, index + 1);
        }
    }

    return 0;
}

uint64_t class_lookup(uint64_t *klass, uint64_t selector)
{
    while (klass != NULL && (uint64_t)klass != TAGGED_NIL)
    {
        uint64_t method_dict = OBJ_FIELD(klass, CLASS_METHOD_DICT);
        if (method_dict != TAGGED_NIL)
        {
            uint64_t method = md_lookup((uint64_t *)method_dict, selector);
            if (method != 0)
            {
                return method;
            }
        }

        klass = (uint64_t *)OBJ_FIELD(klass, CLASS_SUPERCLASS);
    }

    return 0;
}
