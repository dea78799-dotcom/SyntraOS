
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

    assert(info.peak_used_blocks == 0);
    assert(info.peak_usage_percent == 0);

    assert(info.leak_count == 0);
    assert(info.leak_blocks == 0);

    assert(memory_get_history_count() == 0);

    assert(memory_validate() == SYNTRA_STATUS_OK);

    /* Allocate memory */

    void *a = memory_allocate(100);
    void *b = memory_allocate(200);
    void *c = memory_allocate(50);

    assert(a != NULL);
    assert(b != NULL);
    assert(c != NULL);

    /* Check alignment */

    assert((uintptr_t)a % SYNTRA_MEMORY_ALIGNMENT == 0);
    assert((uintptr_t)b % SYNTRA_MEMORY_ALIGNMENT == 0);
    assert((uintptr_t)c % SYNTRA_MEMORY_ALIGNMENT == 0);

    /* Check allocation statistics */

    memory_get_info(&info);

    assert(info.used_blocks == 350);
    assert(info.free_blocks == 674);
    assert(info.active_allocations == 3);

    assert(info.peak_used_blocks == 350);
    assert(info.peak_usage_percent == 34);

    /* Check leak reporting */

    assert(info.leak_count == 3);
    assert(info.leak_blocks == 350);

    /* Check history */

    assert(memory_get_history_count() == 3);

    const syntra_memory_history_t *event;

    event = memory_get_history(0);

    assert(event != NULL);
    assert(event->type == SYNTRA_MEMORY_EVENT_ALLOCATE);
    assert(event->block_count == 100);
    assert(event->sequence == 1);

    event = memory_get_history(1);

    assert(event != NULL);
    assert(event->type == SYNTRA_MEMORY_EVENT_ALLOCATE);
    assert(event->block_count == 200);
    assert(event->sequence == 2);

    event = memory_get_history(2);

    assert(event != NULL);
    assert(event->type == SYNTRA_MEMORY_EVENT_ALLOCATE);
    assert(event->block_count == 50);
    assert(event->sequence == 3);

    /* Check integrity */

    assert(memory_validate() == SYNTRA_STATUS_OK);
    assert(memory_check_integrity() == SYNTRA_STATUS_OK);

    /* Release memory */

    assert(memory_release(b, 200) == SYNTRA_STATUS_OK);
    assert(memory_release(a, 100) == SYNTRA_STATUS_OK);
    assert(memory_release(c, 50) == SYNTRA_STATUS_OK);

    memory_get_info(&info);

    assert(info.used_blocks == 0);
    assert(info.active_allocations == 0);
    assert(info.release_count == 3);

    assert(info.leak_count == 0);
    assert(info.leak_blocks == 0);

    /* Peak usage must remain unchanged */

    assert(info.peak_used_blocks == 350);
    assert(info.peak_usage_percent == 34);

    /* Check release history */

    assert(memory_get_history_count() == 6);

    event = memory_get_history(3);

    assert(event != NULL);
    assert(event->type == SYNTRA_MEMORY_EVENT_RELEASE);
    assert(event->block_count == 200);

    event = memory_get_history(4);

    assert(event != NULL);
    assert(event->type == SYNTRA_MEMORY_EVENT_RELEASE);
    assert(event->block_count == 100);

    event = memory_get_history(5);

    assert(event != NULL);
    assert(event->type == SYNTRA_MEMORY_EVENT_RELEASE);
    assert(event->block_count == 50);

    /* Invalid allocations */

    assert(memory_allocate(0) == NULL);

    assert(
        memory_allocate(SYNTRA_MAX_MEMORY_BLOCKS + 1) == NULL
    );

    memory_get_info(&info);

    assert(info.failed_allocations == 2);

    /* Invalid release */

    assert(
        memory_release(a, 100) == SYNTRA_STATUS_ERROR
    );

    memory_get_info(&info);

    assert(info.validation_errors == 1);
    assert(info.integrity_errors == 0);

    /* Failed allocation history */

    assert(memory_get_history_count() == 8);

    event = memory_get_history(6);

    assert(event != NULL);
    assert(event->type == SYNTRA_MEMORY_EVENT_FAILED);
    assert(event->sequence == 7);

    event = memory_get_history(7);

    assert(event != NULL);
    assert(event->type == SYNTRA_MEMORY_EVENT_FAILED);
    assert(event->sequence == 8);

    /* Final validation */

    assert(memory_validate() == SYNTRA_STATUS_OK);
    assert(memory_check_integrity() == SYNTRA_STATUS_OK);

    printf("Memory V5 tests passed!\n");

    return 0;
}
