#include "smalltalk_world.h"
#include "bootstrap_compiler.h"
#include "primitives.h"

#include <stdio.h>
#include <string.h>

// Install a CompiledMethod into a class's method dictionary (growing by +2 each time).
static void md_append_local(uint64_t *om, uint64_t *class_class, uint64_t *klass,
                            const char *selector, uint64_t method)
{
    uint64_t md_val = OBJ_FIELD(klass, CLASS_METHOD_DICT);
    uint64_t *old_md = (md_val != tagged_nil() && (md_val & 3) == 0) ? (uint64_t *)md_val : NULL;
    uint64_t old_size = old_md ? OBJ_SIZE(old_md) : 0;
    uint64_t *new_md = om_alloc(om, (uint64_t)class_class, FORMAT_INDEXABLE, old_size + 2);

    for (uint64_t i = 0; i < old_size; i++)
    {
        OBJ_FIELD(new_md, i) = OBJ_FIELD(old_md, i);
    }
    OBJ_FIELD(new_md, old_size) = intern_cstring_symbol(om, selector);
    OBJ_FIELD(new_md, old_size + 1) = method;
    OBJ_FIELD(klass, CLASS_METHOD_DICT) = (uint64_t)new_md;
}

// Build a CompiledMethod that dispatches to a VM primitive (the bytecode is
// just BC_HALT so the primitive's value is on top of stack after activation).
static uint64_t *make_primitive_cm_local(uint64_t *om, uint64_t *class_class, int prim, int num_args)
{
    uint64_t *prim_bc = om_alloc(om, (uint64_t)class_class, FORMAT_BYTES, 1);
    ((uint8_t *)&OBJ_FIELD(prim_bc, 0))[0] = BC_HALT;

    uint64_t *cm = om_alloc(om, (uint64_t)class_class, FORMAT_FIELDS, 6);
    OBJ_FIELD(cm, CM_PRIMITIVE) = tag_smallint(prim);
    OBJ_FIELD(cm, CM_NUM_ARGS) = tag_smallint(num_args);
    OBJ_FIELD(cm, CM_NUM_TEMPS) = tag_smallint(0);
    OBJ_FIELD(cm, CM_LITERALS) = tagged_nil();
    OBJ_FIELD(cm, CM_BYTECODES) = (uint64_t)prim_bc;
    OBJ_FIELD(cm, CM_SOURCE) = tagged_nil();
    return cm;
}

// Build a class. with_metaclass=1 creates a per-class metaclass so class-side
// methods live on each class's own dict (the metaclass inherits from
// class_class so Class>>new etc. are still reachable).
static uint64_t *sw_make_class_full(SmalltalkWorld *world, uint64_t *superclass,
                                    const char **ivars, int ivar_count, int format,
                                    int with_metaclass)
{
    uint64_t *class_class = world->class_class;
    uint64_t *class_of_klass = class_class;
    if (with_metaclass)
    {
        uint64_t *metaclass = om_alloc(world->om, (uint64_t)class_class, FORMAT_FIELDS, 5);
        OBJ_FIELD(metaclass, CLASS_SUPERCLASS) = (uint64_t)class_class;
        OBJ_FIELD(metaclass, CLASS_METHOD_DICT) = tagged_nil();
        OBJ_FIELD(metaclass, CLASS_INST_SIZE) = tag_smallint(0);
        OBJ_FIELD(metaclass, CLASS_INST_FORMAT) = tag_smallint(FORMAT_FIELDS);
        OBJ_FIELD(metaclass, CLASS_INST_VARS) = tagged_nil();
        class_of_klass = metaclass;
    }
    uint64_t *klass = om_alloc(world->om, (uint64_t)class_of_klass, FORMAT_FIELDS, 5);
    OBJ_FIELD(klass, CLASS_SUPERCLASS) = superclass ? (uint64_t)superclass : tagged_nil();
    OBJ_FIELD(klass, CLASS_METHOD_DICT) = tagged_nil();
    OBJ_FIELD(klass, CLASS_INST_SIZE) = tag_smallint(ivar_count);
    OBJ_FIELD(klass, CLASS_INST_FORMAT) = tag_smallint(format);
    if (ivar_count <= 0)
    {
        OBJ_FIELD(klass, CLASS_INST_VARS) = tagged_nil();
    }
    else
    {
        uint64_t *iv = om_alloc(world->om, (uint64_t)class_class, FORMAT_INDEXABLE, ivar_count);
        for (int i = 0; i < ivar_count; i++)
        {
            uint64_t len = (uint64_t)strlen(ivars[i]);
            uint64_t *s = om_alloc(world->om, (uint64_t)world->string_class, FORMAT_BYTES, len);
            if (len > 0) memcpy(&OBJ_FIELD(s, 0), ivars[i], len);
            OBJ_FIELD(iv, i) = (uint64_t)s;
        }
        OBJ_FIELD(klass, CLASS_INST_VARS) = (uint64_t)iv;
    }
    return klass;
}

