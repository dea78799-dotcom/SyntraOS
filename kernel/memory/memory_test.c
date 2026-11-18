
#include "memory.h"

#include <assert.h>
#include <stdio.h>

int main(void)
{
    assert(memory_init() == SYNTRA_STATUS_OK);

    syntra_memory_info_t info;

    memory_get_info(&info);

    assert(info.total_blocks == SYNTRA_MAX_MEMORY_BLOCKS);
    assert(info.used_blocks == 0);
    assert(info.free_blocks == SYNTRA_MAX_MEMORY_BLOCKS);
    assert(info.largest_free_block == SYNTRA_MAX_MEMORY_BLOCKS);
    assert(info.free_regions == 1);
    assert(info.fragmentation_blocks == 0);
    assert(info.usage_percent == 0);
    assert(info.failed_allocations == 0);

    /* Allocate memory */

    void *a = memory_allocate(100);
    void *b = memory_allocate(200);
    void *c = memory_allocate(50);

    assert(a != NULL);
    assert(b != NULL);
    assert(c != NULL);

    memory_get_info(&info);

    assert(info.used_blocks == 350);
    assert(info.free_blocks == 674);
    assert(info.allocation_count == 3);
    assert(info.active_allocations == 3);
    assert(info.usage_percent == 34);

    /* Check allocation tracking */

    assert(memory_get_allocation_count() == 3);

    assert(memory_get_allocation(0) != NULL);
    assert(memory_get_allocation(1) != NULL);
    assert(memory_get_allocation(2) != NULL);

    /* Release middle allocation */

    assert(memory_release(b, 200) == SYNTRA_STATUS_OK);

    memory_get_info(&info);

    assert(info.used_blocks == 150);
    assert(info.free_blocks == 874);
    assert(info.largest_free_block == 674);
    assert(info.free_regions == 2);
    assert(info.fragmentation_blocks == 200);
    assert(info.active_allocations == 2);

    /* Release remaining allocations */

    assert(memory_release(a, 100) == SYNTRA_STATUS_OK);
    assert(memory_release(c, 50) == SYNTRA_STATUS_OK);

    memory_get_info(&info);

    assert(info.used_blocks == 0);
    assert(info.free_blocks == SYNTRA_MAX_MEMORY_BLOCKS);
    assert(info.largest_free_block == SYNTRA_MAX_MEMORY_BLOCKS);
    assert(info.free_regions == 1);
    assert(info.fragmentation_blocks == 0);
    assert(info.usage_percent == 0);
    assert(info.release_count == 3);
    assert(info.active_allocations == 0);

    /* Invalid allocation */

    assert(memory_allocate(0) == NULL);
    assert(memory_allocate(SYNTRA_MAX_MEMORY_BLOCKS + 1) == NULL);

    memory_get_info(&info);

    assert(info.failed_allocations == 2);

    /* Invalid release */

    assert(
        memory_release(a, 100) == SYNTRA_STATUS_ERROR
    );

    printf("Memory V3 tests passed!\n");

    return 0;
}
