#include "virtio_mmio.h"

#include <stddef.h>

syntra_status_t syntra_virtio_mmio_validate(
    const syntra_virtio_mmio_device_t *device)
{
    if (device == NULL || device->discovered != 1 ||
        device->base == (uintptr_t)0 ||
        device->region_size < SYNTRA_VIRTIO_MMIO_MIN_REGION_SIZE)
        return SYNTRA_STATUS_INVALID_ARGUMENT;

    if (device->base > UINTPTR_MAX - device->region_size)
        return SYNTRA_STATUS_INVALID_ARGUMENT;

    return SYNTRA_STATUS_OK;
}

syntra_status_t syntra_virtio_mmio_discover_first(
    const syntra_fdt_t *fdt,
    syntra_virtio_mmio_device_t *device)
{
    syntra_fdt_node_t node;
    uintptr_t base;
    size_t region_size;
    size_t reg_count;
    syntra_status_t status;

    if (fdt == NULL || device == NULL)
        return SYNTRA_STATUS_INVALID_ARGUMENT;

    device->base = 0U;
    device->region_size = 0U;
    device->reg_index = 0U;
    device->discovered = 0;

    status = syntra_fdt_find_compatible(fdt, "virtio,mmio", &node);
    if (status != SYNTRA_STATUS_OK)
        return status;

    status = syntra_fdt_get_reg_count(fdt, &node, &reg_count);
    if (status != SYNTRA_STATUS_OK)
        return status;
    if (reg_count == 0U)
        return SYNTRA_STATUS_ERROR;

    status = syntra_fdt_get_reg(fdt, &node, 0U, &base, &region_size);
    if (status != SYNTRA_STATUS_OK)
        return status;

    device->base = base;
    device->region_size = region_size;
    device->reg_index = 0U;
    device->discovered = 1;

    return syntra_virtio_mmio_validate(device);
}

syntra_status_t syntra_virtio_mmio_read_identity_mapped(
    const syntra_virtio_mmio_device_t *device,
    const volatile void *mapped_base,
    size_t mapped_size,
    syntra_virtio_mmio_register_snapshot_t *snapshot)
{
    const volatile uint32_t *registers;

    if (syntra_virtio_mmio_validate(device) != SYNTRA_STATUS_OK ||
        mapped_base == NULL || snapshot == NULL ||
        mapped_size < SYNTRA_VIRTIO_MMIO_IDENTITY_SIZE ||
        mapped_size > device->region_size ||
        (device->base & 0x3U) != 0U ||
        (((uintptr_t)mapped_base) & 0x3U) != 0U)
        return SYNTRA_STATUS_INVALID_ARGUMENT;

    registers = (const volatile uint32_t *)mapped_base;

    snapshot->magic_value =
        registers[SYNTRA_VIRTIO_MMIO_MAGIC_VALUE / sizeof(uint32_t)];
    snapshot->version =
        registers[SYNTRA_VIRTIO_MMIO_VERSION / sizeof(uint32_t)];
    snapshot->device_id =
        registers[SYNTRA_VIRTIO_MMIO_DEVICE_ID / sizeof(uint32_t)];
    snapshot->vendor_id =
        registers[SYNTRA_VIRTIO_MMIO_VENDOR_ID / sizeof(uint32_t)];

#if defined(__aarch64__)
    /* Complete the ordered Device-memory reads before decoding the snapshot. */
    __asm__ volatile("dmb oshld" ::: "memory");
#endif

    return SYNTRA_STATUS_OK;
}

syntra_status_t syntra_virtio_mmio_decode_identity(
    const syntra_virtio_mmio_register_snapshot_t *snapshot,
    syntra_virtio_mmio_identity_t *identity)
{
    if (snapshot == NULL || identity == NULL)
        return SYNTRA_STATUS_INVALID_ARGUMENT;

    identity->version = 0U;
    identity->device_id = 0U;
    identity->vendor_id = 0U;
    identity->function = SYNTRA_VIRTIO_MMIO_FUNCTION_UNKNOWN;

    if (snapshot->magic_value != SYNTRA_VIRTIO_MMIO_EXPECTED_MAGIC)
        return SYNTRA_STATUS_ERROR;

    if (snapshot->version != SYNTRA_VIRTIO_MMIO_VERSION_LEGACY &&
        snapshot->version != SYNTRA_VIRTIO_MMIO_VERSION_MODERN)
        return SYNTRA_STATUS_NOT_IMPLEMENTED;

    identity->version = snapshot->version;
    identity->device_id = snapshot->device_id;
    identity->vendor_id = snapshot->vendor_id;

    if (snapshot->device_id == 0U)
        identity->function = SYNTRA_VIRTIO_MMIO_FUNCTION_UNKNOWN;
    else if (snapshot->device_id == SYNTRA_VIRTIO_MMIO_DEVICE_ID_INPUT)
        identity->function = SYNTRA_VIRTIO_MMIO_FUNCTION_INPUT;
    else
        identity->function = SYNTRA_VIRTIO_MMIO_FUNCTION_OTHER;

    return SYNTRA_STATUS_OK;
}
