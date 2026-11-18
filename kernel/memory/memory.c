
#include "memory.h"

#include <stdint.h>
#include <stddef.h>

/*
 * SyntraOS Memory Manager
 * Version: 0.2.0
 */

static unsigned char memory_pool[SYNTRA_MEMORY_POOL_SIZE];

static unsigned char memory_bitmap[SYNTRA_MAX_MEMORY_BLOCKS];

static syntra_memory_allocation_t allocations[
    SYNTRA_MAX_MEMORY_BLOCKS
];

static unsigned int used_blocks = 0;
static unsigned int allocation_count = 0;
static unsigned int release_count = 0;
static unsigned int active_allocations = 0;

static int memory_initialized = 0;

/* =================================
   INITIALIZE MEMORY
   ================================= */

syntra_status_t memory_init(void)
{
    if (memory_initialized)
    {
        return SYNTRA_STATUS_ERROR;
    }

    for (unsigned int i = 0; i < SYNTRA_MAX_MEMORY_BLOCKS; i++)
    {
        memory_bitmap[i] = 0;

        allocations[i].address = NULL;
        allocations[i].start_block = 0;
        allocations[i].block_count = 0;
        allocations[i].active = 0;
    }

    used_blocks = 0;
    allocation_count = 0;
    release_count = 0;
    active_allocations = 0;

    memory_initialized = 1;

    return SYNTRA_STATUS_OK;
}

/* =================================
   FIND FREE ALLOCATION SLOT
   ================================= */

static int find_free_slot(void)
{
    for (unsigned int i = 0; i < SYNTRA_MAX_MEMORY_BLOCKS; i++)
    {
        if (!allocations[i].active)
        {
            return (int)i;
        }
    }

    return -1;
}

/* =================================
   ALLOCATE MEMORY
   ================================= */

void *memory_allocate(unsigned int blocks)
{
    if (!memory_initialized || blocks == 0)
    {
        return NULL;
    }

    if (blocks > SYNTRA_MAX_MEMORY_BLOCKS)
    {
        return NULL;
    }

    int slot = find_free_slot();

    if (slot < 0)
    {
        return NULL;
    }

    unsigned int consecutive = 0;

    for (unsigned int i = 0; i < SYNTRA_MAX_MEMORY_BLOCKS; i++)
    {
        if (memory_bitmap[i] == 0)
        {
            consecutive++;
        }
        else
        {
            consecutive = 0;
        }

        if (consecutive == blocks)
        {
            unsigned int start = i + 1 - blocks;

            for (unsigned int j = start; j <= i; j++)
            {
                memory_bitmap[j] = 1;
            }

            void *address =
                &memory_pool[start * SYNTRA_MEMORY_BLOCK_SIZE];

            allocations[slot].address = address;
            allocations[slot].start_block = start;
            allocations[slot].block_count = blocks;
            allocations[slot].active = 1;

            used_blocks += blocks;
            allocation_count++;
            active_allocations++;

            return address;
        }
    }

    return NULL;
}

/* =================================
   RELEASE MEMORY
   ================================= */

syntra_status_t memory_release(
    void *address,
    unsigned int blocks
)
{
    if (
        !memory_initialized ||
        address == NULL ||
        blocks == 0
    )
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    int slot = -1;

    for (unsigned int i = 0; i < SYNTRA_MAX_MEMORY_BLOCKS; i++)
    {
        if (
            allocations[i].active &&
            allocations[i].address == address
        )
        {
            slot = (int)i;
            break;
        }
    }

    if (slot < 0)
    {
        return SYNTRA_STATUS_ERROR;
    }

    syntra_memory_allocation_t *allocation =
        &allocations[slot];

    if (allocation->block_count != blocks)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    unsigned int start = allocation->start_block;

    for (
        unsigned int i = start;
        i < start + allocation->block_count;
        i++
    )
    {
        if (memory_bitmap[i] == 0)
        {
            return SYNTRA_STATUS_ERROR;
        }
    }

    for (
        unsigned int i = start;
        i < start + allocation->block_count;
        i++
    )
    {
        memory_bitmap[i] = 0;
    }

    used_blocks -= allocation->block_count;

    allocation->address = NULL;
    allocation->start_block = 0;
    allocation->block_count = 0;
    allocation->active = 0;

    release_count++;
    active_allocations--;

    return SYNTRA_STATUS_OK;
}

/* =================================
   GET MEMORY INFORMATION
   ================================= */

void memory_get_info(syntra_memory_info_t *info)
{
    if (info == NULL)
    {
        return;
    }

    info->total_blocks = SYNTRA_MAX_MEMORY_BLOCKS;

    info->allocation_count = allocation_count;
    info->release_count = release_count;
    info->active_allocations = active_allocations;

    if (!memory_initialized)
    {
        info->used_blocks = 0;
        info->free_blocks = 0;
        return;
    }

    info->used_blocks = used_blocks;

    info->free_blocks =
        SYNTRA_MAX_MEMORY_BLOCKS - used_blocks;
}

/* =================================
   GET ALLOCATION COUNT
   ================================= */

unsigned int memory_get_allocation_count(void)
{
    return active_allocations;
}

/* =================================
   GET ALLOCATION BY INDEX
   ================================= */

const syntra_memory_allocation_t *memory_get_allocation(
    unsigned int index
)
{
    if (
        !memory_initialized ||
        index >= SYNTRA_MAX_MEMORY_BLOCKS
    )
    {
        return NULL;
    }

    if (!allocations[index].active)
    {
        return NULL;
    }

    return &allocations[index];
}
