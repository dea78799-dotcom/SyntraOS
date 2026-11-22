#ifndef SYNTRA_PAGE_ALLOCATOR_H
#define SYNTRA_PAGE_ALLOCATOR_H

#include <stddef.h>
#include <stdint.h>

#include "../core/status.h"

#define SYNTRA_PAGE_SIZE ((size_t)4096U)
#define SYNTRA_PAGE_ALLOCATOR_MAX_PHYSICAL_ADDRESS ((uint64_t)0x100000000ULL)

typedef enum
{
    SYNTRA_PAGE_OWNER_NONE = 0,
    SYNTRA_PAGE_OWNER_KERNEL,
    SYNTRA_PAGE_OWNER_PAGE_TABLE,
    SYNTRA_PAGE_OWNER_PROCESS,
    SYNTRA_PAGE_OWNER_DRIVER,
    SYNTRA_PAGE_OWNER_MAX
} syntra_page_owner_t;

typedef struct
{
    size_t total_pages;
    size_t free_pages;
    size_t allocated_pages;
    size_t owner_pages[SYNTRA_PAGE_OWNER_MAX];
    int valid;
} syntra_page_allocator_stats_t;

/*
 * V1 uses a static bitmap for the first 4 GiB of physical address space.
 * RAM ranges crossing 4 GiB are clipped; ranges wholly above it are ignored.
 * Only complete 4 KiB pages inside SYNTRA_MEMORY_REGION_RAM are eligible.
 */
syntra_status_t syntra_page_allocator_init(void);
/* Full bitmap/summary/accounting audit; intended for diagnostics and tests. */
syntra_status_t syntra_page_allocator_validate(void);
/* O(1) consistent snapshot; owner counters are maintained on each mutation. */
syntra_status_t syntra_page_allocator_get_stats(
    syntra_page_allocator_stats_t *stats);
/* Owned APIs enforce owner tags; legacy APIs allocate/free as KERNEL/any. */
syntra_status_t syntra_page_alloc_owned(
    uintptr_t *physical_address, syntra_page_owner_t owner);
syntra_status_t syntra_page_alloc_contiguous_owned(
    size_t page_count, uintptr_t *physical_address, syntra_page_owner_t owner);
syntra_status_t syntra_page_free_owned(
    uintptr_t physical_address, syntra_page_owner_t expected_owner);
syntra_status_t syntra_page_get_owner(
    uintptr_t physical_address, syntra_page_owner_t *owner);
syntra_status_t syntra_page_alloc(uintptr_t *physical_address);
/* Allocate an aligned physically contiguous run of complete 4 KiB pages. */
syntra_status_t syntra_page_alloc_contiguous(
    size_t page_count,
    uintptr_t *physical_address
);
syntra_status_t syntra_page_free(uintptr_t physical_address);
size_t syntra_page_allocator_free_count(void);
size_t syntra_page_allocator_total_count(void);

#endif