static uint64_t *sw_make_class(SmalltalkWorld *world, uint64_t *superclass,
                               const char **ivars, int ivar_count, int format)
{
    return sw_make_class_full(world, superclass, ivars, ivar_count, format, 0);
}

static void sw_dict_put(SmalltalkWorld *world, const char *name, uint64_t value)
{
    uint64_t key = intern_cstring_symbol(world->om, name);
    uint64_t associations_oop = OBJ_FIELD(world->smalltalk_dict, 0);
    uint64_t tally = OBJ_FIELD(world->smalltalk_dict, 1) == tagged_nil()
                         ? 0
                         : (uint64_t)untag_smallint(OBJ_FIELD(world->smalltalk_dict, 1));

    if (associations_oop == tagged_nil())
    {
        uint64_t *arr = om_alloc(world->om, (uint64_t)world->array_class, FORMAT_INDEXABLE, 16);
        for (uint64_t i = 0; i < 16; i++) OBJ_FIELD(arr, i) = tagged_nil();
        OBJ_FIELD(world->smalltalk_dict, 0) = (uint64_t)arr;
        OBJ_FIELD(world->smalltalk_dict, 1) = tag_smallint(0);
        associations_oop = (uint64_t)arr;
    }

    uint64_t *associations = (uint64_t *)associations_oop;
    for (uint64_t i = 0; i < tally; i++)
    {
        uint64_t a = OBJ_FIELD(associations, i);
        if (!is_object_ptr(a)) continue;
        if (OBJ_FIELD((uint64_t *)a, 0) == key)
        {
            OBJ_FIELD((uint64_t *)a, 1) = value;
            return;
        }
    }
    if (tally >= OBJ_SIZE(associations))
    {
        uint64_t new_size = OBJ_SIZE(associations) * 2;
        uint64_t *grown = om_alloc(world->om, (uint64_t)world->array_class, FORMAT_INDEXABLE, new_size);
        for (uint64_t i = 0; i < new_size; i++)
        {
            OBJ_FIELD(grown, i) = i < OBJ_SIZE(associations) ? OBJ_FIELD(associations, i) : tagged_nil();
        }
        OBJ_FIELD(world->smalltalk_dict, 0) = (uint64_t)grown;
        associations = grown;
    }
    uint64_t *assoc = om_alloc(world->om, (uint64_t)world->association_class, FORMAT_FIELDS, 2);
    OBJ_FIELD(assoc, 0) = key;
    OBJ_FIELD(assoc, 1) = value;
    OBJ_FIELD(associations, tally) = (uint64_t)assoc;
    OBJ_FIELD(world->smalltalk_dict, 1) = tag_smallint(tally + 1);
}

static int sw_copy_symbol_to_cstring(uint64_t symbol_oop, char *buffer, size_t capacity)
{
    uint64_t *symbol;
    uint64_t size;

    if (!is_object_ptr(symbol_oop) || buffer == NULL || capacity == 0)
    {
        return 0;
    }
    symbol = (uint64_t *)symbol_oop;
    if (OBJ_FORMAT(symbol) != FORMAT_BYTES)
    {
        return 0;
    }
    size = OBJ_SIZE(symbol);
    if (size >= capacity)
    {
        return 0;
    }
    if (size > 0)
    {
        memcpy(&buffer[0], &OBJ_FIELD(symbol, 0), (size_t)size);
    }
    buffer[size] = '\0';
    return 1;
}

