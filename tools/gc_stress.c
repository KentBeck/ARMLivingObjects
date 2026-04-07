#include "test_defs.h"

#include <errno.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define SYS_ROOT_COUNT 4
#define DEFAULT_ROOT_SLOTS 32
#define DEFAULT_MAX_OBJECTS 4096
#define DEFAULT_MAX_RUN_STEPS 100000
#define DEFAULT_SPACE_SIZE (64 * 1024)
#define MAX_OBJECT_SLOTS 16

typedef enum
{
    CLASS_KIND_CLASS = 0,
    CLASS_KIND_FIELDS = 1,
    CLASS_KIND_INDEXABLE = 2,
    CLASS_KIND_BYTES = 3,
} StressClassKind;

typedef enum
{
    VALUE_RAW = 0,
    VALUE_OBJECT = 1,
} StressValueKind;

typedef struct
{
    StressValueKind kind;
    uint64_t raw;
    uint32_t object_id;
} StressValue;

typedef struct
{
    uint32_t id;
    StressClassKind class_kind;
    uint64_t format;
    uint64_t size;
    uint64_t *ptr;
    uint64_t seen_epoch;
    StressValue fields[MAX_OBJECT_SLOTS];
    uint8_t bytes[MAX_OBJECT_SLOTS];
} StressObject;

typedef struct
{
    uint64_t seed;
    uint64_t state;
    uint64_t step_limit;
    uint64_t run_index;
    uint64_t space_size;
    uint64_t verify_every;
    uint32_t root_slots;
    const char *log_dir;
    const char *self_path;
    bool supervise;
    bool seed_provided;
    uint64_t runs;
} StressConfig;

typedef struct
{
    FILE *log;
    StressConfig cfg;
    uint8_t *space_a;
    uint8_t *space_b;
    uint64_t gc_ctx[10];
    uint64_t root_values[SYS_ROOT_COUNT + DEFAULT_ROOT_SLOTS];
    StressValue root_model[DEFAULT_ROOT_SLOTS];
    StressObject objects[DEFAULT_MAX_OBJECTS];
    uint32_t object_count;
    uint64_t verify_epoch;
    uint64_t *class_class;
    uint64_t *field_class;
    uint64_t *indexable_class;
    uint64_t *byte_class;
} StressState;

static void stress_log(StressState *st, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(st->log, fmt, ap);
    va_end(ap);
    fputc('\n', st->log);
    fflush(st->log);
}

static void append_value_desc(char *buf, size_t cap, StressValue value)
{
    if (value.kind == VALUE_OBJECT)
    {
        snprintf(buf, cap, "obj:%u", value.object_id);
        return;
    }
    snprintf(buf, cap, "raw:0x%016" PRIx64, value.raw);
}

static void fatalf(StressState *st, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "gc_stress fatal: ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    if (st != NULL && st->log != NULL)
    {
        fprintf(st->log, "FATAL: ");
        va_end(ap);
        va_start(ap, fmt);
        vfprintf(st->log, fmt, ap);
        fprintf(st->log, "\n");
        fflush(st->log);
    }
    va_end(ap);
    abort();
}

static void ensure_dir_or_die(const char *path)
{
    if (mkdir(path, 0777) == 0)
    {
        return;
    }
    if (errno == EEXIST)
    {
        return;
    }
    fprintf(stderr, "gc_stress: mkdir %s failed: %s\n", path, strerror(errno));
    exit(2);
}

static void join_path(char *out, size_t cap, const char *a, const char *b)
{
    snprintf(out, cap, "%s/%s", a, b);
}

static uint64_t rng_next(StressState *st)
{
    uint64_t x = st->cfg.state;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    st->cfg.state = x;
    return x * UINT64_C(2685821657736338717);
}

static uint64_t rng_range(StressState *st, uint64_t n)
{
    if (n == 0)
    {
        return 0;
    }
    return rng_next(st) % n;
}

static StressValue make_raw_value(uint64_t raw)
{
    StressValue v;
    v.kind = VALUE_RAW;
    v.raw = raw;
    v.object_id = 0;
    return v;
}

static StressValue make_object_value(uint32_t object_id)
{
    StressValue v;
    v.kind = VALUE_OBJECT;
    v.raw = 0;
    v.object_id = object_id;
    return v;
}

