#include "bootstrap_compiler.h"
#include "primitives.h"
#include "smalltalk_tool_support.h"
#include "smalltalk_world.h"
#include "test_defs.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define LSP_HEAP_SIZE (64 * 1024 * 1024)
#define MAX_OPEN_DOCUMENTS 128
#define MAX_MATERIALIZED_METHODS 1024
#define MAX_QUERY_RESULTS 256
#define LIVE_IMAGE_ROOT "/tmp/armlivingobjects-live-image"

static uint8_t heap[LSP_HEAP_SIZE] __attribute__((aligned(8)));

extern Oop class_lookup(ObjPtr klass, Oop selector);

typedef struct
{
    char *data;
    size_t length;
    size_t capacity;
} StringBuffer;

typedef struct
{
    char *uri;
    char *text;
} OpenDocument;

typedef struct
{
    char uri[PATH_MAX];
    char class_name[64];
    char selector[128];
    int class_side;
} MaterializedMethod;

typedef struct
{
    char class_name[64];
    char selector[128];
    int class_side;
    char uri[PATH_MAX];
    int symbol_kind;
} QueryResult;

typedef struct
{
    SmalltalkWorld world;
    OpenDocument open_documents[MAX_OPEN_DOCUMENTS];
    int open_document_count;
    MaterializedMethod materialized[MAX_MATERIALIZED_METHODS];
    int materialized_count;
    int shutdown_requested;
} LspServer;

static void sb_init(StringBuffer *buffer)
{
    buffer->data = NULL;
    buffer->length = 0;
    buffer->capacity = 0;
}

static void sb_free(StringBuffer *buffer)
{
    free(buffer->data);
    buffer->data = NULL;
    buffer->length = 0;
    buffer->capacity = 0;
}

static int sb_reserve(StringBuffer *buffer, size_t needed)
{
    char *grown;
    size_t capacity;

    if (needed <= buffer->capacity)
    {
        return 1;
    }

    capacity = buffer->capacity == 0 ? 256 : buffer->capacity;
    while (capacity < needed)
    {
        capacity *= 2;
    }

    grown = realloc(buffer->data, capacity);
    if (grown == NULL)
    {
        return 0;
    }

    buffer->data = grown;
    buffer->capacity = capacity;
    return 1;
}

static int sb_append_n(StringBuffer *buffer, const char *text, size_t length)
{
    if (!sb_reserve(buffer, buffer->length + length + 1))
    {
        return 0;
    }

    memcpy(buffer->data + buffer->length, text, length);
    buffer->length += length;
    buffer->data[buffer->length] = '\0';
    return 1;
}

static int sb_append(StringBuffer *buffer, const char *text)
{
    return sb_append_n(buffer, text, strlen(text));
}

static int sb_appendf(StringBuffer *buffer, const char *format, ...)
{
    va_list args;
    va_list copy;
    int needed;

    va_start(args, format);
    va_copy(copy, args);
    needed = vsnprintf(NULL, 0, format, copy);
    va_end(copy);
    if (needed < 0)
    {
        va_end(args);
        return 0;
    }
    if (!sb_reserve(buffer, buffer->length + (size_t)needed + 1))
    {
        va_end(args);
        return 0;
    }
    vsnprintf(buffer->data + buffer->length, buffer->capacity - buffer->length, format, args);
    va_end(args);
    buffer->length += (size_t)needed;
    return 1;
}

static int sb_append_json_string(StringBuffer *buffer, const char *text)
{
    const unsigned char *cursor = (const unsigned char *)text;

    if (!sb_append(buffer, "\""))
    {
        return 0;
    }

    while (*cursor != '\0')
    {
        unsigned char ch = *cursor++;
        switch (ch)
        {
            case '\\':
                if (!sb_append(buffer, "\\\\"))
                {
                    return 0;
                }
                break;
            case '"':
                if (!sb_append(buffer, "\\\""))
                {
                    return 0;
                }
                break;
            case '\n':
                if (!sb_append(buffer, "\\n"))
                {
                    return 0;
                }
                break;
            case '\r':
                if (!sb_append(buffer, "\\r"))
                {
                    return 0;
                }
                break;
            case '\t':
                if (!sb_append(buffer, "\\t"))
                {
                    return 0;
                }
                break;
            default:
                if (ch < 0x20)
                {
                    if (!sb_appendf(buffer, "\\u%04x", (unsigned)ch))
                    {
                        return 0;
                    }
                }
                else
                {
                    char literal[2];
                    literal[0] = (char)ch;
                    literal[1] = '\0';
                    if (!sb_append(buffer, literal))
                    {
                        return 0;
                    }
                }
                break;
        }
    }

    return sb_append(buffer, "\"");
}

static int copy_oop_bytes_to_cstring(Oop oop, char *buffer, size_t capacity)
{
    ObjPtr object;
    uint64_t size;

    if (!is_object_ptr(oop) || buffer == NULL || capacity == 0)
    {
        return 0;
    }

    object = (ObjPtr)oop;
    if (OBJ_FORMAT(object) != FORMAT_BYTES)
    {
        return 0;
    }

    size = OBJ_SIZE(object);
    if (size >= capacity)
    {
        return 0;
    }

    if (size > 0)
    {
        memcpy(buffer, &OBJ_FIELD(object, 0), (size_t)size);
    }
    buffer[size] = '\0';
    return 1;
}

static char *dup_oop_bytes(Oop oop)
{
    ObjPtr object;
    uint64_t size;
    char *text;

    if (!is_object_ptr(oop))
    {
        return NULL;
    }
    object = (ObjPtr)oop;
    if (OBJ_FORMAT(object) != FORMAT_BYTES)
    {
        return NULL;
    }

    size = OBJ_SIZE(object);
    text = malloc((size_t)size + 1);
    if (text == NULL)
    {
        return NULL;
    }
    if (size > 0)
    {
        memcpy(text, &OBJ_FIELD(object, 0), (size_t)size);
    }
    text[size] = '\0';
    return text;
}

