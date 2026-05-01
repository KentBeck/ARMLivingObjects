#include "vm_defs.h"

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

extern uint64_t om_page_bytes(void);

typedef struct ImageCheckpointHeader
{
    uint64_t magic;
    uint64_t used_size;
    uint64_t page_bytes;
    uint64_t page_count;
    uint64_t generation;
    uint64_t symbol_table_offset;
    uint64_t symbol_class_offset;
    uint64_t context_class_offset;
    uint64_t smalltalk_dict_offset;
    uint64_t class_table_offset;
    uint64_t metadata_checksum;
} ImageCheckpointHeader;

static int checkpoint_test_force_dir_fsync_failure = 0;

#define IMAGE_CHECKPOINT_MAGIC UINT64_C(0x41524c4f494d4731)

static uint64_t checkpoint_page_checksum(const uint8_t *bytes, uint64_t size)
{
    uint64_t hash = UINT64_C(1469598103934665603);

    for (uint64_t index = 0; index < size; index++)
    {
        hash ^= bytes[index];
        hash *= UINT64_C(1099511628211);
    }

    return hash;
}

static uint64_t checkpoint_hash_bytes(uint64_t hash, const uint8_t *bytes, uint64_t size)
{
    for (uint64_t index = 0; index < size; index++)
    {
        hash ^= bytes[index];
        hash *= UINT64_C(1099511628211);
    }

    return hash;
}

static uint64_t checkpoint_metadata_checksum(const ImageCheckpointHeader *header,
                                             const uint64_t *page_state_table,
                                             const uint64_t *page_used_table,
                                             const uint64_t *page_first_table,
                                             const uint64_t *page_covering_table,
                                             const uint64_t *page_checksum_table)
{
    ImageCheckpointHeader copy = *header;
    uint64_t hash = UINT64_C(1469598103934665603);

    copy.metadata_checksum = 0;
    hash = checkpoint_hash_bytes(hash, (const uint8_t *)&copy, sizeof(copy));
    if (copy.page_count != 0)
    {
        uint64_t table_bytes = copy.page_count * sizeof(uint64_t);
        hash = checkpoint_hash_bytes(hash, (const uint8_t *)page_state_table, table_bytes);
        hash = checkpoint_hash_bytes(hash, (const uint8_t *)page_used_table, table_bytes);
        hash = checkpoint_hash_bytes(hash, (const uint8_t *)page_first_table, table_bytes);
        hash = checkpoint_hash_bytes(hash, (const uint8_t *)page_covering_table, table_bytes);
        hash = checkpoint_hash_bytes(hash, (const uint8_t *)page_checksum_table, table_bytes);
    }
    return hash;
}

static int checkpoint_header_is_valid(const ImageCheckpointHeader *header,
                                      const uint64_t *page_state_table,
                                      const uint64_t *page_used_table,
                                      const uint64_t *page_first_table,
                                      const uint64_t *page_covering_table,
                                      const uint64_t *page_checksum_table)
{
    if (header->magic != IMAGE_CHECKPOINT_MAGIC ||
        (header->page_bytes != 0 && header->page_bytes != OM_PAGE_BYTES))
    {
        return 0;
    }

    for (uint64_t page_id = 0; page_id < header->page_count; page_id++)
    {
        if (page_state_table[page_id] != OM_PAGE_STATE_FREE &&
            page_state_table[page_id] != OM_PAGE_STATE_HEAD &&
            page_state_table[page_id] != OM_PAGE_STATE_CONTINUATION)
        {
            return 0;
        }
        if (page_state_table[page_id] == OM_PAGE_STATE_FREE &&
            (page_used_table[page_id] != 0 ||
             page_first_table[page_id] != 0 ||
             page_covering_table[page_id] != 0))
        {
            return 0;
        }
    }

    return header->metadata_checksum ==
           checkpoint_metadata_checksum(header,
                                        page_state_table,
                                        page_used_table,
                                        page_first_table,
                                        page_covering_table,
                                        page_checksum_table);
}