static uint64_t *class_ptr_for_kind(StressState *st, StressClassKind kind)
{
    switch (kind)
    {
    case CLASS_KIND_CLASS:
        return st->class_class;
    case CLASS_KIND_FIELDS:
        return st->field_class;
    case CLASS_KIND_INDEXABLE:
        return st->indexable_class;
    case CLASS_KIND_BYTES:
        return st->byte_class;
    default:
        return NULL;
    }
}

static uint64_t sys_root_index_for_kind(StressClassKind kind)
{
    return (uint64_t)kind;
}

static StressObject *object_by_id(StressState *st, uint32_t object_id)
{
    if (object_id == 0 || object_id > st->object_count)
    {
        return NULL;
    }
    return &st->objects[object_id - 1];
}

static uint64_t *resolve_live_object_ptr(StressState *st, uint32_t object_id)
{
    StressObject *obj = object_by_id(st, object_id);
    if (obj == NULL)
    {
        return NULL;
    }

    for (uint32_t i = 0; i < st->cfg.root_slots; i++)
    {
        if (st->root_model[i].kind == VALUE_OBJECT && st->root_model[i].object_id == object_id)
        {
            uint64_t value = st->root_values[SYS_ROOT_COUNT + i];
            if (is_object_ptr(value))
            {
                obj->ptr = (uint64_t *)value;
                return obj->ptr;
            }
        }
    }

    return obj->ptr;
}

static uint64_t actual_value_for_model(StressState *st, StressValue value)
{
    if (value.kind == VALUE_RAW)
    {
        return value.raw;
    }
    uint64_t *ptr = resolve_live_object_ptr(st, value.object_id);
    if (ptr == NULL)
    {
        fatalf(st, "object id %u has no live pointer", value.object_id);
    }
    return (uint64_t)ptr;
}

static StressValue random_immediate(StressState *st)
{
    switch (rng_range(st, 6))
    {
    case 0:
        return make_raw_value(tag_smallint((int64_t)rng_range(st, 200) - 100));
    case 1:
        return make_raw_value(tagged_nil());
    case 2:
        return make_raw_value(tagged_true());
    case 3:
        return make_raw_value(tagged_false());
    case 4:
        return make_raw_value(tag_character(rng_range(st, 128)));
    default:
        return make_raw_value(0);
    }
}

static StressValue random_value(StressState *st)
{
    if (st->cfg.root_slots == 0 || rng_range(st, 4) != 0)
    {
        return random_immediate(st);
    }

    uint32_t attempts = st->cfg.root_slots;
    while (attempts-- > 0)
    {
        uint32_t slot = (uint32_t)rng_range(st, st->cfg.root_slots);
        StressValue v = st->root_model[slot];
        if (v.kind == VALUE_OBJECT)
        {
            return v;
        }
    }
    return random_immediate(st);
}

static void perform_gc(StressState *st, const char *reason)
{
    uint64_t root_count = SYS_ROOT_COUNT + st->cfg.root_slots;
    stress_log(st, "step=%" PRIu64 " gc reason=%s roots=%" PRIu64,
               st->cfg.run_index, reason, root_count);
    st->gc_ctx[GC_TO_FREE] = st->gc_ctx[GC_TO_START];
    gc_collect(st->root_values, root_count,
               &st->gc_ctx[GC_FROM_FREE], &st->gc_ctx[GC_TO_FREE],
               st->gc_ctx[GC_FROM_START], st->gc_ctx[GC_FROM_END]);
    gc_ctx_swap(st->gc_ctx);
    st->class_class = (uint64_t *)st->root_values[sys_root_index_for_kind(CLASS_KIND_CLASS)];
    st->field_class = (uint64_t *)st->root_values[sys_root_index_for_kind(CLASS_KIND_FIELDS)];
    st->indexable_class = (uint64_t *)st->root_values[sys_root_index_for_kind(CLASS_KIND_INDEXABLE)];
    st->byte_class = (uint64_t *)st->root_values[sys_root_index_for_kind(CLASS_KIND_BYTES)];
}

static void verify_value(StressState *st, StressValue expected, uint64_t actual);

