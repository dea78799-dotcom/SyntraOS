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

static int fdt_regions_overlap(
    uint32_t first_offset,
    uint32_t first_size,
    uint32_t second_offset,
    uint32_t second_size
)
{
    if (first_size == 0U || second_size == 0U)
    {
        return 0;
    }

    return first_offset < second_offset + second_size &&
           second_offset < first_offset + first_size;
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
    uint32_t reserve_size;

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

        if (size == 0U || address > UINT64_MAX - size)
        {
            return SYNTRA_STATUS_ERROR;
        }
    }

    reserve_size = (uint32_t)(reserve_cursor - reserve_offset);

    /*
     * All three FDT data regions must be distinct. Overlapping blocks can
     * make a malformed blob reinterpret structure bytes as strings or
     * reservation entries.
     */
    if (fdt_regions_overlap(
            structure_offset, structure_size,
            strings_offset, strings_size) ||
        fdt_regions_overlap(
            structure_offset, structure_size,
            reserve_offset, reserve_size) ||
        fdt_regions_overlap(
            strings_offset, strings_size,
            reserve_offset, reserve_size))
    {
        return SYNTRA_STATUS_ERROR;
    }

    fdt->blob = bytes;
    fdt->total_size = total_size;
    fdt->structure = bytes + structure_offset;
    fdt->structure_size = structure_size;
    fdt->strings = bytes + strings_offset;
    fdt->strings_size = strings_size;
    fdt->reserve_map = bytes + reserve_offset;
    fdt->reserve_map_size = reserve_size;

    return SYNTRA_STATUS_OK;
}

syntra_status_t syntra_fdt_get_reservation_count(
    const syntra_fdt_t *fdt,
    size_t *reservation_count
)
{
    if (fdt == NULL || reservation_count == NULL ||
        fdt->reserve_map == NULL || fdt->reserve_map_size < 16U ||
        (fdt->reserve_map_size & 15U) != 0U)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    *reservation_count = (fdt->reserve_map_size / 16U) - 1U;
    return SYNTRA_STATUS_OK;
}