static int sw_collect_class_bindings(SmalltalkWorld *world,
                                     BClassBinding *bindings,
                                     char names[][64],
                                     int max_bindings)
{
    uint64_t associations_oop = OBJ_FIELD(world->smalltalk_dict, 0);
    uint64_t tally_oop = OBJ_FIELD(world->smalltalk_dict, 1);
    uint64_t *associations;
    uint64_t tally;
    int count = 0;

    if (!is_object_ptr(associations_oop) || !is_smallint(tally_oop))
    {
        return 0;
    }

    associations = (uint64_t *)associations_oop;
    tally = (uint64_t)untag_smallint(tally_oop);
    for (uint64_t index = 0; index < tally && count < max_bindings; index++)
    {
        uint64_t assoc_oop = OBJ_FIELD(associations, index);
        uint64_t key_oop;
        uint64_t value_oop;

        if (!is_object_ptr(assoc_oop))
        {
            continue;
        }
        key_oop = OBJ_FIELD((uint64_t *)assoc_oop, 0);
        value_oop = OBJ_FIELD((uint64_t *)assoc_oop, 1);
        if (!is_object_ptr(value_oop))
        {
            continue;
        }
        if (!sw_copy_symbol_to_cstring(key_oop, names[count], sizeof(names[count])))
        {
            continue;
        }
        bindings[count].class_name = names[count];
        bindings[count].klass = (ObjPtr)value_oop;
        count++;
    }

    return count;
}

