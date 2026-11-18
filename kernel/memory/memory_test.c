
#include <stdio.h>
#include <stddef.h>

#include "memory.h"

static unsigned int passed = 0U;
static unsigned int failed = 0U;

static void check(const char *name, int condition)
{
    if (condition) {
        passed++;
        printf("[PASS] %s\n", name);
    } else {
        failed++;
        printf("[FAIL] %s\n", name);
    }
}

int main(void)
{
    syntra_memory_info_t info;
    syntra_memory_report_t report;
    syntra_memory_diagnostics_t diagnostics;
    syntra_memory_pool_stats_t pool_stats;
    syntra_memory_tracking_t tracking;
    syntra_memory_metrics_t metrics;

    void *a;
    void *b;
    void *c;
    void *full;
    int invalid_address = 0;

    const syntra_memory_allocation_t *allocation;
    const syntra_memory_history_t *event;
    const syntra_memory_snapshot_t *snapshot;
    const syntra_memory_tag_t *tag;

    unsigned int i;

    printf("\n");
    printf("====================================\n");
    printf("       SYNTRAOS MEMORY V8 TEST\n");
    printf("====================================\n");

    /* INITIALIZATION */

    check(
        "Memory initialization",
        memory_init() == SYNTRA_STATUS_OK
    );

    memory_get_info(&info);

    check(
        "Initial memory is empty",
        info.used_blocks == 0U &&
        info.free_blocks == SYNTRA_MAX_MEMORY_BLOCKS
    );

    check(
        "Initial allocation count is zero",
        memory_get_allocation_count() == 0U
    );

    /* BASIC ALLOCATION */

    a = memory_allocate(1U);

    check(
        "Allocate one block",
        a != NULL
    );

    memory_get_info(&info);

    check(
        "Used blocks increase after allocation",
        info.used_blocks == 1U
    );

    check(
        "Free blocks decrease after allocation",
        info.free_blocks ==
        SYNTRA_MAX_MEMORY_BLOCKS - 1U
    );

    check(
        "Active allocation count increases",
        memory_get_allocation_count() == 1U
    );

    allocation = memory_get_allocation(0U);

    check(
        "Get allocation by active index",
        allocation != NULL &&
        allocation->address == a &&
        allocation->block_count == 1U
    );

    /* ALLOCATION TAG */

    check(
        "Set allocation tag",
        memory_set_tag(0U, "V8-Test") ==
        SYNTRA_STATUS_OK
    );

    tag = memory_get_tag(0U);

    check(
        "Read allocation tag",
        tag != NULL &&
        tag->active &&
        tag->name[0] == 'V'
    );

    /* INVALID ALLOCATION */

    check(
        "Reject zero-block allocation",
        memory_allocate(0U) == NULL
    );

    check(
        "Reject oversized allocation",
        memory_allocate(
            SYNTRA_MAX_MEMORY_BLOCKS + 1U
        ) == NULL
    );

    memory_get_info(&info);

    check(
        "Failed allocation counter",
        info.failed_allocations == 2U
    );

    /* INVALID RELEASE */

    check(
        "Reject incorrect release size",
        memory_release(a, 2U) != SYNTRA_STATUS_OK
    );

    memory_get_info(&info);

    check(
        "Invalid release does not free memory",
        info.used_blocks == 1U
    );

    check(
        "Reject unknown memory address",
        memory_release(
            &invalid_address,
            1U
        ) != SYNTRA_STATUS_OK
    );

    /* VALID RELEASE */

    check(
        "Release valid allocation",
        memory_release(a, 1U) == SYNTRA_STATUS_OK
    );

    memory_get_info(&info);

    check(
        "All memory is free after release",
        info.used_blocks == 0U &&
        info.free_blocks == SYNTRA_MAX_MEMORY_BLOCKS
    );

    check(
        "Active allocation count returns to zero",
        memory_get_allocation_count() == 0U
    );

    check(
        "Released allocation tag is unavailable",
        memory_get_tag(0U) == NULL
    );

    /* MULTIPLE ALLOCATIONS */

    a = memory_allocate(2U);
    b = memory_allocate(3U);
    c = memory_allocate(4U);

    check(
        "Allocate multiple memory regions",
        a != NULL && b != NULL && c != NULL
    );

    memory_get_info(&info);

    check(
        "Multiple allocations use nine blocks",
        info.used_blocks == 9U
    );

    check(
        "Three active allocations are tracked",
        memory_get_allocation_count() == 3U
    );

    /* TAG INDEX MAPPING */

    check(
        "Set first allocation tag",
        memory_set_tag(0U, "First") ==
        SYNTRA_STATUS_OK
    );

    check(
        "Set second allocation tag",
        memory_set_tag(1U, "Second") ==
        SYNTRA_STATUS_OK
    );

    check(
        "Release first allocation",
        memory_release(a, 2U) == SYNTRA_STATUS_OK
    );

    tag = memory_get_tag(0U);

    check(
        "Active tag index maps after release",
        tag != NULL &&
        tag->active &&
        tag->name[0] == 'S'
    );

    check(
        "Update tag after allocation index changes",
        memory_set_tag(0U, "Updated") ==
        SYNTRA_STATUS_OK
    );

    tag = memory_get_tag(0U);

    check(
        "Updated tag is readable",
        tag != NULL &&
        tag->name[0] == 'U'
    );

    check(
        "Release second allocation",
        memory_release(b, 3U) == SYNTRA_STATUS_OK
    );

    check(
        "Release third allocation",
        memory_release(c, 4U) == SYNTRA_STATUS_OK
    );

    memory_get_info(&info);

    check(
        "Memory returns to empty state",
        info.used_blocks == 0U &&
        info.active_allocations == 0U
    );

    /* FRAGMENTATION */

    a = memory_allocate(2U);
    b = memory_allocate(2U);
    c = memory_allocate(2U);

    check(
        "Prepare fragmentation test",
        a != NULL && b != NULL && c != NULL
    );

    check(
        "Release middle allocation",
        memory_release(b, 2U) == SYNTRA_STATUS_OK
    );

    memory_get_info(&info);

    check(
        "Free region count detects fragmentation",
        info.free_regions == 2U
    );

    check(
        "Fragmentation block count is correct",
        info.fragmentation_blocks == 2U
    );

    check(
        "Largest free region is detected",
        info.largest_free_block ==
        SYNTRA_MAX_MEMORY_BLOCKS - 6U
    );

    check(
        "Release first fragmentation allocation",
        memory_release(a, 2U) == SYNTRA_STATUS_OK
    );

    check(
        "Release final fragmentation allocation",
        memory_release(c, 2U) == SYNTRA_STATUS_OK
    );

    /* SNAPSHOTS */

    check(
        "Create memory snapshot",
        memory_create_snapshot() == SYNTRA_STATUS_OK
    );

    check(
        "Snapshot count increases",
        memory_get_snapshot_count() == 1U
    );

    snapshot = memory_get_snapshot(0U);

    check(
        "Read snapshot",
        snapshot != NULL &&
        snapshot->valid
    );

    check(
        "Snapshot records free memory",
        snapshot != NULL &&
        snapshot->used_blocks == 0U &&
        snapshot->free_blocks ==
        SYNTRA_MAX_MEMORY_BLOCKS
    );

    check(
        "Reject invalid snapshot index",
        memory_get_snapshot(1U) == NULL
    );

    /* SNAPSHOT RING BUFFER */

    for (i = 0U; i < SYNTRA_MEMORY_MAX_SNAPSHOTS + 2U; ++i) {
        memory_create_snapshot();
    }

    check(
        "Snapshot history respects maximum capacity",
        memory_get_snapshot_count() ==
        SYNTRA_MEMORY_MAX_SNAPSHOTS
    );

    /* HISTORY */

    check(
        "History contains recorded events",
        memory_get_history_count() > 0U
    );

    event = memory_get_history(0U);

    check(
        "Read oldest history event",
        event != NULL &&
        event->sequence > 0U
    );

    check(
        "Reject invalid history index",
        memory_get_history(
            memory_get_history_count()
        ) == NULL
    );

    /* REPORT */

    check(
        "Generate memory report",
        memory_generate_report(&report) ==
        SYNTRA_STATUS_OK
    );

    check(
        "Memory report is valid",
        report.valid
    );

    check(
        "Report contains snapshot count",
        report.total_snapshots ==
        SYNTRA_MEMORY_MAX_SNAPSHOTS
    );

    check(
        "Reject null report",
        memory_generate_report(NULL) !=
        SYNTRA_STATUS_OK
    );

    /* POOL STATISTICS */

    check(
        "Get memory pool statistics",
        memory_get_pool_stats(&pool_stats) ==
        SYNTRA_STATUS_OK
    );

    check(
        "Pool statistics are valid",
        pool_stats.valid
    );

    check(
        "Pool byte size is correct",
        pool_stats.total_bytes ==
        SYNTRA_MEMORY_POOL_SIZE
    );

    check(
        "Pool is completely free",
        pool_stats.used_bytes == 0U &&
        pool_stats.free_bytes ==
        SYNTRA_MEMORY_POOL_SIZE
    );

    /* TRACKING */

    check(
        "Get allocation tracking",
        memory_get_tracking(&tracking) ==
        SYNTRA_STATUS_OK
    );

    check(
        "Tracking data is valid",
        tracking.valid
    );

    check(
        "Tracking reports no active allocations",
        tracking.active_tracked == 0U
    );

    check(
        "Tracking reports allocated blocks",
        tracking.total_allocated_blocks ==
        tracking.total_released_blocks
    );

    /* METRICS */

    check(
        "Get memory metrics",
        memory_get_metrics(&metrics) ==
        SYNTRA_STATUS_OK
    );

    check(
        "Metrics are valid",
        metrics.valid
    );

    check(
        "Metrics record allocation failures",
        metrics.failed_operations == 2U
    );

    check(
        "Metrics report zero current usage",
        metrics.current_used_blocks == 0U
    );

    /* VALIDATION */

    check(
        "Memory integrity check",
        memory_check_integrity() ==
        SYNTRA_STATUS_OK
    );

    check(
        "Memory validation",
        memory_validate() ==
        SYNTRA_STATUS_OK
    );

    /* DIAGNOSTICS */

    check(
        "Run memory diagnostics",
        memory_run_diagnostics(&diagnostics) ==
        SYNTRA_STATUS_OK
    );

    check(
        "Diagnostics report valid state",
        diagnostics.valid
    );

    check(
        "Diagnostics detect no integrity errors",
        diagnostics.error_count == 0U
    );

    /* FULL CAPACITY */

    full = memory_allocate(
        SYNTRA_MAX_MEMORY_BLOCKS
    );

    check(
        "Allocate entire memory pool",
        full != NULL
    );

    memory_get_info(&info);

    check(
        "Memory usage reaches one hundred percent",
        info.usage_percent == 100U &&
        info.used_blocks == SYNTRA_MAX_MEMORY_BLOCKS
    );

    check(
        "No free region remains",
        info.largest_free_block == 0U
    );

    check(
        "Reject allocation when pool is full",
        memory_allocate(1U) == NULL
    );

    memory_get_info(&info);

    check(
        "Full-pool allocation failure is recorded",
        info.failed_allocations == 3U
    );

    check(
        "Release entire memory pool",
        memory_release(
            full,
            SYNTRA_MAX_MEMORY_BLOCKS
        ) == SYNTRA_STATUS_OK
    );

    memory_get_info(&info);

    check(
        "Memory pool returns to full free capacity",
        info.used_blocks == 0U &&
        info.free_blocks == SYNTRA_MAX_MEMORY_BLOCKS
    );

    /* FINAL INTEGRITY */

    check(
        "Final memory integrity check",
        memory_check_integrity() ==
        SYNTRA_STATUS_OK
    );

    check(
        "Final memory validation",
        memory_validate() ==
        SYNTRA_STATUS_OK
    );

    printf("\n");
    printf("====================================\n");
    printf("          MEMORY V8 RESULT\n");
    printf("====================================\n");

    printf("Passed: %u\n", passed);
    printf("Failed: %u\n", failed);
    printf("Total : %u\n", passed + failed);

    if (failed == 0U) {
        printf("\nMEMORY V8: ALL TESTS PASSED\n");
        return 0;
    }

    printf("\nMEMORY V8: TESTS FAILED\n");
    return 1;
}
