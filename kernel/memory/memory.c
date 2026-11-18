
#include "memory.h"

#include <stddef.h>
#include <stdint.h>

#define SYNTRA_MEMORY_BLOCK_SIZE 4096U
#define SYNTRA_MEMORY_POOL_SIZE (4U * 1024U * 1024U)
#define SYNTRA_MAX_MEMORY_BLOCKS \
    (SYNTRA_MEMORY_POOL_SIZE / SYNTRA_MEMORY_BLOCK_SIZE)

#define SYNTRA_MEMORY_BITMAP_SIZE \
    ((SYNTRA_MAX_MEMORY_BLOCKS + 7U) / 8U)

#define SYNTRA_MEMORY_MAX_ALLOCATIONS 256U
#define SYNTRA_MEMORY_HISTORY_SIZE 128U

typedef struct {
    uint32_t start_block;
    uint32_t block_count;
    uint8_t active;
    char tag[32];
} memory_allocation_t;

typedef struct {
    uint32_t operation;
    uint32_t start_block;
    uint32_t block_count;
    int32_t status;
} memory_history_entry_t;

static uint8_t memory_pool[SYNTRA_MEMORY_POOL_SIZE];
static uint8_t memory_bitmap[SYNTRA_MEMORY_BITMAP_SIZE];

static memory_allocation_t allocations[SYNTRA_MEMORY_MAX_ALLOCATIONS];
static memory_history_entry_t history[SYNTRA_MEMORY_HISTORY_SIZE];

static uint32_t allocation_count;
static uint32_t active_allocations;
static uint32_t release_count;
static uint32_t failed_allocations;

static uint32_t used_blocks;
static uint32_t peak_used_blocks;

static uint32_t total_allocated_blocks;
static uint32_t total_released_blocks;

static uint32_t history_count;
static uint32_t history_index;

static uint32_t snapshot_count;
static uint32_t snapshot_index;

static uint32_t integrity_errors;

static uint8_t initialized;

static uint32_t min_u32(uint32_t a, uint32_t b)
{
    return a < b ? a : b;
}

static void copy_tag(char *destination, const char *source)
{
    uint32_t i = 0;

    if (destination == NULL) {
        return;
    }

    for (i = 0; i < 31U; ++i) {
        if (source == NULL || source[i] == '\0') {
            break;
        }

        destination[i] = source[i];
    }

    destination[i] = '\0';
}

static void add_history(
    uint32_t operation,
    uint32_t start_block,
    uint32_t block_count,
    int32_t status
)
{
    memory_history_entry_t *entry;

    entry = &history[history_index];

    entry->operation = operation;
    entry->start_block = start_block;
    entry->block_count = block_count;
    entry->status = status;

    history_index =
        (history_index + 1U) % SYNTRA_MEMORY_HISTORY_SIZE;

    if (history_count < SYNTRA_MEMORY_HISTORY_SIZE) {
        history_count++;
    }
}

static int is_block_used(uint32_t block)
{
    uint32_t byte_index;
    uint32_t bit_index;

    if (block >= SYNTRA_MAX_MEMORY_BLOCKS) {
        return 1;
    }

    byte_index = block / 8U;
    bit_index = block % 8U;

    return (memory_bitmap[byte_index] &
            (uint8_t)(1U << bit_index)) != 0U;
}

static void set_block_used(uint32_t block)
{
    uint32_t byte_index = block / 8U;
    uint32_t bit_index = block % 8U;

    memory_bitmap[byte_index] |=
        (uint8_t)(1U << bit_index);
}

static void set_block_free(uint32_t block)
{
    uint32_t byte_index = block / 8U;
    uint32_t bit_index = block % 8U;

    memory_bitmap[byte_index] &=
        (uint8_t)~(1U << bit_index);
}

static int find_free_region(
    uint32_t requested_blocks,
    uint32_t *start_block
)
{
    uint32_t i;
    uint32_t j;

    if (start_block == NULL ||
        requested_blocks == 0U ||
        requested_blocks > SYNTRA_MAX_MEMORY_BLOCKS) {
        return 0;
    }

    for (i = 0U;
         i <= SYNTRA_MAX_MEMORY_BLOCKS - requested_blocks;
         ++i) {

        for (j = 0U; j < requested_blocks; ++j) {
            if (is_block_used(i + j)) {
                break;
            }
        }

        if (j == requested_blocks) {
            *start_block = i;
            return 1;
        }

        if (j > 0U) {
            i += j - 1U;
        }
    }

    return 0;
}

static int find_free_allocation_slot(uint32_t *slot)
{
    uint32_t i;

    if (slot == NULL) {
        return 0;
    }

    for (i = 0U; i < SYNTRA_MEMORY_MAX_ALLOCATIONS; ++i) {
        if (!allocations[i].active) {
            *slot = i;
            return 1;
        }
    }

    return 0;
}