static ObjPtr lookup_class_by_name(LspServer *server, const char *name)
{
    Oop associations_oop = OBJ_FIELD(server->world.smalltalk_dict, 0);
    Oop tally_oop = OBJ_FIELD(server->world.smalltalk_dict, 1);
    ObjPtr associations;
    int64_t tally;
    int64_t index;

    if (!is_object_ptr(associations_oop) || !is_smallint(tally_oop))
    {
        return NULL;
    }

    associations = (ObjPtr)associations_oop;
    tally = untag_smallint(tally_oop);
    for (index = 0; index < tally; index++)
    {
        Oop assoc_oop = OBJ_FIELD(associations, (uint64_t)index);
        char key_buffer[64];
        if (!is_object_ptr(assoc_oop))
        {
            continue;
        }
        if (!copy_oop_bytes_to_cstring(OBJ_FIELD((ObjPtr)assoc_oop, 0), key_buffer, sizeof(key_buffer)))
        {
            continue;
        }
        if (strcmp(key_buffer, name) == 0 && is_object_ptr(OBJ_FIELD((ObjPtr)assoc_oop, 1)))
        {
            return (ObjPtr)OBJ_FIELD((ObjPtr)assoc_oop, 1);
        }
    }

    return NULL;
}

static ObjPtr local_method_lookup(ObjPtr klass, Oop selector_oop)
{
    Oop method_dict_oop;
    uint64_t size;
    uint64_t index;
    ObjPtr method_dict;

    if (klass == NULL || (Oop)klass == tagged_nil())
    {
        return NULL;
    }

    method_dict_oop = OBJ_FIELD(klass, CLASS_METHOD_DICT);
    if (!is_object_ptr(method_dict_oop))
    {
        return NULL;
    }

    method_dict = (ObjPtr)method_dict_oop;
    size = OBJ_SIZE(method_dict);
    for (index = 0; index + 1 < size; index += 2)
    {
        if (OBJ_FIELD(method_dict, index) == selector_oop && is_object_ptr(OBJ_FIELD(method_dict, index + 1)))
        {
            return (ObjPtr)OBJ_FIELD(method_dict, index + 1);
        }
    }

    return NULL;
}

static ObjPtr find_method_owner(ObjPtr klass, Oop selector_oop, ObjPtr *owner_out)
{
    while (klass != NULL && (Oop)klass != tagged_nil())
    {
        ObjPtr method = local_method_lookup(klass, selector_oop);
        if (method != NULL)
        {
            if (owner_out != NULL)
            {
                *owner_out = klass;
            }
            return method;
        }
        klass = (ObjPtr)OBJ_FIELD(klass, CLASS_SUPERCLASS);
    }

    if (owner_out != NULL)
    {
        *owner_out = NULL;
    }
    return NULL;
}

static int oop_equals_cstring_symbol(Oop symbol_oop, const char *text)
{
    char buffer[128];
    return copy_oop_bytes_to_cstring(symbol_oop, buffer, sizeof(buffer)) && strcmp(buffer, text) == 0;
}

static int copy_registered_class_name(LspServer *server, ObjPtr target, int class_side,
                                      char *buffer, size_t capacity)
{
    Oop associations_oop = OBJ_FIELD(server->world.smalltalk_dict, 0);
    Oop tally_oop = OBJ_FIELD(server->world.smalltalk_dict, 1);
    ObjPtr associations;
    int64_t tally;
    int64_t index;

    if (!is_object_ptr(associations_oop) || !is_smallint(tally_oop))
    {
        return 0;
    }

    associations = (ObjPtr)associations_oop;
    tally = untag_smallint(tally_oop);
    for (index = 0; index < tally; index++)
    {
        Oop assoc_oop = OBJ_FIELD(associations, (uint64_t)index);
        ObjPtr klass;

        if (!is_object_ptr(assoc_oop))
        {
            continue;
        }
        klass = (ObjPtr)OBJ_FIELD((ObjPtr)assoc_oop, 1);
        if (!is_object_ptr((Oop)klass))
        {
            continue;
        }
        if ((!class_side && klass == target) ||
            (class_side && (ObjPtr)OBJ_CLASS(klass) == target))
        {
            return copy_oop_bytes_to_cstring(OBJ_FIELD((ObjPtr)assoc_oop, 0), buffer, capacity);
        }
    }

    return 0;
}

static int count_local_methods(ObjPtr klass)
{
    Oop method_dict_oop;
    ObjPtr method_dict;

    if (klass == NULL || (Oop)klass == tagged_nil())
    {
        return 0;
    }
    method_dict_oop = OBJ_FIELD(klass, CLASS_METHOD_DICT);
    if (!is_object_ptr(method_dict_oop))
    {
        return 0;
    }
    method_dict = (ObjPtr)method_dict_oop;
    return (int)(OBJ_SIZE(method_dict) / 2);
}

static int current_document_find(OpenDocument *documents, int count, const char *uri)
{
    int index;
    for (index = 0; index < count; index++)
    {
        if (documents[index].uri != NULL && strcmp(documents[index].uri, uri) == 0)
        {
            return index;
        }
    }
    return -1;
}

static int set_open_document(LspServer *server, const char *uri, const char *text)
{
    int index = current_document_find(server->open_documents, server->open_document_count, uri);
    char *uri_copy;
    char *text_copy;

    if (index < 0)
    {
        if (server->open_document_count >= MAX_OPEN_DOCUMENTS)
        {
            return 0;
        }
        index = server->open_document_count++;
        server->open_documents[index].uri = NULL;
        server->open_documents[index].text = NULL;
    }

    uri_copy = strdup(uri);
    text_copy = strdup(text);
    if (uri_copy == NULL || text_copy == NULL)
    {
        free(uri_copy);
        free(text_copy);
        return 0;
    }

    free(server->open_documents[index].uri);
    free(server->open_documents[index].text);
    server->open_documents[index].uri = uri_copy;
    server->open_documents[index].text = text_copy;
    return 1;
}

static void close_open_document(LspServer *server, const char *uri)
{
    int index = current_document_find(server->open_documents, server->open_document_count, uri);
    int move_index;

    if (index < 0)
    {
        return;
    }

    free(server->open_documents[index].uri);
    free(server->open_documents[index].text);
    for (move_index = index; move_index + 1 < server->open_document_count; move_index++)
    {
        server->open_documents[move_index] = server->open_documents[move_index + 1];
    }
    server->open_document_count--;
}

static int hex_value(int ch)
{
    if (ch >= '0' && ch <= '9')
    {
        return ch - '0';
    }
    if (ch >= 'a' && ch <= 'f')
    {
        return 10 + ch - 'a';
    }
    if (ch >= 'A' && ch <= 'F')
    {
        return 10 + ch - 'A';
    }
    return -1;
}

