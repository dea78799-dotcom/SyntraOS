#ifndef SYNTRA_ARM64_ADDRESS_SPACE_H
#define SYNTRA_ARM64_ADDRESS_SPACE_H

#include <stddef.h>
#include <stdint.h>

#include "page_table_builder.h"

#define SYNTRA_ARM64_ADDRESS_SPACE_MAX_MAPPINGS 256U

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
typedef struct
{
    uint32_t address_space_id;
    uint8_t initialized;
    syntra_arm64_page_table_t page_table;
    syntra_arm64_address_space_resolve_fn resolve;
    void *resolve_context;
    size_t mapping_count;
    syntra_arm64_address_space_mapping_t mappings[
        SYNTRA_ARM64_ADDRESS_SPACE_MAX_MAPPINGS];
} syntra_arm64_address_space_t;

/*
 * Creates an independent L1 root; all page-table pages are tagged PAGE_TABLE.
 * The caller must pass a zero-initialized space object on first creation.
 */
syntra_status_t syntra_arm64_address_space_create(
    syntra_arm64_address_space_t *space,
    uint32_t address_space_id,
    syntra_arm64_address_space_resolve_fn resolve,
    void *resolve_context);

/* Maps one owned PROCESS page into EL0 at a 4-KiB-aligned virtual address. */
syntra_status_t syntra_arm64_address_space_map_user_page(
    syntra_arm64_address_space_t *space,
    uint64_t virtual_address,
    uint64_t physical_address,
    syntra_arm64_mmu_access_t access,
    int executable);

/* Translate through this address space's root, without changing TTBR0_EL1. */
syntra_status_t syntra_arm64_address_space_translate(
    syntra_arm64_address_space_t *space,
    uint64_t virtual_address,
    syntra_arm64_mmu_translation_t *translation);

/* Audit root/table ownership, mapping ownership, and user permissions. */
syntra_status_t syntra_arm64_address_space_validate(
    syntra_arm64_address_space_t *space);

/*
 * Destroys this software address space and releases its mapped PROCESS pages
 * and PAGE_TABLE pages. Caller must first ensure this root is not active on
 * any CPU. V1 does not provide SMP coordination or hardware TLB shootdown.
 */
syntra_status_t syntra_arm64_address_space_destroy(
    syntra_arm64_address_space_t *space);

#endif