syntra_status_t syntra_fdt_get_reservation(
    const syntra_fdt_t *fdt,
    size_t index,
    uint64_t *address,
    uint64_t *size
)
{
    size_t count;
    size_t offset;
    syntra_status_t status;

    if (address == NULL || size == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    status = syntra_fdt_get_reservation_count(fdt, &count);
    if (status != SYNTRA_STATUS_OK)
    {
        return status;
    }

    if (index >= count)
    {
        return SYNTRA_STATUS_ERROR;
    }

    offset = index * 16U;
    *address = ((uint64_t)fdt_read_be32(fdt->reserve_map + offset) << 32U) |
               fdt_read_be32(fdt->reserve_map + offset + 4U);
    *size = ((uint64_t)fdt_read_be32(fdt->reserve_map + offset + 8U) << 32U) |
            fdt_read_be32(fdt->reserve_map + offset + 12U);

    if (*size == 0U || *address > UINT64_MAX - *size)
    {
        return SYNTRA_STATUS_ERROR;
    }

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
    token->offset = cursor - 4U;
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

static int fdt_string_equals(const char *left, const char *right)
{
    size_t index = 0U;

    if (left == NULL || right == NULL)
    {
        return 0;
    }

    while (left[index] != '\0' && right[index] != '\0')
    {
        if (left[index] != right[index])
        {
            return 0;
        }
        ++index;
    }

    return left[index] == right[index];
}

static int fdt_string_list_contains(
    const uint8_t *data,
    uint32_t data_size,
    const char *value
)
{
    size_t value_length = 0U;
    size_t offset = 0U;

    if (data == NULL || value == NULL)
    {
        return 0;
    }

    while (value[value_length] != '\0')
    {
        ++value_length;
    }

    while (offset < data_size)
    {
        size_t remaining = (size_t)data_size - offset;
        size_t item_length;

        if (!fdt_find_string_end(data + offset, remaining, &item_length))
        {
            return 0;
        }

        if (item_length == value_length)
        {
            size_t index;
            int equal = 1;

            for (index = 0U; index < item_length; ++index)
            {
                if ((char)data[offset + index] != value[index])
                {
                    equal = 0;
                    break;
                }
            }

            if (equal)
            {
                return 1;
            }
        }

        offset += item_length + 1U;
    }

    return 0;
}

syntra_status_t syntra_fdt_find_compatible(
    const syntra_fdt_t *fdt,
    const char *compatible,
    syntra_fdt_node_t *node
)
{
    syntra_fdt_iterator_t iterator;
    syntra_fdt_token_t token;
    syntra_fdt_node_t candidate;
    int candidate_valid = 0;
    syntra_status_t status;

    if (fdt == NULL || compatible == NULL || node == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    syntra_fdt_iterator_init(&iterator);

    for (;;)
    {
        status = syntra_fdt_next(fdt, &iterator, &token);
        if (status != SYNTRA_STATUS_OK)
        {
            return status == SYNTRA_STATUS_NOT_IMPLEMENTED
                ? SYNTRA_STATUS_ERROR
                : status;
        }

        if (token.type == SYNTRA_FDT_TOKEN_BEGIN_NODE)
        {
            candidate.begin_offset = token.offset;
            candidate.depth = token.depth;
            candidate_valid = 1;
        }
        else if (token.type == SYNTRA_FDT_TOKEN_PROPERTY &&
                 candidate_valid &&
                 token.depth == candidate.depth + 1U &&
                 fdt_string_equals(token.name, "compatible") &&
                 fdt_string_list_contains(
                     token.data,
                     token.data_size,
                     compatible))
        {
            *node = candidate;
            return SYNTRA_STATUS_OK;
        }
        else if (token.type == SYNTRA_FDT_TOKEN_END_NODE &&
                 candidate_valid &&
                 token.depth == candidate.depth)
        {
            candidate_valid = 0;
        }
    }
}


syntra_status_t syntra_fdt_find_phandle(
    const syntra_fdt_t *fdt,
    uint32_t phandle,
    syntra_fdt_node_t *node)
{
    syntra_fdt_iterator_t iterator;
    syntra_fdt_token_t token;
    syntra_fdt_node_t stack[64];
    syntra_status_t status;

    if (fdt == NULL || node == NULL || phandle == 0U)
        return SYNTRA_STATUS_INVALID_ARGUMENT;

    syntra_fdt_iterator_init(&iterator);
    for (;;)
    {
        status = syntra_fdt_next(fdt, &iterator, &token);
        if (status != SYNTRA_STATUS_OK)
            return status == SYNTRA_STATUS_NOT_IMPLEMENTED
                ? SYNTRA_STATUS_NOT_IMPLEMENTED : status;

        if (token.type == SYNTRA_FDT_TOKEN_BEGIN_NODE)
        {
            if (token.depth >= 64U)
                return SYNTRA_STATUS_NOT_IMPLEMENTED;
            stack[token.depth].begin_offset = token.offset;
            stack[token.depth].depth = token.depth;
        }
        else if (token.type == SYNTRA_FDT_TOKEN_PROPERTY &&
                 token.depth != 0U && token.depth <= 64U &&
                 token.data_size == 4U &&
                 (fdt_string_equals(token.name, "phandle") ||
                  fdt_string_equals(token.name, "linux,phandle")))
        {
            uint32_t value = fdt_read_be32(token.data);
            if (value == phandle)
            {
                *node = stack[token.depth - 1U];
                return SYNTRA_STATUS_OK;
            }
        }
    }
}

syntra_status_t syntra_fdt_get_property(
    const syntra_fdt_t *fdt,
    const syntra_fdt_node_t *node,
    const char *property_name,
    const uint8_t **data,
    uint32_t *data_size
)
{
    syntra_fdt_iterator_t iterator;
    syntra_fdt_token_t token;
    syntra_status_t status;

    if (fdt == NULL || node == NULL || property_name == NULL ||
        data == NULL || data_size == NULL ||
        node->begin_offset >= fdt->structure_size)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    iterator.offset = node->begin_offset;
    iterator.depth = node->depth;
    iterator.ended = 0U;

    status = syntra_fdt_next(fdt, &iterator, &token);
    if (status != SYNTRA_STATUS_OK ||
        token.type != SYNTRA_FDT_TOKEN_BEGIN_NODE ||
        token.depth != node->depth)
    {
        return SYNTRA_STATUS_ERROR;
    }

    for (;;)
    {
        status = syntra_fdt_next(fdt, &iterator, &token);
        if (status != SYNTRA_STATUS_OK)
        {
            return status == SYNTRA_STATUS_NOT_IMPLEMENTED
                ? SYNTRA_STATUS_NOT_IMPLEMENTED
                : status;
        }

        if (token.type == SYNTRA_FDT_TOKEN_PROPERTY &&
            token.depth == node->depth + 1U)
        {
            if (fdt_string_equals(token.name, property_name))
            {
                *data = token.data;
                *data_size = token.data_size;
                return SYNTRA_STATUS_OK;
            }
        }
        else if (token.type == SYNTRA_FDT_TOKEN_BEGIN_NODE ||
                 (token.type == SYNTRA_FDT_TOKEN_END_NODE &&
                  token.depth == node->depth))
        {
            return SYNTRA_STATUS_NOT_IMPLEMENTED;
        }
    }
}

static syntra_status_t fdt_get_parent_node(
    const syntra_fdt_t *fdt,
    const syntra_fdt_node_t *child,
    syntra_fdt_node_t *parent
)
{
    syntra_fdt_iterator_t iterator;
    syntra_fdt_token_t token;
    syntra_fdt_node_t candidate;
    int candidate_valid = 0;
    syntra_status_t status;

    if (fdt == NULL || child == NULL || parent == NULL ||
        child->depth == 0U)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    syntra_fdt_iterator_init(&iterator);

    while ((status = syntra_fdt_next(fdt, &iterator, &token)) ==
           SYNTRA_STATUS_OK)
    {
        if (token.type == SYNTRA_FDT_TOKEN_BEGIN_NODE)
        {
            if (token.depth + 1U == child->depth)
            {
                candidate.begin_offset = token.offset;
                candidate.depth = token.depth;
                candidate_valid = 1;
            }
            else if (token.depth == child->depth &&
                     token.offset == child->begin_offset &&
                     candidate_valid)
            {
                *parent = candidate;
                return SYNTRA_STATUS_OK;
            }
        }
    }

    return SYNTRA_STATUS_ERROR;
}


syntra_status_t syntra_fdt_get_inherited_property(
    const syntra_fdt_t *fdt,
    const syntra_fdt_node_t *node,
    const char *property_name,
    const uint8_t **data,
    uint32_t *data_size)
{
    syntra_fdt_node_t current;
    syntra_status_t status;
    unsigned int depth;

    if (fdt == NULL || node == NULL || property_name == NULL ||
        data == NULL || data_size == NULL)
        return SYNTRA_STATUS_INVALID_ARGUMENT;

    current = *node;
    for (depth = 0U; depth < 64U; ++depth)
    {
        status = syntra_fdt_get_property(
            fdt, &current, property_name, data, data_size);
        if (status == SYNTRA_STATUS_OK) return SYNTRA_STATUS_OK;
        if (status != SYNTRA_STATUS_NOT_IMPLEMENTED) return status;
        if (current.depth == 0U) return SYNTRA_STATUS_NOT_IMPLEMENTED;

        status = fdt_get_parent_node(fdt, &current, &current);
        if (status != SYNTRA_STATUS_OK) return status;
    }

    return SYNTRA_STATUS_NOT_IMPLEMENTED;
}

static int fdt_read_cells(
    const uint8_t *data,
    uint32_t cell_count,
    uint64_t *value
)
{
    uint64_t result = 0U;
    uint32_t index;

    if (data == NULL || value == NULL ||
        cell_count == 0U || cell_count > 2U)
    {
        return 0;
    }

    for (index = 0U; index < cell_count; ++index)
    {
        result = (result << 32) | fdt_read_be32(data + index * 4U);
    }

    *value = result;
    return 1;
}

static syntra_status_t fdt_get_cell_count(
    const syntra_fdt_t *fdt,
    const syntra_fdt_node_t *node,
    const char *property_name,
    uint32_t default_value,
    uint32_t *cell_count
)
{
    const uint8_t *data;
    uint32_t data_size;
    syntra_status_t status;

    if (cell_count == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    status = syntra_fdt_get_property(
        fdt, node, property_name, &data, &data_size);
    if (status == SYNTRA_STATUS_NOT_IMPLEMENTED)
    {
        *cell_count = default_value;
        return SYNTRA_STATUS_OK;
    }

    if (status != SYNTRA_STATUS_OK)
    {
        return status;
    }

    if (data_size != 4U)
    {
        return SYNTRA_STATUS_ERROR;
    }

    *cell_count = fdt_read_be32(data);
    return SYNTRA_STATUS_OK;
}

syntra_status_t syntra_fdt_get_reg_count(
    const syntra_fdt_t *fdt,
    const syntra_fdt_node_t *node,
    size_t *reg_count
)
{
    syntra_fdt_node_t parent;
    const uint8_t *reg_data;
    uint32_t reg_size;
    uint32_t address_cells;
    uint32_t size_cells;
    size_t tuple_size;
    syntra_status_t status;

    if (fdt == NULL || node == NULL || reg_count == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    status = fdt_get_parent_node(fdt, node, &parent);
    if (status != SYNTRA_STATUS_OK)
    {
        return SYNTRA_STATUS_ERROR;
    }

    status = fdt_get_cell_count(
        fdt, &parent, "#address-cells", 2U, &address_cells);
    if (status != SYNTRA_STATUS_OK)
    {
        return status;
    }

    status = fdt_get_cell_count(
        fdt, &parent, "#size-cells", 1U, &size_cells);
    if (status != SYNTRA_STATUS_OK)
    {
        return status;
    }

    if (address_cells == 0U || address_cells > 2U ||
        size_cells == 0U || size_cells > 2U)
    {
        return SYNTRA_STATUS_NOT_IMPLEMENTED;
    }

    status = syntra_fdt_get_property(
        fdt, node, "reg", &reg_data, &reg_size);
    if (status != SYNTRA_STATUS_OK)
    {
        return SYNTRA_STATUS_ERROR;
    }

    tuple_size = ((size_t)address_cells + (size_t)size_cells) * 4U;
    if (tuple_size == 0U || reg_size == 0U ||
        (size_t)reg_size % tuple_size != 0U)
    {
        return SYNTRA_STATUS_ERROR;
    }

    *reg_count = (size_t)reg_size / tuple_size;
    return SYNTRA_STATUS_OK;
}

syntra_status_t syntra_fdt_get_reg(
    const syntra_fdt_t *fdt,
    const syntra_fdt_node_t *node,
    uint32_t reg_index,
    uintptr_t *base,
    size_t *size
)
{
    syntra_fdt_node_t parent;
    const uint8_t *reg_data;
    uint32_t reg_size;
    uint32_t address_cells;
    uint32_t size_cells;
    size_t tuple_size;
    size_t offset;
    uint64_t address;
    uint64_t region_size;

    if (fdt == NULL || node == NULL || base == NULL || size == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (fdt_get_parent_node(fdt, node, &parent) != SYNTRA_STATUS_OK)
    {
        return SYNTRA_STATUS_ERROR;
    }

    {
        syntra_status_t status;

        status = fdt_get_cell_count(
            fdt, &parent, "#address-cells", 2U, &address_cells);
        if (status != SYNTRA_STATUS_OK)
        {
            return status;
        }

        status = fdt_get_cell_count(
            fdt, &parent, "#size-cells", 1U, &size_cells);
        if (status != SYNTRA_STATUS_OK)
        {
            return status;
        }
    }

    if (address_cells == 0U || address_cells > 2U ||
        size_cells == 0U || size_cells > 2U)
    {
        return SYNTRA_STATUS_NOT_IMPLEMENTED;
    }

    if (syntra_fdt_get_property(
            fdt, node, "reg", &reg_data, &reg_size) != SYNTRA_STATUS_OK)
    {
        return SYNTRA_STATUS_ERROR;
    }

    tuple_size = ((size_t)address_cells + size_cells) * 4U;
    if (tuple_size == 0U || reg_size == 0U ||
        (size_t)reg_size % tuple_size != 0U ||
        (size_t)reg_index >= (size_t)reg_size / tuple_size)
    {
        return SYNTRA_STATUS_ERROR;
    }

    offset = (size_t)reg_index * tuple_size;

    if (!fdt_read_cells(reg_data + offset, address_cells, &address) ||
        !fdt_read_cells(
            reg_data + offset + (size_t)address_cells * 4U,
            size_cells,
            &region_size))
    {
        return SYNTRA_STATUS_ERROR;
    }

    if (address > (uint64_t)UINTPTR_MAX ||
        region_size > (uint64_t)SIZE_MAX ||
        address == 0U || region_size == 0U ||
        address > (uint64_t)UINTPTR_MAX - (uintptr_t)region_size)
    {
        return SYNTRA_STATUS_ERROR;
    }

    *base = (uintptr_t)address;
    *size = (size_t)region_size;
    return SYNTRA_STATUS_OK;
}
