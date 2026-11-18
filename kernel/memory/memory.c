
#include "memory.h"

static unsigned char memory_pool[SYNTRA_MEMORY_POOL_SIZE];

static unsigned char memory_bitmap[SYNTRA_MAX_MEMORY_BLOCKS];

static unsigned int used_blocks = 0;
static int memory_initialized = 0;


syntra_status_t memory_init(void)
{
    if (memory_initialized)
    {
        return SYNTRA_STATUS_ERROR;
    }

    for (unsigned int i = 0; i < SYNTRA_MAX_MEMORY_BLOCKS; i++)
    {
        memory_bitmap[i] = 0;
    }

    used_blocks = 0;
    memory_initialized = 1;

    return SYNTRA_STATUS_OK;
}


void *memory_allocate(unsigned int blocks)
{
    if (!memory_initialized || blocks == 0 ||
        blocks > SYNTRA_MAX_MEMORY_BLOCKS)
    {
        return 0;
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
            unsigned int start = i - blocks + 1;

            for (unsigned int j = start; j <= i; j++)
            {
                memory_bitmap[j] = 1;
            }

            used_blocks += blocks;

            return &memory_pool[
                start * SYNTRA_MEMORY_BLOCK_SIZE
            ];
        }
    }

    return 0;
}


syntra_status_t memory_release(
    void *address,
    unsigned int blocks
)
{
    if (!memory_initialized || address == 0 || blocks == 0)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    unsigned char *start = (unsigned char *)address;

    if (start < memory_pool ||
        start >= memory_pool + SYNTRA_MEMORY_POOL_SIZE)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    unsigned int offset = (unsigned int)(start - memory_pool);

    if (offset % SYNTRA_MEMORY_BLOCK_SIZE != 0)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    unsigned int first_block =
        offset / SYNTRA_MEMORY_BLOCK_SIZE;

    if (blocks > SYNTRA_MAX_MEMORY_BLOCKS - first_block)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    for (unsigned int i = first_block;
         i < first_block + blocks;
         i++)
    {
        if (memory_bitmap[i] == 0)
        {
            return SYNTRA_STATUS_ERROR;
        }
    }

    for (unsigned int i = first_block;
         i < first_block + blocks;
         i++)
    {
        memory_bitmap[i] = 0;
    }

    used_blocks -= blocks;

    return SYNTRA_STATUS_OK;
}


void memory_get_info(syntra_memory_info_t *info)
{
    if (info == 0)
    {
        return;
    }

    info->total_blocks = SYNTRA_MAX_MEMORY_BLOCKS;
    info->used_blocks = used_blocks;
    info->free_blocks = SYNTRA_MAX_MEMORY_BLOCKS - used_blocks;
}
