#ifndef SYNTRA_ARM64_KERNEL_VA_REGISTRY_H
#define SYNTRA_ARM64_KERNEL_VA_REGISTRY_H

#include <stddef.h>
#include <stdint.h>

#include "../../core/status.h"

#define SYNTRA_ARM64_KERNEL_VA_PAGE_SIZE 4096ULL
#define SYNTRA_ARM64_KERNEL_VA_BASE 0x0000001000000000ULL /* 64 GiB */
#define SYNTRA_ARM64_KERNEL_VA_LIMIT 0x0000001800000000ULL /* 96 GiB, exclusive */
#define SYNTRA_ARM64_KERNEL_VA_SIZE \
    (SYNTRA_ARM64_KERNEL_VA_LIMIT - SYNTRA_ARM64_KERNEL_VA_BASE)
#define SYNTRA_ARM64_KERNEL_VA_REGISTRY_CAPACITY 64U

typedef struct
{
    uint64_t allocation_id;
    uint32_t owner;
    uint32_t guard_pages;
    uint64_t requested_size;
    uint64_t reservation_base;
    uint64_t reservation_size;
    uint64_t usable_base;
    uint64_t usable_size;
} syntra_arm64_kernel_va_reservation_t;

/*
 * Initializes once during boot, before publication and before IRQ enablement.
 * Runtime operations mask local IRQ exceptions on the current single-core target;
 * this is not an SMP lock and does not provide cross-core exclusion.
 * It reserves virtual ranges only: it does not allocate physical pages, create
 * page-table mappings, or enable the MMU.
 */
syntra_status_t syntra_arm64_kernel_va_registry_init(void);

/*
 * First-fit reserve. The requested usable size is rounded up to 4 KiB.
 * guard_pages reserves unmapped space immediately before the usable range.
 * owner must be a nonzero caller-defined tag. Output remains untouched on error.
 */
syntra_status_t syntra_arm64_kernel_va_reserve(
    uint64_t requested_size,
    uint32_t guard_pages,
    uint32_t owner,
    syntra_arm64_kernel_va_reservation_t *reservation);

/* Releases only an exact live reservation descriptor; stale/altered tokens fail. */
syntra_status_t syntra_arm64_kernel_va_release(
    const syntra_arm64_kernel_va_reservation_t *reservation);

/* Diagnostics: validates registry invariants, returns live count, or reads a slot. */
syntra_status_t syntra_arm64_kernel_va_registry_validate(void);
size_t syntra_arm64_kernel_va_registry_count(void);
syntra_status_t syntra_arm64_kernel_va_registry_get(
    size_t slot,
    syntra_arm64_kernel_va_reservation_t *reservation);

#endif