static void verify_object(StressState *st, uint32_t object_id, uint64_t actual)
{
    StressObject *obj = object_by_id(st, object_id);
    if (obj == NULL)
    {
        fatalf(st, "missing object id %u", object_id);
    }
    if (!is_object_ptr(actual))
    {
        fatalf(st, "object id %u expected heap object but got 0x%016" PRIx64, object_id, actual);
    }

    uint64_t *ptr = (uint64_t *)actual;
    if (obj->seen_epoch == st->verify_epoch)
    {
        if (obj->ptr != ptr)
        {
            fatalf(st, "object id %u aliased to different ptrs %p vs %p", object_id, (void *)obj->ptr, (void *)ptr);
        }
        return;
    }

    obj->seen_epoch = st->verify_epoch;
    obj->ptr = ptr;

    if (OBJ_FORMAT(ptr) != obj->format)
    {
        fatalf(st, "object id %u ptr=%p format mismatch expected=%" PRIu64 " actual=%" PRIu64,
               object_id, (void *)ptr, obj->format, OBJ_FORMAT(ptr));
    }
    if (OBJ_SIZE(ptr) != obj->size)
    {
        fatalf(st, "object id %u ptr=%p size mismatch expected=%" PRIu64 " actual=%" PRIu64,
               object_id, (void *)ptr, obj->size, OBJ_SIZE(ptr));
    }
    if (OBJ_CLASS(ptr) != (uint64_t)class_ptr_for_kind(st, obj->class_kind))
    {
        fatalf(st, "object id %u ptr=%p class mismatch expected=%p actual=%p",
               object_id, (void *)ptr,
               (void *)class_ptr_for_kind(st, obj->class_kind),
               (void *)OBJ_CLASS(ptr));
    }

    if (obj->format == FORMAT_BYTES)
    {
        uint8_t *bytes = (uint8_t *)&OBJ_FIELD(ptr, 0);
        for (uint64_t i = 0; i < obj->size; i++)
        {
            if (bytes[i] != obj->bytes[i])
            {
                fatalf(st, "object id %u byte[%" PRIu64 "] mismatch expected=%u actual=%u",
                       object_id, i, (unsigned)obj->bytes[i], (unsigned)bytes[i]);
            }
        }
        return;
    }

    for (uint64_t i = 0; i < obj->size; i++)
    {
        verify_value(st, obj->fields[i], OBJ_FIELD(ptr, i));
    }
}

static void verify_value(StressState *st, StressValue expected, uint64_t actual)
{
    if (expected.kind == VALUE_RAW)
    {
        if (actual != expected.raw)
        {
            fatalf(st, "raw value mismatch expected=0x%016" PRIx64 " actual=0x%016" PRIx64,
                   expected.raw, actual);
        }
        return;
    }
    verify_object(st, expected.object_id, actual);
}

static void verify_roots(StressState *st)
{
    st->verify_epoch++;
    for (uint32_t i = 0; i < st->cfg.root_slots; i++)
    {
        verify_value(st, st->root_model[i], st->root_values[SYS_ROOT_COUNT + i]);
    }
}

static void relieve_pressure(StressState *st)
{
    if (st->cfg.root_slots == 0)
    {
        return;
    }
    uint32_t slot = (uint32_t)rng_range(st, st->cfg.root_slots);
    st->root_model[slot] = make_raw_value(tagged_nil());
    st->root_values[SYS_ROOT_COUNT + slot] = tagged_nil();
    stress_log(st, "step=%" PRIu64 " drop-root slot=%u", st->cfg.run_index, slot);
}

static uint64_t *alloc_with_gc(StressState *st, StressClassKind class_kind, uint64_t format, uint64_t size)
{
    uint64_t *obj = om_alloc(&st->gc_ctx[GC_FROM_FREE],
                             (uint64_t)class_ptr_for_kind(st, class_kind),
                             format, size);
    if (obj != NULL)
    {
        return obj;
    }

    perform_gc(st, "oom");
    verify_roots(st);
    obj = om_alloc(&st->gc_ctx[GC_FROM_FREE],
                   (uint64_t)class_ptr_for_kind(st, class_kind),
                   format, size);
    if (obj != NULL)
    {
        return obj;
    }

    relieve_pressure(st);
    perform_gc(st, "relief");
    verify_roots(st);
    obj = om_alloc(&st->gc_ctx[GC_FROM_FREE],
                   (uint64_t)class_ptr_for_kind(st, class_kind),
                   format, size);
    if (obj != NULL)
    {
        return obj;
    }

    fatalf(st, "allocation still failed after GC size=%" PRIu64 " format=%" PRIu64, size, format);
    return NULL;
}

