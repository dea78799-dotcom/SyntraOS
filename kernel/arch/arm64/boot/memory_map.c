#include "memory_map.h"

#include <stddef.h>
#include <stdint.h>

static int memory_name_matches(const char *name)
{
    static const char prefix[] = "memory";
    size_t index;

    if (name == NULL)
    {
        return 0;
    }

    for (index = 0U; index < sizeof(prefix) - 1U; ++index)
    {
        if (name[index] != prefix[index])
        {
            return 0;
        }
    }

    return name[index] == '\0' || name[index] == '@';
}

static int property_string_equals(
    const uint8_t *data,
    uint32_t size,
    const char *expected
)
{
    size_t index = 0U;

    if (data == NULL || expected == NULL)
    {
        return 0;
    }

    while (index < size && expected[index] != '\0')
    {
        if ((char)data[index] != expected[index])
        {
            return 0;
        }
        ++index;
    }

    return index < size && data[index] == 0U && expected[index] == '\0';
}

static int node_is_disabled(
    const syntra_fdt_t *fdt,
    const syntra_fdt_node_t *node
)
{
    const uint8_t *data;
    uint32_t size;

    if (syntra_fdt_get_property(
            fdt, node, "status", &data, &size) != SYNTRA_STATUS_OK)
    {
        return 0;
    }

    return property_string_equals(data, size, "disabled") ||
           property_string_equals(data, size, "fail");
}

static int node_is_memory(
    const syntra_fdt_t *fdt,
    const syntra_fdt_node_t *node,
    const char *name
)
{
    const uint8_t *data;
    uint32_t size;

    if (memory_name_matches(name))
    {
        return 1;
    }

    if (syntra_fdt_get_property(
            fdt, node, "device_type", &data, &size) != SYNTRA_STATUS_OK)
    {
        return 0;
    }

    return property_string_equals(data, size, "memory");
}

static syntra_status_t add_node_reg(
    const syntra_fdt_t *fdt,
    const syntra_fdt_node_t *node,
    syntra_memory_region_type_t type
)
{
    size_t count;
    size_t index;
    syntra_status_t status;

    status = syntra_fdt_get_reg_count(fdt, node, &count);
    if (status != SYNTRA_STATUS_OK)
    {
        return status;
    }

    for (index = 0U; index < count; ++index)
    {
        uintptr_t base;
        size_t size;

        status = syntra_fdt_get_reg(
            fdt, node, (uint32_t)index, &base, &size);
        if (status != SYNTRA_STATUS_OK)
        {
            return status;
        }

        status = syntra_memory_region_add(base, size, type);
        if (status != SYNTRA_STATUS_OK)
        {
            return status;
        }
    }

    return SYNTRA_STATUS_OK;
}

/*
 * Reservations outside discovered RAM (for example reserved MMIO) do not
 * enter the RAM allocator map. A range already covered by a reservation is
 * accepted; partial overlap is rejected rather than silently misclassified.
 */
static syntra_status_t reserve_ram_range(
    uintptr_t base,
    size_t size,
    syntra_memory_region_type_t type
)
{
    syntra_memory_region_t region;
    uintptr_t end;

    if (size == 0U || base > UINTPTR_MAX - size)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    end = base + size;

    if (syntra_memory_region_find(base, &region) != SYNTRA_STATUS_OK)
    {
        return SYNTRA_STATUS_OK;
    }

    if (region.type != SYNTRA_MEMORY_REGION_RAM)
    {
        uintptr_t region_end = region.base + region.size;
        return end <= region_end ? SYNTRA_STATUS_OK : SYNTRA_STATUS_ERROR;
    }

    if (end > region.base + region.size)
    {
        return SYNTRA_STATUS_ERROR;
    }

    return syntra_memory_region_reserve(base, size, type);
}

static syntra_status_t apply_header_reservations(const syntra_fdt_t *fdt)
{
    size_t count;
    size_t index;
    syntra_status_t status;

    status = syntra_fdt_get_reservation_count(fdt, &count);
    if (status != SYNTRA_STATUS_OK)
    {
        return status;
    }

    for (index = 0U; index < count; ++index)
    {
        uint64_t address;
        uint64_t size;

        status = syntra_fdt_get_reservation(
            fdt, index, &address, &size);
        if (status != SYNTRA_STATUS_OK)
        {
            return status;
        }

        if (address > (uint64_t)UINTPTR_MAX ||
            size > (uint64_t)SIZE_MAX)
        {
            return SYNTRA_STATUS_NOT_IMPLEMENTED;
        }

        status = reserve_ram_range(
            (uintptr_t)address,
            (size_t)size,
            SYNTRA_MEMORY_REGION_RESERVED);
        if (status != SYNTRA_STATUS_OK)
        {
            return status;
        }
    }

    return SYNTRA_STATUS_OK;
}

