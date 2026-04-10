#include "primitives.h"
#include <string.h> // For memcmp

uint64_t *global_symbol_table; // Define global symbol table
uint64_t *global_symbol_class;
uint64_t *global_context_class;
uint64_t *global_smalltalk_dictionary;

static uint64_t selector_token_from_cstring(const char *selector)
{
    uint32_t hash = 2166136261u;
    for (const unsigned char *current = (const unsigned char *)selector; *current != '\0'; current++)
    {
        hash ^= (uint32_t)(*current);
        hash *= 16777619u;
    }
    return tag_smallint((int64_t)(hash & 0x1FFFFFFF));
}

uint64_t cannot_return_selector_oop(void)
{
    return selector_token_from_cstring("cannotReturn:");
}

// Helper to get raw byte pointer and size from a FORMAT_BYTES object
static inline void get_byte_obj_data(uint64_t obj_ptr, uint8_t **data, uint64_t *size) {
    uint64_t *obj = (uint64_t *)obj_ptr;
    *data = (uint8_t *)&OBJ_FIELD(obj, 0);
    *size = OBJ_SIZE(obj);
}

uint64_t lookup_cstring_symbol(const char *text)
{
    if (global_symbol_table == NULL || global_symbol_class == NULL || text == NULL)
    {
        return tagged_nil();
    }

    uint64_t text_size = (uint64_t)strlen(text);
    uint64_t table_size = OBJ_SIZE(global_symbol_table);

    for (uint64_t i = 0; i < table_size; i++)
    {
        uint64_t existing_symbol = OBJ_FIELD(global_symbol_table, i);
        if (!is_object_ptr(existing_symbol))
        {
            continue;
        }

        uint64_t *symbol_obj = (uint64_t *)existing_symbol;
        if (OBJ_SIZE(symbol_obj) != text_size)
        {
            continue;
        }

        if (memcmp((const void *)&OBJ_FIELD(symbol_obj, 0), text, (size_t)text_size) == 0)
        {
            return existing_symbol;
        }
    }

    return tagged_nil();
}

uint64_t intern_cstring_symbol(uint64_t *om, const char *text)
{
    if (global_symbol_table == NULL || global_symbol_class == NULL || text == NULL)
    {
        return tagged_nil();
    }

    uint64_t existing = lookup_cstring_symbol(text);
    if (existing != tagged_nil())
    {
        return existing;
    }

    uint64_t text_size = (uint64_t)strlen(text);
    uint64_t table_size = OBJ_SIZE(global_symbol_table);

    uint64_t *symbol_obj = om_alloc(om, (uint64_t)global_symbol_class, FORMAT_BYTES, text_size);
    if (symbol_obj == NULL)
    {
        return tagged_nil();
    }

    if (text_size > 0)
    {
        memcpy((void *)&OBJ_FIELD(symbol_obj, 0), text, (size_t)text_size);
    }

    for (uint64_t i = 0; i < table_size; i++)
    {
        if (OBJ_FIELD(global_symbol_table, i) == tagged_nil())
        {
            OBJ_FIELD(global_symbol_table, i) = (uint64_t)symbol_obj;
            return (uint64_t)symbol_obj;
        }
    }

    return tagged_nil();
}

// PRIM_STRING_EQ: Compares two strings byte by byte
// receiver: tagged object pointer to String
// arg: tagged object pointer to String
uint64_t prim_string_eq(uint64_t receiver, uint64_t arg) {
    // Both must be object pointers
    if (!is_object_ptr(receiver) || !is_object_ptr(arg)) {
        return tagged_false(); // Or error, depending on strictness
    }

    uint8_t *recv_data, *arg_data;
    uint64_t recv_size, arg_size;

    get_byte_obj_data(receiver, &recv_data, &recv_size);
    get_byte_obj_data(arg, &arg_data, &arg_size);

    if (recv_size != arg_size) {
        return tagged_false();
    }

    if (memcmp(recv_data, arg_data, recv_size) == 0) {
        return tagged_true();
    } else {
        return tagged_false();
    }
}

// PRIM_STRING_HASH: Calculates a hash for a string (FNV-1a)
// receiver: tagged object pointer to String
uint64_t prim_string_hash_fnv(uint64_t receiver) {
    if (!is_object_ptr(receiver)) {
        return tag_smallint(0); // Or error
    }

    uint8_t *data;
    uint64_t size;
    get_byte_obj_data(receiver, &data, &size);

    uint32_t hash = 0x811C9DC5; // FNV-1a 32-bit prime
    for (uint64_t i = 0; i < size; i++) {
        hash ^= data[i];
        hash *= 0x01000193; // FNV-1a 32-bit basis
    }
    return tag_smallint(hash);
}

// PRIM_STRING_AS_SYMBOL: Interns a string into the global symbol table
// receiver: tagged object pointer to String
uint64_t prim_string_as_symbol(uint64_t receiver) {
    if (!is_object_ptr(receiver)) {
        return tagged_nil(); // Or error
    }

    if (global_symbol_table == NULL || global_symbol_class == NULL) {
        return tagged_nil();
    }

    uint8_t *recv_data;
    uint64_t recv_size;
    get_byte_obj_data(receiver, &recv_data, &recv_size);

    uint64_t table_size = OBJ_SIZE(global_symbol_table);
    for (uint64_t i = 0; i < table_size; i++) {
        uint64_t existing_symbol = OBJ_FIELD(global_symbol_table, i);
        if (!is_object_ptr(existing_symbol)) {
            continue;
        }

        uint64_t *symbol_obj = (uint64_t *)existing_symbol;
        if (OBJ_SIZE(symbol_obj) != recv_size) {
            continue;
        }

        if (memcmp((const void *)&OBJ_FIELD(symbol_obj, 0), recv_data, (size_t)recv_size) == 0) {
            return existing_symbol;
        }
    }

    for (uint64_t i = 0; i < table_size; i++) {
        if (OBJ_FIELD(global_symbol_table, i) == tagged_nil()) {
            OBJ_CLASS((uint64_t *)receiver) = (uint64_t)global_symbol_class;
            OBJ_FIELD(global_symbol_table, i) = receiver;
            return receiver;
        }
    }

    return tagged_nil();
}

