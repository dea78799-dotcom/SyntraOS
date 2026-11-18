
#include "memory.h"

#include <stddef.h>
#include <string.h>

#if defined(__GNUC__) || defined(__clang__)
#define SYNTRA_ALIGN __attribute__((aligned(SYNTRA_MEMORY_ALIGNMENT)))
#else
#define SYNTRA_ALIGN
#endif

static unsigned char memory_pool[SYNTRA_MEMORY_POOL_SIZE]
    SYNTRA_ALIGN;

static unsigned char memory_bitmap[SYNTRA_MAX_MEMORY_BLOCKS];

static syntra_memory_allocation_t allocations[
    SYNTRA_MAX_MEMORY_BLOCKS
];

static syntra_memory_history_t history[
    SYNTRA_MEMORY_HISTORY_SIZE
];

static syntra_memory_snapshot_t snapshots[
    SYNTRA_MEMORY_MAX_SNAPSHOTS
];

static syntra_memory_tag_t allocation_tags[
    SYNTRA_MAX_MEMORY_BLOCKS
];

static unsigned int history_count;
static unsigned int history_sequence;

static unsigned int snapshot_count;
static unsigned int snapshot_sequence;

static unsigned int allocation_count;
static unsigned int release_count;
static unsigned int failed_allocations;

static unsigned int used_blocks;
static unsigned int active_allocations;
static unsigned int peak_used_blocks;

static unsigned int integrity_errors;
static unsigned int validation_errors;

static int initialized;

/* Internal helpers */

static void record_history(
    syntra_memory_event_type_t type,
    unsigned int start_block,
    unsigned int block_count
)
{
    unsigned int i;

    if (history_count < SYNTRA_MEMORY_HISTORY_SIZE)
    {
        i = history_count;
        history_count++;
    }
    else
    {
        for (i = 1; i < SYNTRA_MEMORY_HISTORY_SIZE; i++)
        {
            history[i - 1] = history[i];
        }

        i = SYNTRA_MEMORY_HISTORY_SIZE - 1;
    }

    history[i].type = type;
    history[i].start_block = start_block;
    history[i].block_count = block_count;
    history[i].sequence = ++history_sequence;
    history[i].valid = 1;
}

static int allocation_is_active(unsigned int index)
{
    return index < SYNTRA_MAX_MEMORY_BLOCKS &&
           allocations[index].active;
}

static unsigned int calculate_usage_percent(void)
{
    if (used_blocks == 0)
    {
        return 0;
    }

    return (used_blocks * 100U) /
           SYNTRA_MAX_MEMORY_BLOCKS;
}

static unsigned int find_free_allocation_slot(void)
{
    unsigned int i;

    for (i = 0; i < SYNTRA_MAX_MEMORY_BLOCKS; i++)
    {
        if (!allocations[i].active)
        {
            return i;
        }
    }

    return SYNTRA_MAX_MEMORY_BLOCKS;
}

static unsigned int find_free_region(unsigned int blocks)
{
    unsigned int i;
    unsigned int j;

    if (blocks == 0 ||
        blocks > SYNTRA_MAX_MEMORY_BLOCKS)
    {
        return SYNTRA_MAX_MEMORY_BLOCKS;
    }

    for (i = 0;
         i <= SYNTRA_MAX_MEMORY_BLOCKS - blocks;
         i++)
    {
        for (j = 0; j < blocks; j++)
        {
            if (memory_bitmap[i + j] != 0)
            {
                break;
            }
        }

        if (j == blocks)
        {
            return i;
        }

        if (j > 0)
        {
            i += j;
        }
    }

    return SYNTRA_MAX_MEMORY_BLOCKS;
}

/* Core memory API */

syntra_status_t memory_init(void)
{
    if (initialized)
    {
        return SYNTRA_STATUS_OK;
    }

    memset(memory_pool, 0, sizeof(memory_pool));
    memset(memory_bitmap, 0, sizeof(memory_bitmap));
    memset(allocations, 0, sizeof(allocations));
    memset(history, 0, sizeof(history));
    memset(snapshots, 0, sizeof(snapshots));
    memset(allocation_tags, 0, sizeof(allocation_tags));

    history_count = 0;
    history_sequence = 0;

    snapshot_count = 0;
    snapshot_sequence = 0;

    allocation_count = 0;
    release_count = 0;
    failed_allocations = 0;

    used_blocks = 0;
    active_allocations = 0;
    peak_used_blocks = 0;

    integrity_errors = 0;
    validation_errors = 0;

    initialized = 1;

    return SYNTRA_STATUS_OK;
}