static char *uri_to_path(const char *uri)
{
    const char *cursor = uri;
    StringBuffer path;

    if (strncmp(uri, "file://", 7) != 0)
    {
        return NULL;
    }

    cursor += 7;
    sb_init(&path);
    while (*cursor != '\0')
    {
        if (*cursor == '%' && isxdigit((unsigned char)cursor[1]) && isxdigit((unsigned char)cursor[2]))
        {
            int hi = hex_value((unsigned char)cursor[1]);
            int lo = hex_value((unsigned char)cursor[2]);
            char decoded = (char)((hi << 4) | lo);
            if (!sb_append_n(&path, &decoded, 1))
            {
                sb_free(&path);
                return NULL;
            }
            cursor += 3;
        }
        else
        {
            if (!sb_append_n(&path, cursor, 1))
            {
                sb_free(&path);
                return NULL;
            }
            cursor++;
        }
    }

    return path.data;
}

static char *read_uri_text(LspServer *server, const char *uri)
{
    int index = current_document_find(server->open_documents, server->open_document_count, uri);
    char *path;
    FILE *file;
    long size;
    char *text;

    if (index >= 0)
    {
        return strdup(server->open_documents[index].text);
    }

    path = uri_to_path(uri);
    if (path == NULL)
    {
        return NULL;
    }

    file = fopen(path, "rb");
    free(path);
    if (file == NULL)
    {
        return NULL;
    }

    if (fseek(file, 0, SEEK_END) != 0)
    {
        fclose(file);
        return NULL;
    }
    size = ftell(file);
    if (size < 0 || fseek(file, 0, SEEK_SET) != 0)
    {
        fclose(file);
        return NULL;
    }

    text = malloc((size_t)size + 1);
    if (text == NULL)
    {
        fclose(file);
        return NULL;
    }

    if (size > 0 && fread(text, 1, (size_t)size, file) != (size_t)size)
    {
        free(text);
        fclose(file);
        return NULL;
    }
    fclose(file);
    text[size] = '\0';
    return text;
}

static int ensure_directory(const char *path)
{
    char buffer[PATH_MAX];
    size_t length;
    size_t index;

    length = strlen(path);
    if (length >= sizeof(buffer))
    {
        return 0;
    }
    memcpy(buffer, path, length + 1);

    for (index = 1; index < length; index++)
    {
        if (buffer[index] == '/')
        {
            buffer[index] = '\0';
            if (mkdir(buffer, 0777) != 0 && errno != EEXIST)
            {
                return 0;
            }
            buffer[index] = '/';
        }
    }

    if (mkdir(buffer, 0777) != 0 && errno != EEXIST)
    {
        return 0;
    }
    return 1;
}

static void sanitize_path_component(const char *input, char *output, size_t capacity)
{
    size_t out = 0;
    while (*input != '\0' && out + 5 < capacity)
    {
        unsigned char ch = (unsigned char)*input++;
        if (isalnum(ch) || ch == '_' || ch == '-')
        {
            output[out++] = (char)ch;
        }
        else
        {
            int written = snprintf(output + out, capacity - out, "_%02X", ch);
            if (written <= 0)
            {
                break;
            }
            out += (size_t)written;
        }
    }
    output[out] = '\0';
}

static MaterializedMethod *find_materialized(LspServer *server, const char *uri)
{
    int index;
    for (index = 0; index < server->materialized_count; index++)
    {
        if (strcmp(server->materialized[index].uri, uri) == 0)
        {
            return &server->materialized[index];
        }
    }
    return NULL;
}

static const char *materialize_method_source(LspServer *server, const char *class_name,
                                             const char *selector, int class_side,
                                             const char *source)
{
    char class_component[128];
    char selector_component[256];
    char directory[PATH_MAX];
    char path[PATH_MAX];
    FILE *file;
    int index;

    for (index = 0; index < server->materialized_count; index++)
    {
        MaterializedMethod *entry = &server->materialized[index];
        if (strcmp(entry->class_name, class_name) == 0 &&
            strcmp(entry->selector, selector) == 0 &&
            entry->class_side == class_side)
        {
            return entry->uri;
        }
    }

    sanitize_path_component(class_name, class_component, sizeof(class_component));
    sanitize_path_component(selector, selector_component, sizeof(selector_component));
    snprintf(directory, sizeof(directory), "%s/%s%s",
             LIVE_IMAGE_ROOT, class_component, class_side ? ".class" : "");
    snprintf(path, sizeof(path), "%s/%s.st", directory, selector_component);

    if (!ensure_directory(LIVE_IMAGE_ROOT) || !ensure_directory(directory))
    {
        return NULL;
    }

    file = fopen(path, "wb");
    if (file == NULL)
    {
        return NULL;
    }
    if (fwrite(source, 1, strlen(source), file) != strlen(source))
    {
        fclose(file);
        return NULL;
    }
    fclose(file);

    if (server->materialized_count >= MAX_MATERIALIZED_METHODS)
    {
        return NULL;
    }

    snprintf(server->materialized[server->materialized_count].uri,
             sizeof(server->materialized[server->materialized_count].uri),
             "file://%s", path);
    strncpy(server->materialized[server->materialized_count].class_name,
            class_name,
            sizeof(server->materialized[server->materialized_count].class_name) - 1);
    server->materialized[server->materialized_count].class_name[
        sizeof(server->materialized[server->materialized_count].class_name) - 1] = '\0';
    strncpy(server->materialized[server->materialized_count].selector,
            selector,
            sizeof(server->materialized[server->materialized_count].selector) - 1);
    server->materialized[server->materialized_count].selector[
        sizeof(server->materialized[server->materialized_count].selector) - 1] = '\0';
    server->materialized[server->materialized_count].class_side = class_side;
    server->materialized_count++;
    return server->materialized[server->materialized_count - 1].uri;
}