void memory_init(void)
{
    uint32_t i;

    if (initialized) {
        return;
    }

    for (i = 0U; i < SYNTRA_MEMORY_BITMAP_SIZE; ++i) {
        memory_bitmap[i] = 0U;
    }

    for (i = 0U; i < SYNTRA_MEMORY_MAX_ALLOCATIONS; ++i) {
        allocations[i].start_block = 0U;
        allocations[i].block_count = 0U;
        allocations[i].active = 0U;
        allocations[i].tag[0] = '\0';
    }

    allocation_count = 0U;
    active_allocations = 0U;
    release_count = 0U;
    failed_allocations = 0U;

    used_blocks = 0U;
    peak_used_blocks = 0U;

    total_allocated_blocks = 0U;
    total_released_blocks = 0U;

    history_count = 0U;
    history_index = 0U;

    snapshot_count = 0U;
    snapshot_index = 0U;

    integrity_errors = 0U;

    initialized = 1U;

    add_history(1U, 0U, 0U, 0);
}

void *memory_allocate(uint32_t size)
{
    uint32_t requested_blocks;
    uint32_t start_block;
    uint32_t slot;
    uint32_t i;

    if (!initialized || size == 0U) {
        failed_allocations++;
        add_history(2U, 0U, 0U, -1);
        return NULL;
    }

    requested_blocks =
        (size + SYNTRA_MEMORY_BLOCK_SIZE - 1U) /
        SYNTRA_MEMORY_BLOCK_SIZE;

    if (requested_blocks == 0U ||
        requested_blocks > SYNTRA_MAX_MEMORY_BLOCKS) {
        failed_allocations++;
        add_history(2U, 0U, requested_blocks, -1);
        return NULL;
    }

    if (!find_free_allocation_slot(&slot)) {
        failed_allocations++;
        add_history(2U, 0U, requested_blocks, -1);
        return NULL;
    }

    if (!find_free_region(requested_blocks, &start_block)) {
        failed_allocations++;
        add_history(2U, 0U, requested_blocks, -1);
        return NULL;
    }

    for (i = 0U; i < requested_blocks; ++i) {
        set_block_used(start_block + i);
    }

    allocations[slot].start_block = start_block;
    allocations[slot].block_count = requested_blocks;
    allocations[slot].active = 1U;
    allocations[slot].tag[0] = '\0';

    allocation_count++;
    active_allocations++;

    used_blocks += requested_blocks;

    total_allocated_blocks += requested_blocks;

    if (used_blocks > peak_used_blocks) {
        peak_used_blocks = used_blocks;
    }

    add_history(2U, start_block, requested_blocks, 0);

    return &memory_pool[
        start_block * SYNTRA_MEMORY_BLOCK_SIZE
    ];
}

int memory_release(void *address)
{
    uint32_t i;
    uint32_t j;
    uint32_t start_block;
    uint32_t blocks;
    uintptr_t pool_start;
    uintptr_t pool_end;
    uintptr_t target;

    if (!initialized || address == NULL) {
        add_history(3U, 0U, 0U, -1);
        return -1;
    }

    pool_start = (uintptr_t)&memory_pool[0];
    pool_end = pool_start + SYNTRA_MEMORY_POOL_SIZE;
    target = (uintptr_t)address;

    if (target < pool_start || target >= pool_end) {
        add_history(3U, 0U, 0U, -1);
        return -1;
    }

    if ((target - pool_start) % SYNTRA_MEMORY_BLOCK_SIZE != 0U) {
        add_history(3U, 0U, 0U, -1);
        return -1;
    }

    start_block =
        (uint32_t)((target - pool_start) /
                   SYNTRA_MEMORY_BLOCK_SIZE);

    for (i = 0U; i < SYNTRA_MEMORY_MAX_ALLOCATIONS; ++i) {
        if (allocations[i].active &&
            allocations[i].start_block == start_block) {
            break;
        }
    }

    if (i == SYNTRA_MEMORY_MAX_ALLOCATIONS) {
        add_history(3U, start_block, 0U, -1);
        return -1;
    }

    blocks = allocations[i].block_count;

    if (blocks == 0U ||
        start_block > SYNTRA_MAX_MEMORY_BLOCKS - blocks ||
        used_blocks < blocks ||
        active_allocations == 0U) {
        integrity_errors++;
        add_history(3U, start_block, blocks, -1);
        return -1;
    }

    for (j = 0U; j < blocks; ++j) {
        if (!is_block_used(start_block + j)) {
            integrity_errors++;
            add_history(3U, start_block, blocks, -1);
            return -1;
        }
    }

    for (j = 0U; j < blocks; ++j) {
        set_block_free(start_block + j);
    }

    used_blocks -= blocks;
    active_allocations--;

    release_count++;
    total_released_blocks += blocks;

    allocations[i].active = 0U;
    allocations[i].start_block = 0U;
    allocations[i].block_count = 0U;
    allocations[i].tag[0] = '\0';

    add_history(3U, start_block, blocks, 0);

    return 0;
}

