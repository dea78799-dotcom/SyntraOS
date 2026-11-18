
#include "memory.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

int main(void)
{
    assert(memory_init() == SYNTRA_STATUS_OK);

    syntra_memory_info_t info;

    /* Initial state */

    memory_get_info(&info);

    assert(info.total_blocks == SYNTRA_MAX_MEMORY_BLOCKS);
    assert(info.used_blocks == 0);
    assert(info.free_blocks == SYNTRA_MAX_MEMORY_BLOCKS);
    assert(info.usage_percent == 0);
    assert(info.integrity_errors == 0);
    assert(info.validation_errors == 0);

    assert(memory_validate() == SYNTRA_STATUS_OK);
    assert(memory_check_integrity() == SYNTRA_STATUS_OK);

    /* Allocate memory */

    void *a = memory_allocate(100);
    void *b = memory_allocate(200);
    void *c = memory_allocate(50);

    assert(a != NULL);
    assert(b != NULL);
    assert(c != NULL);

    /* Check alignment */

    assert(
        (uintptr_t)a % SYNTRA_MEMORY_ALIGNMENT == 0
    );

    assert(
        (uintptr_t)b % SYNTRA_MEMORY_ALIGNMENT == 0
    );

    assert(
        (uintptr_t)c % SYNTRA_MEMORY_ALIGNMENT == 0
    );

    /* Check memory information */

    memory_get_info(&info);

    assert(info.used_blocks == 350);
    assert(info.free_blocks == 674);
    assert(info.active_allocations == 3);

    /* Validate memory */

    assert(memory_validate() == SYNTRA_STATUS_OK);
    assert(memory_check_integrity() == SYNTRA_STATUS_OK);

    /* Release memory */

    assert(memory_release(b, 200) == SYNTRA_STATUS_OK);

    assert(memory_validate() == SYNTRA_STATUS_OK);

    assert(memory_release(a, 100) == SYNTRA_STATUS_OK);
    assert(memory_release(c, 50) == SYNTRA_STATUS_OK);

    /* Check final state */

    memory_get_info(&info);

    assert(info.used_blocks == 0);
    assert(info.free_blocks == SYNTRA_MAX_MEMORY_BLOCKS);
    assert(info.active_allocations == 0);
    assert(info.release_count == 3);

    assert(memory_validate() == SYNTRA_STATUS_OK);
    assert(memory_check_integrity() == SYNTRA_STATUS_OK);

    /* Invalid allocation */

    assert(memory_allocate(0) == NULL);

    assert(
        memory_allocate(SYNTRA_MAX_MEMORY_BLOCKS + 1) == NULL
    );

    /* Invalid release */

    assert(
        memory_release(a, 100) == SYNTRA_STATUS_ERROR
    );

    memory_get_info(&info);

    assert(info.failed_allocations == 2);
    assert(info.validation_errors == 1);
    assert(info.integrity_errors == 0);

    printf("Memory V4 tests passed!\n");

    return 0;
}
