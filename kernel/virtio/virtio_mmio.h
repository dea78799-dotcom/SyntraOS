#ifndef SYNTRA_VIRTIO_MMIO_H
#define SYNTRA_VIRTIO_MMIO_H

#include <stddef.h>
#include <stdint.h>

#include "../core/status.h"
#include "../arch/arm64/boot/fdt.h"

/*
 * Device-Tree discovery metadata for a VirtIO-MMIO transport.
 * No MMIO access or IRQ enable occurs during discovery.
 */
#define SYNTRA_VIRTIO_MMIO_MIN_REGION_SIZE 0x200U

/* Register offsets from the public VirtIO-MMIO transport specification. */
#define SYNTRA_VIRTIO_MMIO_MAGIC_VALUE 0x000U
#define SYNTRA_VIRTIO_MMIO_VERSION     0x004U
#define SYNTRA_VIRTIO_MMIO_DEVICE_ID   0x008U
#define SYNTRA_VIRTIO_MMIO_VENDOR_ID   0x00CU
#define SYNTRA_VIRTIO_MMIO_IDENTITY_SIZE 0x010U
#define SYNTRA_VIRTIO_MMIO_DEVICE_FEATURES       0x010U
#define SYNTRA_VIRTIO_MMIO_DEVICE_FEATURES_SEL   0x014U
#define SYNTRA_VIRTIO_MMIO_DRIVER_FEATURES       0x020U
#define SYNTRA_VIRTIO_MMIO_DRIVER_FEATURES_SEL   0x024U
#define SYNTRA_VIRTIO_MMIO_DEVICE_STATUS         0x070U
#define SYNTRA_VIRTIO_MMIO_DEVICE_STATUS_SIZE    0x074U

#define SYNTRA_VIRTIO_STATUS_ACKNOWLEDGE 0x01U
#define SYNTRA_VIRTIO_STATUS_DRIVER      0x02U
#define SYNTRA_VIRTIO_STATUS_DRIVER_OK   0x04U
#define SYNTRA_VIRTIO_STATUS_FEATURES_OK 0x08U
#define SYNTRA_VIRTIO_STATUS_FAILED     0x80U
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

/* A copied register snapshot; decoding never dereferences a device address. */
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

syntra_status_t syntra_virtio_mmio_discover_first(
    const syntra_fdt_t *fdt,
    syntra_virtio_mmio_device_t *device
);

syntra_status_t syntra_virtio_mmio_validate(
    const syntra_virtio_mmio_device_t *device
);

/*
 * Read only the four identification registers from a mapped device region.
 * Caller contract: mapped_base must be the verified Device-memory mapping
 * for device->base, and this function may only be called after the MMU
 * mapping audit has passed. The supplied size bounds every register read.
 */
syntra_status_t syntra_virtio_mmio_read_identity_mapped(
    const syntra_virtio_mmio_device_t *device,
    const volatile void *mapped_base,
    size_t mapped_size,
    syntra_virtio_mmio_register_snapshot_t *snapshot
);

/*
 * Negotiate the empty feature subset for a confirmed VirtIO Input device.
 * It resets the device and advances only to ACKNOWLEDGE/DRIVER (plus
 * FEATURES_OK on transport v2). It does not configure a queue, mark
 * DRIVER_OK, or enable interrupts.
 */
syntra_status_t syntra_virtio_mmio_prepare_input_mapped(
    const syntra_virtio_mmio_device_t *device,
    volatile void *mapped_base,
    size_t mapped_size,
    const syntra_virtio_mmio_identity_t *identity
);

/* Decode a copied snapshot; no MMIO access or device mutation occurs here. */
syntra_status_t syntra_virtio_mmio_decode_identity(
    const syntra_virtio_mmio_register_snapshot_t *snapshot,
    syntra_virtio_mmio_identity_t *identity
);

#endif
