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
        .base = (uintptr_t)0x0A000000U,
        .region_size = 0x200U,
        .reg_index = 0U,
        .discovered = 1
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
        _Alignas(4) uint8_t unaligned_storage[sizeof(registers) + 1U];
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


static void test_input_transport_prepare(void)
{
    syntra_virtio_mmio_device_t device = {
        .base = (uintptr_t)0x0A000000U,
        .region_size = 0x200U,
        .reg_index = 0U,
        .interrupt_id = 0U,
        .interrupt_flags = 0U,
        .interrupt_valid = 0,
        .discovered = 1
    };
    _Alignas(4) volatile uint32_t registers[0x200U / sizeof(uint32_t)] = {0};
    syntra_virtio_mmio_identity_t identity = {
        SYNTRA_VIRTIO_MMIO_VERSION_LEGACY,
        SYNTRA_VIRTIO_MMIO_DEVICE_ID_INPUT,
        0x554D4551U,
        SYNTRA_VIRTIO_MMIO_FUNCTION_INPUT
    };

    registers[SYNTRA_VIRTIO_MMIO_DEVICE_ID / sizeof(uint32_t)] =
        SYNTRA_VIRTIO_MMIO_DEVICE_ID_INPUT;

    assert(syntra_virtio_mmio_prepare_input_mapped(
        &device, registers, sizeof(registers), &identity) ==
        SYNTRA_STATUS_OK);
    assert(registers[SYNTRA_VIRTIO_MMIO_DEVICE_STATUS / sizeof(uint32_t)] ==
        (SYNTRA_VIRTIO_STATUS_ACKNOWLEDGE | SYNTRA_VIRTIO_STATUS_DRIVER));
    assert(registers[SYNTRA_VIRTIO_MMIO_DRIVER_FEATURES / sizeof(uint32_t)] == 0U);
    assert(registers[SYNTRA_VIRTIO_MMIO_DRIVER_FEATURES_SEL / sizeof(uint32_t)] == 0U);

    identity.version = SYNTRA_VIRTIO_MMIO_VERSION_MODERN;
    registers[SYNTRA_VIRTIO_MMIO_DEVICE_STATUS / sizeof(uint32_t)] = 0U;
    assert(syntra_virtio_mmio_prepare_input_mapped(
        &device, registers, sizeof(registers), &identity) ==
        SYNTRA_STATUS_OK);
    assert(registers[SYNTRA_VIRTIO_MMIO_DEVICE_STATUS / sizeof(uint32_t)] ==
        (SYNTRA_VIRTIO_STATUS_ACKNOWLEDGE |
         SYNTRA_VIRTIO_STATUS_DRIVER |
         SYNTRA_VIRTIO_STATUS_FEATURES_OK));
    assert(registers[SYNTRA_VIRTIO_MMIO_DRIVER_FEATURES_SEL / sizeof(uint32_t)] == 1U);

    identity.function = SYNTRA_VIRTIO_MMIO_FUNCTION_OTHER;
    assert(syntra_virtio_mmio_prepare_input_mapped(
        &device, registers, sizeof(registers), &identity) ==
        SYNTRA_STATUS_INVALID_ARGUMENT);
    identity.function = SYNTRA_VIRTIO_MMIO_FUNCTION_INPUT;

    assert(syntra_virtio_mmio_prepare_input_mapped(
        &device, registers, SYNTRA_VIRTIO_MMIO_DEVICE_STATUS_SIZE - 1U,
        &identity) == SYNTRA_STATUS_INVALID_ARGUMENT);

    registers[SYNTRA_VIRTIO_MMIO_DEVICE_ID / sizeof(uint32_t)] = 2U;
    assert(syntra_virtio_mmio_prepare_input_mapped(
        &device, registers, sizeof(registers), &identity) ==
        SYNTRA_STATUS_ERROR);
}