void smalltalk_world_init(SmalltalkWorld *world, void *buffer, uint64_t buffer_size)
{
    world->saved_symbol_table = global_symbol_table;
    world->saved_symbol_class = global_symbol_class;
    world->saved_context_class = global_context_class;
    world->saved_smalltalk_dict = global_smalltalk_dictionary;

    om_init(buffer, buffer_size, world->om);
    // Root class: Class.
    world->class_class = om_alloc(world->om, 0, FORMAT_FIELDS, 5);
    OBJ_CLASS(world->class_class) = (uint64_t)world->class_class;
    OBJ_FIELD(world->class_class, CLASS_SUPERCLASS) = tagged_nil();
    OBJ_FIELD(world->class_class, CLASS_METHOD_DICT) = tagged_nil();
    OBJ_FIELD(world->class_class, CLASS_INST_SIZE) = tag_smallint(5);
    OBJ_FIELD(world->class_class, CLASS_INST_FORMAT) = tag_smallint(FORMAT_FIELDS);
    OBJ_FIELD(world->class_class, CLASS_INST_VARS) = tagged_nil();

    // String class is needed by sw_make_class to make ivar name strings.
    // Bootstrap it before object_class by temporarily setting string_class
    // to a sentinel that we'll replace once String is ready.
    world->string_class = NULL; // mark for first-make special-case
    uint64_t *temp_string = om_alloc(world->om, (uint64_t)world->class_class, FORMAT_FIELDS, 5);
    OBJ_FIELD(temp_string, CLASS_SUPERCLASS) = tagged_nil();
    OBJ_FIELD(temp_string, CLASS_METHOD_DICT) = tagged_nil();
    OBJ_FIELD(temp_string, CLASS_INST_SIZE) = tag_smallint(0);
    OBJ_FIELD(temp_string, CLASS_INST_FORMAT) = tag_smallint(FORMAT_BYTES);
    OBJ_FIELD(temp_string, CLASS_INST_VARS) = tagged_nil();
    world->string_class = temp_string;

    world->object_class = sw_make_class(world, NULL, NULL, 0, FORMAT_FIELDS);
    // Fix up String's superclass to Object now that object_class exists.
    OBJ_FIELD(world->string_class, CLASS_SUPERCLASS) = (uint64_t)world->object_class;

    world->smallint_class = sw_make_class(world, world->object_class, NULL, 0, FORMAT_FIELDS);
    world->block_class = sw_make_class(world, world->object_class, NULL, 0, FORMAT_FIELDS);
    world->undefined_class = sw_make_class(world, world->object_class, NULL, 0, FORMAT_FIELDS);
    world->true_class = sw_make_class(world, world->object_class, NULL, 0, FORMAT_FIELDS);
    world->false_class = sw_make_class(world, world->object_class, NULL, 0, FORMAT_FIELDS);
    world->symbol_class = sw_make_class(world, world->string_class, NULL, 0, FORMAT_BYTES);
    world->character_class = sw_make_class(world, world->object_class, NULL, 0, FORMAT_FIELDS);
    world->array_class = sw_make_class(world, world->object_class, NULL, 0, FORMAT_INDEXABLE);

    const char *assoc_ivars[] = {"key", "value"};
    world->association_class = sw_make_class(world, world->object_class, assoc_ivars, 2, FORMAT_FIELDS);
    const char *dict_ivars[] = {"associations", "tally"};
    world->dictionary_class = sw_make_class(world, world->object_class, dict_ivars, 2, FORMAT_FIELDS);
    // Context class — needed by the runtime for non-local block returns. Ivars
    // match the frame layout expected by ensure_frame_context.
    const char *context_ivars[] = {
        "sender", "ip", "method", "receiver", "home", "closure", "flags", "numArgs", "numTemps"
    };
    world->context_class = sw_make_class(world, world->object_class, context_ivars, 9, FORMAT_FIELDS);

    // Symbol table — 2048 slots, enough for a fully loaded compiler image.
    world->symbol_table = om_alloc(world->om, (uint64_t)world->class_class, FORMAT_INDEXABLE, 2048);
    for (int i = 0; i < 2048; i++)
    {
        OBJ_FIELD(world->symbol_table, i) = tagged_nil();
    }

    // Install required globals before we start creating symbols.
    global_symbol_table = world->symbol_table;
    global_symbol_class = world->symbol_class;
    global_context_class = world->context_class;

    // Smalltalk global dictionary.
    world->smalltalk_dict = om_alloc(world->om, (uint64_t)world->dictionary_class, FORMAT_FIELDS, 2);
    OBJ_FIELD(world->smalltalk_dict, 0) = tagged_nil();
    OBJ_FIELD(world->smalltalk_dict, 1) = tag_smallint(0);
    global_smalltalk_dictionary = world->smalltalk_dict;

    // Register core classes in the Smalltalk dictionary.
    sw_dict_put(world, "Class", (uint64_t)world->class_class);
    sw_dict_put(world, "Object", (uint64_t)world->object_class);
    sw_dict_put(world, "SmallInteger", (uint64_t)world->smallint_class);
    sw_dict_put(world, "Integer", (uint64_t)world->smallint_class);
    sw_dict_put(world, "BlockClosure", (uint64_t)world->block_class);
    sw_dict_put(world, "UndefinedObject", (uint64_t)world->undefined_class);
    sw_dict_put(world, "True", (uint64_t)world->true_class);
    sw_dict_put(world, "False", (uint64_t)world->false_class);
    sw_dict_put(world, "String", (uint64_t)world->string_class);
    sw_dict_put(world, "Symbol", (uint64_t)world->symbol_class);
    sw_dict_put(world, "Character", (uint64_t)world->character_class);
    sw_dict_put(world, "Array", (uint64_t)world->array_class);
    sw_dict_put(world, "Association", (uint64_t)world->association_class);
    sw_dict_put(world, "Dictionary", (uint64_t)world->dictionary_class);
    sw_dict_put(world, "Context", (uint64_t)world->context_class);

    // Framework class table used by send_selector helpers.
    world->class_table = om_alloc(world->om, (uint64_t)world->class_class, FORMAT_INDEXABLE, 6);
    OBJ_FIELD(world->class_table, 0) = (uint64_t)world->smallint_class;
    OBJ_FIELD(world->class_table, 1) = (uint64_t)world->block_class;
    OBJ_FIELD(world->class_table, 2) = (uint64_t)world->true_class;
    OBJ_FIELD(world->class_table, 3) = (uint64_t)world->false_class;
    OBJ_FIELD(world->class_table, 4) = (uint64_t)world->character_class;
    OBJ_FIELD(world->class_table, 5) = (uint64_t)world->undefined_class;

    // Install primitives.
    md_append_local(world->om, world->class_class, world->class_class, "new",
              (uint64_t)make_primitive_cm_local(world->om, world->class_class, PRIM_BASIC_NEW, 0));
    md_append_local(world->om, world->class_class, world->class_class, "new:",
              (uint64_t)make_primitive_cm_local(world->om, world->class_class, PRIM_BASIC_NEW_SIZE, 1));
    md_append_local(world->om, world->class_class, world->class_class, "basicNew",
              (uint64_t)make_primitive_cm_local(world->om, world->class_class, PRIM_BASIC_NEW, 0));
    md_append_local(world->om, world->class_class, world->class_class, "basicNew:",
              (uint64_t)make_primitive_cm_local(world->om, world->class_class, PRIM_BASIC_NEW_SIZE, 1));
    md_append_local(world->om, world->class_class, world->class_class, "==",
              (uint64_t)make_primitive_cm_local(world->om, world->class_class, PRIM_IDENTITY_EQ, 1));

    md_append_local(world->om, world->class_class, world->smallint_class, "+",
              (uint64_t)make_primitive_cm_local(world->om, world->class_class, PRIM_SMALLINT_ADD, 1));
    md_append_local(world->om, world->class_class, world->smallint_class, "-",
              (uint64_t)make_primitive_cm_local(world->om, world->class_class, PRIM_SMALLINT_SUB, 1));
    md_append_local(world->om, world->class_class, world->smallint_class, "*",
              (uint64_t)make_primitive_cm_local(world->om, world->class_class, PRIM_SMALLINT_MUL, 1));
    md_append_local(world->om, world->class_class, world->smallint_class, "<",
              (uint64_t)make_primitive_cm_local(world->om, world->class_class, PRIM_SMALLINT_LT, 1));
    md_append_local(world->om, world->class_class, world->smallint_class, "=",
              (uint64_t)make_primitive_cm_local(world->om, world->class_class, PRIM_SMALLINT_EQ, 1));
    md_append_local(world->om, world->class_class, world->smallint_class, "asCharacter",
              (uint64_t)make_primitive_cm_local(world->om, world->class_class, PRIM_AS_CHARACTER, 0));

    md_append_local(world->om, world->class_class, world->block_class, "value",
              (uint64_t)make_primitive_cm_local(world->om, world->class_class, PRIM_BLOCK_VALUE, 0));
    md_append_local(world->om, world->class_class, world->block_class, "value:",
              (uint64_t)make_primitive_cm_local(world->om, world->class_class, PRIM_BLOCK_VALUE_ARG, 1));

    md_append_local(world->om, world->class_class, world->character_class, "value",
              (uint64_t)make_primitive_cm_local(world->om, world->class_class, PRIM_CHAR_VALUE, 0));
    md_append_local(world->om, world->class_class, world->character_class, "isLetter",
              (uint64_t)make_primitive_cm_local(world->om, world->class_class, PRIM_CHAR_IS_LETTER, 0));
    md_append_local(world->om, world->class_class, world->character_class, "isDigit",
              (uint64_t)make_primitive_cm_local(world->om, world->class_class, PRIM_CHAR_IS_DIGIT, 0));

    md_append_local(world->om, world->class_class, world->string_class, "=",
              (uint64_t)make_primitive_cm_local(world->om, world->class_class, PRIM_STRING_EQ, 1));
    md_append_local(world->om, world->class_class, world->string_class, "asSymbol",
              (uint64_t)make_primitive_cm_local(world->om, world->class_class, PRIM_STRING_AS_SYMBOL, 0));
    md_append_local(world->om, world->class_class, world->string_class, "size",
              (uint64_t)make_primitive_cm_local(world->om, world->class_class, PRIM_SIZE, 0));
    md_append_local(world->om, world->class_class, world->string_class, "at:",
              (uint64_t)make_primitive_cm_local(world->om, world->class_class, PRIM_AT, 1));
    md_append_local(world->om, world->class_class, world->string_class, "at:put:",
              (uint64_t)make_primitive_cm_local(world->om, world->class_class, PRIM_AT_PUT, 2));

    md_append_local(world->om, world->class_class, world->symbol_class, "=",
              (uint64_t)make_primitive_cm_local(world->om, world->class_class, PRIM_SYMBOL_EQ, 1));
    md_append_local(world->om, world->class_class, world->symbol_class, "size",
              (uint64_t)make_primitive_cm_local(world->om, world->class_class, PRIM_SIZE, 0));

    md_append_local(world->om, world->class_class, world->array_class, "size",
              (uint64_t)make_primitive_cm_local(world->om, world->class_class, PRIM_SIZE, 0));
    md_append_local(world->om, world->class_class, world->array_class, "at:",
              (uint64_t)make_primitive_cm_local(world->om, world->class_class, PRIM_AT, 1));
    md_append_local(world->om, world->class_class, world->array_class, "at:put:",
              (uint64_t)make_primitive_cm_local(world->om, world->class_class, PRIM_AT_PUT, 2));
}

