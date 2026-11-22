#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "../../kernel/memory/memory_region.h"
#include "../../kernel/memory/page_allocator.h"
#include "../../kernel/memory/stack_allocator.h"

int main(void)
{
    syntra_stack_allocation_t a = {0}, b = {0}, c = {0};
    syntra_stack_allocation_t capacity[SYNTRA_STACK_ALLOCATOR_MAX_STACKS];
    syntra_page_allocator_stats_t stats;

    assert(syntra_memory_regions_init() == SYNTRA_STATUS_OK);
    assert(syntra_memory_region_add(0x10000000U, 40U * SYNTRA_PAGE_SIZE,
        SYNTRA_MEMORY_REGION_RAM) == SYNTRA_STATUS_OK);
    assert(syntra_page_allocator_init() == SYNTRA_STATUS_OK);
    assert(syntra_stack_allocator_validate() == SYNTRA_STATUS_OK);
    assert(syntra_stack_allocator_live_count() == 0U);

    assert(syntra_stack_alloc(1U, &a) == SYNTRA_STATUS_OK);
    assert(a.size == SYNTRA_PAGE_SIZE && a.page_count == 1U);
    assert(syntra_stack_alloc(4097U, &b) == SYNTRA_STATUS_OK);
    assert(b.size == 2U * SYNTRA_PAGE_SIZE && b.page_count == 2U);
    assert(a.physical_base != b.physical_base);
    assert(syntra_stack_allocator_live_count() == 2U);
    assert(syntra_stack_allocator_validate() == SYNTRA_STATUS_OK);
    assert(syntra_page_allocator_get_stats(&stats) == SYNTRA_STATUS_OK);
    assert(stats.owner_pages[SYNTRA_PAGE_OWNER_KERNEL] == 3U);

    assert(syntra_stack_free(&a) == SYNTRA_STATUS_OK);
    assert(syntra_stack_free(&a) == SYNTRA_STATUS_INVALID_ARGUMENT);
    assert(syntra_stack_free(&b) == SYNTRA_STATUS_OK);
    assert(syntra_stack_allocator_live_count() == 0U);
    assert(syntra_page_allocator_get_stats(&stats) == SYNTRA_STATUS_OK);
    assert(stats.owner_pages[SYNTRA_PAGE_OWNER_KERNEL] == 0U);
    assert(stats.free_pages == stats.total_pages);
    assert(syntra_page_allocator_validate() == SYNTRA_STATUS_OK);

    assert(syntra_stack_alloc(0U, &c) == SYNTRA_STATUS_INVALID_ARGUMENT);
    assert(syntra_stack_alloc(SIZE_MAX, &c) == SYNTRA_STATUS_INVALID_ARGUMENT);
    assert(syntra_stack_free(NULL) == SYNTRA_STATUS_INVALID_ARGUMENT);

    /* Capacity is bounded and failure must not alter the output descriptor. */
    for (size_t i = 0U; i < SYNTRA_STACK_ALLOCATOR_MAX_STACKS; ++i)
    {
        capacity[i] = (syntra_stack_allocation_t){0};
        assert(syntra_stack_alloc(SYNTRA_PAGE_SIZE, &capacity[i]) == SYNTRA_STATUS_OK);
    }
    assert(syntra_stack_allocator_live_count() == SYNTRA_STACK_ALLOCATOR_MAX_STACKS);
    c = (syntra_stack_allocation_t){0xABC000U, 0x2000U, 2U};
    assert(syntra_stack_alloc(SYNTRA_PAGE_SIZE, &c) == SYNTRA_STATUS_OUT_OF_MEMORY);
    assert(c.physical_base == 0xABC000U && c.size == 0x2000U && c.page_count == 2U);
    for (size_t i = 0U; i < SYNTRA_STACK_ALLOCATOR_MAX_STACKS; ++i)
        assert(syntra_stack_free(&capacity[i]) == SYNTRA_STATUS_OK);
    assert(syntra_stack_allocator_validate() == SYNTRA_STATUS_OK);
    assert(syntra_stack_allocator_live_count() == 0U);

    /* Slot reuse does not leak pages or registry records. */
    for (unsigned int i = 0U; i < 256U; ++i)
    {
        assert(syntra_stack_alloc(8192U, &c) == SYNTRA_STATUS_OK);
        assert(syntra_stack_allocator_live_count() == 1U);
        assert(syntra_stack_free(&c) == SYNTRA_STATUS_OK);
        assert(syntra_stack_allocator_validate() == SYNTRA_STATUS_OK);
    }

    puts("Stack physical allocation ownership tests passed");
    return 0;
}
