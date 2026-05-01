#include "primitives.h"
#include <errno.h>
#include <fcntl.h>
#include <string.h> // For memcmp
#include <sys/stat.h>
#include <unistd.h>

uint64_t *global_symbol_table; // Define global symbol table
uint64_t *global_symbol_class;
uint64_t *global_context_class;
uint64_t *global_smalltalk_dictionary;

static const char *DEFAULT_DURABLE_TXN_LOG_PATH = "/tmp/arlo_transactions.log";
static char durable_txn_log_path_buffer[1024];
static const char *durable_txn_log_path_override = NULL;
static const uint64_t DURABLE_TXN_FRAME_MAGIC = UINT64_C(0x41524c4f54584e31);

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

Oop cannot_return_selector_oop(void)
{
    return selector_token_from_cstring("cannotReturn:");
}

// Helper to get raw byte pointer and size from a FORMAT_BYTES object
static inline void get_byte_obj_data(uint64_t obj_ptr, uint8_t **data, uint64_t *size) {
    uint64_t *obj = (uint64_t *)obj_ptr;
    *data = (uint8_t *)&OBJ_FIELD(obj, 0);
    *size = OBJ_SIZE(obj);
}

static int copy_oop_bytes_to_cstring(Oop oop, char *buffer, uint64_t buffer_size)
{
    if (!is_object_ptr(oop) || buffer == NULL || buffer_size == 0)
    {
        return 0;
    }

    uint64_t *object = (uint64_t *)oop;
    if (OBJ_FORMAT(object) != FORMAT_BYTES || OBJ_SIZE(object) >= buffer_size)
    {
        return 0;
    }

    if (OBJ_SIZE(object) > 0)
    {
        memcpy(buffer, (const void *)&OBJ_FIELD(object, 0), (size_t)OBJ_SIZE(object));
    }
    buffer[OBJ_SIZE(object)] = '\0';
    return 1;
}

extern void om_mark_object_dirty(Om om, ObjPtr object);
extern void om_mark_field_dirty(Om om, ObjPtr object, uint64_t field_index);
extern void om_mark_byte_dirty(Om om, ObjPtr object, uint64_t byte_index);
extern Om om_registered_for_address(uint64_t address);

const char *txn_durable_log_path(void)
{
    return durable_txn_log_path_override != NULL
               ? durable_txn_log_path_override
               : DEFAULT_DURABLE_TXN_LOG_PATH;
}

void txn_set_durable_log_path(const char *path)
{
    if (path == NULL || path[0] == '\0')
    {
        durable_txn_log_path_override = NULL;
        durable_txn_log_path_buffer[0] = '\0';
        return;
    }

    size_t path_size = strlen(path);
    if (path_size + 1 > sizeof(durable_txn_log_path_buffer))
    {
        return;
    }

    memcpy(durable_txn_log_path_buffer, path, path_size + 1);
    durable_txn_log_path_override = durable_txn_log_path_buffer;
}

static int write_all(int fd, const void *buffer, size_t size)
{
    const uint8_t *bytes = (const uint8_t *)buffer;
    size_t offset = 0;

    while (offset < size)
    {
        ssize_t written = write(fd, bytes + offset, size - offset);
        if (written < 0 && errno == EINTR)
        {
            continue;
        }
        if (written <= 0)
        {
            return 0;
        }
        offset += (size_t)written;
    }

    return 1;
}

static uint64_t encode_durable_log_value(Oop value, uint64_t heap_start, uint64_t heap_limit)
{
    if (is_object_ptr(value) &&
        value >= heap_start &&
        value < heap_limit)
    {
        return (value - heap_start) + 8;
    }
    return value;
}

static uint64_t decode_durable_log_value(uint64_t value, uint64_t heap_start, uint64_t heap_used)
{
    if ((value & TAG_MASK) == TAG_OBJECT &&
        value >= 8 &&
        value <= heap_used)
    {
        return heap_start + (value - 8);
    }
    return value;
}

static uint64_t durable_txn_checksum(const uint64_t *words, uint64_t count)
{
    uint64_t hash = UINT64_C(1469598103934665603);

    for (uint64_t index = 0; index < count; index++)
    {
        hash ^= words[index];
        hash *= UINT64_C(1099511628211);
    }

    return hash;
}

