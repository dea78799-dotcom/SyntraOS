#ifndef SYNTRA_MEMORY_H
#define SYNTRA_MEMORY_H

#include "../core/status.h"

#define SYNTRA_MEMORY_BLOCK_SIZE 4096
#define SYNTRA_MAX_MEMORY_BLOCKS 1024

typedef struct
{
    unsigned int total_blocks;
    unsigned int used_blocks;
    unsigned int free_blocks;
} syntra_memory_info_t;

syntra_status_t memory_init(void);

syntra_status_t memory_allocate(
    unsigned int blocks
);

syntra_status_t memory_release(
    unsigned int blocks
);

void memory_get_info(
    syntra_memory_info_t *info
);

#endif