static void bootstrap_classes(StressState *st)
{
    st->class_class = alloc_with_gc(st, CLASS_KIND_CLASS, FORMAT_FIELDS, 4);
    st->root_values[sys_root_index_for_kind(CLASS_KIND_CLASS)] = (uint64_t)st->class_class;
    OBJ_CLASS(st->class_class) = (uint64_t)st->class_class;
    OBJ_FIELD(st->class_class, CLASS_SUPERCLASS) = tagged_nil();
    OBJ_FIELD(st->class_class, CLASS_METHOD_DICT) = tagged_nil();
    OBJ_FIELD(st->class_class, CLASS_INST_SIZE) = tag_smallint(4);
    OBJ_FIELD(st->class_class, CLASS_INST_FORMAT) = tag_smallint(FORMAT_FIELDS);

    st->field_class = alloc_with_gc(st, CLASS_KIND_CLASS, FORMAT_FIELDS, 4);
    st->root_values[sys_root_index_for_kind(CLASS_KIND_FIELDS)] = (uint64_t)st->field_class;
    OBJ_FIELD(st->field_class, CLASS_SUPERCLASS) = tagged_nil();
    OBJ_FIELD(st->field_class, CLASS_METHOD_DICT) = tagged_nil();
    OBJ_FIELD(st->field_class, CLASS_INST_SIZE) = tag_smallint(0);
    OBJ_FIELD(st->field_class, CLASS_INST_FORMAT) = tag_smallint(FORMAT_FIELDS);

    st->indexable_class = alloc_with_gc(st, CLASS_KIND_CLASS, FORMAT_FIELDS, 4);
    st->root_values[sys_root_index_for_kind(CLASS_KIND_INDEXABLE)] = (uint64_t)st->indexable_class;
    OBJ_FIELD(st->indexable_class, CLASS_SUPERCLASS) = tagged_nil();
    OBJ_FIELD(st->indexable_class, CLASS_METHOD_DICT) = tagged_nil();
    OBJ_FIELD(st->indexable_class, CLASS_INST_SIZE) = tag_smallint(0);
    OBJ_FIELD(st->indexable_class, CLASS_INST_FORMAT) = tag_smallint(FORMAT_INDEXABLE);

    st->byte_class = alloc_with_gc(st, CLASS_KIND_CLASS, FORMAT_FIELDS, 4);
    st->root_values[sys_root_index_for_kind(CLASS_KIND_BYTES)] = (uint64_t)st->byte_class;
    OBJ_FIELD(st->byte_class, CLASS_SUPERCLASS) = tagged_nil();
    OBJ_FIELD(st->byte_class, CLASS_METHOD_DICT) = tagged_nil();
    OBJ_FIELD(st->byte_class, CLASS_INST_SIZE) = tag_smallint(0);
    OBJ_FIELD(st->byte_class, CLASS_INST_FORMAT) = tag_smallint(FORMAT_BYTES);
}

static void op_allocate(StressState *st)
{
    if (st->object_count >= DEFAULT_MAX_OBJECTS)
    {
        relieve_pressure(st);
        return;
    }

    uint64_t format = rng_range(st, 3);
    uint64_t size = rng_range(st, MAX_OBJECT_SLOTS + 1);
    StressClassKind class_kind = (format == FORMAT_FIELDS)
                                     ? CLASS_KIND_FIELDS
                                     : (format == FORMAT_INDEXABLE ? CLASS_KIND_INDEXABLE : CLASS_KIND_BYTES);
    uint32_t slot = (uint32_t)rng_range(st, st->cfg.root_slots);
    uint64_t *obj = alloc_with_gc(st, class_kind, format, size);

    StressObject *record = &st->objects[st->object_count];
    memset(record, 0, sizeof(*record));
    record->id = st->object_count + 1;
    record->class_kind = class_kind;
    record->format = format;
    record->size = size;
    record->ptr = obj;
    for (uint64_t i = 0; i < size && i < MAX_OBJECT_SLOTS; i++)
    {
        record->fields[i] = make_raw_value(0);
        record->bytes[i] = 0;
    }
    st->object_count++;

    st->root_model[slot] = make_object_value(record->id);
    st->root_values[SYS_ROOT_COUNT + slot] = (uint64_t)obj;
    stress_log(st,
               "step=%" PRIu64 " alloc slot=%u id=%u ptr=%p format=%" PRIu64 " size=%" PRIu64 " live_format=%" PRIu64,
               st->cfg.run_index, slot, record->id, (void *)obj, format, size, OBJ_FORMAT(obj));
}