void smalltalk_world_teardown(SmalltalkWorld *world)
{
    global_symbol_table = world->saved_symbol_table;
    global_symbol_class = world->saved_symbol_class;
    global_context_class = world->saved_context_class;
    global_smalltalk_dictionary = world->saved_smalltalk_dict;
}

uint64_t *smalltalk_world_define_class(SmalltalkWorld *world, const char *name,
                                       uint64_t *superclass,
                                       const char **ivars, int ivar_count,
                                       int format)
{
    // User-defined classes (Token, Tokenizer, Parser, ...) get per-class
    // metaclasses so their class-side methods don't collide on class_class.
    uint64_t *klass = sw_make_class_full(world,
                                         superclass ? superclass : world->object_class,
                                         ivars, ivar_count, format, 1);
    sw_dict_put(world, name, (uint64_t)klass);
    return klass;
}

int smalltalk_world_install_st_file(SmalltalkWorld *world, const char *path)
{
    BClassBinding bindings[128];
    char binding_names[128][64];
    int binding_count = sw_collect_class_bindings(world, bindings, binding_names, 128);
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    static char buf[32768];
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    buf[n] = '\0';
    return bc_compile_and_install_source_methods(world->om, world->class_class,
                                                 bindings, binding_count, buf);
}

uint64_t *smalltalk_world_install_class_file(SmalltalkWorld *world, const char *path)
{
    BClassBinding bindings[128];
    char binding_names[128][64];
    int binding_count = sw_collect_class_bindings(world, bindings, binding_names, 128);
    return bc_compile_and_install_class_file(world->om, world->class_class,
                                             world->string_class, world->array_class,
                                             world->association_class, bindings, binding_count, path);
}