int memory_get_info(syntra_memory_info_t *info)
{
    if (info == NULL || !initialized) {
        return -1;
    }

    info->total_memory = SYNTRA_MEMORY_POOL_SIZE;
    info->used_memory =
        used_blocks * SYNTRA_MEMORY_BLOCK_SIZE;
    info->free_memory =
        SYNTRA_MEMORY_POOL_SIZE - info->used_memory;

    info->total_blocks = SYNTRA_MAX_MEMORY_BLOCKS;
    info->used_blocks = used_blocks;
    info->free_blocks =
        SYNTRA_MAX_MEMORY_BLOCKS - used_blocks;

    info->allocation_count = active_allocations;
    info->release_count = release_count;
    info->failed_allocations = failed_allocations;

    info->leak_count = active_allocations;
    info->leak_blocks = used_blocks;

    return 0;
}

uint32_t memory_get_allocation_count(void)
{
    return active_allocations;
}

int memory_get_allocation(
    uint32_t index,
    syntra_memory_allocation_t *result
)
{
    uint32_t i;
    uint32_t current = 0U;

    if (result == NULL || !initialized) {
        return -1;
    }

    for (i = 0U; i < SYNTRA_MEMORY_MAX_ALLOCATIONS; ++i) {
        if (!allocations[i].active) {
            continue;
        }

        if (current == index) {
            result->start_block = allocations[i].start_block;
            result->block_count = allocations[i].block_count;
            result->size =
                allocations[i].block_count *
                SYNTRA_MEMORY_BLOCK_SIZE;

            copy_tag(result->tag, allocations[i].tag);

            return 0;
        }

        current++;
    }

    return -1;
}

int memory_check_integrity(void)
{
    uint32_t i;
    uint32_t j;
    uint32_t bitmap_used = 0U;
    uint32_t allocation_used = 0U;
    uint32_t active_count = 0U;

    if (!initialized) {
        return -1;
    }

    for (i = 0U; i < SYNTRA_MAX_MEMORY_BLOCKS; ++i) {
        if (is_block_used(i)) {
            bitmap_used++;
        }
    }

    for (i = 0U; i < SYNTRA_MEMORY_MAX_ALLOCATIONS; ++i) {
        if (!allocations[i].active) {
            continue;
        }

        active_count++;

        if (allocations[i].block_count == 0U ||
            allocations[i].start_block >
                SYNTRA_MAX_MEMORY_BLOCKS -
                allocations[i].block_count) {
            return -1;
        }

        allocation_used += allocations[i].block_count;

        for (j = 0U; j < allocations[i].block_count; ++j) {
            if (!is_block_used(
                    allocations[i].start_block + j)) {
                return -1;
            }
        }
    }

    if (bitmap_used != used_blocks ||
        allocation_used != used_blocks ||
        active_count != active_allocations) {
        return -1;
    }

    for (i = 0U; i < SYNTRA_MEMORY_MAX_ALLOCATIONS; ++i) {
        if (!allocations[i].active) {
            continue;
        }

        for (j = i + 1U; j < SYNTRA_MEMORY_MAX_ALLOCATIONS; ++j) {
            uint32_t a_start;
            uint32_t a_end;
            uint32_t b_start;
            uint32_t b_end;

            if (!allocations[j].active) {
                continue;
            }

            a_start = allocations[i].start_block;
            a_end = a_start + allocations[i].block_count;

            b_start = allocations[j].start_block;
            b_end = b_start + allocations[j].block_count;

            if (a_start < b_end && b_start < a_end) {
                return -1;
            }
        }
    }

    return 0;
}

int memory_validate(void)
{
    return memory_check_integrity();
}

uint32_t memory_get_history_count(void)
{
    return history_count;
}

int memory_get_history(
    uint32_t index,
    syntra_memory_history_t *result
)
{
    uint32_t oldest;
    uint32_t actual;
    memory_history_entry_t *entry;

    if (result == NULL || index >= history_count) {
        return -1;
    }

    oldest =
        (history_index + SYNTRA_MEMORY_HISTORY_SIZE -
         history_count) % SYNTRA_MEMORY_HISTORY_SIZE;

    actual =
        (oldest + index) % SYNTRA_MEMORY_HISTORY_SIZE;

    entry = &history[actual];

    result->operation = entry->operation;
    result->start_block = entry->start_block;
    result->block_count = entry->block_count;
    result->status = entry->status;

    return 0;
}