static char *materialize_class_summary(LspServer *server, const char *class_name)
{
    ObjPtr klass = lookup_class_by_name(server, class_name);
    ObjPtr metaclass;
    StringBuffer summary;
    char class_component[128];
    char path[PATH_MAX];
    char directory[PATH_MAX];
    char superclass_name[64];
    FILE *file;
    Oop method_dict_oop;
    ObjPtr method_dict;
    uint64_t index;
    Oop class_name_oop;

    if (klass == NULL)
    {
        return NULL;
    }

    sanitize_path_component(class_name, class_component, sizeof(class_component));
    snprintf(directory, sizeof(directory), "%s/%s", LIVE_IMAGE_ROOT, class_component);
    snprintf(path, sizeof(path), "%s/class-summary.st", directory);
    if (!ensure_directory(LIVE_IMAGE_ROOT) || !ensure_directory(directory))
    {
        return NULL;
    }

    sb_init(&summary);
    class_name_oop = prim_class_name((Oop)klass);
    if (!copy_oop_bytes_to_cstring(class_name_oop, superclass_name, sizeof(superclass_name)))
    {
        strcpy(superclass_name, class_name);
    }
    sb_appendf(&summary, "\"Live image class summary\"\n%s\n", superclass_name);
    if (is_object_ptr(OBJ_FIELD(klass, CLASS_SUPERCLASS)))
    {
        if (!copy_oop_bytes_to_cstring(prim_class_name(OBJ_FIELD(klass, CLASS_SUPERCLASS)),
                                       superclass_name, sizeof(superclass_name)))
        {
            strcpy(superclass_name, "nil");
        }
    }
    else
    {
        strcpy(superclass_name, "nil");
    }
    sb_appendf(&summary, "superclass: %s\n\n", superclass_name);
    sb_append(&summary, "\"Instance methods\"\n");
    method_dict_oop = OBJ_FIELD(klass, CLASS_METHOD_DICT);
    if (is_object_ptr(method_dict_oop))
    {
        method_dict = (ObjPtr)method_dict_oop;
        for (index = 0; index + 1 < OBJ_SIZE(method_dict); index += 2)
        {
            char selector[128];
            if (copy_oop_bytes_to_cstring(OBJ_FIELD(method_dict, index), selector, sizeof(selector)))
            {
                sb_appendf(&summary, "%s\n", selector);
            }
        }
    }

    metaclass = (ObjPtr)OBJ_CLASS(klass);
    sb_append(&summary, "\n\"Class methods\"\n");
    method_dict_oop = OBJ_FIELD(metaclass, CLASS_METHOD_DICT);
    if (is_object_ptr(method_dict_oop))
    {
        method_dict = (ObjPtr)method_dict_oop;
        for (index = 0; index + 1 < OBJ_SIZE(method_dict); index += 2)
        {
            char selector[128];
            if (copy_oop_bytes_to_cstring(OBJ_FIELD(method_dict, index), selector, sizeof(selector)))
            {
                sb_appendf(&summary, "%s\n", selector);
            }
        }
    }

    file = fopen(path, "wb");
    if (file == NULL)
    {
        sb_free(&summary);
        return NULL;
    }
    fwrite(summary.data, 1, summary.length, file);
    fclose(file);
    sb_free(&summary);
    return strdup(path);
}

static int is_binary_selector_char(int ch)
{
    const char *binary = "!%&*+,/<=>?@\\|-~";
    return strchr(binary, ch) != NULL;
}

static int offset_for_position(const char *text, int line, int character)
{
    int current_line = 0;
    int current_character = 0;
    int offset = 0;

    while (text[offset] != '\0')
    {
        if (current_line == line && current_character == character)
        {
            return offset;
        }
        if (text[offset] == '\n')
        {
            current_line++;
            current_character = 0;
        }
        else
        {
            current_character++;
        }
        offset++;
    }

    return offset;
}

static int extract_token_at_position(const char *text, int line, int character,
                                     char *token, size_t capacity)
{
    int offset = offset_for_position(text, line, character);
    int start;
    int end;
    int token_length;

    if (capacity == 0 || text[0] == '\0')
    {
        return 0;
    }

    if (text[offset] == '\0' && offset > 0)
    {
        offset--;
    }
    if (text[offset] == '\0')
    {
        return 0;
    }

    if (isalpha((unsigned char)text[offset]) || text[offset] == '#')
    {
        start = offset;
        if (text[start] == '#')
        {
            start++;
        }
        while (start > 0 && (isalnum((unsigned char)text[start - 1]) || text[start - 1] == '_'))
        {
            start--;
        }
        end = offset;
        while (isalnum((unsigned char)text[end]) || text[end] == '_')
        {
            end++;
        }
        while (text[end] == ':')
        {
            end++;
            while (isalnum((unsigned char)text[end]) || text[end] == '_')
            {
                end++;
            }
        }
    }
    else if (is_binary_selector_char((unsigned char)text[offset]))
    {
        start = offset;
        while (start > 0 && is_binary_selector_char((unsigned char)text[start - 1]))
        {
            start--;
        }
        end = offset;
        while (is_binary_selector_char((unsigned char)text[end]))
        {
            end++;
        }
    }
    else
    {
        return 0;
    }

    token_length = end - start;
    if (token_length <= 0 || (size_t)token_length >= capacity)
    {
        return 0;
    }

    memcpy(token, text + start, (size_t)token_length);
    token[token_length] = '\0';
    return 1;
}

static int extract_json_string_value(const char *json, const char *key, char **out)
{
    char pattern[64];
    const char *cursor;
    StringBuffer decoded;

    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    cursor = strstr(json, pattern);
    if (cursor == NULL)
    {
        return 0;
    }
    cursor += strlen(pattern);
    cursor = strchr(cursor, ':');
    if (cursor == NULL)
    {
        return 0;
    }
    cursor++;
    while (isspace((unsigned char)*cursor))
    {
        cursor++;
    }
    if (*cursor != '"')
    {
        return 0;
    }
    cursor++;

    sb_init(&decoded);
    while (*cursor != '\0' && *cursor != '"')
    {
        if (*cursor == '\\')
        {
            cursor++;
            switch (*cursor)
            {
                case '"':
                case '\\':
                case '/':
                    if (!sb_append_n(&decoded, cursor, 1))
                    {
                        sb_free(&decoded);
                        return 0;
                    }
                    cursor++;
                    break;
                case 'n':
                    if (!sb_append_n(&decoded, "\n", 1))
                    {
                        sb_free(&decoded);
                        return 0;
                    }
                    cursor++;
                    break;
                case 'r':
                    if (!sb_append_n(&decoded, "\r", 1))
                    {
                        sb_free(&decoded);
                        return 0;
                    }
                    cursor++;
                    break;
                case 't':
                    if (!sb_append_n(&decoded, "\t", 1))
                    {
                        sb_free(&decoded);
                        return 0;
                    }
                    cursor++;
                    break;
                case 'u':
                    if (isxdigit((unsigned char)cursor[1]) &&
                        isxdigit((unsigned char)cursor[2]) &&
                        isxdigit((unsigned char)cursor[3]) &&
                        isxdigit((unsigned char)cursor[4]))
                    {
                        char decoded_char = '?';
                        if (cursor[1] == '0' && cursor[2] == '0')
                        {
                            decoded_char = (char)((hex_value(cursor[3]) << 4) | hex_value(cursor[4]));
                        }
                        if (!sb_append_n(&decoded, &decoded_char, 1))
                        {
                            sb_free(&decoded);
                            return 0;
                        }
                        cursor += 5;
                    }
                    else
                    {
                        sb_free(&decoded);
                        return 0;
                    }
                    break;
                default:
                    sb_free(&decoded);
                    return 0;
            }
        }
        else
        {
            if (!sb_append_n(&decoded, cursor, 1))
            {
                sb_free(&decoded);
                return 0;
            }
            cursor++;
        }
    }

    if (*cursor != '"')
    {
        sb_free(&decoded);
        return 0;
    }

    *out = decoded.data;
    return 1;
}

