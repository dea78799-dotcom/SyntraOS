#ifndef SYNTRA_MEMORY_REGION_H
#define SYNTRA_MEMORY_REGION_H

#include <stddef.h>
#include <stdint.h>

#include "../core/status.h"

#define SYNTRA_MEMORY_REGION_MAX 32U

typedef enum
{
    SYNTRA_MEMORY_REGION_RAM = 0,
    SYNTRA_MEMORY_REGION_RESERVED,
    SYNTRA_MEMORY_REGION_KERNEL,
    SYNTRA_MEMORY_REGION_STACK,
    SYNTRA_MEMORY_REGION_DTB,
    SYNTRA_MEMORY_REGION_MMIO,
    SYNTRA_MEMORY_REGION_RECLAIMABLE
} syntra_memory_region_type_t;

typedef struct
{
    uintptr_t base;
    size_t size;
    syntra_memory_region_type_t type;
} syntra_memory_region_t;

syntra_status_t syntra_memory_regions_init(void);
syntra_status_t syntra_memory_region_add(
    uintptr_t base,
    size_t size,
    syntra_memory_region_type_t type
);
size_t syntra_memory_region_count(void);
syntra_status_t syntra_memory_region_get(
    size_t index,
    syntra_memory_region_t *region
);
syntra_status_t syntra_memory_region_find(
    uintptr_t address,
    syntra_memory_region_t *region
);

#endif
