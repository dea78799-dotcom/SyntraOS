
#ifndef SYNTRA_MEMORY_H
#define SYNTRA_MEMORY_H

#include "../core/status.h"

#define SYNTRA_MEMORY_BLOCK_SIZE 4096
#define SYNTRA_MAX_MEMORY_BLOCKS 1024

#define SYNTRA_MEMORY_POOL_SIZE \
    (SYNTRA_MEMORY_BLOCK_SIZE * SYNTRA_MAX_MEMORY_BLOCKS)

#define SYNTRA_MEMORY_ALIGNMENT 4096

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

} syntra_memory_info_t;

typedef struct
{
    void *address;
    unsigned int start_block;
    unsigned int block_count;
    int active;

} syntra_memory_allocation_t;

syntra_status_t memory_init(void);

void *memory_allocate(unsigned int blocks);

syntra_status_t memory_release(
    void *address,
    unsigned int blocks
);

void memory_get_info(
    syntra_memory_info_t *info
);

unsigned int memory_get_allocation_count(void);

const syntra_memory_allocation_t *memory_get_allocation(
    unsigned int index
);

syntra_status_t memory_validate(void);

syntra_status_t memory_check_integrity(void);

#endif