int memory_create_snapshot(syntra_memory_snapshot_t *snapshot)
{
    if (snapshot == NULL || !initialized) {
        return -1;
    }

    snapshot->used_blocks = used_blocks;
    snapshot->free_blocks =
        SYNTRA_MAX_MEMORY_BLOCKS - used_blocks;
    snapshot->active_allocations = active_allocations;
    snapshot->allocation_count = allocation_count;
    snapshot->release_count = release_count;

    snapshot_count++;

    snapshot_index =
        (snapshot_index + 1U) % SYNTRA_MEMORY_MAX_SNAPSHOTS;

    return 0;
}

uint32_t memory_get_snapshot_count(void)
{
    return min_u32(
        snapshot_count,
        SYNTRA_MEMORY_MAX_SNAPSHOTS
    );
}

int memory_get_report(syntra_memory_report_t *report)
{
    if (report == NULL || !initialized) {
        return -1;
    }

    report->total_memory = SYNTRA_MEMORY_POOL_SIZE;
    report->used_memory =
        used_blocks * SYNTRA_MEMORY_BLOCK_SIZE;
    report->free_memory =
        SYNTRA_MEMORY_POOL_SIZE - report->used_memory;

    report->allocation_count = allocation_count;
    report->active_allocations = active_allocations;
    report->release_count = release_count;
    report->failed_allocations = failed_allocations;

    report->integrity_errors = integrity_errors;

    return 0;
}

int memory_set_allocation_tag(void *address, const char *tag)
{
    uint32_t i;
    uintptr_t pool_start;
    uintptr_t pool_end;
    uintptr_t target;
    uint32_t start_block;

    if (!initialized || address == NULL || tag == NULL) {
        return -1;
    }

    pool_start = (uintptr_t)&memory_pool[0];
    pool_end = pool_start + SYNTRA_MEMORY_POOL_SIZE;
    target = (uintptr_t)address;

    if (target < pool_start || target >= pool_end) {
        return -1;
    }

    if ((target - pool_start) % SYNTRA_MEMORY_BLOCK_SIZE != 0U) {
        return -1;
    }

    start_block =
        (uint32_t)((target - pool_start) /
                   SYNTRA_MEMORY_BLOCK_SIZE);

    for (i = 0U; i < SYNTRA_MEMORY_MAX_ALLOCATIONS; ++i) {
        if (allocations[i].active &&
            allocations[i].start_block == start_block) {
            copy_tag(allocations[i].tag, tag);
            return 0;
        }
    }

    return -1;
}

int memory_run_diagnostics(void)
{
    if (!initialized) {
        return -1;
    }

    if (memory_check_integrity() != 0) {
        integrity_errors++;
        return -1;
    }

    return 0;
}

int memory_get_pool_stats(syntra_memory_pool_stats_t *stats)
{
    if (stats == NULL || !initialized) {
        return -1;
    }

    stats->total_blocks = SYNTRA_MAX_MEMORY_BLOCKS;
    stats->used_blocks = used_blocks;
    stats->free_blocks =
        SYNTRA_MAX_MEMORY_BLOCKS - used_blocks;

    stats->total_allocations = allocation_count;
    stats->active_allocations = active_allocations;
    stats->total_releases = release_count;
    stats->failed_allocations = failed_allocations;

    stats->peak_used_blocks = peak_used_blocks;

    return 0;
}

int memory_get_tracking(syntra_memory_tracking_t *tracking)
{
    if (tracking == NULL || !initialized) {
        return -1;
    }

    tracking->total_tracked =
        allocation_count + failed_allocations;

    tracking->active_tracked = active_allocations;
    tracking->released_tracked = release_count;
    tracking->failed_tracked = failed_allocations;

    tracking->total_allocated_blocks =
        total_allocated_blocks;

    tracking->total_released_blocks =
        total_released_blocks;

    tracking->current_allocated_blocks =
        used_blocks;

    tracking->peak_allocated_blocks =
        peak_used_blocks;

    tracking->valid =
        (memory_check_integrity() == 0) ? 1U : 0U;

    return 0;
}

int memory_get_metrics(syntra_memory_metrics_t *metrics)
{
    uint32_t total_attempts;

    if (metrics == NULL || !initialized) {
        return -1;
    }

    total_attempts =
        allocation_count + failed_allocations;

    metrics->allocation_count = allocation_count;
    metrics->release_count = release_count;
    metrics->failed_allocations = failed_allocations;

    metrics->used_blocks = used_blocks;
    metrics->peak_used_blocks = peak_used_blocks;

    if (total_attempts == 0U) {
        metrics->allocation_success_rate = 0U;
    } else {
        metrics->allocation_success_rate =
            (allocation_count * 100U) / total_attempts;
    }

    return 0;
}