static int extract_json_int_value(const char *json, const char *key, int *out)
{
    char pattern[64];
    const char *cursor;
    char *end;
    long value;

    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    cursor = strstr(json, pattern);
    if (cursor == NULL)
    {
        return 0;
    }
    cursor += strlen(pattern);
    cursor = strchr(cursor, ':');
    if (cursor == NULL)
    {
        return 0;
    }
    cursor++;
    while (isspace((unsigned char)*cursor))
    {
        cursor++;
    }
    value = strtol(cursor, &end, 10);
    if (cursor == end)
    {
        return 0;
    }
    *out = (int)value;
    return 1;
}

static int extract_json_id(const char *json, char *id_buffer, size_t capacity)
{
    const char *cursor = strstr(json, "\"id\"");
    const char *start;
    size_t length = 0;

    if (cursor == NULL)
    {
        return 0;
    }
    cursor = strchr(cursor, ':');
    if (cursor == NULL)
    {
        return 0;
    }
    cursor++;
    while (isspace((unsigned char)*cursor))
    {
        cursor++;
    }

    if (*cursor == '"')
    {
        start = cursor;
        cursor++;
        while (*cursor != '\0')
        {
            if (*cursor == '\\' && cursor[1] != '\0')
            {
                cursor += 2;
                continue;
            }
            if (*cursor == '"')
            {
                cursor++;
                break;
            }
            cursor++;
        }
        length = (size_t)(cursor - start);
    }
    else
    {
        start = cursor;
        while (*cursor != '\0' && *cursor != ',' && *cursor != '}')
        {
            cursor++;
        }
        while (cursor > start && isspace((unsigned char)cursor[-1]))
        {
            cursor--;
        }
        length = (size_t)(cursor - start);
    }

    if (length == 0 || length >= capacity)
    {
        return 0;
    }
    memcpy(id_buffer, start, length);
    id_buffer[length] = '\0';
    return 1;
}

static int read_message(FILE *input, char **message_out)
{
    char line[1024];
    size_t content_length = 0;
    int saw_header = 0;
    char *body;

    while (fgets(line, sizeof(line), input) != NULL)
    {
        size_t line_length = strlen(line);
        saw_header = 1;

        if (strcmp(line, "\n") == 0 || strcmp(line, "\r\n") == 0)
        {
            break;
        }

        if (strncmp(line, "Content-Length:", 15) == 0)
        {
            content_length = (size_t)strtoull(line + 15, NULL, 10);
        }

        if (line_length == sizeof(line) - 1 && line[line_length - 1] != '\n')
        {
            return 0;
        }
    }

    if (!saw_header)
    {
        return 0;
    }
    if (content_length == 0)
    {
        return 0;
    }

    body = malloc(content_length + 1);
    if (body == NULL)
    {
        return 0;
    }
    if (fread(body, 1, content_length, input) != content_length)
    {
        free(body);
        return 0;
    }
    body[content_length] = '\0';
    *message_out = body;
    return 1;
}

static int write_response(const char *body)
{
    size_t length = strlen(body);
    if (fprintf(stdout, "Content-Length: %zu\r\n\r\n", length) < 0)
    {
        return 0;
    }
    if (fwrite(body, 1, length, stdout) != length)
    {
        return 0;
    }
    return fflush(stdout) == 0;
}

static void add_query_result(QueryResult *results, int *count,
                             const char *class_name, const char *selector,
                             int class_side, const char *uri, int symbol_kind)
{
    if (*count >= MAX_QUERY_RESULTS)
    {
        return;
    }
    strncpy(results[*count].class_name, class_name, sizeof(results[*count].class_name) - 1);
    results[*count].class_name[sizeof(results[*count].class_name) - 1] = '\0';
    strncpy(results[*count].selector, selector, sizeof(results[*count].selector) - 1);
    results[*count].selector[sizeof(results[*count].selector) - 1] = '\0';
    strncpy(results[*count].uri, uri, sizeof(results[*count].uri) - 1);
    results[*count].uri[sizeof(results[*count].uri) - 1] = '\0';
    results[*count].class_side = class_side;
    results[*count].symbol_kind = symbol_kind;
    (*count)++;
}

static int matches_query(const char *query, const char *class_name, const char *selector, int class_side)
{
    char full_name[256];

    if (query == NULL || *query == '\0')
    {
        return 1;
    }

    snprintf(full_name, sizeof(full_name), "%s%s>>%s", class_name, class_side ? " class" : "", selector);
    return strstr(class_name, query) != NULL ||
           strstr(selector, query) != NULL ||
           strstr(full_name, query) != NULL;
}

