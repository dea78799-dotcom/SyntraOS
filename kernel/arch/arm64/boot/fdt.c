#include "fdt.h"

#include <stddef.h>
#include <stdint.h>

#define FDT_HEADER_SIZE 40U
#define FDT_BEGIN_NODE 1U
#define FDT_END_NODE 2U
#define FDT_PROP 3U
#define FDT_NOP 4U
#define FDT_END 9U

static uint32_t fdt_read_be32(const uint8_t *data)
{
    return ((uint32_t)data[0] << 24) |
           ((uint32_t)data[1] << 16) |
           ((uint32_t)data[2] << 8) |
           (uint32_t)data[3];
}

static int fdt_range_valid(
    uint32_t offset,
    uint32_t length,
    uint32_t total_size
)
{
    return offset <= total_size && length <= total_size - offset;
}

static size_t fdt_align4(size_t value)
{
    return (value + 3U) & ~(size_t)3U;
}

static int fdt_find_string_end(
    const uint8_t *data,
    size_t available,
    size_t *length
)
{
    size_t index;

    for (index = 0; index < available; ++index)
    {
        if (data[index] == 0)
        {
            *length = index;
            return 1;
        }
    }

    return 0;
}

syntra_status_t syntra_fdt_open(
    const void *blob,
    size_t accessible_size,
    syntra_fdt_t *fdt
)
{
    const uint8_t *bytes = (const uint8_t *)blob;
    uint32_t total_size;
    uint32_t structure_offset;
    uint32_t strings_offset;
    uint32_t reserve_offset;
    uint32_t version;
    uint32_t last_compatible_version;
    uint32_t strings_size;
    uint32_t structure_size;
    size_t reserve_cursor;

    if (bytes == NULL || fdt == NULL || accessible_size < FDT_HEADER_SIZE)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (fdt_read_be32(bytes) != SYNTRA_FDT_MAGIC)
    {
        return SYNTRA_STATUS_ERROR;
    }

    total_size = fdt_read_be32(bytes + 4);
    structure_offset = fdt_read_be32(bytes + 8);
    strings_offset = fdt_read_be32(bytes + 12);
    reserve_offset = fdt_read_be32(bytes + 16);
    version = fdt_read_be32(bytes + 20);
    last_compatible_version = fdt_read_be32(bytes + 24);
    strings_size = fdt_read_be32(bytes + 32);
    structure_size = fdt_read_be32(bytes + 36);

    if (total_size < FDT_HEADER_SIZE || total_size > accessible_size)
    {
        return SYNTRA_STATUS_ERROR;
    }

    if (version < 17U || version > SYNTRA_FDT_MAX_VERSION ||
        last_compatible_version > version ||
        last_compatible_version > SYNTRA_FDT_MAX_VERSION)
    {
        return SYNTRA_STATUS_NOT_IMPLEMENTED;
    }

    if ((structure_offset & 3U) != 0U || (reserve_offset & 7U) != 0U)
    {
        return SYNTRA_STATUS_ERROR;
    }

    if (!fdt_range_valid(structure_offset, structure_size, total_size) ||
        !fdt_range_valid(strings_offset, strings_size, total_size) ||
        reserve_offset > total_size ||
        structure_offset < FDT_HEADER_SIZE ||
        strings_offset < FDT_HEADER_SIZE ||
        reserve_offset < FDT_HEADER_SIZE ||
        structure_size == 0U)
    {
        return SYNTRA_STATUS_ERROR;
    }

    /* The reservation map is a sequence of 16-byte entries ending in zeroes. */
    reserve_cursor = reserve_offset;
    for (;;)
    {
        uint64_t address;
        uint64_t size;

        if (reserve_cursor > total_size ||
            total_size - reserve_cursor < 16U)
        {
            return SYNTRA_STATUS_ERROR;
        }

        address = ((uint64_t)fdt_read_be32(bytes + reserve_cursor) << 32) |
                  fdt_read_be32(bytes + reserve_cursor + 4U);
        size = ((uint64_t)fdt_read_be32(bytes + reserve_cursor + 8U) << 32) |
               fdt_read_be32(bytes + reserve_cursor + 12U);
        reserve_cursor += 16U;

        if (address == 0U && size == 0U)
        {
            break;
        }
    }

    fdt->blob = bytes;
    fdt->total_size = total_size;
    fdt->structure = bytes + structure_offset;
    fdt->structure_size = structure_size;
    fdt->strings = bytes + strings_offset;
    fdt->strings_size = strings_size;

    return SYNTRA_STATUS_OK;
}

