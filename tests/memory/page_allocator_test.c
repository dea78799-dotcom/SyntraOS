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
    assert(syntra_page_allocator_validate() == SYNTRA_STATUS_OK);

    /* Ownership tags reject cross-subsystem frees and clear on release. */
    {
        syntra_page_owner_t owner = SYNTRA_PAGE_OWNER_NONE;
        assert(syntra_page_alloc_owned(&third, SYNTRA_PAGE_OWNER_PAGE_TABLE) == SYNTRA_STATUS_OK);
        assert(syntra_page_get_owner(third, &owner) == SYNTRA_STATUS_OK);
        assert(owner == SYNTRA_PAGE_OWNER_PAGE_TABLE);
        assert(syntra_page_free_owned(third, SYNTRA_PAGE_OWNER_PROCESS) == SYNTRA_STATUS_PERMISSION_DENIED);
        assert(syntra_page_allocator_validate() == SYNTRA_STATUS_OK);
        assert(syntra_page_free_owned(third, SYNTRA_PAGE_OWNER_PAGE_TABLE) == SYNTRA_STATUS_OK);
        assert(syntra_page_get_owner(third, &owner) == SYNTRA_STATUS_ERROR);
        assert(syntra_page_allocator_validate() == SYNTRA_STATUS_OK);
        assert(syntra_page_alloc_owned(&third, SYNTRA_PAGE_OWNER_NONE) == SYNTRA_STATUS_INVALID_ARGUMENT);
        assert(syntra_page_alloc_owned(&third, SYNTRA_PAGE_OWNER_MAX) == SYNTRA_STATUS_INVALID_ARGUMENT);
    }

    assert(syntra_page_alloc(NULL) == SYNTRA_STATUS_INVALID_ARGUMENT);
    assert(syntra_page_alloc(&first) == SYNTRA_STATUS_OK);
    assert(syntra_page_alloc(&second) == SYNTRA_STATUS_OK);
    assert(first == (uintptr_t)0x10000000U);
    assert(second == (uintptr_t)0x10002000U);
    assert(first % SYNTRA_PAGE_SIZE == 0U);
    assert(second % SYNTRA_PAGE_SIZE == 0U);
    assert(syntra_page_allocator_free_count() == 0U);
    assert(syntra_page_allocator_validate() == SYNTRA_STATUS_OK);

    assert(syntra_page_alloc(&third) == SYNTRA_STATUS_OUT_OF_MEMORY);
    assert(syntra_page_free(first + 1U) == SYNTRA_STATUS_INVALID_ARGUMENT);
    assert(syntra_page_free((uintptr_t)0x10001000U) == SYNTRA_STATUS_ERROR);
    assert(syntra_page_free(first) == SYNTRA_STATUS_OK);
    assert(syntra_page_free(first) == SYNTRA_STATUS_ERROR);
    assert(syntra_page_allocator_free_count() == 1U);
    assert(syntra_page_allocator_validate() == SYNTRA_STATUS_OK);

    assert(syntra_page_alloc(&third) == SYNTRA_STATUS_OK);
    assert(third == first);
    assert(syntra_page_free(second) == SYNTRA_STATUS_OK);
    assert(syntra_page_free(third) == SYNTRA_STATUS_OK);
    assert(syntra_page_allocator_free_count() == 2U);
    assert(syntra_page_allocator_validate() == SYNTRA_STATUS_OK);

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

    /* Exercise allocations across primary-bitmap and summary-bitmap boundaries. */
    assert(syntra_memory_regions_init() == SYNTRA_STATUS_OK);
    assert(syntra_memory_region_add(
        (uintptr_t)0x30000000U, (size_t)(4100U * SYNTRA_PAGE_SIZE),
        SYNTRA_MEMORY_REGION_RAM) == SYNTRA_STATUS_OK);
    assert(syntra_page_allocator_init() == SYNTRA_STATUS_OK);
    assert(syntra_page_allocator_total_count() == 4100U);
    for (unsigned int i = 0U; i < 4100U; ++i)
    {
        assert(syntra_page_alloc(&first) == SYNTRA_STATUS_OK);
        assert(first == (uintptr_t)0x30000000U +
                        (uintptr_t)i * SYNTRA_PAGE_SIZE);
    }
    assert(syntra_page_allocator_free_count() == 0U);
    assert(syntra_page_allocator_validate() == SYNTRA_STATUS_OK);
    assert(syntra_page_alloc(&second) == SYNTRA_STATUS_OUT_OF_MEMORY);
    assert(syntra_page_free((uintptr_t)0x30000000U +
                            (uintptr_t)65U * SYNTRA_PAGE_SIZE) == SYNTRA_STATUS_OK);
    assert(syntra_page_alloc(&third) == SYNTRA_STATUS_OK);
    assert(third == (uintptr_t)0x30000000U +
                    (uintptr_t)65U * SYNTRA_PAGE_SIZE);
    for (unsigned int i = 0U; i < 4100U; ++i)
    {
        uintptr_t address = (uintptr_t)0x30000000U +
                            (uintptr_t)i * SYNTRA_PAGE_SIZE;
        if (i != 65U)
        {
            assert(syntra_page_free(address) == SYNTRA_STATUS_OK);
        }
    }
    assert(syntra_page_free(third) == SYNTRA_STATUS_OK);
    assert(syntra_page_allocator_free_count() == 4100U);
    assert(syntra_page_allocator_validate() == SYNTRA_STATUS_OK);

    /* Contiguous runs are allocated only from adjacent free physical pages. */
    assert(syntra_memory_regions_init() == SYNTRA_STATUS_OK);
    assert(syntra_memory_region_add(
        (uintptr_t)0x40000000U, (size_t)(4U * SYNTRA_PAGE_SIZE),
        SYNTRA_MEMORY_REGION_RAM) == SYNTRA_STATUS_OK);
    assert(syntra_page_allocator_init() == SYNTRA_STATUS_OK);
    assert(syntra_page_alloc_contiguous(0U, &first) ==
           SYNTRA_STATUS_INVALID_ARGUMENT);
    assert(syntra_page_alloc_contiguous(2U, NULL) ==
           SYNTRA_STATUS_INVALID_ARGUMENT);
    assert(syntra_page_alloc_contiguous_owned(2U, &first,
        SYNTRA_PAGE_OWNER_DRIVER) == SYNTRA_STATUS_OK);
    {
        syntra_page_owner_t owner = SYNTRA_PAGE_OWNER_NONE;
        assert(syntra_page_get_owner(first, &owner) == SYNTRA_STATUS_OK);
        assert(owner == SYNTRA_PAGE_OWNER_DRIVER);
        assert(syntra_page_get_owner(first + SYNTRA_PAGE_SIZE, &owner) == SYNTRA_STATUS_OK);
        assert(owner == SYNTRA_PAGE_OWNER_DRIVER);
        assert(syntra_page_free_owned(first, SYNTRA_PAGE_OWNER_DRIVER) == SYNTRA_STATUS_OK);
        assert(syntra_page_free_owned(first + SYNTRA_PAGE_SIZE,
            SYNTRA_PAGE_OWNER_DRIVER) == SYNTRA_STATUS_OK);
    }
    assert(syntra_page_alloc_contiguous(2U, &first) == SYNTRA_STATUS_OK);
    assert(first == (uintptr_t)0x40000000U);
    assert(syntra_page_allocator_free_count() == 2U);
    assert(syntra_page_allocator_validate() == SYNTRA_STATUS_OK);
    assert(syntra_page_alloc_contiguous(3U, &second) ==
           SYNTRA_STATUS_OUT_OF_MEMORY);
    assert(syntra_page_free(first + SYNTRA_PAGE_SIZE) == SYNTRA_STATUS_OK);
    assert(syntra_page_free(first) == SYNTRA_STATUS_OK);
    assert(syntra_page_allocator_free_count() == 4U);
    assert(syntra_page_allocator_validate() == SYNTRA_STATUS_OK);

    /* Boundary policy: clip RAM crossing 4 GiB and ignore high-only RAM. */
    assert(syntra_memory_regions_init() == SYNTRA_STATUS_OK);
    assert(syntra_memory_region_add(
        (uintptr_t)0xFFFFF000ULL, (size_t)0x3000U,
        SYNTRA_MEMORY_REGION_RAM) == SYNTRA_STATUS_OK);
    assert(syntra_page_allocator_init() == SYNTRA_STATUS_OK);
    assert(syntra_page_allocator_total_count() == 1U);
    assert(syntra_page_allocator_free_count() == 1U);
    assert(syntra_page_allocator_validate() == SYNTRA_STATUS_OK);
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
    assert(syntra_page_allocator_validate() == SYNTRA_STATUS_INVALID_ARGUMENT);

    puts("Physical Page Allocator V1 tests passed.");
    return 0;
}