static void test_input_event_queue_setup(void)
{
    syntra_virtio_mmio_device_t device = {
        .base = (uintptr_t)0x0A000000U,
        .region_size = 0x200U,
        .reg_index = 0U,
        .interrupt_id = 48U,
        .interrupt_flags = 1U,
        .interrupt_valid = 1,
        .discovered = 1
    };
    _Alignas(4) volatile uint32_t registers[0x200U / sizeof(uint32_t)] = {0};
    static _Alignas(4096) uint8_t queue_memory[8192];
    syntra_virtio_mmio_identity_t identity = {
        SYNTRA_VIRTIO_MMIO_VERSION_LEGACY,
        SYNTRA_VIRTIO_MMIO_DEVICE_ID_INPUT,
        0x554D4551U,
        SYNTRA_VIRTIO_MMIO_FUNCTION_INPUT
    };
    syntra_virtio_input_queue_t queue;
    uint32_t descriptor_flags;

    registers[SYNTRA_VIRTIO_MMIO_DEVICE_ID / sizeof(uint32_t)] =
        SYNTRA_VIRTIO_MMIO_DEVICE_ID_INPUT;
    registers[SYNTRA_VIRTIO_MMIO_DEVICE_STATUS / sizeof(uint32_t)] =
        SYNTRA_VIRTIO_STATUS_ACKNOWLEDGE | SYNTRA_VIRTIO_STATUS_DRIVER;
    registers[SYNTRA_VIRTIO_MMIO_QUEUE_NUM_MAX / sizeof(uint32_t)] =
        SYNTRA_VIRTIO_INPUT_EVENT_QUEUE_SIZE;

    assert(syntra_virtio_mmio_setup_input_event_queue_mapped(
        &device, registers, sizeof(registers), &identity,
        queue_memory, (uintptr_t)0x40000000U, sizeof(queue_memory),
        &queue) == SYNTRA_STATUS_OK);

    assert(queue.ready == 1);
    assert(queue.physical_base == (uintptr_t)0x40000000U);
    assert(queue.queue_size == SYNTRA_VIRTIO_INPUT_EVENT_QUEUE_SIZE);
    assert(queue.used_ring_offset == 4096U);
    assert(queue.event_buffer_offset == 4168U);
    assert(queue.last_used_index == 0U);

    assert(registers[SYNTRA_VIRTIO_MMIO_QUEUE_SEL / sizeof(uint32_t)] == 0U);
    assert(registers[SYNTRA_VIRTIO_MMIO_QUEUE_NUM / sizeof(uint32_t)] ==
           SYNTRA_VIRTIO_INPUT_EVENT_QUEUE_SIZE);
    assert(registers[SYNTRA_VIRTIO_MMIO_QUEUE_ALIGN / sizeof(uint32_t)] == 4096U);
    assert(registers[SYNTRA_VIRTIO_MMIO_QUEUE_PFN / sizeof(uint32_t)] ==
           0x40000U);

    /* First descriptor points at its writable event buffer. */
    assert(queue_memory[0] == 0x48U);
    assert(queue_memory[1] == 0x10U);
    assert(queue_memory[2] == 0x00U);
    assert(queue_memory[3] == 0x40U);
    assert(queue_memory[4] == 0x00U);
    assert(queue_memory[5] == 0x00U);
    assert(queue_memory[6] == 0x00U);
    assert(queue_memory[7] == 0x00U);
    assert(queue_memory[8] == SYNTRA_VIRTIO_INPUT_EVENT_SIZE);
    descriptor_flags = (uint32_t)queue_memory[12] |
                       ((uint32_t)queue_memory[13] << 8U);
    assert(descriptor_flags == SYNTRA_VIRTIO_SPLIT_DESC_F_WRITE);

    /* Available ring exposes all eight buffers; used ring starts empty. */
    assert(queue_memory[128U + 2U] ==
           SYNTRA_VIRTIO_INPUT_EVENT_QUEUE_SIZE);
    assert(queue_memory[queue.used_ring_offset + 2U] == 0U);
    assert((registers[SYNTRA_VIRTIO_MMIO_DEVICE_STATUS / sizeof(uint32_t)] &
            SYNTRA_VIRTIO_STATUS_DRIVER_OK) == 0U);

    /* Reject undersized queues without publishing a PFN. */
    registers[SYNTRA_VIRTIO_MMIO_QUEUE_NUM_MAX / sizeof(uint32_t)] = 4U;
    registers[SYNTRA_VIRTIO_MMIO_QUEUE_PFN / sizeof(uint32_t)] = 0U;
    assert(syntra_virtio_mmio_setup_input_event_queue_mapped(
        &device, registers, sizeof(registers), &identity,
        queue_memory, (uintptr_t)0x40000000U, sizeof(queue_memory),
        &queue) == SYNTRA_STATUS_OUT_OF_MEMORY);
    assert(queue.ready == 0);
    assert(registers[SYNTRA_VIRTIO_MMIO_QUEUE_PFN / sizeof(uint32_t)] == 0U);

    registers[SYNTRA_VIRTIO_MMIO_QUEUE_NUM_MAX / sizeof(uint32_t)] =
        SYNTRA_VIRTIO_INPUT_EVENT_QUEUE_SIZE;
    registers[SYNTRA_VIRTIO_MMIO_DEVICE_STATUS / sizeof(uint32_t)] =
        SYNTRA_VIRTIO_STATUS_ACKNOWLEDGE |
        SYNTRA_VIRTIO_STATUS_DRIVER |
        SYNTRA_VIRTIO_STATUS_DRIVER_OK;
    assert(syntra_virtio_mmio_setup_input_event_queue_mapped(
        &device, registers, sizeof(registers), &identity,
        queue_memory, (uintptr_t)0x40000000U, sizeof(queue_memory),
        &queue) == SYNTRA_STATUS_PERMISSION_DENIED);
}