void *memory_allocate(unsigned int blocks)
{
    unsigned int start;
    unsigned int slot;
    unsigned int i;

    if (!initialized || blocks == 0)
    {
        failed_allocations++;

        record_history(
            SYNTRA_MEMORY_EVENT_FAILED,
            0,
            blocks
        );

        return NULL;
    }

    if (blocks > SYNTRA_MAX_MEMORY_BLOCKS - used_blocks)
    {
        failed_allocations++;

        record_history(
            SYNTRA_MEMORY_EVENT_FAILED,
            0,
            blocks
        );

        return NULL;
    }

    slot = find_free_allocation_slot();

    if (slot == SYNTRA_MAX_MEMORY_BLOCKS)
    {
        failed_allocations++;

        record_history(
            SYNTRA_MEMORY_EVENT_FAILED,
            0,
            blocks
        );

        return NULL;
    }

    start = find_free_region(blocks);

    if (start == SYNTRA_MAX_MEMORY_BLOCKS)
    {
        failed_allocations++;

        record_history(
            SYNTRA_MEMORY_EVENT_FAILED,
            0,
            blocks
        );

        return NULL;
    }

    for (i = 0; i < blocks; i++)
    {
        memory_bitmap[start + i] = 1;
    }

    allocations[slot].address =
        &memory_pool[start * SYNTRA_MEMORY_BLOCK_SIZE];

    allocations[slot].start_block = start;
    allocations[slot].block_count = blocks;
    allocations[slot].active = 1;

    allocation_tags[slot].allocation_index = slot;
    allocation_tags[slot].name[0] = '\0';
    allocation_tags[slot].active = 0;

    used_blocks += blocks;
    active_allocations++;
    allocation_count++;

    if (used_blocks > peak_used_blocks)
    {
        peak_used_blocks = used_blocks;
    }

    record_history(
        SYNTRA_MEMORY_EVENT_ALLOCATE,
        start,
        blocks
    );

    return allocations[slot].address;
}

syntra_status_t memory_release(
    void *address,
    unsigned int blocks
)
{
    unsigned int i;
    unsigned int j;

    if (!initialized || address == NULL || blocks == 0)
    {
        validation_errors++;
        return SYNTRA_STATUS_ERROR;
    }

    for (i = 0; i < SYNTRA_MAX_MEMORY_BLOCKS; i++)
    {
        if (allocations[i].active &&
            allocations[i].address == address)
        {
            break;
        }
    }

    if (i == SYNTRA_MAX_MEMORY_BLOCKS)
    {
        validation_errors++;
        return SYNTRA_STATUS_ERROR;
    }

    if (allocations[i].block_count != blocks)
    {
        validation_errors++;
        return SYNTRA_STATUS_ERROR;
    }

    if (allocations[i].start_block >
        SYNTRA_MAX_MEMORY_BLOCKS - blocks)
    {
        integrity_errors++;
        return SYNTRA_STATUS_ERROR;
    }

    for (j = 0; j < blocks; j++)
    {
        if (memory_bitmap[
                allocations[i].start_block + j
            ] != 1)
        {
            integrity_errors++;
            return SYNTRA_STATUS_ERROR;
        }
    }

    memset(
        &memory_pool[
            allocations[i].start_block *
            SYNTRA_MEMORY_BLOCK_SIZE
        ],
        0,
        blocks * SYNTRA_MEMORY_BLOCK_SIZE
    );

    for (j = 0; j < blocks; j++)
    {
        memory_bitmap[
            allocations[i].start_block + j
        ] = 0;
    }

    record_history(
        SYNTRA_MEMORY_EVENT_RELEASE,
        allocations[i].start_block,
        blocks
    );

    allocation_tags[i].active = 0;

    allocations[i].address = NULL;
    allocations[i].start_block = 0;
    allocations[i].block_count = 0;
    allocations[i].active = 0;

    used_blocks -= blocks;
    active_allocations--;
    release_count++;

    return SYNTRA_STATUS_OK;
}

/* Memory information */

