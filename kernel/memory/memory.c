
#include "memory.h"
#include "../arch/irq.h"

#include <stdint.h>
#include <stddef.h>

#define MEMORY_BITMAP_SIZE \
    ((SYNTRA_MAX_MEMORY_BLOCKS + 7U) / 8U)

_Alignas(SYNTRA_MEMORY_ALIGNMENT)
static uint8_t memory_pool[SYNTRA_MEMORY_POOL_SIZE];

static uint8_t memory_bitmap[MEMORY_BITMAP_SIZE];

static syntra_memory_allocation_t allocations[
    SYNTRA_MAX_MEMORY_BLOCKS
];

static syntra_memory_history_t history[
    SYNTRA_MEMORY_HISTORY_SIZE
];

static syntra_memory_snapshot_t snapshots[
    SYNTRA_MEMORY_MAX_SNAPSHOTS
];

static syntra_memory_tag_t tags[
    SYNTRA_MAX_MEMORY_BLOCKS
];

static unsigned int allocation_count;
static unsigned int active_allocations;
static unsigned int release_count;
static unsigned int failed_allocations;

static unsigned int used_blocks;
static unsigned int peak_used_blocks;

static unsigned int total_allocated_blocks;
static unsigned int total_released_blocks;

static unsigned int history_count;
static unsigned int history_sequence;

static unsigned int snapshot_count;
static unsigned int snapshot_sequence;

static unsigned int integrity_errors;
static unsigned int validation_errors;

static syntra_memory_diagnostic_type_t last_diagnostic_type;

static int initialized;

/* ---------------------------------------------------------
   BASIC HELPERS
   --------------------------------------------------------- */

static unsigned int max_unsigned(
    unsigned int a,
    unsigned int b
)
{
    return a > b ? a : b;
}

static unsigned int get_usage_percent(void)
{
    if (SYNTRA_MAX_MEMORY_BLOCKS == 0U) {
        return 0U;
    }

    return (unsigned int)(
        ((uint64_t)used_blocks * 100U) /
        SYNTRA_MAX_MEMORY_BLOCKS
    );
}

static void copy_name(
    char *destination,
    const char *source
)
{
    unsigned int i;

    if (destination == NULL) {
        return;
    }

    for (i = 0U; i < SYNTRA_MEMORY_TAG_LENGTH - 1U; ++i) {
        if (source == NULL || source[i] == '\0') {
            break;
        }

        destination[i] = source[i];
    }

    destination[i] = '\0';
}

static int allocation_range_is_valid(
    unsigned int start_block,
    unsigned int blocks
)
{
    if (blocks == 0U ||
        blocks > SYNTRA_MAX_MEMORY_BLOCKS) {
        return 0;
    }

    if (start_block >= SYNTRA_MAX_MEMORY_BLOCKS) {
        return 0;
    }

    if (blocks > SYNTRA_MAX_MEMORY_BLOCKS - start_block) {
        return 0;
    }

    return 1;
}

/* ---------------------------------------------------------
   BITMAP MANAGEMENT
   --------------------------------------------------------- */

static int block_is_used(unsigned int block)
{
    unsigned int byte_index;
    unsigned int bit_index;

    if (block >= SYNTRA_MAX_MEMORY_BLOCKS) {
        return 1;
    }

    byte_index = block / 8U;
    bit_index = block % 8U;

    return (
        memory_bitmap[byte_index] &
        (uint8_t)(1U << bit_index)
    ) != 0U;
}

static void mark_block_used(unsigned int block)
{
    unsigned int byte_index = block / 8U;
    unsigned int bit_index = block % 8U;

    memory_bitmap[byte_index] |=
        (uint8_t)(1U << bit_index);
}

static void mark_block_free(unsigned int block)
{
    unsigned int byte_index = block / 8U;
    unsigned int bit_index = block % 8U;

    memory_bitmap[byte_index] &=
        (uint8_t)~(1U << bit_index);
}

/* ---------------------------------------------------------
   HISTORY
   --------------------------------------------------------- */

static void add_history(
    syntra_memory_event_type_t type,
    unsigned int start_block,
    unsigned int block_count,
    int valid
)
{
    unsigned int index;

    index = history_sequence % SYNTRA_MEMORY_HISTORY_SIZE;

    history[index].type = type;
    history[index].start_block = start_block;
    history[index].block_count = block_count;
    history[index].sequence = history_sequence + 1U;
    history[index].valid = valid;

    history_sequence++;

    if (history_count < SYNTRA_MEMORY_HISTORY_SIZE) {
        history_count++;
    }
}

/* ---------------------------------------------------------
   ALLOCATION SEARCH
   --------------------------------------------------------- */

