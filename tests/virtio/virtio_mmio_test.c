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
    test_identity_decode();
    puts("SyntraOS VirtIO-MMIO discovery and identity tests passed.");
    return 0;
}
