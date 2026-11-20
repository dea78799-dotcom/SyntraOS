#ifndef SYNTRA_PAGE_ALLOCATOR_H
#define SYNTRA_PAGE_ALLOCATOR_H

#include <stddef.h>
#include <stdint.h>

#include "../core/status.h"

#define SYNTRA_PAGE_SIZE ((size_t)4096U)
#define SYNTRA_PAGE_ALLOCATOR_MAX_PHYSICAL_ADDRESS ((uint64_t)0x100000000ULL)

/*
 * V1 uses a static bitmap for the first 4 GiB of physical address space.
 * Only complete 4 KiB pages inside SYNTRA_MEMORY_REGION_RAM are eligible.
 */
syntra_status_t syntra_page_allocator_init(void);
syntra_status_t syntra_page_alloc(uintptr_t *physical_address);
syntra_status_t syntra_page_free(uintptr_t physical_address);
size_t syntra_page_allocator_free_count(void);
size_t syntra_page_allocator_total_count(void);

#endif