static void op_store_word(StressState *st)
{
    uint32_t slot = (uint32_t)rng_range(st, st->cfg.root_slots);
    StressValue root = st->root_model[slot];
    if (root.kind != VALUE_OBJECT)
    {
        return;
    }
    StressObject *obj = object_by_id(st, root.object_id);
    uint64_t *live_ptr = resolve_live_object_ptr(st, root.object_id);
    if (obj == NULL || live_ptr == NULL || obj->format == FORMAT_BYTES || obj->size == 0)
    {
        return;
    }
    obj->ptr = live_ptr;

    uint64_t index = rng_range(st, obj->size);
    StressValue value = random_value(st);
    char value_desc[64];
    append_value_desc(value_desc, sizeof(value_desc), value);
    OBJ_FIELD(live_ptr, index) = actual_value_for_model(st, value);
    obj->fields[index] = value;
    stress_log(st, "step=%" PRIu64 " store-word slot=%u id=%u index=%" PRIu64 " value=%s",
               st->cfg.run_index, slot, obj->id, index, value_desc);
}

static void op_store_byte(StressState *st)
{
    uint32_t slot = (uint32_t)rng_range(st, st->cfg.root_slots);
    StressValue root = st->root_model[slot];
    if (root.kind != VALUE_OBJECT)
    {
        return;
    }
    StressObject *obj = object_by_id(st, root.object_id);
    uint64_t *live_ptr = resolve_live_object_ptr(st, root.object_id);
    if (obj == NULL || live_ptr == NULL || obj->format != FORMAT_BYTES || obj->size == 0)
    {
        return;
    }
    obj->ptr = live_ptr;

    uint64_t index = rng_range(st, obj->size);
    uint8_t byte = (uint8_t)rng_range(st, 256);
    uint8_t *bytes = (uint8_t *)&OBJ_FIELD(live_ptr, 0);
    bytes[index] = byte;
    obj->bytes[index] = byte;
    stress_log(st, "step=%" PRIu64 " store-byte slot=%u id=%u index=%" PRIu64 " value=%u",
               st->cfg.run_index, slot, obj->id, index, (unsigned)byte);
}

static void op_set_root(StressState *st)
{
    uint32_t dst = (uint32_t)rng_range(st, st->cfg.root_slots);
    StressValue value = random_value(st);
    uint64_t actual = actual_value_for_model(st, value);
    char value_desc[64];
    append_value_desc(value_desc, sizeof(value_desc), value);
    st->root_model[dst] = value;
    st->root_values[SYS_ROOT_COUNT + dst] = actual;
    stress_log(st, "step=%" PRIu64 " set-root slot=%u value=%s actual=%p",
               st->cfg.run_index, dst, value_desc, (void *)actual);
}