void memory_get_info(syntra_memory_info_t *info)
{
    unsigned int i;
    unsigned int current_region = 0;

    if (info == NULL)
    {
        return;
    }

    memset(info, 0, sizeof(*info));

    info->total_blocks = SYNTRA_MAX_MEMORY_BLOCKS;
    info->used_blocks = used_blocks;
    info->free_blocks =
        SYNTRA_MAX_MEMORY_BLOCKS - used_blocks;

    info->allocation_count = allocation_count;
    info->release_count = release_count;
    info->active_allocations = active_allocations;
    info->failed_allocations = failed_allocations;

    info->peak_used_blocks = peak_used_blocks;
    info->usage_percent = calculate_usage_percent();

    info->peak_usage_percent =
        (peak_used_blocks * 100U) /
        SYNTRA_MAX_MEMORY_BLOCKS;

    info->integrity_errors = integrity_errors;
    info->validation_errors = validation_errors;

    info->leak_count = active_allocations;
    info->leak_blocks = used_blocks;

    for (i = 0; i < SYNTRA_MAX_MEMORY_BLOCKS; i++)
    {
        if (memory_bitmap[i] == 0)
        {
            current_region++;
        }
        else
        {
            if (current_region > info->largest_free_block)
            {
                info->largest_free_block = current_region;
            }

            if (current_region > 0)
            {
                info->free_regions++;
            }

            current_region = 0;
        }
    }

    if (current_region > info->largest_free_block)
    {
        info->largest_free_block = current_region;
    }

    if (current_region > 0)
    {
        info->free_regions++;
    }

    info->fragmentation_blocks =
        info->free_blocks - info->largest_free_block;
}

unsigned int memory_get_allocation_count(void)
{
    return active_allocations;
}

const syntra_memory_allocation_t *memory_get_allocation(
    unsigned int index
)
{
    if (index >= SYNTRA_MAX_MEMORY_BLOCKS ||
        !allocations[index].active)
    {
        return NULL;
    }

    return &allocations[index];
}

/* Memory validation */

syntra_status_t memory_check_integrity(void)
{
    unsigned int i;
    unsigned int j;
    unsigned int bitmap_used = 0;
    unsigned int allocation_used = 0;
    unsigned int active_count = 0;

    if (!initialized)
    {
        integrity_errors++;
        return SYNTRA_STATUS_ERROR;
    }

    for (i = 0; i < SYNTRA_MAX_MEMORY_BLOCKS; i++)
    {
        if (memory_bitmap[i] == 1)
        {
            bitmap_used++;
        }
    }

    for (i = 0; i < SYNTRA_MAX_MEMORY_BLOCKS; i++)
    {
        unsigned int a_start;
        unsigned int a_end;

        if (!allocations[i].active)
        {
            continue;
        }

        active_count++;
        allocation_used += allocations[i].block_count;

        if (allocations[i].block_count == 0 ||
            allocations[i].start_block >
            SYNTRA_MAX_MEMORY_BLOCKS -
            allocations[i].block_count)
        {
            integrity_errors++;
            return SYNTRA_STATUS_ERROR;
        }

        if (allocations[i].address !=
            &memory_pool[
                allocations[i].start_block *
                SYNTRA_MEMORY_BLOCK_SIZE
            ])
        {
            integrity_errors++;
            return SYNTRA_STATUS_ERROR;
        }

        a_start = allocations[i].start_block;
        a_end = a_start + allocations[i].block_count;

        for (j = 0; j < allocations[i].block_count; j++)
        {
            if (memory_bitmap[a_start + j] != 1)
            {
                integrity_errors++;
                return SYNTRA_STATUS_ERROR;
            }
        }

        for (j = i + 1; j < SYNTRA_MAX_MEMORY_BLOCKS; j++)
        {
            unsigned int b_start;
            unsigned int b_end;

            if (!allocations[j].active)
            {
                continue;
            }

            b_start = allocations[j].start_block;
            b_end = b_start + allocations[j].block_count;

            if (a_start < b_end && b_start < a_end)
            {
                integrity_errors++;
                return SYNTRA_STATUS_ERROR;
            }
        }
    }

    if (bitmap_used != used_blocks ||
        allocation_used != used_blocks ||
        active_count != active_allocations)
    {
        integrity_errors++;
        return SYNTRA_STATUS_ERROR;
    }

    return SYNTRA_STATUS_OK;
}

