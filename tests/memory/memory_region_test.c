#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "../../kernel/memory/memory_region.h"

int main(void)
{
    syntra_memory_region_t region;

    assert(syntra_memory_regions_init() == SYNTRA_STATUS_OK);
    assert(syntra_memory_region_count() == 0U);

    assert(syntra_memory_region_add(
        (uintptr_t)0x10000000U, (size_t)0x10000U,
        SYNTRA_MEMORY_REGION_RAM) == SYNTRA_STATUS_OK);
    assert(syntra_memory_region_add(
        (uintptr_t)0x10010000U, (size_t)0x1000U,
        SYNTRA_MEMORY_REGION_RESERVED) == SYNTRA_STATUS_OK);
    assert(syntra_memory_region_count() == 2U);

    assert(syntra_memory_region_find(
        (uintptr_t)0x10000010U, &region) == SYNTRA_STATUS_OK);
    assert(region.type == SYNTRA_MEMORY_REGION_RAM);
    assert(region.base == (uintptr_t)0x10000000U);

    assert(syntra_memory_region_find(
        (uintptr_t)0x10010000U, &region) == SYNTRA_STATUS_OK);
    assert(region.type == SYNTRA_MEMORY_REGION_RESERVED);

    assert(syntra_memory_region_find(
        (uintptr_t)0x10011000U, &region) == SYNTRA_STATUS_NOT_IMPLEMENTED);
    assert(syntra_memory_region_add(
        (uintptr_t)0x10008000U, (size_t)0x2000U,
        SYNTRA_MEMORY_REGION_MMIO) == SYNTRA_STATUS_ERROR);
    assert(syntra_memory_region_add(
        (uintptr_t)0x20000000U, 0U,
        SYNTRA_MEMORY_REGION_RAM) == SYNTRA_STATUS_INVALID_ARGUMENT);
    assert(syntra_memory_region_add(
        UINTPTR_MAX - 3U, 8U,
        SYNTRA_MEMORY_REGION_RAM) == SYNTRA_STATUS_INVALID_ARGUMENT);

    assert(syntra_memory_region_get(0U, &region) == SYNTRA_STATUS_OK);
    assert(region.size == (size_t)0x10000U);
    assert(syntra_memory_region_get(9U, &region) == SYNTRA_STATUS_ERROR);
    assert(syntra_memory_regions_init() == SYNTRA_STATUS_OK);
    assert(syntra_memory_region_count() == 0U);

    puts("Memory Region Manager tests passed.");
    return 0;
}
