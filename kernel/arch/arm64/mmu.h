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

typedef struct
{
    uint64_t sctlr_el1;
    uint64_t mair_el1;
    uint64_t tcr_el1;
    uint64_t ttbr0_el1;
} syntra_arm64_mmu_registers_t;

typedef struct
{
    uint64_t physical_address;
    uint64_t page_descriptor;
    uint8_t level;
} syntra_arm64_mmu_translation_t;

/* Resolve a physical page-table address to a readable 512-entry table. */
typedef const uint64_t *(*syntra_arm64_mmu_table_resolver_t)(
    uint64_t table_physical_address,
    void *context
);

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

/*
 * Walk a 3-level, 4 KiB-granule translation table without touching hardware.
 * Block descriptors at levels 1/2 are deliberately not implemented yet.
 */
syntra_status_t syntra_arm64_mmu_walk(
    uint64_t root_table_physical_address,
    uint64_t virtual_address,
    syntra_arm64_mmu_table_resolver_t resolver,
    void *context,
    syntra_arm64_mmu_translation_t *translation
);

/* Read SCTLR_EL1, MAIR_EL1, TCR_EL1 and TTBR0_EL1 on an AArch64 CPU. */
syntra_status_t syntra_arm64_mmu_read_registers(
    syntra_arm64_mmu_registers_t *registers
);

/*
 * Enable stage-1 EL1 translation using a prebuilt identity-mapped root table.
 * The caller must map all live code/data/stack/DTB and MMIO before calling.
 * This sets M only; it does not allocate tables or enable caches.
 */
syntra_status_t syntra_arm64_mmu_enable(uint64_t root_table_physical_address);

/* Validate a page-aligned VA-to-PA mapping range without creating tables. */
syntra_status_t syntra_arm64_mmu_validate_mapping_range(
    uint64_t virtual_address,
    uint64_t physical_address,
    uint64_t size
);

#endif