syntra_status_t memory_validate(void)
{
    if (memory_check_integrity() != SYNTRA_STATUS_OK)
    {
        validation_errors++;
        return SYNTRA_STATUS_ERROR;
    }

    return SYNTRA_STATUS_OK;
}

/* Memory history */

unsigned int memory_get_history_count(void)
{
    return history_count;
}

const syntra_memory_history_t *memory_get_history(
    unsigned int index
)
{
    if (index >= history_count)
    {
        return NULL;
    }

    return &history[index];
}

/* Memory snapshot */

syntra_status_t memory_create_snapshot(void)
{
    syntra_memory_snapshot_t *snapshot;

    if (!initialized ||
        snapshot_count >= SYNTRA_MEMORY_MAX_SNAPSHOTS)
    {
        return SYNTRA_STATUS_ERROR;
    }

    snapshot = &snapshots[snapshot_count];

    snapshot->total_blocks = SYNTRA_MAX_MEMORY_BLOCKS;
    snapshot->used_blocks = used_blocks;
    snapshot->free_blocks =
        SYNTRA_MAX_MEMORY_BLOCKS - used_blocks;

    snapshot->active_allocations = active_allocations;
    snapshot->allocation_count = allocation_count;
    snapshot->release_count = release_count;
    snapshot->failed_allocations = failed_allocations;

    snapshot->usage_percent = calculate_usage_percent();
    snapshot->peak_used_blocks = peak_used_blocks;

    snapshot->snapshot_sequence = ++snapshot_sequence;
    snapshot->valid = 1;

    snapshot_count++;

    return SYNTRA_STATUS_OK;
}

unsigned int memory_get_snapshot_count(void)
{
    return snapshot_count;
}

const syntra_memory_snapshot_t *memory_get_snapshot(
    unsigned int index
)
{
    if (index >= snapshot_count || !snapshots[index].valid)
    {
        return NULL;
    }

    return &snapshots[index];
}

/* Memory report */

syntra_status_t memory_generate_report(
    syntra_memory_report_t *report
)
{
    if (report == NULL || !initialized)
    {
        return SYNTRA_STATUS_ERROR;
    }

    memset(report, 0, sizeof(*report));

    report->total_snapshots = snapshot_count;
    report->total_allocations = allocation_count;
    report->total_releases = release_count;
    report->total_failures = failed_allocations;

    report->current_used_blocks = used_blocks;
    report->current_free_blocks =
        SYNTRA_MAX_MEMORY_BLOCKS - used_blocks;

    report->peak_used_blocks = peak_used_blocks;
    report->current_usage_percent = calculate_usage_percent();

    report->integrity_errors = integrity_errors;
    report->validation_errors = validation_errors;

    report->valid = 1;

    return SYNTRA_STATUS_OK;
}

/* Allocation tags */

syntra_status_t memory_set_tag(
    unsigned int allocation_index,
    const char *name
)
{
    unsigned int i;

    if (!initialized ||
        name == NULL ||
        !allocation_is_active(allocation_index))
    {
        return SYNTRA_STATUS_ERROR;
    }

    for (i = 0; i < SYNTRA_MEMORY_TAG_LENGTH - 1; i++)
    {
        allocation_tags[allocation_index].name[i] = name[i];

        if (name[i] == '\0')
        {
            break;
        }
    }

    allocation_tags[allocation_index].name[i] = '\0';

    allocation_tags[allocation_index].allocation_index =
        allocation_index;

    allocation_tags[allocation_index].active = 1;

    return SYNTRA_STATUS_OK;
}

const syntra_memory_tag_t *memory_get_tag(
    unsigned int allocation_index
)
{
    if (!initialized ||
        allocation_index >= SYNTRA_MAX_MEMORY_BLOCKS ||
        !allocation_tags[allocation_index].active ||
        !allocation_is_active(allocation_index))
    {
        return NULL;
    }

    return &allocation_tags[allocation_index];
}

/* Memory diagnostics */