static void run_worker(StressConfig cfg)
{
    StressState st;
    memset(&st, 0, sizeof(st));
    st.cfg = cfg;
    st.cfg.state = (cfg.seed << 1) | 1;

    char current_log_path[1024];
    join_path(current_log_path, sizeof(current_log_path), cfg.log_dir, "current.log");
    st.log = fopen(current_log_path, "w");
    if (st.log == NULL)
    {
        fprintf(stderr, "gc_stress: open %s failed: %s\n", current_log_path, strerror(errno));
        exit(2);
    }
    setvbuf(st.log, NULL, _IOLBF, 0);

    st.space_a = (uint8_t *)malloc(cfg.space_size);
    st.space_b = (uint8_t *)malloc(cfg.space_size);
    if (st.space_a == NULL || st.space_b == NULL)
    {
        fatalf(&st, "failed to allocate semispaces");
    }
    gc_ctx_init(st.gc_ctx, st.space_a, st.space_b, cfg.space_size);

    for (uint32_t i = 0; i < cfg.root_slots; i++)
    {
        st.root_model[i] = make_raw_value(tagged_nil());
        st.root_values[SYS_ROOT_COUNT + i] = tagged_nil();
    }

    stress_log(&st, "seed=%" PRIu64 " steps=%" PRIu64 " roots=%u space=%" PRIu64 " run=%" PRIu64,
               cfg.seed, cfg.step_limit, cfg.root_slots, cfg.space_size, cfg.run_index);

    bootstrap_classes(&st);
    verify_roots(&st);

    for (uint64_t step = 0; cfg.step_limit == 0 || step < cfg.step_limit; step++)
    {
        st.cfg.run_index = step;
        switch (rng_range(&st, 8))
        {
        case 0:
        case 1:
        case 2:
            op_allocate(&st);
            break;
        case 3:
        case 4:
            op_store_word(&st);
            break;
        case 5:
            op_store_byte(&st);
            break;
        case 6:
            op_set_root(&st);
            break;
        default:
            perform_gc(&st, "random");
            break;
        }

        if (st.cfg.verify_every != 0 && (step % st.cfg.verify_every) == 0)
        {
            verify_roots(&st);
        }
    }

    verify_roots(&st);
    stress_log(&st, "completed seed=%" PRIu64, cfg.seed);
    fclose(st.log);
    free(st.space_a);
    free(st.space_b);
}

static int copy_file(const char *src, const char *dst)
{
    FILE *in = fopen(src, "rb");
    FILE *out;
    char buf[4096];
    size_t n;

    if (in == NULL)
    {
        return -1;
    }
    out = fopen(dst, "wb");
    if (out == NULL)
    {
        fclose(in);
        return -1;
    }
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0)
    {
        if (fwrite(buf, 1, n, out) != n)
        {
            fclose(in);
            fclose(out);
            return -1;
        }
    }
    fclose(in);
    fclose(out);
    return 0;
}

static uint64_t make_seed(uint64_t ordinal)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return ((uint64_t)ts.tv_sec << 32) ^ (uint64_t)ts.tv_nsec ^ (uint64_t)getpid() ^ ordinal;
}

static int run_supervisor(const StressConfig *cfg)
{
    char crashes_dir[1024];
    char current_log[1024];
    char summary_log[1024];
    join_path(crashes_dir, sizeof(crashes_dir), cfg->log_dir, "crashes");
    join_path(current_log, sizeof(current_log), cfg->log_dir, "current.log");
    join_path(summary_log, sizeof(summary_log), cfg->log_dir, "summary.log");
    ensure_dir_or_die(cfg->log_dir);
    ensure_dir_or_die(crashes_dir);

    FILE *summary = fopen(summary_log, "a");
    if (summary == NULL)
    {
        fprintf(stderr, "gc_stress: open %s failed: %s\n", summary_log, strerror(errno));
        return 2;
    }
    setvbuf(summary, NULL, _IOLBF, 0);

    for (uint64_t run = 0; cfg->runs == 0 || run < cfg->runs; run++)
    {
        uint64_t seed = cfg->seed_provided ? (cfg->seed + run) : make_seed(run + 1);
        pid_t pid = fork();
        if (pid < 0)
        {
            fprintf(stderr, "gc_stress: fork failed: %s\n", strerror(errno));
            fclose(summary);
            return 2;
        }
        if (pid == 0)
        {
            char seed_arg[64];
            char steps_arg[64];
            char roots_arg[32];
            char space_arg[64];
            char run_arg[64];
            snprintf(seed_arg, sizeof(seed_arg), "%" PRIu64, seed);
            snprintf(steps_arg, sizeof(steps_arg), "%" PRIu64, cfg->step_limit);
            snprintf(roots_arg, sizeof(roots_arg), "%u", cfg->root_slots);
            snprintf(space_arg, sizeof(space_arg), "%" PRIu64, cfg->space_size);
            snprintf(run_arg, sizeof(run_arg), "%" PRIu64, run);
            execl(cfg->self_path, cfg->self_path,
                  "--worker",
                  "--seed", seed_arg,
                  "--steps", steps_arg,
                  "--roots", roots_arg,
                  "--space-size", space_arg,
                  "--run-index", run_arg,
                  "--log-dir", cfg->log_dir,
                  (char *)NULL);
            _exit(127);
        }

        int status = 0;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
        {
            fprintf(summary, "run=%" PRIu64 " seed=%" PRIu64 " status=ok\n", run, seed);
            continue;
        }

        char crash_path[1024];
        snprintf(crash_path, sizeof(crash_path), "%s/crash-seed-%" PRIu64 "-run-%" PRIu64 ".log",
                 crashes_dir, seed, run);
        (void)copy_file(current_log, crash_path);
        if (WIFSIGNALED(status))
        {
            fprintf(summary, "run=%" PRIu64 " seed=%" PRIu64 " status=signal signal=%d crash=%s\n",
                    run, seed, WTERMSIG(status), crash_path);
        }
        else
        {
            fprintf(summary, "run=%" PRIu64 " seed=%" PRIu64 " status=exit code=%d crash=%s\n",
                    run, seed, WEXITSTATUS(status), crash_path);
        }
    }

    fclose(summary);
    return 0;
}