void syntra_fdt_iterator_init(syntra_fdt_iterator_t *iterator)
{
    if (iterator == NULL)
    {
        return;
    }

    iterator->offset = 0U;
    iterator->depth = 0U;
    iterator->ended = 0U;
}

syntra_status_t syntra_fdt_next(
    const syntra_fdt_t *fdt,
    syntra_fdt_iterator_t *iterator,
    syntra_fdt_token_t *token
)
{
    uint32_t type;
    size_t cursor;

    if (fdt == NULL || iterator == NULL || token == NULL ||
        fdt->structure == NULL || fdt->strings == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (iterator->ended != 0U)
    {
        return SYNTRA_STATUS_NOT_IMPLEMENTED;
    }

    if (iterator->offset > fdt->structure_size ||
        fdt->structure_size - iterator->offset < 4U)
    {
        return SYNTRA_STATUS_ERROR;
    }

    cursor = iterator->offset;
    type = fdt_read_be32(fdt->structure + cursor);
    cursor += 4U;

    token->type = (syntra_fdt_token_type_t)type;
    token->depth = iterator->depth;
    token->name = NULL;
    token->data = NULL;
    token->data_size = 0U;

    if (type == FDT_BEGIN_NODE)
    {
        size_t name_length;

        if (!fdt_find_string_end(
                fdt->structure + cursor,
                fdt->structure_size - cursor,
                &name_length))
        {
            return SYNTRA_STATUS_ERROR;
        }

        token->name = (const char *)(fdt->structure + cursor);
        cursor = fdt_align4(cursor + name_length + 1U);
        if (cursor > fdt->structure_size || iterator->depth == UINT32_MAX)
        {
            return SYNTRA_STATUS_ERROR;
        }

        iterator->depth++;
    }
    else if (type == FDT_END_NODE)
    {
        if (iterator->depth == 0U)
        {
            return SYNTRA_STATUS_ERROR;
        }

        iterator->depth--;
        token->depth = iterator->depth;
    }
    else if (type == FDT_PROP)
    {
        uint32_t data_size;
        uint32_t name_offset;
        size_t name_length;

        if (fdt->structure_size - cursor < 8U || iterator->depth == 0U)
        {
            return SYNTRA_STATUS_ERROR;
        }

        data_size = fdt_read_be32(fdt->structure + cursor);
        name_offset = fdt_read_be32(fdt->structure + cursor + 4U);
        cursor += 8U;

        if (data_size > fdt->structure_size - cursor ||
            name_offset >= fdt->strings_size ||
            !fdt_find_string_end(
                fdt->strings + name_offset,
                fdt->strings_size - name_offset,
                &name_length))
        {
            return SYNTRA_STATUS_ERROR;
        }

        token->name = (const char *)(fdt->strings + name_offset);
        token->data = fdt->structure + cursor;
        token->data_size = data_size;
        cursor = fdt_align4(cursor + data_size);
        if (cursor > fdt->structure_size)
        {
            return SYNTRA_STATUS_ERROR;
        }
    }
    else if (type == FDT_NOP)
    {
        /* No payload. */
    }
    else if (type == FDT_END)
    {
        if (iterator->depth != 0U)
        {
            return SYNTRA_STATUS_ERROR;
        }

        iterator->ended = 1U;
    }
    else
    {
        return SYNTRA_STATUS_ERROR;
    }

    iterator->offset = cursor;
    return SYNTRA_STATUS_OK;
}
