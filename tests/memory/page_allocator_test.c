#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "../../kernel/memory/memory_region.h"
#include "../../kernel/memory/page_allocator.h"

int main(void)
{
    uintptr_t first;
    uintptr_t second;
    uintptr_t third;

    assert(syntra_memory_regions_init() == SYNTRA_STATUS_OK);
    assert(syntra_memory_region_add(
        (uintptr_t)0x10000000U, (size_t)0x3000U,
        SYNTRA_MEMORY_REGION_RAM) == SYNTRA_STATUS_OK);
    assert(syntra_memory_region_reserve(
        (uintptr_t)0x10001000U, (size_t)0x1000U,
        SYNTRA_MEMORY_REGION_KERNEL) == SYNTRA_STATUS_OK);
    assert(syntra_memory_region_add(
        (uintptr_t)0x20000000U, (size_t)0x1000U,
        SYNTRA_MEMORY_REGION_MMIO) == SYNTRA_STATUS_OK);

    assert(syntra_page_allocator_init() == SYNTRA_STATUS_OK);
    assert(syntra_page_allocator_total_count() == 2U);
    assert(syntra_page_allocator_free_count() == 2U);

    assert(syntra_page_alloc(NULL) == SYNTRA_STATUS_INVALID_ARGUMENT);
    assert(syntra_page_alloc(&first) == SYNTRA_STATUS_OK);
    assert(syntra_page_alloc(&second) == SYNTRA_STATUS_OK);
    assert(first == (uintptr_t)0x10000000U);
    assert(second == (uintptr_t)0x10002000U);
    assert(first % SYNTRA_PAGE_SIZE == 0U);
    assert(second % SYNTRA_PAGE_SIZE == 0U);
    assert(syntra_page_allocator_free_count() == 0U);

    assert(syntra_page_alloc(&third) == SYNTRA_STATUS_OUT_OF_MEMORY);
    assert(syntra_page_free(first + 1U) == SYNTRA_STATUS_INVALID_ARGUMENT);
    assert(syntra_page_free((uintptr_t)0x10001000U) == SYNTRA_STATUS_ERROR);
    assert(syntra_page_free(first) == SYNTRA_STATUS_OK);
    assert(syntra_page_free(first) == SYNTRA_STATUS_ERROR);
    assert(syntra_page_allocator_free_count() == 1U);

    assert(syntra_page_alloc(&third) == SYNTRA_STATUS_OK);
    assert(third == first);
    assert(syntra_page_free(second) == SYNTRA_STATUS_OK);
    assert(syntra_page_free(third) == SYNTRA_STATUS_OK);
    assert(syntra_page_allocator_free_count() == 2U);

    /* Cursor moves backward when a lower page is freed. */
    assert(syntra_page_alloc(&first) == SYNTRA_STATUS_OK);
    assert(syntra_page_alloc(&second) == SYNTRA_STATUS_OK);
    assert(first == (uintptr_t)0x10000000U);
    assert(second == (uintptr_t)0x10002000U);
    assert(syntra_page_free(first) == SYNTRA_STATUS_OK);
    assert(syntra_page_alloc(&third) == SYNTRA_STATUS_OK);
    assert(third == first);
    assert(syntra_page_free(second) == SYNTRA_STATUS_OK);
    assert(syntra_page_free(third) == SYNTRA_STATUS_OK);
    assert(syntra_page_allocator_free_count() == 2U);

    /* Exercise allocations across bitmap-word and summary-word boundaries. */
    assert(syntra_memory_regions_init() == SYNTRA_STATUS_OK);
    assert(syntra_memory_region_add(
        (uintptr_t)0x30000000U, (size_t)(130U * SYNTRA_PAGE_SIZE),
        SYNTRA_MEMORY_REGION_RAM) == SYNTRA_STATUS_OK);
    assert(syntra_page_allocator_init() == SYNTRA_STATUS_OK);
    assert(syntra_page_allocator_total_count() == 130U);
    for (unsigned int i = 0U; i < 130U; ++i)
    {
        assert(syntra_page_alloc(&first) == SYNTRA_STATUS_OK);
        assert(first == (uintptr_t)0x30000000U +
                        (uintptr_t)i * SYNTRA_PAGE_SIZE);
    }
    assert(syntra_page_allocator_free_count() == 0U);
    assert(syntra_page_alloc(&second) == SYNTRA_STATUS_OUT_OF_MEMORY);
    assert(syntra_page_free((uintptr_t)0x30000000U +
                            (uintptr_t)65U * SYNTRA_PAGE_SIZE) == SYNTRA_STATUS_OK);
    assert(syntra_page_alloc(&third) == SYNTRA_STATUS_OK);
    assert(third == (uintptr_t)0x30000000U +
                    (uintptr_t)65U * SYNTRA_PAGE_SIZE);
    for (unsigned int i = 0U; i < 130U; ++i)
    {
        uintptr_t address = (uintptr_t)0x30000000U +
                            (uintptr_t)i * SYNTRA_PAGE_SIZE;
        if (i != 65U)
        {
            assert(syntra_page_free(address) == SYNTRA_STATUS_OK);
        }
    }
    assert(syntra_page_free(third) == SYNTRA_STATUS_OK);
    assert(syntra_page_allocator_free_count() == 130U);

    /* Boundary policy: clip RAM crossing 4 GiB and ignore high-only RAM. */
    assert(syntra_memory_regions_init() == SYNTRA_STATUS_OK);
    assert(syntra_memory_region_add(
        (uintptr_t)0xFFFFF000ULL, (size_t)0x3000U,
        SYNTRA_MEMORY_REGION_RAM) == SYNTRA_STATUS_OK);
    assert(syntra_page_allocator_init() == SYNTRA_STATUS_OK);
    assert(syntra_page_allocator_total_count() == 1U);
    assert(syntra_page_allocator_free_count() == 1U);
    assert(syntra_page_alloc(&first) == SYNTRA_STATUS_OK);
    assert(first == (uintptr_t)0xFFFFF000ULL);
    assert(syntra_page_free(first) == SYNTRA_STATUS_OK);

    assert(syntra_memory_regions_init() == SYNTRA_STATUS_OK);
    assert(syntra_memory_region_add(
        (uintptr_t)0x100000000ULL, (size_t)0x4000U,
        SYNTRA_MEMORY_REGION_RAM) == SYNTRA_STATUS_OK);
    assert(syntra_page_allocator_init() == SYNTRA_STATUS_OUT_OF_MEMORY);
    assert(syntra_page_allocator_total_count() == 0U);
    assert(syntra_page_allocator_free_count() == 0U);

    puts("Physical Page Allocator V1 tests passed.");
    return 0;
}
