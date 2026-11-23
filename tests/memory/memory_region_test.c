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
    assert(syntra_memory_region_reserve(
        (uintptr_t)0x10010000U, (size_t)0x1000U,
        SYNTRA_MEMORY_REGION_RESERVED) == SYNTRA_STATUS_ERROR);
    assert(syntra_memory_region_reserve(
        (uintptr_t)0x10004000U, (size_t)0x2000U,
        SYNTRA_MEMORY_REGION_DTB) == SYNTRA_STATUS_OK);
    assert(syntra_memory_region_count() == 3U);

    assert(syntra_memory_region_find(
        (uintptr_t)0x10000010U, &region) == SYNTRA_STATUS_OK);
    assert(region.type == SYNTRA_MEMORY_REGION_RAM);
    assert(region.base == (uintptr_t)0x10000000U);
    assert(region.size == (size_t)0x4000U);

    assert(syntra_memory_region_find(
        (uintptr_t)0x10005000U, &region) == SYNTRA_STATUS_OK);
    assert(region.type == SYNTRA_MEMORY_REGION_DTB);
    assert(region.base == (uintptr_t)0x10004000U);
    assert(region.size == (size_t)0x2000U);

    assert(syntra_memory_region_find(
        (uintptr_t)0x10008000U, &region) == SYNTRA_STATUS_OK);
    assert(region.type == SYNTRA_MEMORY_REGION_RAM);
    assert(region.base == (uintptr_t)0x10006000U);
    assert(region.size == (size_t)0xA000U);

    assert(syntra_memory_region_find(
        (uintptr_t)0x10010000U, &region) == SYNTRA_STATUS_NOT_IMPLEMENTED);
    assert(syntra_memory_region_add(
        (uintptr_t)0x10008000U, (size_t)0x2000U,
        SYNTRA_MEMORY_REGION_MMIO) == SYNTRA_STATUS_ERROR);
    assert(syntra_memory_region_add(
        (uintptr_t)0x20000000U, 0U,
        SYNTRA_MEMORY_REGION_RAM) == SYNTRA_STATUS_INVALID_ARGUMENT);
    assert(syntra_memory_region_add(
        UINTPTR_MAX - 3U, 8U,
        SYNTRA_MEMORY_REGION_RAM) == SYNTRA_STATUS_INVALID_ARGUMENT);
    assert(syntra_memory_region_reserve(
        (uintptr_t)0x10002000U, (size_t)0x1000U,
        SYNTRA_MEMORY_REGION_RAM) == SYNTRA_STATUS_INVALID_ARGUMENT);

    assert(syntra_memory_region_get(0U, &region) == SYNTRA_STATUS_OK);
    assert(region.size == (size_t)0x4000U);
    assert(syntra_memory_region_get(9U, &region) == SYNTRA_STATUS_ERROR);
    assert(syntra_memory_regions_init() == SYNTRA_STATUS_OK);
    assert(syntra_memory_region_count() == 0U);

    /*
     * A reservation that would split RAM must fail atomically when the
     * registry has no capacity for the additional region descriptors.
     */
    assert(syntra_memory_regions_init() == SYNTRA_STATUS_OK);
    assert(syntra_memory_region_add(
        (uintptr_t)0x10000000U, (size_t)(3U * 4096U),
        SYNTRA_MEMORY_REGION_RAM) == SYNTRA_STATUS_OK);
    for (unsigned int i = 1U; i < SYNTRA_MEMORY_REGION_MAX; ++i)
    {
        assert(syntra_memory_region_add(
            (uintptr_t)0x20000000U + (uintptr_t)i * 0x2000U,
            (size_t)4096U, SYNTRA_MEMORY_REGION_RESERVED) == SYNTRA_STATUS_OK);
    }
    assert(syntra_memory_region_count() == SYNTRA_MEMORY_REGION_MAX);
    assert(syntra_memory_region_reserve(
        (uintptr_t)0x10001000U, (size_t)4096U,
        SYNTRA_MEMORY_REGION_DTB) == SYNTRA_STATUS_OUT_OF_MEMORY);
    assert(syntra_memory_region_count() == SYNTRA_MEMORY_REGION_MAX);

    assert(syntra_memory_region_find(
        (uintptr_t)0x10000000U, &region) == SYNTRA_STATUS_OK);
    assert(region.type == SYNTRA_MEMORY_REGION_RAM);
    assert(region.base == (uintptr_t)0x10000000U);
    assert(region.size == (size_t)(3U * 4096U));
    assert(syntra_memory_region_find(
        (uintptr_t)0x10001000U, &region) == SYNTRA_STATUS_OK);
    assert(region.type == SYNTRA_MEMORY_REGION_RAM);
    assert(syntra_memory_region_find(
        (uintptr_t)0x2003E000U, &region) == SYNTRA_STATUS_OK);
    assert(region.type == SYNTRA_MEMORY_REGION_RESERVED);

    puts("Memory Region Manager tests passed.");
    return 0;
}