syntra_status_t memory_run_diagnostics(
    syntra_memory_diagnostics_t *diagnostics
)
{
    syntra_memory_info_t info;

    if (diagnostics == NULL)
    {
        return SYNTRA_STATUS_ERROR;
    }

    memset(diagnostics, 0, sizeof(*diagnostics));

    if (!initialized)
    {
        diagnostics->type =
            SYNTRA_MEMORY_DIAGNOSTIC_UNINITIALIZED;

        diagnostics->error_count = 1;
        diagnostics->has_warning = 1;
        diagnostics->valid = 1;

        return SYNTRA_STATUS_ERROR;
    }

    memory_get_info(&info);

    diagnostics->used_blocks = info.used_blocks;
    diagnostics->free_blocks = info.free_blocks;

    diagnostics->error_count =
        info.integrity_errors +
        info.validation_errors;

    if (memory_check_integrity() != SYNTRA_STATUS_OK)
    {
        diagnostics->type =
            SYNTRA_MEMORY_DIAGNOSTIC_INTEGRITY_FAILURE;

        diagnostics->has_warning = 1;
        diagnostics->valid = 1;

        return SYNTRA_STATUS_ERROR;
    }

    if (info.free_blocks == 0)
    {
        diagnostics->type =
            SYNTRA_MEMORY_DIAGNOSTIC_CAPACITY_WARNING;

        diagnostics->has_warning = 1;
    }
    else
    {
        diagnostics->type =
            SYNTRA_MEMORY_DIAGNOSTIC_NONE;

        diagnostics->has_warning = 0;
    }

    diagnostics->valid = 1;

    return SYNTRA_STATUS_OK;
}

/* V7: Memory Pool Statistics */

syntra_status_t memory_get_pool_stats(
    syntra_memory_pool_stats_t *stats
)
{
    syntra_memory_info_t info;

    if (stats == NULL || !initialized)
    {
        return SYNTRA_STATUS_ERROR;
    }

    memset(stats, 0, sizeof(*stats));

    memory_get_info(&info);

    stats->total_blocks = info.total_blocks;
    stats->used_blocks = info.used_blocks;
    stats->free_blocks = info.free_blocks;

    stats->total_bytes = SYNTRA_MEMORY_POOL_SIZE;

    stats->used_bytes =
        used_blocks * SYNTRA_MEMORY_BLOCK_SIZE;

    stats->free_bytes =
        stats->total_bytes - stats->used_bytes;

    stats->usage_percent = info.usage_percent;

    stats->largest_free_region_bytes =
        info.largest_free_block *
        SYNTRA_MEMORY_BLOCK_SIZE;

    stats->valid = 1;

    return SYNTRA_STATUS_OK;
}

/* V7: Allocation Tracking */

syntra_status_t memory_get_tracking(
    syntra_memory_tracking_t *tracking
)
{
    if (tracking == NULL || !initialized)
    {
        return SYNTRA_STATUS_ERROR;
    }

    memset(tracking, 0, sizeof(*tracking));

    tracking->total_tracked =
        allocation_count + failed_allocations;

    tracking->active_tracked = active_allocations;
    tracking->released_tracked = release_count;
    tracking->failed_tracked = failed_allocations;

    tracking->total_allocated_blocks =
        allocation_count == 0
            ? 0
            : 0;

    tracking->current_allocated_blocks = used_blocks;
    tracking->peak_allocated_blocks = peak_used_blocks;

    tracking->valid = 1;

    return SYNTRA_STATUS_OK;
}

/* V7: Memory Performance Metrics */

syntra_status_t memory_get_metrics(
    syntra_memory_metrics_t *metrics
)
{
    unsigned int allocation_attempts;

    if (metrics == NULL || !initialized)
    {
        return SYNTRA_STATUS_ERROR;
    }

    memset(metrics, 0, sizeof(*metrics));

    allocation_attempts =
        allocation_count + failed_allocations;

    metrics->allocation_operations = allocation_count;
    metrics->release_operations = release_count;
    metrics->failed_operations = failed_allocations;

    metrics->total_operations =
        allocation_count +
        release_count +
        failed_allocations;

    metrics->current_used_blocks = used_blocks;
    metrics->peak_used_blocks = peak_used_blocks;
    metrics->usage_percent = calculate_usage_percent();

    if (allocation_attempts > 0)
    {
        metrics->allocation_success_rate =
            (allocation_count * 100U) /
            allocation_attempts;
    }

    metrics->valid = 1;

    return SYNTRA_STATUS_OK;
}
