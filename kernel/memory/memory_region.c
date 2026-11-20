#include "memory_region.h"

#include <stdint.h>

static syntra_memory_region_t regions[SYNTRA_MEMORY_REGION_MAX];
static size_t region_count;
static int initialized;

static int region_type_valid(syntra_memory_region_type_t type)
{
    return type >= SYNTRA_MEMORY_REGION_RAM &&
           type <= SYNTRA_MEMORY_REGION_RECLAIMABLE;
}

static int ranges_overlap(
    uintptr_t first_base,
    size_t first_size,
    uintptr_t second_base,
    size_t second_size
)
{
    uintptr_t first_end = first_base + first_size;
    uintptr_t second_end = second_base + second_size;

    return first_base < second_end && second_base < first_end;
}

syntra_status_t syntra_memory_regions_init(void)
{
    size_t index;

    for (index = 0U; index < SYNTRA_MEMORY_REGION_MAX; ++index)
    {
        regions[index].base = 0U;
        regions[index].size = 0U;
        regions[index].type = SYNTRA_MEMORY_REGION_RESERVED;
    }

    region_count = 0U;
    initialized = 1;
    return SYNTRA_STATUS_OK;
}

syntra_status_t syntra_memory_region_add(
    uintptr_t base,
    size_t size,
    syntra_memory_region_type_t type
)
{
    size_t index;

    if (!initialized)
    {
        return SYNTRA_STATUS_ERROR;
    }

    if (size == 0U || !region_type_valid(type) ||
        base > UINTPTR_MAX - size)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    for (index = 0U; index < region_count; ++index)
    {
        if (ranges_overlap(
                base, size, regions[index].base, regions[index].size))
        {
            return SYNTRA_STATUS_ERROR;
        }
    }

    if (region_count >= SYNTRA_MEMORY_REGION_MAX)
    {
        return SYNTRA_STATUS_OUT_OF_MEMORY;
    }

    regions[region_count].base = base;
    regions[region_count].size = size;
    regions[region_count].type = type;
    region_count++;

    return SYNTRA_STATUS_OK;
}

size_t syntra_memory_region_count(void)
{
    return initialized ? region_count : 0U;
}

syntra_status_t syntra_memory_region_get(
    size_t index,
    syntra_memory_region_t *region
)
{
    if (!initialized || region == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (index >= region_count)
    {
        return SYNTRA_STATUS_ERROR;
    }

    *region = regions[index];
    return SYNTRA_STATUS_OK;
}

syntra_status_t syntra_memory_region_find(
    uintptr_t address,
    syntra_memory_region_t *region
)
{
    size_t index;

    if (!initialized || region == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    for (index = 0U; index < region_count; ++index)
    {
        uintptr_t end = regions[index].base + regions[index].size;

        if (address >= regions[index].base && address < end)
        {
            *region = regions[index];
            return SYNTRA_STATUS_OK;
        }
    }

    return SYNTRA_STATUS_NOT_IMPLEMENTED;
}