static int collect_workspace_symbols(LspServer *server, const char *query,
                                     QueryResult *results, int *result_count)
{
    Oop associations_oop = OBJ_FIELD(server->world.smalltalk_dict, 0);
    Oop tally_oop = OBJ_FIELD(server->world.smalltalk_dict, 1);
    ObjPtr associations;
    int64_t tally;
    int64_t assoc_index;

    *result_count = 0;
    if (!is_object_ptr(associations_oop) || !is_smallint(tally_oop))
    {
        return 1;
    }

    associations = (ObjPtr)associations_oop;
    tally = untag_smallint(tally_oop);
    for (assoc_index = 0; assoc_index < tally && *result_count < MAX_QUERY_RESULTS; assoc_index++)
    {
        Oop assoc_oop = OBJ_FIELD(associations, (uint64_t)assoc_index);
        ObjPtr klass;
        ObjPtr metaclass;
        char class_name[64];
        Oop method_dict_oop;
        ObjPtr method_dict;
        uint64_t method_index;

        if (!is_object_ptr(assoc_oop))
        {
            continue;
        }
        if (!copy_oop_bytes_to_cstring(OBJ_FIELD((ObjPtr)assoc_oop, 0), class_name, sizeof(class_name)))
        {
            continue;
        }
        klass = (ObjPtr)OBJ_FIELD((ObjPtr)assoc_oop, 1);
        if (!is_object_ptr((Oop)klass))
        {
            continue;
        }

        method_dict_oop = OBJ_FIELD(klass, CLASS_METHOD_DICT);
        if (is_object_ptr(method_dict_oop))
        {
            method_dict = (ObjPtr)method_dict_oop;
            for (method_index = 0; method_index + 1 < OBJ_SIZE(method_dict) && *result_count < MAX_QUERY_RESULTS; method_index += 2)
            {
                char selector[128];
                ObjPtr method;
                Oop source_oop;
                char *source;
                const char *uri;

                if (!copy_oop_bytes_to_cstring(OBJ_FIELD(method_dict, method_index), selector, sizeof(selector)))
                {
                    continue;
                }
                if (!matches_query(query, class_name, selector, 0))
                {
                    continue;
                }
                method = (ObjPtr)OBJ_FIELD(method_dict, method_index + 1);
                source_oop = OBJ_FIELD(method, CM_SOURCE);
                source = dup_oop_bytes(source_oop);
                if (source == NULL)
                {
                    continue;
                }
                uri = materialize_method_source(server, class_name, selector, 0, source);
                free(source);
                if (uri != NULL)
                {
                    add_query_result(results, result_count, class_name, selector, 0, uri, 6);
                }
            }
        }

        metaclass = (ObjPtr)OBJ_CLASS(klass);
        method_dict_oop = OBJ_FIELD(metaclass, CLASS_METHOD_DICT);
        if (is_object_ptr(method_dict_oop))
        {
            method_dict = (ObjPtr)method_dict_oop;
            for (method_index = 0; method_index + 1 < OBJ_SIZE(method_dict) && *result_count < MAX_QUERY_RESULTS; method_index += 2)
            {
                char selector[128];
                ObjPtr method;
                Oop source_oop;
                char *source;
                const char *uri;

                if (!copy_oop_bytes_to_cstring(OBJ_FIELD(method_dict, method_index), selector, sizeof(selector)))
                {
                    continue;
                }
                if (!matches_query(query, class_name, selector, 1))
                {
                    continue;
                }
                method = (ObjPtr)OBJ_FIELD(method_dict, method_index + 1);
                source_oop = OBJ_FIELD(method, CM_SOURCE);
                source = dup_oop_bytes(source_oop);
                if (source == NULL)
                {
                    continue;
                }
                uri = materialize_method_source(server, class_name, selector, 1, source);
                free(source);
                if (uri != NULL)
                {
                    add_query_result(results, result_count, class_name, selector, 1, uri, 6);
                }
            }
        }
    }

    return 1;
}

static ObjPtr infer_current_class(LspServer *server, const char *uri, int *class_side_out)
{
    MaterializedMethod *materialized = find_materialized(server, uri);
    if (materialized != NULL)
    {
        if (class_side_out != NULL)
        {
            *class_side_out = materialized->class_side;
        }
        return lookup_class_by_name(server, materialized->class_name);
    }
    if (class_side_out != NULL)
    {
        *class_side_out = 0;
    }
    return NULL;
}

static int build_hover_text_for_selector(LspServer *server, const char *uri, const char *selector,
                                         char **hover_text_out)
{
    ObjPtr current_class;
    ObjPtr owner = NULL;
    ObjPtr method;
    Oop selector_oop;
    int class_side = 0;
    char owner_name[64];
    char *source;
    StringBuffer hover;

    current_class = infer_current_class(server, uri, &class_side);
    selector_oop = intern_cstring_symbol(server->world.om, selector);
    if (current_class != NULL)
    {
        method = find_method_owner(class_side ? (ObjPtr)OBJ_CLASS(current_class) : current_class,
                                   selector_oop, &owner);
        if (method != NULL)
        {
            if (!copy_registered_class_name(server, owner, class_side, owner_name, sizeof(owner_name)))
            {
                strncpy(owner_name, "Unknown", sizeof(owner_name) - 1);
                owner_name[sizeof(owner_name) - 1] = '\0';
            }
            source = dup_oop_bytes(OBJ_FIELD(method, CM_SOURCE));
            if (source == NULL)
            {
                return 0;
            }
            sb_init(&hover);
            sb_appendf(&hover, "%s%s>>%s\n\n", owner_name, class_side ? " class" : "", selector);
            sb_append(&hover, source);
            free(source);
            *hover_text_out = hover.data;
            return 1;
        }
    }

    if (isupper((unsigned char)selector[0]))
    {
        ObjPtr klass = lookup_class_by_name(server, selector);
        if (klass != NULL)
        {
            char class_name[64];
            char superclass_name[64];
            if (!copy_oop_bytes_to_cstring(prim_class_name((Oop)klass), class_name, sizeof(class_name)))
            {
                return 0;
            }
            if (is_object_ptr(OBJ_FIELD(klass, CLASS_SUPERCLASS)) &&
                copy_oop_bytes_to_cstring(prim_class_name(OBJ_FIELD(klass, CLASS_SUPERCLASS)),
                                          superclass_name, sizeof(superclass_name)))
            {
                ;
            }
            else
            {
                strcpy(superclass_name, "nil");
            }
            sb_init(&hover);
            sb_appendf(&hover,
                       "Class %s\nsuperclass: %s\ninstance methods: %d\nclass methods: %d",
                       class_name, superclass_name,
                       count_local_methods(klass),
                       count_local_methods((ObjPtr)OBJ_CLASS(klass)));
            *hover_text_out = hover.data;
            return 1;
        }
    }

    return 0;
}