int txn_log_append_fsync(const Oop *log, uint64_t heap_start, uint64_t heap_limit)
{
    int fd;
    uint64_t count;
    uint64_t *body;
    uint64_t header[3];

    if (log == NULL)
    {
        return 0;
    }

    fd = open(txn_durable_log_path(), O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd < 0)
    {
        return 0;
    }

    count = log[0];
    body = (uint64_t *)malloc((size_t)(count * 3 * sizeof(uint64_t)));
    if (body == NULL && count != 0)
    {
        close(fd);
        return 0;
    }

    for (uint64_t entry = 0; entry < count; entry++)
    {
        uint64_t object = log[1 + (entry * 3)];
        uint64_t field_index = log[2 + (entry * 3)];
        uint64_t value = log[3 + (entry * 3)];

        if (!is_object_ptr(object) || object < heap_start || object >= heap_limit)
        {
            free(body);
            close(fd);
            return 0;
        }

        body[entry * 3] = object - heap_start;
        body[(entry * 3) + 1] = field_index;
        body[(entry * 3) + 2] = encode_durable_log_value(value, heap_start, heap_limit);
    }

    header[0] = DURABLE_TXN_FRAME_MAGIC;
    header[1] = count;
    header[2] = durable_txn_checksum(body, count * 3);

    if (!write_all(fd, header, sizeof(header)))
    {
        free(body);
        close(fd);
        return 0;
    }
    if (count != 0 && !write_all(fd, body, (size_t)(count * 3 * sizeof(uint64_t))))
    {
        free(body);
        close(fd);
        return 0;
    }

    if (fsync(fd) != 0)
    {
        free(body);
        close(fd);
        return 0;
    }

    free(body);
    close(fd);
    return 1;
}

int txn_log_replay(uint64_t heap_start, uint64_t heap_used)
{
    FILE *file;
    uint64_t header[3];
    uint64_t *body = NULL;

    file = fopen(txn_durable_log_path(), "rb");
    if (file == NULL)
    {
        return errno == ENOENT ? 1 : 0;
    }

    for (;;)
    {
        size_t read_header = fread(header, sizeof(uint64_t), 3, file);
        uint64_t count;

        if (read_header == 0)
        {
            fclose(file);
            return 1;
        }
        if (read_header != 3)
        {
            fclose(file);
            return ferror(file) ? 0 : 1;
        }
        if (header[0] != DURABLE_TXN_FRAME_MAGIC)
        {
            fclose(file);
            return 0;
        }

        count = header[1];
        if (count > UINT64_MAX / (3 * sizeof(uint64_t)))
        {
            fclose(file);
            return 0;
        }

        if (count != 0)
        {
            body = (uint64_t *)malloc((size_t)(count * 3 * sizeof(uint64_t)));
            if (body == NULL)
            {
                fclose(file);
                return 0;
            }
            if (fread(body, sizeof(uint64_t), (size_t)(count * 3), file) != (size_t)(count * 3))
            {
                free(body);
                fclose(file);
                return ferror(file) ? 0 : 1;
            }
            if (durable_txn_checksum(body, count * 3) != header[2])
            {
                free(body);
                fclose(file);
                return 1;
            }
        }

        for (uint64_t entry = 0; entry < count; entry++)
        {
            uint64_t object_offset;
            uint64_t field_index;
            uint64_t value;
            uint64_t *object;

            object_offset = body[entry * 3];
            field_index = body[(entry * 3) + 1];
            value = decode_durable_log_value(body[(entry * 3) + 2], heap_start, heap_used);

            if (object_offset >= heap_used)
            {
                free(body);
                fclose(file);
                return 0;
            }

            object = (uint64_t *)(heap_start + object_offset);
            if ((field_index >> 63) != 0)
            {
                uint64_t byte_index = field_index & ~(UINT64_C(1) << 63);
                int64_t byte_value = untag_smallint(value);
                ((uint8_t *)&OBJ_FIELD(object, 0))[byte_index] = (uint8_t)byte_value;
                {
                    Om om = om_registered_for_address((uint64_t)object);
                    if (om != NULL)
                    {
                        om_mark_byte_dirty(om, object, byte_index);
                    }
                }
            }
            else
            {
                OBJ_FIELD(object, field_index) = value;
                Om om = om_registered_for_address((uint64_t)object);
                if (om != NULL)
                {
                    om_mark_field_dirty(om, object, field_index);
                }
            }
        }
        free(body);
        body = NULL;
    }
}

int txn_durable_log_clear(void)
{
    if (unlink(txn_durable_log_path()) == 0)
    {
        return 1;
    }
    return errno == ENOENT ? 1 : 0;
}

static uint64_t method_dict_size(Oop method_dict_oop)
{
    if (!is_object_ptr(method_dict_oop))
    {
        return 0;
    }
    return OBJ_SIZE((ObjPtr)method_dict_oop);
}

