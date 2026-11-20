#ifndef SYNTRA_ARM64_MMU_H
#define SYNTRA_ARM64_MMU_H

#include <stdint.h>
#include "../../core/status.h"

#define SYNTRA_ARM64_MMU_PAGE_SHIFT 12U
#define SYNTRA_ARM64_MMU_PAGE_SIZE (1ULL << SYNTRA_ARM64_MMU_PAGE_SHIFT)
#define SYNTRA_ARM64_MMU_BLOCK_SIZE (1ULL << 21U)
#define SYNTRA_ARM64_MMU_TABLE_ENTRIES 512U
#define SYNTRA_ARM64_MMU_VA_BITS 39U
#define SYNTRA_ARM64_MMU_PA_BITS 48U
#define SYNTRA_ARM64_MMU_PA_MASK 0x0000FFFFFFFFF000ULL

typedef struct
{
    uint64_t sctlr_el1;
    uint64_t mair_el1;
    uint64_t tcr_el1;
    uint64_t ttbr0_el1;
    uint64_t id_aa64mmfr0_el1;
} syntra_arm64_mmu_registers_t;

typedef enum
{
    SYNTRA_ARM64_MMU_MEMORY_NORMAL = 0,
    SYNTRA_ARM64_MMU_MEMORY_DEVICE = 1,
    SYNTRA_ARM64_MMU_MEMORY_UNKNOWN = 2
} syntra_arm64_mmu_memory_type_t;

typedef enum
{
    SYNTRA_ARM64_MMU_ACCESS_READ_WRITE = 0,
    SYNTRA_ARM64_MMU_ACCESS_READ_ONLY = 1
} syntra_arm64_mmu_access_t;

typedef struct
{
    uint64_t physical_address;
    uint64_t page_descriptor;
    uint64_t page_size;
    uint8_t level;
    uint8_t attribute_index;
    uint8_t memory_type;
    uint8_t shareability;
    uint8_t access_flag;
    uint8_t ap;
    uint8_t privileged_read;
    uint8_t privileged_write;
    uint8_t user_read;
    uint8_t user_write;
    uint8_t privileged_execute;
    uint8_t user_execute;
} syntra_arm64_mmu_translation_t;

typedef const uint64_t *(*syntra_arm64_mmu_table_resolver_t)(
    uint64_t table_physical_address, void *context);

syntra_status_t syntra_arm64_mmu_make_table_descriptor(
    uint64_t table_physical_address, uint64_t *descriptor);
/* User mappings are normal-memory 4-KiB pages; PXN is always set. */
syntra_status_t syntra_arm64_mmu_make_user_page_descriptor(
    uint64_t physical_address,
    syntra_arm64_mmu_access_t access,
    int executable,
    uint64_t *descriptor
);
syntra_status_t syntra_arm64_mmu_make_page_descriptor(
    uint64_t physical_address, syntra_arm64_mmu_memory_type_t memory_type,
    syntra_arm64_mmu_access_t access, int executable, uint64_t *descriptor);
syntra_status_t syntra_arm64_mmu_make_block_descriptor(
    uint64_t physical_address, syntra_arm64_mmu_memory_type_t memory_type,
    syntra_arm64_mmu_access_t access, int executable, uint64_t *descriptor);

/* Three 9-bit indices (L1/L2/L3) and the 12-bit page offset for T0SZ=25. */
syntra_status_t syntra_arm64_mmu_get_table_indices(
    uint64_t virtual_address, uint16_t indices[4]);

/* Starts at L1; supports L1 1-GiB and L2 2-MiB blocks plus L3 pages. */
syntra_status_t syntra_arm64_mmu_walk(
    uint64_t root_table_physical_address, uint64_t virtual_address,
    syntra_arm64_mmu_table_resolver_t resolver, void *context,
    syntra_arm64_mmu_translation_t *translation);

syntra_status_t syntra_arm64_mmu_read_registers(syntra_arm64_mmu_registers_t *registers);
syntra_status_t syntra_arm64_mmu_prepare(uint64_t root_table_physical_address);
syntra_status_t syntra_arm64_mmu_enable(void);
syntra_status_t syntra_arm64_mmu_validate_mapping_range(
    uint64_t virtual_address, uint64_t physical_address, uint64_t size);

#endif
