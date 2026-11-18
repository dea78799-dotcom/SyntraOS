
#include "memory.h"

#include <stdint.h>
#include <stddef.h>

/*
 * SyntraOS Memory Manager
 * Version: 0.1.0
 */

static unsigned char memory_pool[SYNTRA_MEMORY_POOL_SIZE];

static unsigned char memory_bitmap[SYNTRA_MAX_MEMORY_BLOCKS];

static unsigned int used_blocks = 0;

static int memory_initialized = 0;

/* =================================
   MEMORY INITIALIZATION
   ================================= */

syntra_status_t memory_init(void)
{
    if (memory_initialized)
    {
        return SYNTRA_STATUS_ERROR;
    }

    for (unsigned int i = 0;
         i < SYNTRA_MAX_MEMORY_BLOCKS;
         i++)
    {
        memory_bitmap[i] = 0;
    }

    used_blocks = 0;
    memory_initialized = 1;

    return SYNTRA_STATUS_OK;
}

/* =================================
   MEMORY ALLOCATION
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

    unsigned int consecutive = 0;

    for (unsigned int i = 0;
         i < SYNTRA_MAX_MEMORY_BLOCKS;
         i++)
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

            for (unsigned int j = start;
                 j <= i;
                 j++)
            {
                memory_bitmap[j] = 1;
            }

            used_blocks += blocks;

            return &memory_pool[
                start * SYNTRA_MEMORY_BLOCK_SIZE
            ];
        }
    }

    return NULL;
}

/* =================================
   MEMORY RELEASE
   ================================= */

syntra_status_t memory_release(
    void *address,
    unsigned int blocks
)
{
    if (!memory_initialized ||
        address == NULL ||
        blocks == 0)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    uintptr_t pool_start =
        (uintptr_t)(void *)memory_pool;

    uintptr_t address_value =
        (uintptr_t)address;

    if (address_value < pool_start)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    uintptr_t offset = address_value - pool_start;

    if (offset >= SYNTRA_MEMORY_POOL_SIZE)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (offset % SYNTRA_MEMORY_BLOCK_SIZE != 0)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    unsigned int start_block =
        (unsigned int)(offset / SYNTRA_MEMORY_BLOCK_SIZE);

    if (blocks > SYNTRA_MAX_MEMORY_BLOCKS - start_block)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    for (unsigned int i = start_block;
         i < start_block + blocks;
         i++)
    {
        if (memory_bitmap[i] == 0)
        {
            return SYNTRA_STATUS_ERROR;
        }
    }

    for (unsigned int i = start_block;
         i < start_block + blocks;
         i++)
    {
        memory_bitmap[i] = 0;
    }

    used_blocks -= blocks;

    return SYNTRA_STATUS_OK;
}

/* =================================
   MEMORY INFORMATION
   ================================= */

void memory_get_info(syntra_memory_info_t *info)
{
    if (info == NULL)
    {
        return;
    }

    info->total_blocks = SYNTRA_MAX_MEMORY_BLOCKS;

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