static Oop method_dict_lookup_local(Oop method_dict_oop, Oop selector)
{
    if (!is_object_ptr(method_dict_oop))
    {
        return 0;
    }

    ObjPtr method_dict = (ObjPtr)method_dict_oop;
    for (uint64_t index = 0; index < OBJ_SIZE(method_dict); index += 2)
    {
        if (OBJ_FIELD(method_dict, index) == selector)
        {
            return OBJ_FIELD(method_dict, index + 1);
        }
    }

    return 0;
}

static int smalltalk_dictionary_bindings(ObjPtr *associations_out, uint64_t *tally_out)
{
    Oop associations_oop;
    Oop tally_oop;

    if (global_smalltalk_dictionary == NULL)
    {
        return 0;
    }

    associations_oop = OBJ_FIELD(global_smalltalk_dictionary, 0);
    tally_oop = OBJ_FIELD(global_smalltalk_dictionary, 1);
    if (!is_object_ptr(associations_oop) || !is_smallint(tally_oop))
    {
        return 0;
    }

    *associations_out = (ObjPtr)associations_oop;
    *tally_out = (uint64_t)untag_smallint(tally_oop);
    return 1;
}

static Oop smalltalk_find_association_by_key(Oop key)
{
    ObjPtr associations;
    uint64_t tally;

    if (!is_object_ptr(key) || !smalltalk_dictionary_bindings(&associations, &tally))
    {
        return tagged_nil();
    }

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
            return OBJ_FIELD(assoc, 1);
        }
    }

    return tagged_nil();
}

static Oop smalltalk_find_association_by_value(Oop value)
{
    ObjPtr associations;
    uint64_t tally;

    if (!is_object_ptr(value) || !smalltalk_dictionary_bindings(&associations, &tally))
    {
        return tagged_nil();
    }

    for (uint64_t index = 0; index < tally; index++)
    {
        Oop assoc_oop = OBJ_FIELD(associations, index);
        if (!is_object_ptr(assoc_oop))
        {
            continue;
        }
        ObjPtr assoc = (ObjPtr)assoc_oop;
        if (OBJ_FIELD(assoc, 1) == value)
        {
            return OBJ_FIELD(assoc, 0);
        }
    }

    return tagged_nil();
}

static Oop smalltalk_lookup_global_value(const char *name)
{
    Oop key;

    if (name == NULL)
    {
        return tagged_nil();
    }

    key = lookup_cstring_symbol(name);
    return key == tagged_nil() ? tagged_nil() : smalltalk_find_association_by_key(key);
}

static Oop class_name_symbol_for_class(Oop class_oop)
{
    return smalltalk_find_association_by_value(class_oop);
}

Oop prim_class_superclass(Oop receiver)
{
    if (!is_object_ptr(receiver))
    {
        return tagged_nil();
    }
    return OBJ_FIELD((ObjPtr)receiver, CLASS_SUPERCLASS);
}

Oop prim_class_name(Oop receiver)
{
    return class_name_symbol_for_class(receiver);
}

Oop prim_class_includes_selector(Oop receiver, Oop selector)
{
    if (!is_object_ptr(receiver) || !is_object_ptr(selector))
    {
        return tagged_false();
    }

    return method_dict_lookup_local(OBJ_FIELD((ObjPtr)receiver, CLASS_METHOD_DICT), selector) != 0
               ? tagged_true()
               : tagged_false();
}

Oop prim_smalltalk_globals(void)
{
    return global_smalltalk_dictionary == NULL ? tagged_nil() : (Oop)global_smalltalk_dictionary;
}

Oop prim_method_source_for_class_selector(Oop class_name, Oop selector, Om om)
{
    char class_name_text[64];
    Oop klass;
    Oop method;
    Oop source;

    (void)om;
    if (!copy_oop_bytes_to_cstring(class_name, class_name_text, sizeof(class_name_text)))
    {
        return tagged_nil();
    }
    klass = smalltalk_lookup_global_value(class_name_text);
    if (!is_object_ptr(klass))
    {
        return tagged_nil();
    }
    method = method_dict_lookup_local(OBJ_FIELD((ObjPtr)klass, CLASS_METHOD_DICT), selector);
    if (!is_object_ptr(method))
    {
        return tagged_nil();
    }
    source = OBJ_FIELD((ObjPtr)method, CM_SOURCE);
    return is_object_ptr(source) ? source : tagged_nil();
}

