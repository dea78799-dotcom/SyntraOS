#ifndef SYNTRA_VIRTIO_MMIO_H
#define SYNTRA_VIRTIO_MMIO_H

#include <stddef.h>
#include <stdint.h>

#include "../core/status.h"
#include "../arch/arm64/boot/fdt.h"

/*
 * Device-Tree discovery metadata for a VirtIO-MMIO transport.
 * This layer deliberately performs no MMIO access and enables no IRQ.
 */
#define SYNTRA_VIRTIO_MMIO_MIN_REGION_SIZE 0x200U

typedef struct
{
    uintptr_t base;
    size_t region_size;
    uint32_t reg_index;
    int discovered;
} syntra_virtio_mmio_device_t;

/*
 * Discover the first "virtio,mmio" node from a validated FDT.
 * The returned region is metadata only; callers must establish a safe
 * device-memory mapping before accessing registers.
 */
syntra_status_t syntra_virtio_mmio_discover_first(
    const syntra_fdt_t *fdt,
    syntra_virtio_mmio_device_t *device
);

/* Validate metadata before a later transport initialization stage. */
syntra_status_t syntra_virtio_mmio_validate(
    const syntra_virtio_mmio_device_t *device
);

#endif
