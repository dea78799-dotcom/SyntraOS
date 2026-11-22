#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "../../kernel/memory/memory_region.h"
#include "../../kernel/memory/page_allocator.h"

#define STRESS_PAGE_COUNT 256U
#define STRESS_CYCLES 32U

int main(void)
{
    uintptr_t pages[STRESS_PAGE_COUNT];
    syntra_page_owner_t owner;

    assert(syntra_memory_regions_init() == SYNTRA_STATUS_OK);
    assert(syntra_memory_region_add(
        (uintptr_t)0x50000000U,
        (size_t)STRESS_PAGE_COUNT * SYNTRA_PAGE_SIZE,
        SYNTRA_MEMORY_REGION_RAM) == SYNTRA_STATUS_OK);
    assert(syntra_page_allocator_init() == SYNTRA_STATUS_OK);
    assert(syntra_page_allocator_total_count() == STRESS_PAGE_COUNT);
    assert(syntra_page_allocator_free_count() == STRESS_PAGE_COUNT);
    assert(syntra_page_allocator_validate() == SYNTRA_STATUS_OK);

    for (unsigned int cycle = 0U; cycle < STRESS_CYCLES; ++cycle)
    {
        /* Fill the entire managed region and verify uniqueness/alignment. */
        for (unsigned int i = 0U; i < STRESS_PAGE_COUNT; ++i)
        {
            assert(syntra_page_alloc_owned(&pages[i], SYNTRA_PAGE_OWNER_PROCESS)
                   == SYNTRA_STATUS_OK);
            assert(pages[i] >= (uintptr_t)0x50000000U);
            assert(pages[i] < (uintptr_t)0x50000000U +
                              (uintptr_t)STRESS_PAGE_COUNT * SYNTRA_PAGE_SIZE);
            assert(pages[i] % SYNTRA_PAGE_SIZE == 0U);

            for (unsigned int j = 0U; j < i; ++j)
                assert(pages[i] != pages[j]);

            assert(syntra_page_get_owner(pages[i], &owner) == SYNTRA_STATUS_OK);
            assert(owner == SYNTRA_PAGE_OWNER_PROCESS);
        }

        assert(syntra_page_allocator_free_count() == 0U);
        assert(syntra_page_allocator_validate() == SYNTRA_STATUS_OK);
        {
            uintptr_t exhausted = 0U;
            assert(syntra_page_alloc(&exhausted) == SYNTRA_STATUS_OUT_OF_MEMORY);
        }

        /* Release in a deterministic permutation (stride 73 is coprime to 256). */
        for (unsigned int i = 0U; i < STRESS_PAGE_COUNT; ++i)
        {
            unsigned int index = (i * 73U + cycle) % STRESS_PAGE_COUNT;
            assert(syntra_page_free_owned(
                pages[index], SYNTRA_PAGE_OWNER_PROCESS) == SYNTRA_STATUS_OK);
            if ((i + 1U) % 64U == 0U)
                assert(syntra_page_allocator_validate() == SYNTRA_STATUS_OK);
        }

        assert(syntra_page_allocator_validate() == SYNTRA_STATUS_OK);
        assert(syntra_page_allocator_free_count() == STRESS_PAGE_COUNT);
    }

    /* Verify contiguous allocation and owner checks after repeated churn. */
    {
        uintptr_t run = 0U;
        assert(syntra_page_alloc_contiguous_owned(
            8U, &run, SYNTRA_PAGE_OWNER_DRIVER) == SYNTRA_STATUS_OK);
        for (unsigned int i = 0U; i < 8U; ++i)
        {
            assert(syntra_page_get_owner(
                run + (uintptr_t)i * SYNTRA_PAGE_SIZE, &owner) == SYNTRA_STATUS_OK);
            assert(owner == SYNTRA_PAGE_OWNER_DRIVER);
        }
        assert(syntra_page_free_owned(run, SYNTRA_PAGE_OWNER_PROCESS)
               == SYNTRA_STATUS_PERMISSION_DENIED);
        assert(syntra_page_allocator_validate() == SYNTRA_STATUS_OK);
        for (unsigned int i = 0U; i < 8U; ++i)
            assert(syntra_page_free_owned(
                run + (uintptr_t)i * SYNTRA_PAGE_SIZE,
                SYNTRA_PAGE_OWNER_DRIVER) == SYNTRA_STATUS_OK);
    }

    assert(syntra_page_allocator_free_count() == STRESS_PAGE_COUNT);
    assert(syntra_page_allocator_validate() == SYNTRA_STATUS_OK);
    puts("Page allocator stress test: PASS");
    return 0;
}
