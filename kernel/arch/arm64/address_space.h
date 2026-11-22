#ifndef SYNTRA_ARM64_ADDRESS_SPACE_H
#define SYNTRA_ARM64_ADDRESS_SPACE_H

#include <stddef.h>
#include <stdint.h>

#include "page_table_builder.h"

#define SYNTRA_ARM64_ADDRESS_SPACE_MAX_MAPPINGS 256U
#define SYNTRA_ARM64_ADDRESS_SPACE_MAX_INSTANCES 64U
#define SYNTRA_ARM64_USER_VA_BASE (1ULL << 38U)
#define SYNTRA_ARM64_USER_VA_LIMIT (1ULL << SYNTRA_ARM64_MMU_VA_BITS)
#define SYNTRA_ARM64_ADDRESS_SPACE_ROOT_BITMAP_WORDS 8U

typedef uint64_t *(*syntra_arm64_address_space_resolve_fn)(
    uint64_t physical_address, void *context);

typedef struct
{
    uint64_t virtual_address;
    uint64_t physical_address;
    syntra_arm64_mmu_access_t access;
    uint8_t executable;
} syntra_arm64_address_space_mapping_t;

/*
 * V1 owns its page-table pages and mapped PROCESS pages.
 * The resolver must provide a writable kernel mapping for allocated table pages.
 */
typedef struct syntra_arm64_address_space
{
    uint32_t address_space_id;
    uint8_t initialized;
    syntra_arm64_page_table_t page_table;
    syntra_arm64_address_space_resolve_fn resolve;
    void *resolve_context;
    size_t mapping_count;
    uint8_t kernel_mappings_inherited;
    uint64_t kernel_root_physical_address;
    uint64_t borrowed_root_entries[
        SYNTRA_ARM64_ADDRESS_SPACE_ROOT_BITMAP_WORDS];
    syntra_arm64_address_space_mapping_t mappings[
        SYNTRA_ARM64_ADDRESS_SPACE_MAX_MAPPINGS];
} syntra_arm64_address_space_t;

/*
 * Creates an independent L1 root; all page-table pages are tagged PAGE_TABLE.
 * The caller must pass a zero-initialized space object on first creation and
 * keep it at a stable address until destruction. V1 permits each PROCESS page
 * to be mapped by only one registered address space (no shared-page refcounts).
 */
syntra_status_t syntra_arm64_address_space_create(
    syntra_arm64_address_space_t *space,
    uint32_t address_space_id,
    syntra_arm64_address_space_resolve_fn resolve,
    void *resolve_context);

/* Copy kernel L1 entries into this inactive root; shared subtrees stay borrowed. */
syntra_status_t syntra_arm64_address_space_inherit_kernel_mappings(
    syntra_arm64_address_space_t *space,
    uint64_t kernel_root_physical_address);

/*
 * Register the already-active kernel root after MMU enable. V1 supports one
 * CPU and verifies TTBR0_EL1 on ARM64 hardware.
 */
syntra_status_t syntra_arm64_address_space_register_kernel_root(
    uint64_t kernel_root_physical_address);

/* Activate an inherited process root or return to the registered kernel root. */
syntra_status_t syntra_arm64_address_space_activate(
    syntra_arm64_address_space_t *space);
syntra_status_t syntra_arm64_address_space_activate_kernel_root(void);
int syntra_arm64_address_space_kernel_root_registered(void);

/* Maps one owned PROCESS page into the reserved upper TTBR0 user range. */
syntra_status_t syntra_arm64_address_space_map_user_page(
    syntra_arm64_address_space_t *space,
    uint64_t virtual_address,
    uint64_t physical_address,
    syntra_arm64_mmu_access_t access,
    int executable);

/* Remove a mapping and release its exclusively owned PROCESS page. */
syntra_status_t syntra_arm64_address_space_unmap_user_page(
    syntra_arm64_address_space_t *space,
    uint64_t virtual_address);

/* Translate through this address space's root, without changing TTBR0_EL1. */
syntra_status_t syntra_arm64_address_space_translate(
    syntra_arm64_address_space_t *space,
    uint64_t virtual_address,
    syntra_arm64_mmu_translation_t *translation);

/* Audit root/table ownership, mapping ownership, and user permissions. */
syntra_status_t syntra_arm64_address_space_validate(
    syntra_arm64_address_space_t *space);

/*
 * Destroys this address space and releases mapped PROCESS pages and private
 * PAGE_TABLE pages. Destruction rejects the locally active root. V1 supports
 * one CPU and does not provide remote TLB shootdown.
 */
syntra_status_t syntra_arm64_address_space_destroy(
    syntra_arm64_address_space_t *space);

#endif