static syntra_status_t discover_memory_nodes(
    const syntra_fdt_t *fdt
)
{
    syntra_fdt_iterator_t iterator;
    syntra_fdt_token_t token;
    syntra_status_t status;

    syntra_fdt_iterator_init(&iterator);

    while ((status = syntra_fdt_next(fdt, &iterator, &token)) ==
           SYNTRA_STATUS_OK)
    {
        if (token.type == SYNTRA_FDT_TOKEN_BEGIN_NODE &&
            token.depth == 1U)
        {
            syntra_fdt_node_t node;

            node.begin_offset = token.offset;
            node.depth = token.depth;

            if (node_is_memory(fdt, &node, token.name) &&
                !node_is_disabled(fdt, &node))
            {
                status = add_node_reg(
                    fdt, &node, SYNTRA_MEMORY_REGION_RAM);
                if (status != SYNTRA_STATUS_OK)
                {
                    return status;
                }
            }
        }
    }

    return status == SYNTRA_STATUS_NOT_IMPLEMENTED
        ? SYNTRA_STATUS_OK
        : status;
}

static syntra_status_t discover_reserved_memory_nodes(
    const syntra_fdt_t *fdt
)
{
    syntra_fdt_iterator_t iterator;
    syntra_fdt_token_t token;
    int inside_reserved_memory = 0;
    syntra_status_t status;

    syntra_fdt_iterator_init(&iterator);

    while ((status = syntra_fdt_next(fdt, &iterator, &token)) ==
           SYNTRA_STATUS_OK)
    {
        if (token.type == SYNTRA_FDT_TOKEN_BEGIN_NODE &&
            token.depth == 1U)
        {
            inside_reserved_memory =
                token.name != NULL &&
                property_string_equals(
                    (const uint8_t *)token.name,
                    (uint32_t)sizeof("reserved-memory"),
                    "reserved-memory");

        }
        else if (token.type == SYNTRA_FDT_TOKEN_END_NODE &&
                 token.depth == 1U)
        {
            inside_reserved_memory = 0;
        }
        else if (inside_reserved_memory &&
                 token.type == SYNTRA_FDT_TOKEN_BEGIN_NODE &&
                 token.depth == 2U)
        {
            syntra_fdt_node_t node;
            const uint8_t *reusable_data;
            uint32_t reusable_size;
            syntra_memory_region_type_t type =
                SYNTRA_MEMORY_REGION_RESERVED;

            node.begin_offset = token.offset;
            node.depth = token.depth;

            if (node_is_disabled(fdt, &node))
            {
                continue;
            }

            if (syntra_fdt_get_property(
                    fdt, &node, "reusable",
                    &reusable_data, &reusable_size) == SYNTRA_STATUS_OK)
            {
                type = SYNTRA_MEMORY_REGION_RECLAIMABLE;
            }

            {
                size_t count;
                size_t index;

                const uint8_t *reg_data;
                uint32_t reg_data_size;

                status = syntra_fdt_get_property(
                    fdt, &node, "reg", &reg_data, &reg_data_size);
                if (status == SYNTRA_STATUS_NOT_IMPLEMENTED)
                {
                    /* Dynamic reserved-memory nodes may use size/alignment. */
                    continue;
                }
                if (status != SYNTRA_STATUS_OK || reg_data_size == 0U)
                {
                    return SYNTRA_STATUS_ERROR;
                }

                status = syntra_fdt_get_reg_count(fdt, &node, &count);
                if (status != SYNTRA_STATUS_OK)
                {
                    return status;
                }

                for (index = 0U; index < count; ++index)
                {
                    uintptr_t base;
                    size_t size;

                    status = syntra_fdt_get_reg(
                        fdt, &node, (uint32_t)index, &base, &size);
                    if (status != SYNTRA_STATUS_OK)
                    {
                        return status;
                    }

                    status = reserve_ram_range(base, size, type);
                    if (status != SYNTRA_STATUS_OK)
                    {
                        return status;
                    }
                }
            }
        }
    }

    return status == SYNTRA_STATUS_NOT_IMPLEMENTED
        ? SYNTRA_STATUS_OK
        : status;
}

syntra_status_t syntra_arm64_memory_map_discover(
    const syntra_fdt_t *fdt,
    uintptr_t dtb_address,
    size_t dtb_size
)
{
    syntra_status_t status;

    if (fdt == NULL || dtb_address == 0U || dtb_size == 0U ||
        dtb_address > UINTPTR_MAX - dtb_size)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    status = syntra_memory_regions_init();
    if (status != SYNTRA_STATUS_OK)
    {
        return status;
    }

    status = discover_memory_nodes(fdt);
    if (status != SYNTRA_STATUS_OK || syntra_memory_region_count() == 0U)
    {
        return status == SYNTRA_STATUS_OK
            ? SYNTRA_STATUS_ERROR
            : status;
    }

    status = apply_header_reservations(fdt);
    if (status != SYNTRA_STATUS_OK)
    {
        return status;
    }

    status = discover_reserved_memory_nodes(fdt);
    if (status != SYNTRA_STATUS_OK)
    {
        return status;
    }

    return reserve_ram_range(
        dtb_address, dtb_size, SYNTRA_MEMORY_REGION_DTB);
}
