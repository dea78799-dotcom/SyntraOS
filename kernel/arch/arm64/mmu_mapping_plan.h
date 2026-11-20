#ifndef SYNTRA_ARM64_MMU_MAPPING_PLAN_H
#define SYNTRA_ARM64_MMU_MAPPING_PLAN_H
#include <stddef.h>
#include <stdint.h>
#include "mmu.h"
#include "boot/memory_map.h"
#include "gic.h"
#define SYNTRA_ARM64_MMU_MAPPING_PLAN_MAX 48U
typedef struct {
 uint64_t virtual_address, physical_address, size;
 syntra_arm64_mmu_memory_type_t memory_type;
 syntra_arm64_mmu_access_t access;
 uint8_t executable;
} syntra_arm64_mmu_mapping_t;
typedef struct {
 syntra_arm64_mmu_mapping_t mappings[SYNTRA_ARM64_MMU_MAPPING_PLAN_MAX];
 size_t count;
} syntra_arm64_mmu_mapping_plan_t;
/* Checked identity map: ordinary RAM RW+XN, kernel sections explicit,
 * DTB RO+XN, and platform MMIO Device+RW+XN. Planning does not mutate tables. */
syntra_status_t syntra_arm64_mmu_mapping_plan_build(
 const syntra_arm64_boot_memory_metadata_t *metadata,
 const syntra_memory_region_t *regions, size_t region_count,
 const syntra_arm64_gic_config_t *gic,
 uintptr_t uart_base, size_t uart_size,
 uintptr_t fw_cfg_base, size_t fw_cfg_size,
 syntra_arm64_mmu_mapping_plan_t *plan);
#endif