uint64_t *smalltalk_world_install_existing_class_file(SmalltalkWorld *world, const char *path)
{
    BClassBinding bindings[128];
    char binding_names[128][64];
    int binding_count = sw_collect_class_bindings(world, bindings, binding_names, 128);
    return bc_compile_and_install_existing_class_file(world->om, world->class_class,
                                                      bindings, binding_count, path);
}

ObjPtr smalltalk_world_lookup_class(SmalltalkWorld *world, const char *name)
{
    uint64_t key = lookup_cstring_symbol(name);
    if (key == tagged_nil()) return NULL;
    uint64_t associations_oop = OBJ_FIELD(world->smalltalk_dict, 0);
    if (!is_object_ptr(associations_oop)) return NULL;
    uint64_t *associations = (uint64_t *)associations_oop;
    uint64_t tally = (uint64_t)untag_smallint(OBJ_FIELD(world->smalltalk_dict, 1));
    for (uint64_t i = 0; i < tally; i++)
    {
        uint64_t a = OBJ_FIELD(associations, i);
        if (!is_object_ptr(a)) continue;
        if (OBJ_FIELD((uint64_t *)a, 0) == key)
        {
            return (uint64_t *)OBJ_FIELD((uint64_t *)a, 1);
        }
    }
    return NULL;
}

