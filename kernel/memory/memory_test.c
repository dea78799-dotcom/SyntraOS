
#include <stdio.h>

#include "memory/memory.h"

#define CHECK(condition, message)                         \
    do {                                                  \
        if (!(condition)) {                               \
            printf("[FAIL] %s\n", message);               \
            return 1;                                     \
        }                                                 \
    } while (0)

int main(void)
{
    syntra_memory_info_t info;
    syntra_memory_snapshot_t snapshot;
    syntra_memory_report_t report;
    syntra_memory_pool_stats_t pool_stats;
    syntra_memory_tracking_t tracking;
    syntra_memory_metrics_t metrics;
    syntra_memory_diagnostics_t diagnostics;

    void *allocation_a;
    void *allocation_b;
    void *allocation_c;
    void *full_allocation;

    unsigned int i;
    unsigned int previous_history_sequence;
    unsigned int expected_success_rate;

    printf("=== SyntraOS Memory V9 Tests ===\n");

    /* 1. Initialize */
    CHECK(memory_init() == SYNTRA_STATUS_OK,
          "Memory initialization");

    memory_get_info(&info);

    CHECK(info.total_blocks == SYNTRA_MAX_MEMORY_BLOCKS,
          "Initial total blocks");

    CHECK(info.used_blocks == 0U,
          "Initial used blocks");

    CHECK(info.free_blocks == SYNTRA_MAX_MEMORY_BLOCKS,
          "Initial free blocks");

    CHECK(info.active_allocations == 0U,
          "Initial active allocations");

    /* 2. Invalid allocation tests */
    CHECK(memory_allocate(0U) == NULL,
          "Reject zero-block allocation");

    CHECK(memory_allocate(SYNTRA_MAX_MEMORY_BLOCKS + 1U) == NULL,
          "Reject oversized allocation");

    memory_get_info(&info);

    CHECK(info.failed_allocations == 2U,
          "Invalid allocation counter");

    /* 3. Allocate three regions */
    allocation_a = memory_allocate(2U);
    allocation_b = memory_allocate(2U);
    allocation_c = memory_allocate(2U);

    CHECK(allocation_a != NULL, "Allocate A");
    CHECK(allocation_b != NULL, "Allocate B");
    CHECK(allocation_c != NULL, "Allocate C");

    CHECK(memory_get_allocation_count() == 3U,
          "Allocation count");

    CHECK(memory_get_allocation(0U) != NULL,
          "Get allocation A");

    CHECK(memory_get_allocation(1U) != NULL,
          "Get allocation B");

    CHECK(memory_get_allocation(2U) != NULL,
          "Get allocation C");

    CHECK(memory_get_allocation(0U)->block_count == 2U,
          "Allocation A block count");

    /* 4. Invalid release tests */
    CHECK(memory_release(NULL, 2U) == SYNTRA_STATUS_INVALID_ARGUMENT,
          "Reject NULL release");

    CHECK(memory_release(allocation_a, 1U) == SYNTRA_STATUS_INVALID_ARGUMENT,
          "Reject incorrect release size");

    CHECK(memory_get_allocation_count() == 3U,
          "Invalid release preserves allocations");

    /* 5. Tag tests */
    CHECK(memory_set_tag(0U, "Allocation_A") == SYNTRA_STATUS_OK,
          "Set tag A");

    CHECK(memory_set_tag(1U, "Allocation_B") == SYNTRA_STATUS_OK,
          "Set tag B");

    CHECK(memory_get_tag(0U) != NULL,
          "Get tag A");

    CHECK(memory_get_tag(0U)->active != 0,
          "Tag A is active");

    /* 6. Fragmentation test */
    CHECK(memory_release(allocation_b, 2U) == SYNTRA_STATUS_OK,
          "Release allocation B");

    memory_get_info(&info);

    CHECK(info.used_blocks == 4U,
          "Fragmentation used blocks");

    CHECK(info.free_blocks == SYNTRA_MAX_MEMORY_BLOCKS - 4U,
          "Fragmentation free blocks");

    CHECK(info.free_regions == 2U,
          "Fragmentation free regions");

    CHECK(info.fragmentation_blocks == 2U,
          "Fragmentation block count");

    CHECK(info.largest_free_block == SYNTRA_MAX_MEMORY_BLOCKS - 6U,
          "Largest free block");

    /* 7. Tag index update after release */
    CHECK(memory_get_tag(0U) != NULL,
          "Tag A remains available");

    CHECK(memory_release(allocation_a, 2U) == SYNTRA_STATUS_OK,
          "Release allocation A");

    CHECK(memory_get_allocation_count() == 1U,
          "One allocation remains");

    CHECK(memory_get_tag(0U) != NULL,
          "Tag C index lookup");

    CHECK(memory_get_tag(0U)->allocation_index == 0U,
          "Tag index updated");

    CHECK(memory_release(allocation_c, 2U) == SYNTRA_STATUS_OK,
          "Release allocation C");

    CHECK(memory_get_allocation_count() == 0U,
          "All allocations released");

    /* 8. Snapshot tests */
    for (i = 0U; i < 40U; ++i) {
        CHECK(memory_create_snapshot() == SYNTRA_STATUS_OK,
              "Create snapshot");
    }

    CHECK(memory_get_snapshot_count() == SYNTRA_MEMORY_MAX_SNAPSHOTS,
          "Snapshot ring capacity");

    CHECK(memory_get_snapshot(0U) != NULL,
          "Get oldest snapshot");

    CHECK(memory_get_snapshot(
              SYNTRA_MEMORY_MAX_SNAPSHOTS - 1U) != NULL,
          "Get newest snapshot");

    CHECK(memory_get_snapshot(0U)->snapshot_sequence == 9U,
          "Oldest snapshot sequence");

    CHECK(memory_get_snapshot(
              SYNTRA_MEMORY_MAX_SNAPSHOTS - 1U)->snapshot_sequence == 40U,
          "Newest snapshot sequence");

    /* 9. Report tests */
    CHECK(memory_generate_report(&report) == SYNTRA_STATUS_OK,
          "Generate memory report");

    CHECK(report.valid != 0,
          "Report validity");

    CHECK(report.current_used_blocks == 0U,
          "Report current used blocks");

    CHECK(report.current_free_blocks == SYNTRA_MAX_MEMORY_BLOCKS,
          "Report current free blocks");

    /* 10. Pool statistics */
    CHECK(memory_get_pool_stats(&pool_stats) == SYNTRA_STATUS_OK,
          "Get pool statistics");

    CHECK(pool_stats.valid != 0,
          "Pool statistics validity");

    CHECK(pool_stats.total_bytes == SYNTRA_MEMORY_POOL_SIZE,
          "Pool total bytes");

    CHECK(pool_stats.used_bytes == 0U,
          "Pool used bytes");

    CHECK(pool_stats.free_bytes == SYNTRA_MEMORY_POOL_SIZE,
          "Pool free bytes");

    /* 11. Tracking */
    CHECK(memory_get_tracking(&tracking) == SYNTRA_STATUS_OK,
          "Get tracking");

    CHECK(tracking.valid != 0,
          "Tracking validity");

    CHECK(tracking.active_tracked == 0U,
          "No active tracked allocations");

    CHECK(tracking.current_allocated_blocks == 0U,
          "Current tracked blocks");

    /* 12. Metrics */
    CHECK(memory_get_metrics(&metrics) == SYNTRA_STATUS_OK,
          "Get metrics");

    CHECK(metrics.valid != 0,
          "Metrics validity");

    CHECK(metrics.failed_operations == 2U,
          "Failed operations count");

    CHECK(metrics.allocation_operations == 5U,
          "Allocation operations count");

    CHECK(metrics.release_operations == 5U,
          "Release operations count");

    expected_success_rate =
        (metrics.allocation_operations * 100U) /
        (metrics.allocation_operations + metrics.failed_operations);

    CHECK(metrics.allocation_success_rate == expected_success_rate,
          "Allocation success rate");

    /* 13. Diagnostics */
    CHECK(memory_run_diagnostics(&diagnostics) == SYNTRA_STATUS_OK,
          "Run diagnostics");

    CHECK(diagnostics.valid != 0,
          "Diagnostics validity");

    CHECK(diagnostics.has_warning == 0,
          "No diagnostics warning");

    CHECK(diagnostics.error_count == 0U,
          "No integrity or validation errors");

    /* 14. Validation */
    CHECK(memory_validate() == SYNTRA_STATUS_OK,
          "Memory validation");

    CHECK(memory_check_integrity() == SYNTRA_STATUS_OK,
          "Memory integrity");

    /* 15. History ring test */
    CHECK(memory_get_history_count() > 0U,
          "History contains events");

    previous_history_sequence =
        memory_get_history(memory_get_history_count() - 1U)->sequence;

    for (i = 0U; i < 140U; ++i) {
        CHECK(memory_allocate(0U) == NULL,
              "History ring invalid allocation");
    }

    CHECK(memory_get_history_count() == SYNTRA_MEMORY_HISTORY_SIZE,
          "History ring capacity");

    CHECK(memory_get_history(0U) != NULL,
          "Get oldest history event");

    CHECK(memory_get_history(
              SYNTRA_MEMORY_HISTORY_SIZE - 1U) != NULL,
          "Get newest history event");

    CHECK(memory_get_history(0U)->sequence ==
              previous_history_sequence + 13U,
          "Oldest history sequence");

    /* 16. Full memory capacity test */
    full_allocation = memory_allocate(SYNTRA_MAX_MEMORY_BLOCKS);

    CHECK(full_allocation != NULL,
          "Allocate full memory pool");

    memory_get_info(&info);

    CHECK(info.used_blocks == SYNTRA_MAX_MEMORY_BLOCKS,
          "Full pool used blocks");

    CHECK(info.free_blocks == 0U,
          "Full pool free blocks");

    CHECK(memory_allocate(1U) == NULL,
          "Reject allocation when pool is full");

    CHECK(memory_release(
              full_allocation,
              SYNTRA_MAX_MEMORY_BLOCKS) == SYNTRA_STATUS_OK,
          "Release full memory pool");

    memory_get_info(&info);

    CHECK(info.used_blocks == 0U,
          "Memory empty after full release");

    CHECK(info.free_blocks == SYNTRA_MAX_MEMORY_BLOCKS,
          "All blocks available after release");

    /* 17. Final integrity check */
    CHECK(memory_validate() == SYNTRA_STATUS_OK,
          "Final memory validation");

    CHECK(memory_check_integrity() == SYNTRA_STATUS_OK,
          "Final integrity check");

    printf("[PASS] All Memory V9 tests completed.\n");

    return 0;
}
