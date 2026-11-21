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


static syntra_status_t virtio_mmio_mapped_read32(
    const syntra_virtio_mmio_device_t *device,
    const volatile void *mapped_base,
    size_t mapped_size,
    size_t offset,
    uint32_t *value)
{
    const volatile uint32_t *registers;
    if (syntra_virtio_mmio_validate(device) != SYNTRA_STATUS_OK ||
        mapped_base == NULL || value == NULL ||
        mapped_size > device->region_size ||
        offset > mapped_size || mapped_size - offset < sizeof(uint32_t) ||
        (offset & 0x3U) != 0U ||
        (((uintptr_t)mapped_base) & 0x3U) != 0U)
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    registers = (const volatile uint32_t *)mapped_base;
    *value = registers[offset / sizeof(uint32_t)];
    return SYNTRA_STATUS_OK;
}

static syntra_status_t virtio_mmio_mapped_write32(
    const syntra_virtio_mmio_device_t *device,
    volatile void *mapped_base,
    size_t mapped_size,
    size_t offset,
    uint32_t value)
{
    volatile uint32_t *registers;
    if (syntra_virtio_mmio_validate(device) != SYNTRA_STATUS_OK ||
        mapped_base == NULL ||
        mapped_size > device->region_size ||
        offset > mapped_size || mapped_size - offset < sizeof(uint32_t) ||
        (offset & 0x3U) != 0U ||
        (((uintptr_t)mapped_base) & 0x3U) != 0U)
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    registers = (volatile uint32_t *)mapped_base;
    registers[offset / sizeof(uint32_t)] = value;
#if defined(__aarch64__)
    __asm__ volatile("dsb oshst" ::: "memory");
#endif
    return SYNTRA_STATUS_OK;
}

syntra_status_t syntra_virtio_mmio_prepare_input_mapped(
    const syntra_virtio_mmio_device_t *device,
    volatile void *mapped_base,
    size_t mapped_size,
    const syntra_virtio_mmio_identity_t *identity)
{
    uint32_t status_value;
    uint32_t device_id;
    syntra_status_t status;

    if (identity == NULL ||
        identity->function != SYNTRA_VIRTIO_MMIO_FUNCTION_INPUT ||
        identity->device_id != SYNTRA_VIRTIO_MMIO_DEVICE_ID_INPUT ||
        (identity->version != SYNTRA_VIRTIO_MMIO_VERSION_LEGACY &&
         identity->version != SYNTRA_VIRTIO_MMIO_VERSION_MODERN) ||
        mapped_size < SYNTRA_VIRTIO_MMIO_DEVICE_STATUS_SIZE)
        return SYNTRA_STATUS_INVALID_ARGUMENT;

    status = virtio_mmio_mapped_read32(
        device, mapped_base, mapped_size,
        SYNTRA_VIRTIO_MMIO_DEVICE_ID, &device_id);
    if (status != SYNTRA_STATUS_OK || device_id != identity->device_id)
        return SYNTRA_STATUS_ERROR;

    /* Reset, then perform the mandatory ACKNOWLEDGE -> DRIVER transition. */
    status = virtio_mmio_mapped_write32(
        device, mapped_base, mapped_size,
        SYNTRA_VIRTIO_MMIO_DEVICE_STATUS, 0U);
    if (status != SYNTRA_STATUS_OK) return status;
    status = virtio_mmio_mapped_write32(
        device, mapped_base, mapped_size,
        SYNTRA_VIRTIO_MMIO_DEVICE_STATUS,
        SYNTRA_VIRTIO_STATUS_ACKNOWLEDGE);
    if (status != SYNTRA_STATUS_OK) return status;
    status = virtio_mmio_mapped_write32(
        device, mapped_base, mapped_size,
        SYNTRA_VIRTIO_MMIO_DEVICE_STATUS,
        SYNTRA_VIRTIO_STATUS_ACKNOWLEDGE | SYNTRA_VIRTIO_STATUS_DRIVER);
    if (status != SYNTRA_STATUS_OK) return status;

    /* No optional features are claimed until SyntraOS implements them. */
    status = virtio_mmio_mapped_write32(
        device, mapped_base, mapped_size,
        SYNTRA_VIRTIO_MMIO_DRIVER_FEATURES_SEL, 0U);
    if (status != SYNTRA_STATUS_OK) return status;
    status = virtio_mmio_mapped_write32(
        device, mapped_base, mapped_size,
        SYNTRA_VIRTIO_MMIO_DRIVER_FEATURES, 0U);
    if (status != SYNTRA_STATUS_OK) return status;
    status = virtio_mmio_mapped_write32(
        device, mapped_base, mapped_size,
        SYNTRA_VIRTIO_MMIO_DRIVER_FEATURES_SEL, 1U);
    if (status != SYNTRA_STATUS_OK) return status;
    status = virtio_mmio_mapped_write32(
        device, mapped_base, mapped_size,
        SYNTRA_VIRTIO_MMIO_DRIVER_FEATURES, 0U);
    if (status != SYNTRA_STATUS_OK) return status;

    if (identity->version == SYNTRA_VIRTIO_MMIO_VERSION_MODERN)
    {
        status = virtio_mmio_mapped_write32(
            device, mapped_base, mapped_size,
            SYNTRA_VIRTIO_MMIO_DEVICE_STATUS,
            SYNTRA_VIRTIO_STATUS_ACKNOWLEDGE |
            SYNTRA_VIRTIO_STATUS_DRIVER |
            SYNTRA_VIRTIO_STATUS_FEATURES_OK);
        if (status != SYNTRA_STATUS_OK) return status;
    }

    status = virtio_mmio_mapped_read32(
        device, mapped_base, mapped_size,
        SYNTRA_VIRTIO_MMIO_DEVICE_STATUS, &status_value);
    if (status != SYNTRA_STATUS_OK) return status;

    if ((status_value & SYNTRA_VIRTIO_STATUS_FAILED) != 0U ||
        (status_value & (SYNTRA_VIRTIO_STATUS_ACKNOWLEDGE |
                         SYNTRA_VIRTIO_STATUS_DRIVER)) !=
        (SYNTRA_VIRTIO_STATUS_ACKNOWLEDGE |
         SYNTRA_VIRTIO_STATUS_DRIVER))
        return SYNTRA_STATUS_ERROR;

    if (identity->version == SYNTRA_VIRTIO_MMIO_VERSION_MODERN &&
        (status_value & SYNTRA_VIRTIO_STATUS_FEATURES_OK) == 0U)
        return SYNTRA_STATUS_NOT_IMPLEMENTED;

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