static int find_free_region(
    unsigned int requested_blocks,
    unsigned int *start_block
)
{
    unsigned int i;
    unsigned int j;

    if (start_block == NULL ||
        requested_blocks == 0U ||
        requested_blocks > SYNTRA_MAX_MEMORY_BLOCKS) {
        return 0;
    }

    for (
        i = 0U;
        i <= SYNTRA_MAX_MEMORY_BLOCKS - requested_blocks;
        ++i
    ) {
        for (j = 0U; j < requested_blocks; ++j) {
            if (block_is_used(i + j)) {
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

static int find_free_slot(unsigned int *slot)
{
    unsigned int i;

    if (slot == NULL) {
        return 0;
    }

    for (i = 0U; i < SYNTRA_MAX_MEMORY_BLOCKS; ++i) {
        if (!allocations[i].active &&
            allocations[i].generation != UINT64_MAX) {
            *slot = i;
            return 1;
        }
    }

    return 0;
}

static int find_allocation_by_address(
    void *address,
    unsigned int *index
)
{
    unsigned int i;

    if (address == NULL || index == NULL) {
        return 0;
    }

    for (i = 0U; i < SYNTRA_MAX_MEMORY_BLOCKS; ++i) {
        if (allocations[i].active &&
            allocations[i].address == address) {
            *index = i;
            return 1;
        }
    }

    return 0;
}

static int find_allocation_by_handle(
    syntra_memory_allocation_handle_t handle,
    unsigned int *index
)
{
    if (index == NULL ||
        handle.slot >= SYNTRA_MAX_MEMORY_BLOCKS ||
        handle.generation == 0U ||
        !allocations[handle.slot].active ||
        allocations[handle.slot].generation != handle.generation) {
        return 0;
    }

    *index = handle.slot;
    return 1;
}

static int find_active_slot(
    unsigned int allocation_index,
    unsigned int *slot
)
{
    unsigned int i;
    unsigned int current = 0U;

    if (slot == NULL) {
        return 0;
    }

    for (i = 0U; i < SYNTRA_MAX_MEMORY_BLOCKS; ++i) {
        if (!allocations[i].active) {
            continue;
        }

        if (current == allocation_index) {
            *slot = i;
            return 1;
        }

        current++;
    }

    return 0;
}

/* ---------------------------------------------------------
   FREE SPACE STATISTICS
   --------------------------------------------------------- */

static unsigned int count_free_regions(void)
{
    unsigned int i;
    unsigned int regions = 0U;
    int inside_free_region = 0;

    for (i = 0U; i < SYNTRA_MAX_MEMORY_BLOCKS; ++i) {
        if (!block_is_used(i)) {
            if (!inside_free_region) {
                regions++;
                inside_free_region = 1;
            }
        } else {
            inside_free_region = 0;
        }
    }

    return regions;
}

static unsigned int largest_free_region(void)
{
    unsigned int i;
    unsigned int current = 0U;
    unsigned int largest = 0U;

    for (i = 0U; i < SYNTRA_MAX_MEMORY_BLOCKS; ++i) {
        if (!block_is_used(i)) {
            current++;

            if (current > largest) {
                largest = current;
            }
        } else {
            current = 0U;
        }
    }

    return largest;
}

/* ---------------------------------------------------------
   INTERNAL INTEGRITY VALIDATION
   --------------------------------------------------------- */

static syntra_status_t validate_internal_state(void)
{
    unsigned int i;
    unsigned int j;
    unsigned int bitmap_used = 0U;
    unsigned int allocation_used = 0U;
    unsigned int active_count = 0U;

    if (!initialized) {
        return SYNTRA_STATUS_ERROR;
    }

    if (used_blocks > SYNTRA_MAX_MEMORY_BLOCKS ||
        active_allocations > SYNTRA_MAX_MEMORY_BLOCKS) {
        return SYNTRA_STATUS_ERROR;
    }

    for (i = 0U; i < SYNTRA_MAX_MEMORY_BLOCKS; ++i) {
        if (block_is_used(i)) {
            bitmap_used++;
        }
    }

    for (i = 0U; i < SYNTRA_MAX_MEMORY_BLOCKS; ++i) {
        unsigned int expected_offset;

        if (!allocations[i].active) {
            if (allocations[i].address != NULL ||
                allocations[i].block_count != 0U ||
                allocations[i].ownership_state !=
                    SYNTRA_MEMORY_ALLOCATION_FREE ||
                allocations[i].owner.slot != 0U ||
                allocations[i].owner.generation != 0U) {
                return SYNTRA_STATUS_ERROR;
            }

            if (tags[i].active) {
                return SYNTRA_STATUS_ERROR;
            }

            continue;
        }

        active_count++;

        if (allocations[i].generation == 0U ||
            allocations[i].ownership_state < SYNTRA_MEMORY_ALLOCATION_LEGACY ||
            allocations[i].ownership_state >
                SYNTRA_MEMORY_ALLOCATION_THREAD_OWNED) {
            return SYNTRA_STATUS_ERROR;
        }

        if ((allocations[i].ownership_state ==
                 SYNTRA_MEMORY_ALLOCATION_LEGACY ||
             allocations[i].ownership_state ==
                 SYNTRA_MEMORY_ALLOCATION_STAGED) &&
            (allocations[i].owner.slot != 0U ||
             allocations[i].owner.generation != 0U)) {
            return SYNTRA_STATUS_ERROR;
        }

        if ((allocations[i].ownership_state ==
                 SYNTRA_MEMORY_ALLOCATION_RESERVED ||
             allocations[i].ownership_state ==
                 SYNTRA_MEMORY_ALLOCATION_THREAD_OWNED) &&
            allocations[i].owner.generation == 0U) {
            return SYNTRA_STATUS_ERROR;
        }

        if (!allocation_range_is_valid(
                allocations[i].start_block,
                allocations[i].block_count)) {
            return SYNTRA_STATUS_ERROR;
        }

        expected_offset =
            allocations[i].start_block *
            SYNTRA_MEMORY_BLOCK_SIZE;

        if (allocations[i].address !=
            (void *)&memory_pool[expected_offset]) {
            return SYNTRA_STATUS_ERROR;
        }

        if (allocation_used >
            SYNTRA_MAX_MEMORY_BLOCKS -
            allocations[i].block_count) {
            return SYNTRA_STATUS_ERROR;
        }

        allocation_used += allocations[i].block_count;

        for (j = 0U; j < allocations[i].block_count; ++j) {
            if (!block_is_used(
                    allocations[i].start_block + j)) {
                return SYNTRA_STATUS_ERROR;
            }
        }

        if (tags[i].active) {
            unsigned int k;
            int terminated = 0;

            for (k = 0U; k < SYNTRA_MEMORY_TAG_LENGTH; ++k) {
                if (tags[i].name[k] == '\0') {
                    terminated = 1;
                    break;
                }
            }

            if (!terminated) {
                return SYNTRA_STATUS_ERROR;
            }
        }
    }

    if (bitmap_used != used_blocks ||
        allocation_used != used_blocks ||
        active_count != active_allocations) {
        return SYNTRA_STATUS_ERROR;
    }

    for (i = 0U; i < SYNTRA_MAX_MEMORY_BLOCKS; ++i) {
        unsigned int a_start;
        unsigned int a_end;

        if (!allocations[i].active) {
            continue;
        }

        a_start = allocations[i].start_block;
        a_end = a_start + allocations[i].block_count;

        for (j = i + 1U; j < SYNTRA_MAX_MEMORY_BLOCKS; ++j) {
            unsigned int b_start;
            unsigned int b_end;

            if (!allocations[j].active) {
                continue;
            }

            b_start = allocations[j].start_block;
            b_end = b_start + allocations[j].block_count;

            if (a_start < b_end && b_start < a_end) {
                return SYNTRA_STATUS_ERROR;
            }
        }
    }

    return SYNTRA_STATUS_OK;
}

/* ---------------------------------------------------------
   INITIALIZATION
   --------------------------------------------------------- */

syntra_status_t memory_init(void)
{
    unsigned int i;

    if (initialized) {
        return SYNTRA_STATUS_OK;
    }

    for (i = 0U; i < MEMORY_BITMAP_SIZE; ++i) {
        memory_bitmap[i] = 0U;
    }

    for (i = 0U; i < SYNTRA_MAX_MEMORY_BLOCKS; ++i) {
        allocations[i].address = NULL;
        allocations[i].start_block = 0U;
        allocations[i].block_count = 0U;
        allocations[i].active = 0;
        allocations[i].generation = 0U;
        allocations[i].ownership_state = SYNTRA_MEMORY_ALLOCATION_FREE;
        allocations[i].owner.slot = 0U;
        allocations[i].owner.generation = 0U;

        tags[i].allocation_index = i;
        tags[i].name[0] = '\0';
        tags[i].active = 0;
    }

    for (i = 0U; i < SYNTRA_MEMORY_HISTORY_SIZE; ++i) {
        history[i].type = SYNTRA_MEMORY_EVENT_ALLOCATE;
        history[i].start_block = 0U;
        history[i].block_count = 0U;
        history[i].sequence = 0U;
        history[i].valid = 0;
    }

    for (i = 0U; i < SYNTRA_MEMORY_MAX_SNAPSHOTS; ++i) {
        snapshots[i].valid = 0;
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
    history_sequence = 0U;

    snapshot_count = 0U;
    snapshot_sequence = 0U;

    integrity_errors = 0U;
    validation_errors = 0U;

    last_diagnostic_type =
        SYNTRA_MEMORY_DIAGNOSTIC_NONE;

    initialized = 1;

    return SYNTRA_STATUS_OK;
}

/* ---------------------------------------------------------
   MEMORY ALLOCATION
   --------------------------------------------------------- */

static void *memory_allocate_internal(
    unsigned int blocks,
    syntra_memory_allocation_state_t ownership_state,
    syntra_memory_allocation_handle_t *handle_out
)
{
    unsigned int start_block;
    unsigned int slot;
    unsigned int i;

    if (!initialized ||
        blocks == 0U ||
        blocks > SYNTRA_MAX_MEMORY_BLOCKS) {
        failed_allocations++;
        last_diagnostic_type =
            SYNTRA_MEMORY_DIAGNOSTIC_INVALID_ALLOCATION;
        add_history(SYNTRA_MEMORY_EVENT_FAILED, 0U, blocks, 0);
        return NULL;
    }

    if (!find_free_slot(&slot) ||
        !find_free_region(blocks, &start_block)) {
        failed_allocations++;
        last_diagnostic_type =
            SYNTRA_MEMORY_DIAGNOSTIC_INVALID_ALLOCATION;
        add_history(SYNTRA_MEMORY_EVENT_FAILED, 0U, blocks, 0);
        return NULL;
    }

    for (i = 0U; i < blocks; ++i) {
        mark_block_used(start_block + i);
    }

    if (allocations[slot].generation == UINT64_MAX) {
        /* Defensive: find_free_slot() must already exclude this slot. */
        for (i = 0U; i < blocks; ++i) {
            mark_block_free(start_block + i);
        }
        failed_allocations++;
        return NULL;
    }

    allocations[slot].generation++;
    if (allocations[slot].generation == 0U) {
        /* Zero is never a valid generation. Retire this slot. */
        allocations[slot].generation = UINT64_MAX;
        for (i = 0U; i < blocks; ++i) {
            mark_block_free(start_block + i);
        }
        failed_allocations++;
        return NULL;
    }

    allocations[slot].address =
        &memory_pool[start_block * SYNTRA_MEMORY_BLOCK_SIZE];
    allocations[slot].start_block = start_block;
    allocations[slot].block_count = blocks;
    allocations[slot].active = 1;
    allocations[slot].ownership_state = ownership_state;
    allocations[slot].owner.slot = 0U;
    allocations[slot].owner.generation = 0U;

    if (handle_out != NULL) {
        handle_out->slot = slot;
        handle_out->generation = allocations[slot].generation;
    }

    tags[slot].allocation_index = slot;
    tags[slot].name[0] = '\0';
    tags[slot].active = 0;

    allocation_count++;
    active_allocations++;
    used_blocks += blocks;
    total_allocated_blocks += blocks;
    peak_used_blocks = max_unsigned(peak_used_blocks, used_blocks);
    last_diagnostic_type = SYNTRA_MEMORY_DIAGNOSTIC_NONE;
    add_history(SYNTRA_MEMORY_EVENT_ALLOCATE, start_block, blocks, 1);

    return allocations[slot].address;
}

static syntra_status_t memory_release_slot_internal(unsigned int index)
{
    unsigned int start_block;
    unsigned int blocks;
    unsigned int i;

    if (index >= SYNTRA_MAX_MEMORY_BLOCKS || !allocations[index].active)
        return SYNTRA_STATUS_INVALID_ARGUMENT;

    start_block = allocations[index].start_block;
    blocks = allocations[index].block_count;

    if (!allocation_range_is_valid(start_block, blocks) ||
        used_blocks < blocks || active_allocations == 0U) {
        integrity_errors++;
        last_diagnostic_type = SYNTRA_MEMORY_DIAGNOSTIC_INTEGRITY_FAILURE;
        add_history(SYNTRA_MEMORY_EVENT_FAILED, start_block, blocks, 0);
        return SYNTRA_STATUS_ERROR;
    }

    for (i = 0U; i < blocks; ++i) {
        if (!block_is_used(start_block + i)) {
            integrity_errors++;
            last_diagnostic_type = SYNTRA_MEMORY_DIAGNOSTIC_INTEGRITY_FAILURE;
            add_history(SYNTRA_MEMORY_EVENT_FAILED, start_block, blocks, 0);
            return SYNTRA_STATUS_ERROR;
        }
    }

    for (i = 0U; i < blocks; ++i)
        mark_block_free(start_block + i);

    used_blocks -= blocks;
    active_allocations--;
    release_count++;
    total_released_blocks += blocks;

    allocations[index].address = NULL;
    allocations[index].start_block = 0U;
    allocations[index].block_count = 0U;
    allocations[index].active = 0;
    allocations[index].ownership_state = SYNTRA_MEMORY_ALLOCATION_FREE;
    allocations[index].owner.slot = 0U;
    allocations[index].owner.generation = 0U;
    /* Keep generation so a later allocation cannot revive an old token. */

    tags[index].name[0] = '\0';
    tags[index].active = 0;
    last_diagnostic_type = SYNTRA_MEMORY_DIAGNOSTIC_NONE;
    add_history(SYNTRA_MEMORY_EVENT_RELEASE, start_block, blocks, 1);
    return SYNTRA_STATUS_OK;
}

void *memory_allocate(unsigned int blocks)
{
    syntra_irq_state_t irq = syntra_irq_save();
    void *address = memory_allocate_internal(
        blocks, SYNTRA_MEMORY_ALLOCATION_LEGACY, NULL);
    syntra_irq_restore(irq);
    return address;
}

syntra_status_t memory_allocate_staged(
    unsigned int blocks,
    void **address_out,
    syntra_memory_allocation_handle_t *handle_out
)
{
    syntra_irq_state_t irq;
    void *address;

    if (address_out == NULL || handle_out == NULL)
        return SYNTRA_STATUS_INVALID_ARGUMENT;

    *address_out = NULL;
    handle_out->slot = UINT32_MAX;
    handle_out->generation = 0U;

    if (blocks == 0U || blocks > SYNTRA_MAX_MEMORY_BLOCKS)
        return SYNTRA_STATUS_INVALID_ARGUMENT;

    irq = syntra_irq_save();
    address = memory_allocate_internal(
        blocks, SYNTRA_MEMORY_ALLOCATION_STAGED, handle_out);
    syntra_irq_restore(irq);

    if (address == NULL)
        return SYNTRA_STATUS_OUT_OF_MEMORY;

    *address_out = address;
    return SYNTRA_STATUS_OK;
}

static int owner_handle_is_valid(syntra_memory_owner_handle_t owner)
{
    return owner.slot != UINT32_MAX && owner.generation != 0U;
}

syntra_status_t memory_bind_staged_allocation(
    syntra_memory_allocation_handle_t allocation,
    syntra_memory_owner_handle_t owner
)
{
    syntra_irq_state_t irq;
    unsigned int index;
    syntra_status_t status = SYNTRA_STATUS_INVALID_ARGUMENT;

    if (!owner_handle_is_valid(owner))
        return SYNTRA_STATUS_INVALID_ARGUMENT;

    irq = syntra_irq_save();
    if (find_allocation_by_handle(allocation, &index)) {
        if (allocations[index].ownership_state ==
            SYNTRA_MEMORY_ALLOCATION_STAGED) {
            allocations[index].ownership_state =
                SYNTRA_MEMORY_ALLOCATION_RESERVED;
            allocations[index].owner = owner;
            status = SYNTRA_STATUS_OK;
        } else {
            status = SYNTRA_STATUS_PERMISSION_DENIED;
        }
    }
    syntra_irq_restore(irq);
    return status;
}

syntra_status_t memory_commit_thread_allocation(
    syntra_memory_allocation_handle_t allocation,
    syntra_memory_owner_handle_t owner
)
{
    syntra_irq_state_t irq;
    unsigned int index;
    syntra_status_t status = SYNTRA_STATUS_INVALID_ARGUMENT;

    if (!owner_handle_is_valid(owner))
        return SYNTRA_STATUS_INVALID_ARGUMENT;

    irq = syntra_irq_save();
    if (find_allocation_by_handle(allocation, &index)) {
        if (allocations[index].ownership_state !=
                SYNTRA_MEMORY_ALLOCATION_RESERVED ||
            allocations[index].owner.slot != owner.slot ||
            allocations[index].owner.generation != owner.generation) {
            status = SYNTRA_STATUS_PERMISSION_DENIED;
        } else {
            allocations[index].ownership_state =
                SYNTRA_MEMORY_ALLOCATION_THREAD_OWNED;
            status = SYNTRA_STATUS_OK;
        }
    }
    syntra_irq_restore(irq);
    return status;
}

static syntra_status_t release_by_token(
    syntra_memory_allocation_handle_t allocation,
    syntra_memory_owner_handle_t owner,
    syntra_memory_allocation_state_t required_state,
    int check_owner
)
{
    syntra_irq_state_t irq;
    unsigned int index;
    syntra_status_t status = SYNTRA_STATUS_INVALID_ARGUMENT;

    irq = syntra_irq_save();
    if (find_allocation_by_handle(allocation, &index)) {
        if (allocations[index].ownership_state != required_state) {
            status = SYNTRA_STATUS_PERMISSION_DENIED;
        } else if (check_owner &&
                   (allocations[index].owner.slot != owner.slot ||
                    allocations[index].owner.generation != owner.generation)) {
            status = SYNTRA_STATUS_PERMISSION_DENIED;
        } else {
            status = memory_release_slot_internal(index);
        }
    }
    syntra_irq_restore(irq);
    return status;
}

syntra_status_t memory_abort_staged_allocation(
    syntra_memory_allocation_handle_t allocation
)
{
    return release_by_token(
        allocation, (syntra_memory_owner_handle_t){0U, 0U},
        SYNTRA_MEMORY_ALLOCATION_STAGED, 0);
}

syntra_status_t memory_abort_reserved_allocation(
    syntra_memory_allocation_handle_t allocation,
    syntra_memory_owner_handle_t owner
)
{
    if (!owner_handle_is_valid(owner))
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    return release_by_token(
        allocation, owner, SYNTRA_MEMORY_ALLOCATION_RESERVED, 1);
}

syntra_status_t memory_release_owned_allocation(
    syntra_memory_allocation_handle_t allocation,
    syntra_memory_owner_handle_t owner
)
{
    if (!owner_handle_is_valid(owner))
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    return release_by_token(
        allocation, owner, SYNTRA_MEMORY_ALLOCATION_THREAD_OWNED, 1);
}

syntra_status_t memory_get_allocation_by_handle(
    syntra_memory_allocation_handle_t allocation,
    syntra_memory_allocation_t *snapshot_out)
{
    syntra_irq_state_t irq;
    unsigned int index;
    syntra_status_t status = SYNTRA_STATUS_INVALID_ARGUMENT;

    if (snapshot_out == NULL)
        return SYNTRA_STATUS_INVALID_ARGUMENT;

    irq = syntra_irq_save();
    if (find_allocation_by_handle(allocation, &index)) {
        *snapshot_out = allocations[index];
        status = SYNTRA_STATUS_OK;
    }
    syntra_irq_restore(irq);
    return status;
}

syntra_status_t memory_release(
    void *address,
    unsigned int blocks
)
{
    syntra_irq_state_t irq;
    unsigned int index;
    syntra_status_t status = SYNTRA_STATUS_INVALID_ARGUMENT;

    irq = syntra_irq_save();
    if (!initialized || address == NULL || blocks == 0U) {
        last_diagnostic_type = SYNTRA_MEMORY_DIAGNOSTIC_INVALID_RELEASE;
        add_history(SYNTRA_MEMORY_EVENT_FAILED, 0U, blocks, 0);
    } else if (find_allocation_by_address(address, &index)) {
        if (allocations[index].block_count != blocks) {
            last_diagnostic_type = SYNTRA_MEMORY_DIAGNOSTIC_INVALID_RELEASE;
            add_history(SYNTRA_MEMORY_EVENT_FAILED,
                allocations[index].start_block, blocks, 0);
        } else if (allocations[index].ownership_state !=
                   SYNTRA_MEMORY_ALLOCATION_LEGACY) {
            status = SYNTRA_STATUS_PERMISSION_DENIED;
        } else {
            status = memory_release_slot_internal(index);
        }
    } else {
        last_diagnostic_type = SYNTRA_MEMORY_DIAGNOSTIC_INVALID_RELEASE;
        add_history(SYNTRA_MEMORY_EVENT_FAILED, 0U, blocks, 0);
    }
    syntra_irq_restore(irq);
    return status;
}

/* ---------------------------------------------------------
   MEMORY INFORMATION
   --------------------------------------------------------- */

void memory_get_info(syntra_memory_info_t *info)
{
    unsigned int free_blocks;

    if (info == NULL) {
        return;
    }

    if (!initialized) {
        info->total_blocks = 0U;
        info->used_blocks = 0U;
        info->free_blocks = 0U;
        info->allocation_count = 0U;
        info->release_count = 0U;
        info->active_allocations = 0U;
        info->failed_allocations = 0U;
        info->largest_free_block = 0U;
        info->free_regions = 0U;
        info->fragmentation_blocks = 0U;
        info->usage_percent = 0U;
        info->integrity_errors = 0U;
        info->validation_errors = 0U;
        info->peak_used_blocks = 0U;
        info->peak_usage_percent = 0U;
        info->leak_count = 0U;
        info->leak_blocks = 0U;
        return;
    }

    free_blocks =
        SYNTRA_MAX_MEMORY_BLOCKS - used_blocks;

    info->total_blocks = SYNTRA_MAX_MEMORY_BLOCKS;
    info->used_blocks = used_blocks;
    info->free_blocks = free_blocks;

    info->allocation_count = allocation_count;
    info->release_count = release_count;
    info->active_allocations = active_allocations;
    info->failed_allocations = failed_allocations;

    info->largest_free_block = largest_free_region();
    info->free_regions = count_free_regions();

    info->fragmentation_blocks =
        free_blocks - info->largest_free_block;

    info->usage_percent = get_usage_percent();

    info->integrity_errors = integrity_errors;
    info->validation_errors = validation_errors;

    info->peak_used_blocks = peak_used_blocks;

    info->peak_usage_percent =
        (unsigned int)(
            ((uint64_t)peak_used_blocks * 100U) /
            SYNTRA_MAX_MEMORY_BLOCKS
        );

    info->leak_count = active_allocations;
    info->leak_blocks = used_blocks;
}

unsigned int memory_get_allocation_count(void)
{
    return initialized ? active_allocations : 0U;
}

const syntra_memory_allocation_t *memory_get_allocation(
    unsigned int index
)
{
    unsigned int slot;

    if (!initialized ||
        !find_active_slot(index, &slot)) {
        return NULL;
    }

    return &allocations[slot];
}

/* ---------------------------------------------------------
   INTEGRITY CHECK
   --------------------------------------------------------- */

syntra_status_t memory_check_integrity(void)
{
    syntra_status_t result;

    result = validate_internal_state();

    if (result != SYNTRA_STATUS_OK) {
        integrity_errors++;

        last_diagnostic_type =
            SYNTRA_MEMORY_DIAGNOSTIC_INTEGRITY_FAILURE;

        return SYNTRA_STATUS_ERROR;
    }

    return SYNTRA_STATUS_OK;
}

syntra_status_t memory_validate(void)
{
    syntra_status_t result;

    result = memory_check_integrity();

    if (result != SYNTRA_STATUS_OK) {
        validation_errors++;
    }

    return result;
}

/* ---------------------------------------------------------
   HISTORY API
   --------------------------------------------------------- */

unsigned int memory_get_history_count(void)
{
    return history_count;
}

const syntra_memory_history_t *memory_get_history(
    unsigned int index
)
{
    unsigned int oldest;
    unsigned int actual;

    if (index >= history_count) {
        return NULL;
    }

    oldest =
        (history_sequence - history_count) %
        SYNTRA_MEMORY_HISTORY_SIZE;

    actual =
        (oldest + index) %
        SYNTRA_MEMORY_HISTORY_SIZE;

    return &history[actual];
}

/* ---------------------------------------------------------
   SNAPSHOT API
   --------------------------------------------------------- */

syntra_status_t memory_create_snapshot(void)
{
    unsigned int index;
    syntra_memory_snapshot_t *snapshot;

    if (!initialized) {
        return SYNTRA_STATUS_ERROR;
    }

    index =
        snapshot_sequence % SYNTRA_MEMORY_MAX_SNAPSHOTS;

    snapshot = &snapshots[index];

    snapshot->total_blocks = SYNTRA_MAX_MEMORY_BLOCKS;
    snapshot->used_blocks = used_blocks;

    snapshot->free_blocks =
        SYNTRA_MAX_MEMORY_BLOCKS - used_blocks;

    snapshot->active_allocations = active_allocations;
    snapshot->allocation_count = allocation_count;
    snapshot->release_count = release_count;
    snapshot->failed_allocations = failed_allocations;

    snapshot->usage_percent = get_usage_percent();
    snapshot->peak_used_blocks = peak_used_blocks;

    snapshot->snapshot_sequence =
        snapshot_sequence + 1U;

    snapshot->valid = 1;

    snapshot_sequence++;

    if (snapshot_count < SYNTRA_MEMORY_MAX_SNAPSHOTS) {
        snapshot_count++;
    }

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
    unsigned int oldest;
    unsigned int actual;

    if (index >= snapshot_count) {
        return NULL;
    }

    oldest =
        (snapshot_sequence - snapshot_count) %
        SYNTRA_MEMORY_MAX_SNAPSHOTS;

    actual =
        (oldest + index) %
        SYNTRA_MEMORY_MAX_SNAPSHOTS;

    return &snapshots[actual];
}

/* ---------------------------------------------------------
   REPORT API
   --------------------------------------------------------- */

syntra_status_t memory_generate_report(
    syntra_memory_report_t *report
)
{
    if (report == NULL || !initialized) {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    report->total_snapshots = snapshot_count;
    report->total_allocations = allocation_count;
    report->total_releases = release_count;
    report->total_failures = failed_allocations;

    report->current_used_blocks = used_blocks;

    report->current_free_blocks =
        SYNTRA_MAX_MEMORY_BLOCKS - used_blocks;

    report->peak_used_blocks = peak_used_blocks;
    report->current_usage_percent = get_usage_percent();

    report->integrity_errors = integrity_errors;
    report->validation_errors = validation_errors;

    report->valid = 1;

    return SYNTRA_STATUS_OK;
}

/* ---------------------------------------------------------
   ALLOCATION TAG API
   --------------------------------------------------------- */

syntra_status_t memory_set_tag(
    unsigned int allocation_index,
    const char *name
)
{
    unsigned int slot;

    if (!initialized || name == NULL) {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (!find_active_slot(allocation_index, &slot)) {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    tags[slot].allocation_index = allocation_index;

    copy_name(
        tags[slot].name,
        name
    );

    tags[slot].active = 1;

    return SYNTRA_STATUS_OK;
}

const syntra_memory_tag_t *memory_get_tag(
    unsigned int allocation_index
)
{
    unsigned int slot;

    if (!initialized ||
        !find_active_slot(allocation_index, &slot) ||
        !tags[slot].active) {
        return NULL;
    }

    tags[slot].allocation_index = allocation_index;

    return &tags[slot];
}

/* ---------------------------------------------------------
   DIAGNOSTICS API
   --------------------------------------------------------- */

syntra_status_t memory_run_diagnostics(
    syntra_memory_diagnostics_t *diagnostics
)
{
    syntra_status_t result;

    if (diagnostics == NULL) {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    diagnostics->type =
        SYNTRA_MEMORY_DIAGNOSTIC_NONE;

    diagnostics->error_count =
        integrity_errors + validation_errors;

    diagnostics->used_blocks = used_blocks;

    diagnostics->free_blocks =
        initialized
            ? SYNTRA_MAX_MEMORY_BLOCKS - used_blocks
            : SYNTRA_MAX_MEMORY_BLOCKS;

    diagnostics->has_warning = 0;
    diagnostics->valid = 0;

    if (!initialized) {
        diagnostics->type =
            SYNTRA_MEMORY_DIAGNOSTIC_UNINITIALIZED;

        diagnostics->error_count++;

        return SYNTRA_STATUS_ERROR;
    }

    result = validate_internal_state();

    if (result != SYNTRA_STATUS_OK) {
        diagnostics->type =
            SYNTRA_MEMORY_DIAGNOSTIC_INTEGRITY_FAILURE;

        diagnostics->error_count =
            integrity_errors + validation_errors + 1U;

        return SYNTRA_STATUS_ERROR;
    }

    if (last_diagnostic_type ==
            SYNTRA_MEMORY_DIAGNOSTIC_INVALID_ALLOCATION ||
        last_diagnostic_type ==
            SYNTRA_MEMORY_DIAGNOSTIC_INVALID_RELEASE) {

        diagnostics->type = last_diagnostic_type;
    }

    if (diagnostics->free_blocks <
        SYNTRA_MAX_MEMORY_BLOCKS / 10U) {

        diagnostics->type =
            SYNTRA_MEMORY_DIAGNOSTIC_CAPACITY_WARNING;

        diagnostics->has_warning = 1;
    }

    diagnostics->valid = 1;

    return SYNTRA_STATUS_OK;
}

/* ---------------------------------------------------------
   MEMORY POOL STATISTICS
   --------------------------------------------------------- */

syntra_status_t memory_get_pool_stats(
    syntra_memory_pool_stats_t *stats
)
{
    if (stats == NULL || !initialized) {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    stats->total_bytes = SYNTRA_MEMORY_POOL_SIZE;

    stats->used_bytes =
        used_blocks * SYNTRA_MEMORY_BLOCK_SIZE;

    stats->free_bytes =
        stats->total_bytes - stats->used_bytes;

    stats->total_blocks = SYNTRA_MAX_MEMORY_BLOCKS;
    stats->used_blocks = used_blocks;

    stats->free_blocks =
        SYNTRA_MAX_MEMORY_BLOCKS - used_blocks;

    stats->usage_percent = get_usage_percent();

    stats->largest_free_region_bytes =
        largest_free_region() * SYNTRA_MEMORY_BLOCK_SIZE;

    stats->valid = 1;

    return SYNTRA_STATUS_OK;
}

/* ---------------------------------------------------------
   ALLOCATION TRACKING
   --------------------------------------------------------- */

syntra_status_t memory_get_tracking(
    syntra_memory_tracking_t *tracking
)
{
    if (tracking == NULL || !initialized) {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
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
        validate_internal_state() == SYNTRA_STATUS_OK;

    return SYNTRA_STATUS_OK;
}

/* ---------------------------------------------------------
   PERFORMANCE METRICS
   --------------------------------------------------------- */

syntra_status_t memory_get_metrics(
    syntra_memory_metrics_t *metrics
)
{
    unsigned int total_operations;
    unsigned int allocation_attempts;

    if (metrics == NULL || !initialized) {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    total_operations =
        allocation_count +
        release_count +
        failed_allocations;

    allocation_attempts =
        allocation_count + failed_allocations;

    metrics->total_operations = total_operations;

    metrics->allocation_operations = allocation_count;
    metrics->release_operations = release_count;
    metrics->failed_operations = failed_allocations;

    metrics->peak_used_blocks = peak_used_blocks;
    metrics->current_used_blocks = used_blocks;

    if (allocation_attempts == 0U) {
        metrics->allocation_success_rate = 0U;
    } else {
        metrics->allocation_success_rate =
            (unsigned int)(
                ((uint64_t)allocation_count * 100U) /
                allocation_attempts
            );
    }

    metrics->usage_percent = get_usage_percent();
    metrics->valid =
        validate_internal_state() == SYNTRA_STATUS_OK;

    return SYNTRA_STATUS_OK;
}
