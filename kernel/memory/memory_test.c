
#include "../../kernel/memory/memory.h"

#include <assert.h>
#include <stdio.h>

int main(void)
{
    syntra_memory_info_t info;

    /* Test initialization */
    assert(memory_init() == SYNTRA_STATUS_OK);

    /* Test initial information */
    memory_get_info(&info);

    assert(info.total_blocks == SYNTRA_MAX_MEMORY_BLOCKS);
    assert(info.used_blocks == 0);
    assert(info.free_blocks == SYNTRA_MAX_MEMORY_BLOCKS);
    assert(info.allocation_count == 0);
    assert(info.release_count == 0);
    assert(info.active_allocations == 0);

    /* Test first allocation */
    void *first = memory_allocate(2);

    assert(first != NULL);
    assert(memory_get_allocation_count() == 1);

    /* Test second allocation */
    void *second = memory_allocate(3);

    assert(second != NULL);
    assert(first != second);
    assert(memory_get_allocation_count() == 2);

    /* Test allocation tracking */
    const syntra_memory_allocation_t *allocation;

    allocation = memory_get_allocation(0);

    assert(allocation != NULL);
    assert(allocation->address == first);
    assert(allocation->block_count == 2);
    assert(allocation->active == 1);

    allocation = memory_get_allocation(1);

    assert(allocation != NULL);
    assert(allocation->address == second);
    assert(allocation->block_count == 3);
    assert(allocation->active == 1);

    /* Test memory statistics */
    memory_get_info(&info);

    assert(info.used_blocks == 5);
    assert(info.free_blocks == SYNTRA_MAX_MEMORY_BLOCKS - 5);
    assert(info.allocation_count == 2);
    assert(info.active_allocations == 2);

    /* Test invalid allocation */
    assert(memory_allocate(0) == NULL);

    assert(
        memory_allocate(SYNTRA_MAX_MEMORY_BLOCKS + 1) == NULL
    );

    /* Test incorrect release size */
    assert(
        memory_release(first, 1) ==
        SYNTRA_STATUS_INVALID_ARGUMENT
    );

    /* Test invalid address */
    int outside_memory = 0;

    assert(
        memory_release(&outside_memory, 1) ==
        SYNTRA_STATUS_ERROR
    );

    /* Test null address */
    assert(
        memory_release(NULL, 1) ==
        SYNTRA_STATUS_INVALID_ARGUMENT
    );

    /* Test release first allocation */
    assert(
        memory_release(first, 2) ==
        SYNTRA_STATUS_OK
    );

    assert(memory_get_allocation_count() == 1);
    assert(memory_get_allocation(0) == NULL);

    /* Test double release */
    assert(
        memory_release(first, 2) ==
        SYNTRA_STATUS_ERROR
    );

    /* Test release second allocation */
    assert(
        memory_release(second, 3) ==
        SYNTRA_STATUS_OK
    );

    /* Test all memory released */
    memory_get_info(&info);

    assert(info.used_blocks == 0);
    assert(info.free_blocks == SYNTRA_MAX_MEMORY_BLOCKS);
    assert(info.allocation_count == 2);
    assert(info.release_count == 2);
    assert(info.active_allocations == 0);

    /* Test allocation slot reuse */
    void *third = memory_allocate(4);

    assert(third != NULL);
    assert(memory_get_allocation_count() == 1);

    allocation = memory_get_allocation(0);

    assert(allocation != NULL);
    assert(allocation->address == third);
    assert(allocation->block_count == 4);

    /* Test final release */
    assert(
        memory_release(third, 4) ==
        SYNTRA_STATUS_OK
    );

    memory_get_info(&info);

    assert(info.used_blocks == 0);
    assert(info.allocation_count == 3);
    assert(info.release_count == 3);
    assert(info.active_allocations == 0);

    printf("Memory V2 Test: PASSED\n");

    return 0;
}
