#include "../../kernel/virtio/virtio_mmio.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

static void test_metadata_validation(void)
{
    syntra_virtio_mmio_device_t device;

    device.base = (uintptr_t)0x0A000000U;
    device.region_size = 0x200U;
    device.reg_index = 0U;
    device.discovered = 1;
    assert(syntra_virtio_mmio_validate(&device) == SYNTRA_STATUS_OK);

    device.region_size = SYNTRA_VIRTIO_MMIO_MIN_REGION_SIZE - 1U;
    assert(syntra_virtio_mmio_validate(&device) ==
           SYNTRA_STATUS_INVALID_ARGUMENT);

    device.region_size = 0x200U;
    device.discovered = 0;
    assert(syntra_virtio_mmio_validate(&device) ==
           SYNTRA_STATUS_INVALID_ARGUMENT);

    device.discovered = 1;
    device.base = 0U;
    assert(syntra_virtio_mmio_validate(&device) ==
           SYNTRA_STATUS_INVALID_ARGUMENT);

    device.base = UINTPTR_MAX - 0x100U;
    device.region_size = 0x200U;
    assert(syntra_virtio_mmio_validate(&device) ==
           SYNTRA_STATUS_INVALID_ARGUMENT);
}

static void test_mapped_identity_read(void)
{
    syntra_virtio_mmio_device_t device = {
        (uintptr_t)0x0A000000U, 0x200U, 0U, 1
    };
    volatile uint32_t registers[4] = {
        SYNTRA_VIRTIO_MMIO_EXPECTED_MAGIC,
        SYNTRA_VIRTIO_MMIO_VERSION_MODERN,
        SYNTRA_VIRTIO_MMIO_DEVICE_ID_INPUT,
        0x554D4551U
    };
    syntra_virtio_mmio_register_snapshot_t snapshot;

    assert(syntra_virtio_mmio_read_identity_mapped(
        &device, registers, sizeof(registers), &snapshot) ==
        SYNTRA_STATUS_OK);
    assert(snapshot.magic_value == SYNTRA_VIRTIO_MMIO_EXPECTED_MAGIC);
    assert(snapshot.version == SYNTRA_VIRTIO_MMIO_VERSION_MODERN);
    assert(snapshot.device_id == SYNTRA_VIRTIO_MMIO_DEVICE_ID_INPUT);
    assert(snapshot.vendor_id == 0x554D4551U);

    assert(syntra_virtio_mmio_read_identity_mapped(
        &device, registers, SYNTRA_VIRTIO_MMIO_IDENTITY_SIZE - 1U,
        &snapshot) == SYNTRA_STATUS_INVALID_ARGUMENT);

    assert(syntra_virtio_mmio_read_identity_mapped(
        &device, registers, device.region_size + 1U,
        &snapshot) == SYNTRA_STATUS_INVALID_ARGUMENT);

    assert(syntra_virtio_mmio_read_identity_mapped(
        &device, NULL, sizeof(registers), &snapshot) ==
        SYNTRA_STATUS_INVALID_ARGUMENT);

    assert(syntra_virtio_mmio_read_identity_mapped(
        &device, registers, sizeof(registers), NULL) ==
        SYNTRA_STATUS_INVALID_ARGUMENT);

    {
        uint8_t unaligned_storage[sizeof(registers) + 1U];
        const volatile void *unaligned = &unaligned_storage[1];
        assert(syntra_virtio_mmio_read_identity_mapped(
            &device, unaligned, sizeof(registers),
            &snapshot) == SYNTRA_STATUS_INVALID_ARGUMENT);
    }

    device.base += 1U;
    assert(syntra_virtio_mmio_read_identity_mapped(
        &device, registers, sizeof(registers), &snapshot) ==
        SYNTRA_STATUS_INVALID_ARGUMENT);
}

static void test_identity_decode(void)
{
    syntra_virtio_mmio_register_snapshot_t snapshot = {
        SYNTRA_VIRTIO_MMIO_EXPECTED_MAGIC,
        SYNTRA_VIRTIO_MMIO_VERSION_MODERN,
        SYNTRA_VIRTIO_MMIO_DEVICE_ID_INPUT,
        0x554D4551U
    };
    syntra_virtio_mmio_identity_t identity;

    assert(syntra_virtio_mmio_decode_identity(&snapshot, &identity) ==
           SYNTRA_STATUS_OK);
    assert(identity.version == SYNTRA_VIRTIO_MMIO_VERSION_MODERN);
    assert(identity.device_id == SYNTRA_VIRTIO_MMIO_DEVICE_ID_INPUT);
    assert(identity.vendor_id == 0x554D4551U);
    assert(identity.function == SYNTRA_VIRTIO_MMIO_FUNCTION_INPUT);

    snapshot.device_id = 2U;
    assert(syntra_virtio_mmio_decode_identity(&snapshot, &identity) ==
           SYNTRA_STATUS_OK);
    assert(identity.function == SYNTRA_VIRTIO_MMIO_FUNCTION_OTHER);

    snapshot.device_id = 0U;
    assert(syntra_virtio_mmio_decode_identity(&snapshot, &identity) ==
           SYNTRA_STATUS_OK);
    assert(identity.function == SYNTRA_VIRTIO_MMIO_FUNCTION_UNKNOWN);

    snapshot.magic_value = 0U;
    assert(syntra_virtio_mmio_decode_identity(&snapshot, &identity) ==
           SYNTRA_STATUS_ERROR);

    snapshot.magic_value = SYNTRA_VIRTIO_MMIO_EXPECTED_MAGIC;
    snapshot.version = 3U;
    assert(syntra_virtio_mmio_decode_identity(&snapshot, &identity) ==
           SYNTRA_STATUS_NOT_IMPLEMENTED);

    assert(syntra_virtio_mmio_decode_identity(NULL, &identity) ==
           SYNTRA_STATUS_INVALID_ARGUMENT);
    assert(syntra_virtio_mmio_decode_identity(&snapshot, NULL) ==
           SYNTRA_STATUS_INVALID_ARGUMENT);
}

int main(void)
{
    test_metadata_validation();
    test_mapped_identity_read();
    test_identity_decode();
    puts("SyntraOS VirtIO-MMIO mapped identity tests passed.");
    return 0;
}