static int build_definition_locations(LspServer *server, const char *uri, const char *token,
                                      StringBuffer *locations)
{
    ObjPtr current_class;
    ObjPtr owner = NULL;
    ObjPtr method;
    Oop selector_oop;
    int class_side = 0;
    int found = 0;

    current_class = infer_current_class(server, uri, &class_side);
    selector_oop = intern_cstring_symbol(server->world.om, token);

    if (current_class != NULL)
    {
        char selector_name[128];
        if (oop_equals_cstring_symbol(selector_oop, token))
        {
            method = find_method_owner(class_side ? (ObjPtr)OBJ_CLASS(current_class) : current_class,
                                       selector_oop, &owner);
            if (method != NULL)
            {
                char owner_name[64];
                char *source = dup_oop_bytes(OBJ_FIELD(method, CM_SOURCE));
                const char *materialized_uri;
                if (source != NULL && copy_registered_class_name(server, owner, class_side, owner_name, sizeof(owner_name)))
                {
                    materialized_uri = materialize_method_source(server, owner_name, token, class_side, source);
                    free(source);
                    if (materialized_uri != NULL)
                    {
                        if (found)
                        {
                            sb_append(locations, ",");
                        }
                        sb_append(locations, "{\"uri\":");
                        sb_append_json_string(locations, materialized_uri);
                        sb_append(locations, ",\"range\":{\"start\":{\"line\":0,\"character\":0},\"end\":{\"line\":0,\"character\":0}}}");
                        return 1;
                    }
                }
                free(source);
            }
        }
        (void)selector_name;
    }

    if (isupper((unsigned char)token[0]))
    {
        char *summary_path = materialize_class_summary(server, token);
        if (summary_path != NULL)
        {
            if (found)
            {
                sb_append(locations, ",");
            }
            sb_append(locations, "{\"uri\":");
            sb_appendf(locations, "\"file://%s\"", summary_path);
            sb_append(locations, ",\"range\":{\"start\":{\"line\":0,\"character\":0},\"end\":{\"line\":0,\"character\":0}}}");
            free(summary_path);
            found = 1;
        }
    }

    if (found)
    {
        return 1;
    }

    {
        QueryResult results[MAX_QUERY_RESULTS];
        int count = 0;
        int index;
        if (!collect_workspace_symbols(server, token, results, &count))
        {
            return 0;
        }
        for (index = 0; index < count; index++)
        {
            if (strcmp(results[index].selector, token) != 0)
            {
                continue;
            }
            if (found)
            {
                sb_append(locations, ",");
            }
            sb_append(locations, "{\"uri\":");
            sb_append_json_string(locations, results[index].uri);
            sb_append(locations, ",\"range\":{\"start\":{\"line\":0,\"character\":0},\"end\":{\"line\":0,\"character\":0}}}");
            found = 1;
        }
    }

    return found;
}

static int boot_live_image(LspServer *server)
{
    static const LoRuntimeSource runtime_sources[] = {
        {"src/smalltalk/Object.st", LO_RUNTIME_SOURCE_EXISTING_CLASS},
        {"src/smalltalk/Class.st", LO_RUNTIME_SOURCE_METHODS_ONLY},
        {"src/smalltalk/SmallInteger.st", LO_RUNTIME_SOURCE_EXISTING_CLASS},
        {"src/smalltalk/True.st", LO_RUNTIME_SOURCE_EXISTING_CLASS},
        {"src/smalltalk/False.st", LO_RUNTIME_SOURCE_EXISTING_CLASS},
        {"src/smalltalk/UndefinedObject.st", LO_RUNTIME_SOURCE_EXISTING_CLASS},
        {"src/smalltalk/Character.st", LO_RUNTIME_SOURCE_METHODS_ONLY},
        {"src/smalltalk/BlockClosure.st", LO_RUNTIME_SOURCE_METHODS_ONLY},
        {"src/smalltalk/Array.st", LO_RUNTIME_SOURCE_EXISTING_CLASS},
        {"src/smalltalk/String.st", LO_RUNTIME_SOURCE_EXISTING_CLASS},
        {"src/smalltalk/Symbol.st", LO_RUNTIME_SOURCE_METHODS_ONLY},
        {"src/smalltalk/Association.st", LO_RUNTIME_SOURCE_METHODS_ONLY},
        {"src/smalltalk/Dictionary.st", LO_RUNTIME_SOURCE_METHODS_ONLY},
        {"src/smalltalk/Context.st", LO_RUNTIME_SOURCE_METHODS_ONLY},
        {"src/smalltalk/Stdio.st", LO_RUNTIME_SOURCE_NEW_CLASS},
    };

    memset(server, 0, sizeof(*server));
    smalltalk_world_init(&server->world, heap, sizeof(heap));
    if (!lo_install_runtime_sources(&server->world, runtime_sources,
                                    sizeof(runtime_sources) / sizeof(runtime_sources[0])))
    {
        return 0;
    }
    if (!lo_install_smalltalk_compiler_sources(&server->world))
    {
        return 0;
    }
    return 1;
}

static int build_success_response(const char *id, const char *result_json, char **response_out)
{
    StringBuffer response;
    sb_init(&response);
    if (!sb_append(&response, "{\"jsonrpc\":\"2.0\",\"id\":") ||
        !sb_append(&response, id) ||
        !sb_append(&response, ",\"result\":") ||
        !sb_append(&response, result_json) ||
        !sb_append(&response, "}"))
    {
        sb_free(&response);
        return 0;
    }
    *response_out = response.data;
    return 1;
}

static int build_error_response(const char *id, int code, const char *message, char **response_out)
{
    StringBuffer response;
    sb_init(&response);
    if (!sb_append(&response, "{\"jsonrpc\":\"2.0\",\"id\":") ||
        !sb_append(&response, id != NULL ? id : "null") ||
        !sb_appendf(&response, ",\"error\":{\"code\":%d,\"message\":", code) ||
        !sb_append_json_string(&response, message) ||
        !sb_append(&response, "}}"))
    {
        sb_free(&response);
        return 0;
    }
    *response_out = response.data;
    return 1;
}

static int handle_initialize(const char *id, char **response_out)
{
    const char *result =
        "{\"capabilities\":{"
        "\"textDocumentSync\":1,"
        "\"hoverProvider\":true,"
        "\"definitionProvider\":true,"
        "\"workspaceSymbolProvider\":true"
        "},"
        "\"serverInfo\":{\"name\":\"smalltalk_lsp\",\"version\":\"0.1\"}}";

    return build_success_response(id, result, response_out);
}

static int handle_workspace_symbol(LspServer *server, const char *id, const char *message,
                                   char **response_out)
{
    char *query = NULL;
    QueryResult results[MAX_QUERY_RESULTS];
    int count = 0;
    int index;
    StringBuffer result;

    extract_json_string_value(message, "query", &query);
    if (!collect_workspace_symbols(server, query, results, &count))
    {
        free(query);
        return build_error_response(id, -32603, "workspace symbol collection failed", response_out);
    }
    free(query);

    sb_init(&result);
    sb_append(&result, "[");
    for (index = 0; index < count; index++)
    {
        if (index > 0)
        {
            sb_append(&result, ",");
        }
        sb_append(&result, "{\"name\":");
        sb_appendf(&result, "\"%s%s>>%s\"", results[index].class_name,
                   results[index].class_side ? " class" : "",
                   results[index].selector);
        sb_appendf(&result, ",\"kind\":%d,", results[index].symbol_kind);
        sb_append(&result, "\"location\":{\"uri\":");
        sb_append_json_string(&result, results[index].uri);
        sb_append(&result, ",\"range\":{\"start\":{\"line\":0,\"character\":0},\"end\":{\"line\":0,\"character\":0}}},");
        sb_append(&result, "\"containerName\":");
        sb_append_json_string(&result, results[index].class_side ? "class side" : "instance side");
        sb_append(&result, "}");
    }
    sb_append(&result, "]");
    if (!build_success_response(id, result.data != NULL ? result.data : "[]", response_out))
    {
        sb_free(&result);
        return 0;
    }
    sb_free(&result);
    return 1;
}