Oop prim_read_fd_count(Oop fd, Oop count, Om om)
{
    ObjPtr string_class;
    ObjPtr string;
    int read_fd;
    int64_t requested;
    ssize_t bytes_read;

    if (!is_smallint(fd) || !is_smallint(count))
    {
        return tagged_nil();
    }

    read_fd = (int)untag_smallint(fd);
    requested = untag_smallint(count);
    if (requested < 0)
    {
        return tagged_nil();
    }

    string_class = (ObjPtr)smalltalk_lookup_global_value("String");
    if (string_class == NULL || !is_object_ptr((Oop)string_class))
    {
        return tagged_nil();
    }

    string = om_alloc(om, (Oop)string_class, FORMAT_BYTES, (uint64_t)requested);
    if (string == NULL)
    {
        return tagged_nil();
    }
    if (requested == 0)
    {
        return (Oop)string;
    }

    for (;;)
    {
        bytes_read = read(read_fd, (void *)&OBJ_FIELD(string, 0), (size_t)requested);
        if (bytes_read < 0 && errno == EINTR)
        {
            continue;
        }
        break;
    }

    if (bytes_read <= 0)
    {
        return tagged_nil();
    }

    OBJ_SIZE(string) = (uint64_t)bytes_read;
    return (Oop)string;
}

Oop prim_write_fd_string(Oop fd, Oop string)
{
    const uint8_t *bytes;
    uint64_t size;
    size_t offset;
    ssize_t written;
    int write_fd;

    if (!is_smallint(fd) || !is_object_ptr(string))
    {
        return tagged_nil();
    }
    if (OBJ_FORMAT((ObjPtr)string) != FORMAT_BYTES)
    {
        return tagged_nil();
    }

    write_fd = (int)untag_smallint(fd);
    bytes = (const uint8_t *)&OBJ_FIELD((ObjPtr)string, 0);
    size = OBJ_SIZE((ObjPtr)string);
    offset = 0;
    while (offset < size)
    {
        written = write(write_fd, bytes + offset, size - offset);
        if (written < 0 && errno == EINTR)
        {
            continue;
        }
        if (written <= 0)
        {
            return tagged_nil();
        }
        offset += (size_t)written;
    }
    return tag_smallint((int64_t)size);
}

Oop lookup_cstring_symbol(const char *text)
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

Oop intern_cstring_symbol(Om om, const char *text)
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
Oop prim_string_eq(Oop receiver, Oop arg) {
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
Oop prim_string_hash_fnv(Oop receiver) {
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
Oop prim_string_as_symbol(Oop receiver) {
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
Oop prim_symbol_eq(Oop receiver, Oop arg) {
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

ObjPtr ensure_frame_context(ObjPtr fp, Om om, Oop context_class)
{
    return ensure_frame_context_with_sp(fp, NULL, om, context_class);
}

ObjPtr ensure_frame_context_with_sp(ObjPtr fp, Oop *sp, Om om, Oop context_class)
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
    uint64_t num_temps = is_object_ptr((uint64_t)method) && is_smallint(OBJ_FIELD(method, CM_NUM_TEMPS))
                             ? (uint64_t)untag_smallint(OBJ_FIELD(method, CM_NUM_TEMPS))
                             : 0;
    Oop *base_sp = num_temps == 0
                       ? fp + FRAME_RECEIVER
                       : fp - (FP_TEMP_BASE_WORDS + (num_temps - 1));
    Oop *effective_sp = sp == NULL || sp > base_sp ? base_sp : sp;
    uint64_t stack_depth = (uint64_t)(base_sp - effective_sp);
    uint64_t field_count = CONTEXT_VAR_BASE + num_args + num_temps + stack_depth;
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
    OBJ_FIELD(context, CONTEXT_STACK_SIZE) = tag_smallint((int64_t)stack_depth);

    for (uint64_t i = 0; i < num_args; i++)
    {
        OBJ_FIELD(context, CONTEXT_VAR_BASE + i) = frame_arg(fp, i);
    }
    for (uint64_t i = 0; i < num_temps; i++)
    {
        OBJ_FIELD(context, CONTEXT_VAR_BASE + num_args + i) = frame_temp(fp, i);
    }
    for (uint64_t i = 0; i < stack_depth; i++)
    {
        OBJ_FIELD(context, CONTEXT_VAR_BASE + num_args + num_temps + i) = effective_sp[i];
    }

    fp[FRAME_CONTEXT] = (uint64_t)context;
    fp[FRAME_FLAGS] &= ~FRAME_FLAGS_BLOCK_CLOSURE_MASK;
    fp[FRAME_FLAGS] |= FRAME_FLAGS_HAS_CONTEXT_MASK;
    return context;
}

ObjPtr ensure_frame_context_global(ObjPtr fp, Om om)
{
    if (global_context_class == NULL)
    {
        return NULL;
    }
    return ensure_frame_context(fp, om, (uint64_t)global_context_class);
}
