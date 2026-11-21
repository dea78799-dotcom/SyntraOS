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

/* Register offsets from the public VirtIO-MMIO transport specification. */
#define SYNTRA_VIRTIO_MMIO_MAGIC_VALUE 0x000U
#define SYNTRA_VIRTIO_MMIO_VERSION     0x004U
#define SYNTRA_VIRTIO_MMIO_DEVICE_ID   0x008U
#define SYNTRA_VIRTIO_MMIO_VENDOR_ID  0x00CU
#define SYNTRA_VIRTIO_MMIO_DEVICE_ID_INPUT 18U

#define SYNTRA_VIRTIO_MMIO_EXPECTED_MAGIC 0x74726976U
#define SYNTRA_VIRTIO_MMIO_VERSION_LEGACY 1U
#define SYNTRA_VIRTIO_MMIO_VERSION_MODERN 2U

typedef struct
{
    uintptr_t base;
    size_t region_size;
    uint32_t reg_index;
    int discovered;
} syntra_virtio_mmio_device_t;

/*
 * A copied register snapshot. This is data supplied by a future, separately
 * validated MMIO reader; this parser never dereferences a device address.
 */
typedef struct
{
    uint32_t magic_value;
    uint32_t version;
    uint32_t device_id;
    uint32_t vendor_id;
} syntra_virtio_mmio_register_snapshot_t;

typedef enum
{
    SYNTRA_VIRTIO_MMIO_FUNCTION_UNKNOWN = 0,
    SYNTRA_VIRTIO_MMIO_FUNCTION_OTHER = 1,
    SYNTRA_VIRTIO_MMIO_FUNCTION_INPUT = 2
} syntra_virtio_mmio_function_t;

typedef struct
{
    uint32_t version;
    uint32_t device_id;
    uint32_t vendor_id;
    syntra_virtio_mmio_function_t function;
} syntra_virtio_mmio_identity_t;

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

/*
 * Decode an already-copied register snapshot. This validates the transport
 * magic/version and classifies device ID 18 as VirtIO Input. No MMIO access,
 * reset, feature negotiation, or device-state mutation occurs here.
 */
syntra_status_t syntra_virtio_mmio_decode_identity(
    const syntra_virtio_mmio_register_snapshot_t *snapshot,
    syntra_virtio_mmio_identity_t *identity
);

#endif