static int handle_did_open(LspServer *server, const char *message)
{
    char *uri = NULL;
    char *text = NULL;
    int ok;

    if (!extract_json_string_value(message, "uri", &uri) ||
        !extract_json_string_value(message, "text", &text))
    {
        free(uri);
        free(text);
        return 0;
    }
    ok = set_open_document(server, uri, text);
    free(uri);
    free(text);
    return ok;
}

static int handle_did_change(LspServer *server, const char *message)
{
    char *uri = NULL;
    char *text = NULL;
    int ok;

    if (!extract_json_string_value(message, "uri", &uri) ||
        !extract_json_string_value(message, "text", &text))
    {
        free(uri);
        free(text);
        return 0;
    }
    ok = set_open_document(server, uri, text);
    free(uri);
    free(text);
    return ok;
}

static int handle_did_close(LspServer *server, const char *message)
{
    char *uri = NULL;
    if (!extract_json_string_value(message, "uri", &uri))
    {
        free(uri);
        return 0;
    }
    close_open_document(server, uri);
    free(uri);
    return 1;
}

static int handle_hover(LspServer *server, const char *id, const char *message, char **response_out)
{
    char *uri = NULL;
    char *text = NULL;
    char *hover = NULL;
    char token[128];
    int line;
    int character;
    StringBuffer result;
    int ok = 0;

    if (!extract_json_string_value(message, "uri", &uri) ||
        !extract_json_int_value(message, "line", &line) ||
        !extract_json_int_value(message, "character", &character))
    {
        free(uri);
        return build_error_response(id, -32602, "hover params missing uri or position", response_out);
    }

    text = read_uri_text(server, uri);
    if (text == NULL)
    {
        free(uri);
        return build_success_response(id, "null", response_out);
    }

    if (!extract_token_at_position(text, line, character, token, sizeof(token)) ||
        !build_hover_text_for_selector(server, uri, token, &hover))
    {
        free(uri);
        free(text);
        return build_success_response(id, "null", response_out);
    }

    sb_init(&result);
    sb_append(&result, "{\"contents\":{\"kind\":\"plaintext\",\"value\":");
    sb_append_json_string(&result, hover);
    sb_append(&result, "}}");
    ok = build_success_response(id, result.data, response_out);
    sb_free(&result);
    free(uri);
    free(text);
    free(hover);
    return ok;
}

static int handle_definition(LspServer *server, const char *id, const char *message, char **response_out)
{
    char *uri = NULL;
    char *text = NULL;
    char token[128];
    int line;
    int character;
    StringBuffer result;
    int ok;

    if (!extract_json_string_value(message, "uri", &uri) ||
        !extract_json_int_value(message, "line", &line) ||
        !extract_json_int_value(message, "character", &character))
    {
        free(uri);
        return build_error_response(id, -32602, "definition params missing uri or position", response_out);
    }

    text = read_uri_text(server, uri);
    if (text == NULL)
    {
        free(uri);
        return build_success_response(id, "[]", response_out);
    }
    if (!extract_token_at_position(text, line, character, token, sizeof(token)))
    {
        free(uri);
        free(text);
        return build_success_response(id, "[]", response_out);
    }

    sb_init(&result);
    sb_append(&result, "[");
    if (build_definition_locations(server, uri, token, &result))
    {
        sb_append(&result, "]");
    }
    else
    {
        result.length = 0;
        if (result.data != NULL)
        {
            result.data[0] = '\0';
        }
        sb_append(&result, "[]");
    }
    ok = build_success_response(id, result.data, response_out);
    sb_free(&result);
    free(uri);
    free(text);
    return ok;
}

static int handle_shutdown(LspServer *server, const char *id, char **response_out)
{
    server->shutdown_requested = 1;
    return build_success_response(id, "null", response_out);
}

static int handle_message(LspServer *server, const char *message, char **response_out)
{
    char method[128];
    char id[64];
    int has_id = extract_json_id(message, id, sizeof(id));
    char *method_text = NULL;

    *response_out = NULL;
    if (!extract_json_string_value(message, "method", &method_text))
    {
        return has_id ? build_error_response(id, -32600, "request missing method", response_out) : 1;
    }
    strncpy(method, method_text, sizeof(method) - 1);
    method[sizeof(method) - 1] = '\0';
    free(method_text);

    if (strcmp(method, "initialize") == 0)
    {
        return has_id ? handle_initialize(id, response_out) : 1;
    }
    if (strcmp(method, "workspace/symbol") == 0)
    {
        return has_id ? handle_workspace_symbol(server, id, message, response_out) : 1;
    }
    if (strcmp(method, "textDocument/didOpen") == 0)
    {
        return handle_did_open(server, message);
    }
    if (strcmp(method, "textDocument/didChange") == 0)
    {
        return handle_did_change(server, message);
    }
    if (strcmp(method, "textDocument/didClose") == 0)
    {
        return handle_did_close(server, message);
    }
    if (strcmp(method, "textDocument/hover") == 0)
    {
        return has_id ? handle_hover(server, id, message, response_out) : 1;
    }
    if (strcmp(method, "textDocument/definition") == 0)
    {
        return has_id ? handle_definition(server, id, message, response_out) : 1;
    }
    if (strcmp(method, "shutdown") == 0)
    {
        return has_id ? handle_shutdown(server, id, response_out) : 1;
    }
    if (strcmp(method, "exit") == 0)
    {
        if (!server->shutdown_requested)
        {
            exit(1);
        }
        exit(0);
    }
    if (strcmp(method, "initialized") == 0)
    {
        return 1;
    }

    return has_id ? build_error_response(id, -32601, "method not implemented", response_out) : 1;
}

int main(void)
{
    LspServer server;

    if (!boot_live_image(&server))
    {
        fprintf(stderr, "failed to boot live Smalltalk image\n");
        return 1;
    }

    while (1)
    {
        char *message = NULL;
        char *response = NULL;

        if (!read_message(stdin, &message))
        {
            break;
        }
        if (!handle_message(&server, message, &response))
        {
            free(message);
            free(response);
            break;
        }
        free(message);
        if (response != NULL)
        {
            if (!write_response(response))
            {
                free(response);
                break;
            }
            free(response);
        }
    }

    smalltalk_world_teardown(&server.world);
    return 0;
}