ObjPtr sw_make_string(SmalltalkWorld *world, const char *text)
{
    uint64_t len = (uint64_t)strlen(text);
    uint64_t *s = om_alloc(world->om, (uint64_t)world->string_class, FORMAT_BYTES, len);
    if (len > 0) memcpy(&OBJ_FIELD(s, 0), text, len);
    return s;
}

// For an object receiver, dispatch class is its actual class (which may be a
// per-class metaclass when the receiver is itself a class). For tagged
// immediates, we need the caller-provided class.
static ObjPtr sw_dispatch_class(Oop receiver, ObjPtr fallback)
{
    if (is_object_ptr(receiver))
    {
        return (ObjPtr)OBJ_CLASS((ObjPtr)receiver);
    }
    return fallback;
}

Oop sw_send0(SmalltalkWorld *world, TestContext *ctx, Oop receiver,
             ObjPtr receiver_class, const char *selector)
{
    ObjPtr dispatch_class = sw_dispatch_class(receiver, receiver_class);
    Oop selector_oop = intern_cstring_symbol(world->om, selector);
    Oop method_oop = class_lookup(dispatch_class, selector_oop);
    ObjPtr method = (ObjPtr)method_oop;
    ObjPtr bytecodes = (ObjPtr)OBJ_FIELD(method, CM_BYTECODES);
    uint64_t num_temps = (uint64_t)untag_smallint(OBJ_FIELD(method, CM_NUM_TEMPS));
    Oop *sp = ctx->stack + STACK_WORDS;
    ObjPtr fp = (ObjPtr)0xCAFE;
    stack_push(&sp, ctx->stack, receiver);
    activate_method(&sp, &fp, 0, (Oop)method, 0, num_temps);
    return interpret(&sp, &fp, (uint8_t *)&OBJ_FIELD(bytecodes, 0), world->class_table, world->om, NULL);
}

Oop sw_send1(SmalltalkWorld *world, TestContext *ctx, Oop receiver,
             ObjPtr receiver_class, const char *selector, Oop arg)
{
    ObjPtr dispatch_class = sw_dispatch_class(receiver, receiver_class);
    Oop selector_oop = intern_cstring_symbol(world->om, selector);
    Oop method_oop = class_lookup(dispatch_class, selector_oop);
    ObjPtr method = (ObjPtr)method_oop;
    ObjPtr bytecodes = (ObjPtr)OBJ_FIELD(method, CM_BYTECODES);
    uint64_t num_temps = (uint64_t)untag_smallint(OBJ_FIELD(method, CM_NUM_TEMPS));
    Oop *sp = ctx->stack + STACK_WORDS;
    ObjPtr fp = (ObjPtr)0xCAFE;
    stack_push(&sp, ctx->stack, receiver);
    stack_push(&sp, ctx->stack, arg);
    activate_method(&sp, &fp, 0, (Oop)method, 1, num_temps);
    return interpret(&sp, &fp, (uint8_t *)&OBJ_FIELD(bytecodes, 0), world->class_table, world->om, NULL);
}

Oop sw_send2(SmalltalkWorld *world, TestContext *ctx, Oop receiver,
             ObjPtr receiver_class, const char *selector,
             Oop arg0, Oop arg1)
{
    ObjPtr dispatch_class = sw_dispatch_class(receiver, receiver_class);
    Oop selector_oop = intern_cstring_symbol(world->om, selector);
    Oop method_oop = class_lookup(dispatch_class, selector_oop);
    ObjPtr method = (ObjPtr)method_oop;
    ObjPtr bytecodes = (ObjPtr)OBJ_FIELD(method, CM_BYTECODES);
    uint64_t num_temps = (uint64_t)untag_smallint(OBJ_FIELD(method, CM_NUM_TEMPS));
    Oop *sp = ctx->stack + STACK_WORDS;
    ObjPtr fp = (ObjPtr)0xCAFE;
    stack_push(&sp, ctx->stack, receiver);
    stack_push(&sp, ctx->stack, arg0);
    stack_push(&sp, ctx->stack, arg1);
    activate_method(&sp, &fp, 0, (Oop)method, 2, num_temps);
    return interpret(&sp, &fp, (uint8_t *)&OBJ_FIELD(bytecodes, 0), world->class_table, world->om, NULL);
}
