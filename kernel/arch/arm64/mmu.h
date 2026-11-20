#ifndef SYNTRA_ARM64_MMU_H
#define SYNTRA_ARM64_MMU_H

#include <stdint.h>

#include "../../core/status.h"

/*
 * ARMv8-A stage-1 translation descriptor helpers.
 *
 * Geometry: 4 KiB granule, 39-bit input virtual addresses (TTBR0),
 * and up to 48-bit output physical addresses.
 *
 * These helpers only construct descriptors. They do not access system
 * registers, allocate page tables, or enable the MMU.
 */
#define SYNTRA_ARM64_MMU_PAGE_SHIFT       12U
#define SYNTRA_ARM64_MMU_PAGE_SIZE       (1ULL << SYNTRA_ARM64_MMU_PAGE_SHIFT)
#define SYNTRA_ARM64_MMU_TABLE_ENTRIES   512U
#define SYNTRA_ARM64_MMU_VA_BITS         39U
#define SYNTRA_ARM64_MMU_PA_BITS         48U
#define SYNTRA_ARM64_MMU_PA_MASK         0x0000FFFFFFFFF000ULL

typedef enum
{
    SYNTRA_ARM64_MMU_MEMORY_NORMAL = 0,
    SYNTRA_ARM64_MMU_MEMORY_DEVICE = 1
} syntra_arm64_mmu_memory_type_t;

typedef enum
{
    SYNTRA_ARM64_MMU_ACCESS_READ_WRITE = 0,
    SYNTRA_ARM64_MMU_ACCESS_READ_ONLY = 1
} syntra_arm64_mmu_access_t;

/*
 * Construct a level-0/1/2 table descriptor.
 * table_physical_address must be 4 KiB aligned and fit in 48 bits.
 */
syntra_status_t syntra_arm64_mmu_make_table_descriptor(
    uint64_t table_physical_address,
    uint64_t *descriptor
);

/*
 * Construct a level-3 page descriptor.
 * Device memory is always non-executable. User access is never granted.
 */
syntra_status_t syntra_arm64_mmu_make_page_descriptor(
    uint64_t physical_address,
    syntra_arm64_mmu_memory_type_t memory_type,
    syntra_arm64_mmu_access_t access,
    int executable,
    uint64_t *descriptor
);

/* Return three 9-bit table indices and the 12-bit page offset. */
syntra_status_t syntra_arm64_mmu_get_table_indices(
    uint64_t virtual_address,
    uint16_t indices[4]
);

/* Validate a page-aligned VA-to-PA mapping range without creating tables. */
syntra_status_t syntra_arm64_mmu_validate_mapping_range(
    uint64_t virtual_address,
    uint64_t physical_address,
    uint64_t size
);

#endif