static int parse_u64(const char *arg, uint64_t *out)
{
    char *end = NULL;
    errno = 0;
    unsigned long long value = strtoull(arg, &end, 10);
    if (errno != 0 || end == arg || *end != '\0')
    {
        return 0;
    }
    *out = (uint64_t)value;
    return 1;
}

int main(int argc, char **argv)
{
    StressConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.seed = 1;
    cfg.step_limit = DEFAULT_MAX_RUN_STEPS;
    cfg.root_slots = DEFAULT_ROOT_SLOTS;
    cfg.space_size = DEFAULT_SPACE_SIZE;
    cfg.verify_every = 64;
    cfg.log_dir = "gc_stress_logs";
    cfg.self_path = argv[0];

    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--supervise") == 0)
        {
            cfg.supervise = true;
        }
        else if (strcmp(argv[i], "--worker") == 0)
        {
            cfg.supervise = false;
        }
        else if (strcmp(argv[i], "--seed") == 0 && i + 1 < argc)
        {
            if (!parse_u64(argv[++i], &cfg.seed))
            {
                return 2;
            }
            cfg.seed_provided = true;
        }
        else if (strcmp(argv[i], "--steps") == 0 && i + 1 < argc)
        {
            if (!parse_u64(argv[++i], &cfg.step_limit))
            {
                return 2;
            }
        }
        else if (strcmp(argv[i], "--roots") == 0 && i + 1 < argc)
        {
            uint64_t roots = 0;
            if (!parse_u64(argv[++i], &roots) || roots == 0 || roots > DEFAULT_ROOT_SLOTS)
            {
                return 2;
            }
            cfg.root_slots = (uint32_t)roots;
        }
        else if (strcmp(argv[i], "--space-size") == 0 && i + 1 < argc)
        {
            if (!parse_u64(argv[++i], &cfg.space_size))
            {
                return 2;
            }
        }
        else if (strcmp(argv[i], "--verify-every") == 0 && i + 1 < argc)
        {
            if (!parse_u64(argv[++i], &cfg.verify_every))
            {
                return 2;
            }
        }
        else if (strcmp(argv[i], "--runs") == 0 && i + 1 < argc)
        {
            if (!parse_u64(argv[++i], &cfg.runs))
            {
                return 2;
            }
        }
        else if (strcmp(argv[i], "--run-index") == 0 && i + 1 < argc)
        {
            if (!parse_u64(argv[++i], &cfg.run_index))
            {
                return 2;
            }
        }
        else if (strcmp(argv[i], "--log-dir") == 0 && i + 1 < argc)
        {
            cfg.log_dir = argv[++i];
        }
        else
        {
            fprintf(stderr,
                    "usage: %s [--supervise] [--runs N] [--steps N] [--roots N] [--space-size BYTES] [--log-dir DIR]\n",
                    argv[0]);
            return 2;
        }
    }

    ensure_dir_or_die(cfg.log_dir);
    if (cfg.supervise)
    {
        return run_supervisor(&cfg);
    }

    run_worker(cfg);
    return 0;
}
