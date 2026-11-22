#ifndef SYNTRA_ARM64_PAGE_TABLE_BUILDER_H
#define SYNTRA_ARM64_PAGE_TABLE_BUILDER_H

#include <stdint.h>
#include "mmu.h"

typedef syntra_status_t (*syntra_arm64_pt_alloc_fn)(
    void *context, uint64_t *physical_address);
typedef uint64_t *(*syntra_arm64_pt_resolve_fn)(
    uint64_t physical_address, void *context);
typedef syntra_status_t (*syntra_arm64_pt_release_fn)(
    void *context, uint64_t physical_address);

typedef struct
{
    syntra_arm64_pt_alloc_fn allocate;
    syntra_arm64_pt_resolve_fn resolve;
    syntra_arm64_pt_release_fn release;
    void *context;
    uint64_t root_physical_address;
} syntra_arm64_page_table_t;

/* Allocates and clears an L1 root. MMU activation is deliberately separate. */
syntra_status_t syntra_arm64_page_table_init_ex(
    syntra_arm64_page_table_t *table,
    syntra_arm64_pt_alloc_fn allocate,
    syntra_arm64_pt_release_fn release,
    syntra_arm64_pt_resolve_fn resolve,
    void *context);

/* Compatibility initializer; newly allocated pages cannot be reclaimed. */
syntra_status_t syntra_arm64_page_table_init(
    syntra_arm64_page_table_t *table,
    syntra_arm64_pt_alloc_fn allocate,
    syntra_arm64_pt_resolve_fn resolve,
    void *context);

/*
 * Maps aligned 2-MiB L2 blocks where possible, otherwise 4-KiB pages.
 * New intermediate tables are tracked in a bounded 64-entry rollback journal.
 * A release callback enables reclamation of newly-created hierarchy on setup
 * failure. Without it, the legacy initializer remains usable but allocated
 * pages cannot be returned on rollback. This is not yet a fully atomic
 * multi-range leaf update when pre-existing tables are used.
 */
syntra_status_t syntra_arm64_page_table_map(
    syntra_arm64_page_table_t *table,
    uint64_t virtual_address,
    uint64_t physical_address,
    uint64_t size,
    syntra_arm64_mmu_memory_type_t memory_type,
    syntra_arm64_mmu_access_t access,
    int executable);

/*
 * Map normal-memory EL0-accessible 4-KiB pages (no block descriptors).
 * Uses the same bounded intermediate-table rollback rules as map(). A release
 * callback is needed for physical-page reclamation if setup later fails.
 */
syntra_status_t syntra_arm64_page_table_map_user(
    syntra_arm64_page_table_t *table,
    uint64_t virtual_address,
    uint64_t physical_address,
    uint64_t size,
    syntra_arm64_mmu_access_t access,
    int executable);

/* Verify EL0 and EL1 permission attributes for a user mapping. */
syntra_status_t syntra_arm64_page_table_verify_user(
    syntra_arm64_page_table_t *table,
    uint64_t virtual_address,
    uint64_t physical_address,
    uint64_t size,
    syntra_arm64_mmu_access_t access,
    int executable);

/* Verify each mapping with the software walker and its expected permissions. */
syntra_status_t syntra_arm64_page_table_verify(
    syntra_arm64_page_table_t *table,
    uint64_t virtual_address,
    uint64_t physical_address,
    uint64_t size,
    syntra_arm64_mmu_memory_type_t memory_type,
    syntra_arm64_mmu_access_t access,
    int executable);

#endif
