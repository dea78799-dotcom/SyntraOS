
#ifndef SYNTRA_MEMORY_H
#define SYNTRA_MEMORY_H

#include "../core/status.h"
#include <stdint.h>

#define SYNTRA_MEMORY_BLOCK_SIZE 4096
#define SYNTRA_MAX_MEMORY_BLOCKS 1024

#define SYNTRA_MEMORY_POOL_SIZE \
    (SYNTRA_MEMORY_BLOCK_SIZE * SYNTRA_MAX_MEMORY_BLOCKS)

#define SYNTRA_MEMORY_ALIGNMENT 4096
#define SYNTRA_MEMORY_HISTORY_SIZE 128
#define SYNTRA_MEMORY_TAG_LENGTH 32
#define SYNTRA_MEMORY_MAX_SNAPSHOTS 32

typedef struct
{
    unsigned int total_blocks;
    unsigned int used_blocks;
    unsigned int free_blocks;

    unsigned int allocation_count;
    unsigned int release_count;
    unsigned int active_allocations;
    unsigned int failed_allocations;

    unsigned int largest_free_block;
    unsigned int free_regions;
    unsigned int fragmentation_blocks;
    unsigned int usage_percent;

    unsigned int integrity_errors;
    unsigned int validation_errors;

    unsigned int peak_used_blocks;
    unsigned int peak_usage_percent;

    unsigned int leak_count;
    unsigned int leak_blocks;

} syntra_memory_info_t;

typedef enum
{
    SYNTRA_MEMORY_EVENT_ALLOCATE = 1,
    SYNTRA_MEMORY_EVENT_RELEASE = 2,
    SYNTRA_MEMORY_EVENT_FAILED = 3

} syntra_memory_event_type_t;

typedef struct
{
    syntra_memory_event_type_t type;
    unsigned int start_block;
    unsigned int block_count;
    unsigned int sequence;
    int valid;

} syntra_memory_history_t;

typedef struct
{
    uint32_t slot;
    uint64_t generation;
} syntra_memory_allocation_handle_t;

/* Generic exact owner identity; the process/thread coordinator supplies it. */
typedef struct
{
    uint32_t slot;
    uint32_t generation;
} syntra_memory_owner_handle_t;

typedef enum
{
    SYNTRA_MEMORY_ALLOCATION_FREE = 0,
    SYNTRA_MEMORY_ALLOCATION_LEGACY = 1,
    SYNTRA_MEMORY_ALLOCATION_STAGED = 2,
    SYNTRA_MEMORY_ALLOCATION_RESERVED = 3,
    SYNTRA_MEMORY_ALLOCATION_THREAD_OWNED = 4
} syntra_memory_allocation_state_t;

typedef struct
{
    void *address;
    unsigned int start_block;
    unsigned int block_count;
    int active;
    uint64_t generation;
    syntra_memory_allocation_state_t ownership_state;
    syntra_memory_owner_handle_t owner;

} syntra_memory_allocation_t;

typedef struct
{
    unsigned int total_blocks;
    unsigned int used_blocks;
    unsigned int free_blocks;

    unsigned int active_allocations;
    unsigned int allocation_count;
    unsigned int release_count;
    unsigned int failed_allocations;

    unsigned int usage_percent;
    unsigned int peak_used_blocks;

    unsigned int snapshot_sequence;
    int valid;

} syntra_memory_snapshot_t;

typedef struct
{
    unsigned int total_snapshots;
    unsigned int total_allocations;
    unsigned int total_releases;
    unsigned int total_failures;

    unsigned int current_used_blocks;
    unsigned int current_free_blocks;
    unsigned int peak_used_blocks;

    unsigned int current_usage_percent;
    unsigned int integrity_errors;
    unsigned int validation_errors;

    int valid;

} syntra_memory_report_t;

typedef struct
{
    unsigned int allocation_index;
    char name[SYNTRA_MEMORY_TAG_LENGTH];
    int active;

} syntra_memory_tag_t;

typedef enum
{
    SYNTRA_MEMORY_DIAGNOSTIC_NONE = 0,
    SYNTRA_MEMORY_DIAGNOSTIC_UNINITIALIZED = 1,
    SYNTRA_MEMORY_DIAGNOSTIC_INVALID_ALLOCATION = 2,
    SYNTRA_MEMORY_DIAGNOSTIC_INVALID_RELEASE = 3,
    SYNTRA_MEMORY_DIAGNOSTIC_INTEGRITY_FAILURE = 4,
    SYNTRA_MEMORY_DIAGNOSTIC_CAPACITY_WARNING = 5

} syntra_memory_diagnostic_type_t;