// PRIM_SYMBOL_EQ: Compares two symbols for identity (pointer equality)
// receiver: tagged object pointer to Symbol
// arg: tagged object pointer to Symbol
uint64_t prim_symbol_eq(uint64_t receiver, uint64_t arg) {
    // Symbols are interned, so identity equality is sufficient
    if (receiver == arg) {
        return tagged_true();
    } else {
        return tagged_false();
    }
}

static uint64_t context_ip_for_frame(uint64_t *fp)
{
    uint64_t saved_ip = fp[FRAME_SAVED_IP];
    uint64_t *method = (uint64_t *)fp[FRAME_METHOD];
    if (method == NULL || !is_object_ptr((uint64_t)method))
    {
        return tagged_nil();
    }

    uint64_t bytecodes_oop = OBJ_FIELD(method, CM_BYTECODES);
    if (!is_object_ptr(bytecodes_oop))
    {
        return tagged_nil();
    }

    uint8_t *base = (uint8_t *)&OBJ_FIELD((uint64_t *)bytecodes_oop, 0);
    uint64_t size = OBJ_SIZE((uint64_t *)bytecodes_oop);
    uint8_t *ip = (uint8_t *)saved_ip;
    if (ip < base || ip > base + size)
    {
        return tagged_nil();
    }

    return tag_smallint((int64_t)(ip - base));
}

uint64_t *ensure_frame_context(uint64_t *fp, uint64_t *om, uint64_t context_class)
{
    uint64_t closure_oop = 0;

    if (fp == NULL || fp == (uint64_t *)0xCAFE)
    {
        return NULL;
    }

    if ((fp[FRAME_FLAGS] & FRAME_FLAGS_HAS_CONTEXT_MASK) != 0)
    {
        return (uint64_t *)fp[FRAME_CONTEXT];
    }

    if ((fp[FRAME_FLAGS] & FRAME_FLAGS_BLOCK_CLOSURE_MASK) != 0)
    {
        closure_oop = fp[FRAME_CONTEXT];
    }

    uint64_t num_args = (fp[FRAME_FLAGS] >> 8) & 0xFF;
    uint64_t *method = (uint64_t *)fp[FRAME_METHOD];
    uint64_t num_temps = 0;
    if (method != NULL && is_object_ptr((uint64_t)method))
    {
        num_temps = (uint64_t)untag_smallint(OBJ_FIELD(method, CM_NUM_TEMPS));
    }

    uint64_t field_count = CONTEXT_VAR_BASE + num_args + num_temps;
    uint64_t *context = om_alloc(om, context_class, FORMAT_FIELDS, field_count);
    if (context == NULL)
    {
        return NULL;
    }

    uint64_t *caller_fp = (uint64_t *)fp[FRAME_SAVED_FP];
    uint64_t *sender = ensure_frame_context(caller_fp, om, context_class);
    OBJ_FIELD(context, CONTEXT_SENDER) = sender == NULL ? tagged_nil() : (uint64_t)sender;
    OBJ_FIELD(context, CONTEXT_IP) = context_ip_for_frame(fp);
    OBJ_FIELD(context, CONTEXT_METHOD) = (uint64_t)method;
    OBJ_FIELD(context, CONTEXT_RECEIVER) = fp[FRAME_RECEIVER];
    OBJ_FIELD(context, CONTEXT_HOME) =
        closure_oop == 0 ? tagged_nil() : OBJ_FIELD((uint64_t *)closure_oop, BLOCK_HOME_CONTEXT);
    OBJ_FIELD(context, CONTEXT_CLOSURE) = closure_oop == 0 ? tagged_nil() : closure_oop;
    OBJ_FIELD(context, CONTEXT_FLAGS) = tag_smallint((int64_t)fp[FRAME_FLAGS]);
    OBJ_FIELD(context, CONTEXT_NUM_ARGS) = tag_smallint((int64_t)num_args);
    OBJ_FIELD(context, CONTEXT_NUM_TEMPS) = tag_smallint((int64_t)num_temps);

    for (uint64_t i = 0; i < num_args; i++)
    {
        OBJ_FIELD(context, CONTEXT_VAR_BASE + i) = frame_arg(fp, i);
    }
    for (uint64_t i = 0; i < num_temps; i++)
    {
        OBJ_FIELD(context, CONTEXT_VAR_BASE + num_args + i) = frame_temp(fp, i);
    }

    fp[FRAME_CONTEXT] = (uint64_t)context;
    fp[FRAME_FLAGS] &= ~FRAME_FLAGS_BLOCK_CLOSURE_MASK;
    fp[FRAME_FLAGS] |= FRAME_FLAGS_HAS_CONTEXT_MASK;
    return context;
}

uint64_t *ensure_frame_context_global(uint64_t *fp, uint64_t *om)
{
    if (global_context_class == NULL)
    {
        return NULL;
    }
    return ensure_frame_context(fp, om, (uint64_t)global_context_class);
}