static int checkpoint_load_page_tables(FILE *file,
                                       uint64_t page_count,
                                       uint64_t **page_state_table_out,
                                       uint64_t **page_used_table_out,
                                       uint64_t **page_first_table_out,
                                       uint64_t **page_covering_table_out,
                                       uint64_t **page_checksum_table_out)
{
    uint64_t *page_state_table = NULL;
    uint64_t *page_used_table = NULL;
    uint64_t *page_first_table = NULL;
    uint64_t *page_covering_table = NULL;
    uint64_t *page_checksum_table = NULL;

    if (page_count == 0)
    {
        *page_state_table_out = NULL;
        *page_used_table_out = NULL;
        *page_first_table_out = NULL;
        *page_covering_table_out = NULL;
        *page_checksum_table_out = NULL;
        return 1;
    }

    page_state_table = (uint64_t *)malloc((size_t)(page_count * sizeof(uint64_t)));
    page_used_table = (uint64_t *)malloc((size_t)(page_count * sizeof(uint64_t)));
    page_first_table = (uint64_t *)malloc((size_t)(page_count * sizeof(uint64_t)));
    page_covering_table = (uint64_t *)malloc((size_t)(page_count * sizeof(uint64_t)));
    page_checksum_table = (uint64_t *)malloc((size_t)(page_count * sizeof(uint64_t)));
    if (page_state_table == NULL || page_used_table == NULL || page_first_table == NULL ||
        page_covering_table == NULL || page_checksum_table == NULL)
    {
        free(page_state_table);
        free(page_used_table);
        free(page_first_table);
        free(page_covering_table);
        free(page_checksum_table);
        return 0;
    }

    if (fread(page_state_table, sizeof(uint64_t), (size_t)page_count, file) != page_count ||
        fread(page_used_table, sizeof(uint64_t), (size_t)page_count, file) != page_count ||
        fread(page_first_table, sizeof(uint64_t), (size_t)page_count, file) != page_count ||
        fread(page_covering_table, sizeof(uint64_t), (size_t)page_count, file) != page_count ||
        fread(page_checksum_table, sizeof(uint64_t), (size_t)page_count, file) != page_count)
    {
        free(page_state_table);
        free(page_used_table);
        free(page_first_table);
        free(page_covering_table);
        free(page_checksum_table);
        return 0;
    }

    *page_state_table_out = page_state_table;
    *page_used_table_out = page_used_table;
    *page_first_table_out = page_first_table;
    *page_covering_table_out = page_covering_table;
    *page_checksum_table_out = page_checksum_table;
    return 1;
}

void checkpoint_set_test_dir_fsync_failure(int enabled)
{
    checkpoint_test_force_dir_fsync_failure = enabled;
}

int checkpoint_fsync_parent_directory(const char *path)
{
    const char *slash;
    char dir_path[1024];
    int dir_fd;
    int ok = 0;

    if (checkpoint_test_force_dir_fsync_failure)
    {
        return 0;
    }

    slash = strrchr(path, '/');
    if (slash == NULL)
    {
        strcpy(dir_path, ".");
    }
    else if (slash == path)
    {
        strcpy(dir_path, "/");
    }
    else
    {
        size_t dir_len = (size_t)(slash - path);
        if (dir_len + 1 > sizeof(dir_path))
        {
            return 0;
        }
        memcpy(dir_path, path, dir_len);
        dir_path[dir_len] = '\0';
    }

    dir_fd = open(dir_path, O_RDONLY | O_DIRECTORY);
    if (dir_fd < 0)
    {
        return 0;
    }

    if (fsync(dir_fd) == 0)
    {
        ok = 1;
    }
    close(dir_fd);
    return ok;
}

int image_checkpoint_validate(const char *path)
{
    FILE *file;
    ImageCheckpointHeader header;
    uint64_t *page_state_table = NULL;
    uint64_t *page_used_table = NULL;
    uint64_t *page_first_table = NULL;
    uint64_t *page_covering_table = NULL;
    uint64_t *page_checksum_table = NULL;
    uint8_t *page_buffer = NULL;
    int valid = 0;

    file = fopen(path, "rb");
    if (file == NULL)
    {
        return 0;
    }
    if (fread(&header, sizeof(header), 1, file) != 1)
    {
        fclose(file);
        return 0;
    }

    if (!checkpoint_load_page_tables(file, header.page_count,
                                     &page_state_table,
                                     &page_used_table,
                                     &page_first_table,
                                     &page_covering_table,
                                     &page_checksum_table))
    {
        fclose(file);
        return 0;
    }
    if (!checkpoint_header_is_valid(&header,
                                    page_state_table,
                                    page_used_table,
                                    page_first_table,
                                    page_covering_table,
                                    page_checksum_table))
    {
        free(page_state_table);
        free(page_used_table);
        free(page_first_table);
        free(page_covering_table);
        free(page_checksum_table);
        fclose(file);
        return 0;
    }

    page_buffer = (uint8_t *)malloc((size_t)om_page_bytes());
    if (page_buffer == NULL)
    {
        free(page_state_table);
        free(page_used_table);
        free(page_first_table);
        free(page_covering_table);
        free(page_checksum_table);
        fclose(file);
        return 0;
    }

    valid = 1;
    for (uint64_t page_id = 0; page_id < header.page_count; page_id++)
    {
        if (fread(page_buffer, 1, (size_t)om_page_bytes(), file) != om_page_bytes())
        {
            valid = 0;
            break;
        }
        if (page_state_table[page_id] == OM_PAGE_STATE_FREE)
        {
            if (page_checksum_table[page_id] != checkpoint_page_checksum(page_buffer, om_page_bytes()))
            {
                valid = 0;
                break;
            }
            continue;
        }
        if (checkpoint_page_checksum(page_buffer, om_page_bytes()) != page_checksum_table[page_id])
        {
            valid = 0;
            break;
        }
    }

    free(page_buffer);
    free(page_state_table);
    free(page_used_table);
    free(page_first_table);
    free(page_covering_table);
    free(page_checksum_table);
    fclose(file);
    return valid;
}