typedef struct
{
    syntra_memory_diagnostic_type_t type;
    unsigned int error_count;
    unsigned int used_blocks;
    unsigned int free_blocks;
    int has_warning;
    int valid;

} syntra_memory_diagnostics_t;

/* V7: Memory Pool Statistics */

typedef struct
{
    unsigned int total_bytes;
    unsigned int used_bytes;
    unsigned int free_bytes;

    unsigned int total_blocks;
    unsigned int used_blocks;
    unsigned int free_blocks;

    unsigned int usage_percent;
    unsigned int largest_free_region_bytes;

    int valid;

} syntra_memory_pool_stats_t;

/* V7.1: Allocation Tracking */

typedef struct
{
    unsigned int total_tracked;
    unsigned int active_tracked;
    unsigned int released_tracked;
    unsigned int failed_tracked;

    unsigned int total_allocated_blocks;
    unsigned int total_released_blocks;

    unsigned int current_allocated_blocks;
    unsigned int peak_allocated_blocks;

    int valid;

} syntra_memory_tracking_t;

/* V7: Performance Metrics */

typedef struct
{
    unsigned int total_operations;
    unsigned int allocation_operations;
    unsigned int release_operations;
    unsigned int failed_operations;

    unsigned int peak_used_blocks;
    unsigned int current_used_blocks;

    unsigned int allocation_success_rate;
    unsigned int usage_percent;

    int valid;

} syntra_memory_metrics_t;

/* Core API */

syntra_status_t memory_init(void);

void *memory_allocate(unsigned int blocks);

syntra_status_t memory_release(
    void *address,
    unsigned int blocks
);

/*
 * Internal kernel lifecycle API. Allocation tokens are generation-aware and
 * must never be exposed to user mode. Legacy memory_release() cannot release
 * STAGED, RESERVED, or THREAD_OWNED allocations.
 */
syntra_status_t memory_allocate_staged(
    unsigned int blocks,
    void **address_out,
    syntra_memory_allocation_handle_t *handle_out
);

syntra_status_t memory_bind_staged_allocation(
    syntra_memory_allocation_handle_t allocation,
    syntra_memory_owner_handle_t owner
);

syntra_status_t memory_commit_thread_allocation(
    syntra_memory_allocation_handle_t allocation,
    syntra_memory_owner_handle_t owner
);

syntra_status_t memory_abort_staged_allocation(
    syntra_memory_allocation_handle_t allocation
);

syntra_status_t memory_abort_reserved_allocation(
    syntra_memory_allocation_handle_t allocation,
    syntra_memory_owner_handle_t owner
);

syntra_status_t memory_release_owned_allocation(
    syntra_memory_allocation_handle_t allocation,
    syntra_memory_owner_handle_t owner
);

/* Copy-out lookup by exact token; never returns a mutable allocator record. */
syntra_status_t memory_get_allocation_by_handle(
    syntra_memory_allocation_handle_t allocation,
    syntra_memory_allocation_t *snapshot_out
);

void memory_get_info(syntra_memory_info_t *info);

unsigned int memory_get_allocation_count(void);

const syntra_memory_allocation_t *memory_get_allocation(
    unsigned int index
);

syntra_status_t memory_validate(void);

syntra_status_t memory_check_integrity(void);

/* History API */

unsigned int memory_get_history_count(void);

const syntra_memory_history_t *memory_get_history(
    unsigned int index
);

/* Snapshot API */

syntra_status_t memory_create_snapshot(void);

unsigned int memory_get_snapshot_count(void);

const syntra_memory_snapshot_t *memory_get_snapshot(
    unsigned int index
);

/* Report API */

syntra_status_t memory_generate_report(
    syntra_memory_report_t *report
);

/* Allocation Tag API */

syntra_status_t memory_set_tag(
    unsigned int allocation_index,
    const char *name
);

const syntra_memory_tag_t *memory_get_tag(
    unsigned int allocation_index
);

/* Diagnostics API */

syntra_status_t memory_run_diagnostics(
    syntra_memory_diagnostics_t *diagnostics
);

/* V7: Statistics API */

syntra_status_t memory_get_pool_stats(
    syntra_memory_pool_stats_t *stats
);

/* V7.1: Tracking API */

syntra_status_t memory_get_tracking(
    syntra_memory_tracking_t *tracking
);

/* V7: Performance API */

syntra_status_t memory_get_metrics(
    syntra_memory_metrics_t *metrics
);

#endif