static void test_store_le16(uint8_t *memory, size_t offset, uint16_t value)
{
    memory[offset] = (uint8_t)value;
    memory[offset + 1U] = (uint8_t)(value >> 8U);
}

static void test_store_le32(uint8_t *memory, size_t offset, uint32_t value)
{
    memory[offset] = (uint8_t)value;
    memory[offset + 1U] = (uint8_t)(value >> 8U);
    memory[offset + 2U] = (uint8_t)(value >> 16U);
    memory[offset + 3U] = (uint8_t)(value >> 24U);
}

static void test_input_event_irq_drain(void)
{
    syntra_virtio_mmio_device_t device = {
        .base = (uintptr_t)0x0A000000U,
        .region_size = 0x200U,
        .reg_index = 0U,
        .interrupt_id = 48U,
        .interrupt_flags = 1U,
        .interrupt_valid = 1,
        .discovered = 1
    };
    _Alignas(4) volatile uint32_t registers[0x200U / sizeof(uint32_t)] = {0};
    static _Alignas(4096) uint8_t queue_memory[8192];
    syntra_virtio_mmio_identity_t identity = {
        SYNTRA_VIRTIO_MMIO_VERSION_LEGACY,
        SYNTRA_VIRTIO_MMIO_DEVICE_ID_INPUT,
        0x554D4551U,
        SYNTRA_VIRTIO_MMIO_FUNCTION_INPUT
    };
    syntra_virtio_input_queue_t queue;
    syntra_input_state_t input_state;
    uint8_t *memory = queue_memory;
    size_t used_element;

    registers[SYNTRA_VIRTIO_MMIO_DEVICE_ID / sizeof(uint32_t)] =
        SYNTRA_VIRTIO_MMIO_DEVICE_ID_INPUT;
    registers[SYNTRA_VIRTIO_MMIO_DEVICE_STATUS / sizeof(uint32_t)] =
        SYNTRA_VIRTIO_STATUS_ACKNOWLEDGE | SYNTRA_VIRTIO_STATUS_DRIVER;
    registers[SYNTRA_VIRTIO_MMIO_QUEUE_NUM_MAX / sizeof(uint32_t)] =
        SYNTRA_VIRTIO_INPUT_EVENT_QUEUE_SIZE;

    assert(syntra_virtio_mmio_setup_input_event_queue_mapped(
        &device, registers, sizeof(registers), &identity,
        queue_memory, (uintptr_t)0x40000000U, sizeof(queue_memory),
        &queue) == SYNTRA_STATUS_OK);
    assert(syntra_input_init(&input_state, 640U, 480U) == SYNTRA_STATUS_OK);

    /* Start must reject queue metadata that would expose truncated memory. */
    {
        size_t saved_size = queue.memory_size;
        queue.memory_size = queue.event_buffer_offset;
        assert(syntra_virtio_mmio_start_input_mapped(
            &device, registers, sizeof(registers), &identity, &queue) ==
            SYNTRA_STATUS_INVALID_ARGUMENT);
        queue.memory_size = saved_size;
    }

    assert(syntra_virtio_mmio_start_input_mapped(
        &device, registers, sizeof(registers), &identity, &queue) ==
        SYNTRA_STATUS_OK);
    assert(registers[SYNTRA_VIRTIO_MMIO_DEVICE_STATUS / sizeof(uint32_t)] ==
        (SYNTRA_VIRTIO_STATUS_ACKNOWLEDGE |
         SYNTRA_VIRTIO_STATUS_DRIVER |
         SYNTRA_VIRTIO_STATUS_DRIVER_OK));
    assert(registers[SYNTRA_VIRTIO_MMIO_QUEUE_NOTIFY / sizeof(uint32_t)] == 0U);

    /* Simulate a QEMU keyboard press (KEY_A = 30) in used descriptor 0. */
    test_store_le16(memory, queue.event_buffer_offset, SYNTRA_INPUT_EV_KEY);
    test_store_le16(memory, queue.event_buffer_offset + 2U, 30U);
    test_store_le32(memory, queue.event_buffer_offset + 4U, 1U);
    used_element = queue.used_ring_offset + 4U;
    test_store_le32(memory, used_element, 0U);
    test_store_le32(memory, used_element + 4U, SYNTRA_VIRTIO_INPUT_EVENT_SIZE);
    test_store_le16(memory, queue.used_ring_offset + 2U, 1U);
    registers[SYNTRA_VIRTIO_MMIO_INTERRUPT_STATUS / sizeof(uint32_t)] = 1U;

    assert(syntra_virtio_mmio_handle_input_irq(
        &device, registers, sizeof(registers), &queue, &input_state) ==
        SYNTRA_STATUS_OK);
    assert(syntra_input_key_down(&input_state, 30U) == 1);
    assert(queue.key_event_count == 1U);
    assert(queue.key_a_press_count == 1U);
    assert(queue.event_count == 1U);
    assert(queue.last_used_index == 1U);
    assert(memory[128U + 2U] == 9U);
    assert(registers[SYNTRA_VIRTIO_MMIO_INTERRUPT_ACK / sizeof(uint32_t)] == 1U);

    /* Simulate key release in the next used-ring slot. */
    test_store_le32(memory, queue.event_buffer_offset + 4U, 0U);
    used_element += SYNTRA_VIRTIO_SPLIT_USED_ELEMENT_SIZE;
    test_store_le32(memory, used_element, 0U);
    test_store_le32(memory, used_element + 4U, SYNTRA_VIRTIO_INPUT_EVENT_SIZE);
    test_store_le16(memory, queue.used_ring_offset + 2U, 2U);
    registers[SYNTRA_VIRTIO_MMIO_INTERRUPT_STATUS / sizeof(uint32_t)] = 1U;

    assert(syntra_virtio_mmio_handle_input_irq(
        &device, registers, sizeof(registers), &queue, &input_state) ==
        SYNTRA_STATUS_OK);
    assert(syntra_input_key_down(&input_state, 30U) == 0);
    assert(queue.key_event_count == 2U);
    assert(queue.key_a_press_count == 1U);
    assert(queue.event_count == 2U);
    assert(queue.last_used_index == 2U);

    /* FIFO preserves press/release ordering even when the consumer is delayed. */
    {
        syntra_input_event_t queued;
        int available = 0;
        assert(queue.software_event_head == 2U);
        assert(queue.software_event_tail == 0U);
        assert(syntra_virtio_mmio_pop_input_event(&queue, &queued, &available) ==
               SYNTRA_STATUS_OK);
        assert(available == 1);
        assert(queued.type == SYNTRA_INPUT_EV_KEY &&
               queued.code == 30U && queued.value == 1);
        assert(syntra_virtio_mmio_pop_input_event(&queue, &queued) ==
               SYNTRA_STATUS_OK);
        assert(queued.type == SYNTRA_INPUT_EV_KEY &&
               queued.code == 30U && queued.value == 0);
        assert(syntra_virtio_mmio_pop_input_event(&queue, &queued, &available) ==
               SYNTRA_STATUS_OK);
        assert(available == 0);
    }

    /* Reject a corrupted queue boundary before reading ring memory. */
    {
        size_t saved_offset = queue.event_buffer_offset;
        queue.event_buffer_offset = queue.memory_size - 1U;
        assert(syntra_virtio_mmio_handle_input_irq(
            &device, registers, sizeof(registers), &queue, &input_state) ==
            SYNTRA_STATUS_INVALID_ARGUMENT);
        queue.event_buffer_offset = saved_offset;
    }

    /* Invalid event lengths are dropped but descriptors are recycled. */
    used_element += SYNTRA_VIRTIO_SPLIT_USED_ELEMENT_SIZE;
    test_store_le32(memory, used_element, 1U);
    test_store_le32(memory, used_element + 4U, 4U);
    test_store_le16(memory, queue.used_ring_offset + 2U, 3U);
    registers[SYNTRA_VIRTIO_MMIO_INTERRUPT_STATUS / sizeof(uint32_t)] = 1U;
    assert(syntra_virtio_mmio_handle_input_irq(
        &device, registers, sizeof(registers), &queue, &input_state) ==
        SYNTRA_STATUS_OK);
    assert(queue.dropped_event_count == 1U);
    assert(queue.event_count == 2U);
    assert(queue.last_used_index == 3U);
}

int main(void)
{
    test_metadata_validation();
    test_mapped_identity_read();
    test_identity_decode();
    test_input_transport_prepare();
    test_input_event_queue_setup();
    test_input_event_irq_drain();
    puts("SyntraOS VirtIO-MMIO mapped identity tests passed.");
    return 0;
}
