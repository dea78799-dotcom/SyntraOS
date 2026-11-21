#include "virtio_mmio.h"

#include <stddef.h>
#include "../input/input.h"


static uint32_t virtio_mmio_read_be32(const uint8_t *data)
{
    return ((uint32_t)data[0] << 24U) |
           ((uint32_t)data[1] << 16U) |
           ((uint32_t)data[2] << 8U) |
           (uint32_t)data[3];
}

static syntra_status_t virtio_mmio_discover_interrupt(
    const syntra_fdt_t *fdt,
    const syntra_fdt_node_t *device_node,
    uint32_t *interrupt_id,
    uint32_t *interrupt_flags)
{
    const uint8_t *data;
    uint32_t data_size;
    uint32_t parent_phandle;
    uint32_t interrupt_cells;
    uint32_t type;
    uint32_t number;
    syntra_fdt_node_t controller;
    syntra_status_t status;

    status = syntra_fdt_get_inherited_property(
        fdt, device_node, "interrupt-parent", &data, &data_size);
    if (status != SYNTRA_STATUS_OK || data_size != 4U)
        return SYNTRA_STATUS_NOT_IMPLEMENTED;
    parent_phandle = virtio_mmio_read_be32(data);
    status = syntra_fdt_find_phandle(fdt, parent_phandle, &controller);
    if (status != SYNTRA_STATUS_OK) return status;

    status = syntra_fdt_get_property(
        fdt, &controller, "#interrupt-cells", &data, &data_size);
    if (status != SYNTRA_STATUS_OK || data_size != 4U)
        return SYNTRA_STATUS_ERROR;
    interrupt_cells = virtio_mmio_read_be32(data);
    if (interrupt_cells != 3U)
        return SYNTRA_STATUS_NOT_IMPLEMENTED;

    status = syntra_fdt_get_property(
        fdt, device_node, "interrupts", &data, &data_size);
    if (status != SYNTRA_STATUS_OK || data_size < 12U ||
        (data_size % 12U) != 0U)
        return SYNTRA_STATUS_ERROR;

    type = virtio_mmio_read_be32(data);
    number = virtio_mmio_read_be32(data + 4U);
    *interrupt_flags = virtio_mmio_read_be32(data + 8U);

    /* GIC binding: type 0 = SPI, type 1 = PPI. */
    if (type == 0U)
    {
        if (number > 987U) return SYNTRA_STATUS_INVALID_ARGUMENT;
        *interrupt_id = 32U + number;
    }
    else if (type == 1U)
    {
        if (number > 15U) return SYNTRA_STATUS_INVALID_ARGUMENT;
        *interrupt_id = 16U + number;
    }
    else
    {
        return SYNTRA_STATUS_NOT_IMPLEMENTED;
    }

    if (*interrupt_flags != 1U && *interrupt_flags != 2U &&
        *interrupt_flags != 4U && *interrupt_flags != 8U)
        return SYNTRA_STATUS_INVALID_ARGUMENT;

    return SYNTRA_STATUS_OK;
}

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
    uint32_t interrupt_id = 0U;
    uint32_t interrupt_flags = 0U;
    syntra_status_t status;

    if (fdt == NULL || device == NULL)
        return SYNTRA_STATUS_INVALID_ARGUMENT;

    device->base = 0U;
    device->region_size = 0U;
    device->reg_index = 0U;
    device->interrupt_id = 0U;
    device->interrupt_flags = 0U;
    device->interrupt_valid = 0;
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

    status = virtio_mmio_discover_interrupt(
        fdt, &node, &interrupt_id, &interrupt_flags);
    if (status == SYNTRA_STATUS_OK)
    {
        device->interrupt_id = interrupt_id;
        device->interrupt_flags = interrupt_flags;
        device->interrupt_valid = 1;
    }

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

    /* Only the modern transport exposes the upper 32-bit feature word. */
    if (identity->version == SYNTRA_VIRTIO_MMIO_VERSION_MODERN)
    {
        status = virtio_mmio_mapped_write32(
            device, mapped_base, mapped_size,
            SYNTRA_VIRTIO_MMIO_DRIVER_FEATURES_SEL, 1U);
        if (status != SYNTRA_STATUS_OK) return status;
        status = virtio_mmio_mapped_write32(
            device, mapped_base, mapped_size,
            SYNTRA_VIRTIO_MMIO_DRIVER_FEATURES, 0U);
        if (status != SYNTRA_STATUS_OK) return status;
    }

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


static size_t virtio_mmio_align_up(size_t value, size_t alignment)
{
    size_t remainder = value % alignment;
    return remainder == 0U ? value : value + (alignment - remainder);
}

static void virtio_mmio_store_le16(uint8_t *memory, size_t offset, uint16_t value)
{
    memory[offset] = (uint8_t)value;
    memory[offset + 1U] = (uint8_t)(value >> 8U);
}

static void virtio_mmio_store_le32(uint8_t *memory, size_t offset, uint32_t value)
{
    memory[offset] = (uint8_t)value;
    memory[offset + 1U] = (uint8_t)(value >> 8U);
    memory[offset + 2U] = (uint8_t)(value >> 16U);
    memory[offset + 3U] = (uint8_t)(value >> 24U);
}

static void virtio_mmio_store_le64(uint8_t *memory, size_t offset, uint64_t value)
{
    virtio_mmio_store_le32(memory, offset, (uint32_t)value);
    virtio_mmio_store_le32(memory, offset + 4U, (uint32_t)(value >> 32U));
}

syntra_status_t syntra_virtio_mmio_setup_input_event_queue_mapped(
    const syntra_virtio_mmio_device_t *device,
    volatile void *mapped_base,
    size_t mapped_size,
    const syntra_virtio_mmio_identity_t *identity,
    void *queue_memory,
    uintptr_t queue_physical_base,
    size_t queue_memory_size,
    syntra_virtio_input_queue_t *queue)
{
    const size_t queue_size = SYNTRA_VIRTIO_INPUT_EVENT_QUEUE_SIZE;
    const size_t descriptor_bytes =
        queue_size * SYNTRA_VIRTIO_SPLIT_DESC_SIZE;
    const size_t available_bytes = 4U + queue_size * sizeof(uint16_t);
    const size_t used_bytes =
        4U + queue_size * SYNTRA_VIRTIO_SPLIT_USED_ELEMENT_SIZE;
    const size_t available_offset = descriptor_bytes;
    const size_t used_offset =
        virtio_mmio_align_up(available_offset + available_bytes, 4096U);
    const size_t event_offset =
        virtio_mmio_align_up(used_offset + used_bytes, 8U);
    const size_t required_bytes =
        event_offset + queue_size * SYNTRA_VIRTIO_INPUT_EVENT_SIZE;
    uint32_t queue_num_max;
    uint32_t status_value;
    uint8_t *memory;
    size_t index;
    syntra_status_t status;

    if (queue == NULL) return SYNTRA_STATUS_INVALID_ARGUMENT;
    queue->physical_base = 0U;
    queue->mapped_memory = NULL;
    queue->memory_size = 0U;
    queue->used_ring_offset = 0U;
    queue->event_buffer_offset = 0U;
    queue->queue_size = 0U;
    queue->last_used_index = 0U;
    queue->software_event_head = 0U;
    queue->software_event_tail = 0U;
    queue->software_event_dropped = 0U;
    queue->event_count = 0U;
    queue->key_event_count = 0U;
    queue->key_a_press_count = 0U;
    queue->dropped_event_count = 0U;
    queue->irq_error_count = 0U;
    queue->ready = 0;

    if (syntra_virtio_mmio_validate(device) != SYNTRA_STATUS_OK ||
        mapped_base == NULL || mapped_size < SYNTRA_VIRTIO_MMIO_QUEUE_REGISTERS_SIZE ||
        mapped_size > device->region_size ||
        identity == NULL ||
        identity->function != SYNTRA_VIRTIO_MMIO_FUNCTION_INPUT ||
        identity->device_id != SYNTRA_VIRTIO_MMIO_DEVICE_ID_INPUT ||
        identity->version != SYNTRA_VIRTIO_MMIO_VERSION_LEGACY ||
        queue_memory == NULL || queue_memory_size < required_bytes ||
        queue_physical_base == 0U ||
        (queue_physical_base & 0xFFFU) != 0U ||
        queue_physical_base > UINT32_MAX ||
        required_bytes == 0U ||
        queue_physical_base > (uintptr_t)UINT32_MAX - (required_bytes - 1U) ||
        (((uintptr_t)queue_memory) & 0xFU) != 0U)
        return SYNTRA_STATUS_INVALID_ARGUMENT;

    status = virtio_mmio_mapped_read32(
        device, mapped_base, mapped_size,
        SYNTRA_VIRTIO_MMIO_DEVICE_STATUS, &status_value);
    if (status != SYNTRA_STATUS_OK ||
        (status_value & (SYNTRA_VIRTIO_STATUS_ACKNOWLEDGE |
                         SYNTRA_VIRTIO_STATUS_DRIVER)) !=
        (SYNTRA_VIRTIO_STATUS_ACKNOWLEDGE |
         SYNTRA_VIRTIO_STATUS_DRIVER) ||
        (status_value & SYNTRA_VIRTIO_STATUS_DRIVER_OK) != 0U)
        return SYNTRA_STATUS_PERMISSION_DENIED;

    status = virtio_mmio_mapped_write32(
        device, mapped_base, mapped_size, SYNTRA_VIRTIO_MMIO_QUEUE_SEL, 0U);
    if (status != SYNTRA_STATUS_OK) return status;
    status = virtio_mmio_mapped_read32(
        device, mapped_base, mapped_size,
        SYNTRA_VIRTIO_MMIO_QUEUE_NUM_MAX, &queue_num_max);
    if (status != SYNTRA_STATUS_OK) return status;
    if (queue_num_max < queue_size) return SYNTRA_STATUS_OUT_OF_MEMORY;

    memory = (uint8_t *)queue_memory;
    for (index = 0U; index < required_bytes; ++index) memory[index] = 0U;

    /* Split ring descriptor table: one writable 8-byte event buffer per slot. */
    for (index = 0U; index < queue_size; ++index)
    {
        size_t descriptor_offset = index * SYNTRA_VIRTIO_SPLIT_DESC_SIZE;
        uintptr_t event_physical = queue_physical_base +
            (uintptr_t)event_offset +
            (uintptr_t)(index * SYNTRA_VIRTIO_INPUT_EVENT_SIZE);
        virtio_mmio_store_le64(memory, descriptor_offset, (uint64_t)event_physical);
        virtio_mmio_store_le32(
            memory, descriptor_offset + 8U, SYNTRA_VIRTIO_INPUT_EVENT_SIZE);
        virtio_mmio_store_le16(
            memory, descriptor_offset + 12U, SYNTRA_VIRTIO_SPLIT_DESC_F_WRITE);
        virtio_mmio_store_le16(memory, descriptor_offset + 14U, 0U);

        /* Available ring begins after the descriptor table. */
        virtio_mmio_store_le16(
            memory, available_offset + 4U + index * 2U, (uint16_t)index);
    }

    virtio_mmio_store_le16(memory, available_offset, 0U);
    virtio_mmio_store_le16(memory, available_offset + 2U, (uint16_t)queue_size);
    virtio_mmio_store_le16(memory, used_offset, 0U);
    virtio_mmio_store_le16(memory, used_offset + 2U, 0U);

#if defined(__aarch64__)
    __asm__ volatile("dmb oshst" ::: "memory");
#endif

    status = virtio_mmio_mapped_write32(
        device, mapped_base, mapped_size, SYNTRA_VIRTIO_MMIO_QUEUE_NUM,
        (uint32_t)queue_size);
    if (status != SYNTRA_STATUS_OK) return status;
    status = virtio_mmio_mapped_write32(
        device, mapped_base, mapped_size, SYNTRA_VIRTIO_MMIO_QUEUE_ALIGN,
        4096U);
    if (status != SYNTRA_STATUS_OK) return status;

    /* QueuePFN is the publication point and must be written last. */
    status = virtio_mmio_mapped_write32(
        device, mapped_base, mapped_size, SYNTRA_VIRTIO_MMIO_QUEUE_PFN,
        (uint32_t)(queue_physical_base >> 12U));
    if (status != SYNTRA_STATUS_OK) return status;

    queue->physical_base = queue_physical_base;
    queue->mapped_memory = queue_memory;
    queue->memory_size = queue_memory_size;
    queue->used_ring_offset = used_offset;
    queue->event_buffer_offset = event_offset;
    queue->queue_size = (uint16_t)queue_size;
    queue->last_used_index = 0U;
    queue->ready = 1;
    return SYNTRA_STATUS_OK;
}


static uint16_t virtio_mmio_load_volatile_le16(
    const volatile uint8_t *memory, size_t offset)
{
    return (uint16_t)((uint16_t)memory[offset] |
                      ((uint16_t)memory[offset + 1U] << 8U));
}

static uint32_t virtio_mmio_load_volatile_le32(
    const volatile uint8_t *memory, size_t offset)
{
    return (uint32_t)memory[offset] |
           ((uint32_t)memory[offset + 1U] << 8U) |
           ((uint32_t)memory[offset + 2U] << 16U) |
           ((uint32_t)memory[offset + 3U] << 24U);
}

static void virtio_mmio_store_volatile_le16(
    volatile uint8_t *memory, size_t offset, uint16_t value)
{
    memory[offset] = (uint8_t)value;
    memory[offset + 1U] = (uint8_t)(value >> 8U);
}

syntra_status_t syntra_virtio_mmio_start_input_mapped(
    const syntra_virtio_mmio_device_t *device,
    volatile void *mapped_base,
    size_t mapped_size,
    const syntra_virtio_mmio_identity_t *identity,
    const syntra_virtio_input_queue_t *queue)
{
    uint32_t device_status;
    syntra_status_t status;

    if (identity == NULL || queue == NULL || queue->ready != 1 ||
        identity->function != SYNTRA_VIRTIO_MMIO_FUNCTION_INPUT ||
        identity->device_id != SYNTRA_VIRTIO_MMIO_DEVICE_ID_INPUT ||
        identity->version != SYNTRA_VIRTIO_MMIO_VERSION_LEGACY ||
        queue->mapped_memory == NULL || queue->physical_base == 0U ||
        queue->queue_size != SYNTRA_VIRTIO_INPUT_EVENT_QUEUE_SIZE ||
        queue->used_ring_offset > queue->memory_size ||
        queue->memory_size - queue->used_ring_offset <
            4U + (size_t)queue->queue_size *
                 SYNTRA_VIRTIO_SPLIT_USED_ELEMENT_SIZE ||
        queue->event_buffer_offset > queue->memory_size ||
        queue->memory_size - queue->event_buffer_offset <
            (size_t)queue->queue_size * SYNTRA_VIRTIO_INPUT_EVENT_SIZE ||
        mapped_size < SYNTRA_VIRTIO_MMIO_INTERRUPT_REGISTERS_SIZE)
        return SYNTRA_STATUS_INVALID_ARGUMENT;

    status = virtio_mmio_mapped_read32(
        device, mapped_base, mapped_size,
        SYNTRA_VIRTIO_MMIO_DEVICE_STATUS, &device_status);
    if (status != SYNTRA_STATUS_OK) return status;
    if ((device_status & (SYNTRA_VIRTIO_STATUS_ACKNOWLEDGE |
                          SYNTRA_VIRTIO_STATUS_DRIVER)) !=
        (SYNTRA_VIRTIO_STATUS_ACKNOWLEDGE |
         SYNTRA_VIRTIO_STATUS_DRIVER) ||
        (device_status & SYNTRA_VIRTIO_STATUS_DRIVER_OK) != 0U)
        return SYNTRA_STATUS_PERMISSION_DENIED;

    status = virtio_mmio_mapped_write32(
        device, mapped_base, mapped_size,
        SYNTRA_VIRTIO_MMIO_DEVICE_STATUS,
        device_status | SYNTRA_VIRTIO_STATUS_DRIVER_OK);
    if (status != SYNTRA_STATUS_OK) return status;

    /* Queue 0 is the event queue. Notify only after DRIVER_OK is visible. */
    return virtio_mmio_mapped_write32(
        device, mapped_base, mapped_size,
        SYNTRA_VIRTIO_MMIO_QUEUE_NOTIFY, 0U);
}

syntra_status_t syntra_virtio_mmio_pop_input_event(
    syntra_virtio_input_queue_t *queue,
    syntra_input_event_t *event,
    int *available)
{
    uint32_t tail;
    uint32_t head;

    if (queue == NULL || event == NULL || available == NULL ||
        queue->ready != 1)
        return SYNTRA_STATUS_INVALID_ARGUMENT;

    *available = 0;

    tail = queue->software_event_tail;
#if defined(__aarch64__)
    __asm__ volatile("dmb oshld" ::: "memory");
#else
    __asm__ volatile("" ::: "memory");
#endif
    head = queue->software_event_head;
    if (tail == head) return SYNTRA_STATUS_OK;

    *event = queue->software_events[
        tail % SYNTRA_VIRTIO_INPUT_SOFTWARE_EVENT_QUEUE_SIZE];
#if defined(__aarch64__)
    __asm__ volatile("dmb oshst" ::: "memory");
#else
    __asm__ volatile("" ::: "memory");
#endif
    queue->software_event_tail = tail + 1U;
    *available = 1;
    return SYNTRA_STATUS_OK;
}

syntra_status_t syntra_virtio_mmio_handle_input_irq(
    const syntra_virtio_mmio_device_t *device,
    volatile void *mapped_base,
    size_t mapped_size,
    syntra_virtio_input_queue_t *queue,
    syntra_input_state_t *input_state)
{
    uint32_t interrupt_status;
    uint16_t used_index;
    uint16_t pending;
    uint16_t processed = 0U;
    volatile uint8_t *memory;
    const size_t available_offset =
        SYNTRA_VIRTIO_INPUT_EVENT_QUEUE_SIZE * SYNTRA_VIRTIO_SPLIT_DESC_SIZE;
    syntra_status_t status;

    if (queue == NULL || input_state == NULL || queue->ready != 1 ||
        queue->mapped_memory == NULL || queue->physical_base == 0U ||
        queue->queue_size != SYNTRA_VIRTIO_INPUT_EVENT_QUEUE_SIZE ||
        queue->used_ring_offset > queue->memory_size ||
        queue->memory_size - queue->used_ring_offset <
            4U + (size_t)queue->queue_size *
                 SYNTRA_VIRTIO_SPLIT_USED_ELEMENT_SIZE ||
        queue->event_buffer_offset > queue->memory_size ||
        queue->memory_size - queue->event_buffer_offset <
            (size_t)queue->queue_size * SYNTRA_VIRTIO_INPUT_EVENT_SIZE ||
        mapped_size < SYNTRA_VIRTIO_MMIO_INTERRUPT_REGISTERS_SIZE)
        return SYNTRA_STATUS_INVALID_ARGUMENT;

    status = virtio_mmio_mapped_read32(
        device, mapped_base, mapped_size,
        SYNTRA_VIRTIO_MMIO_INTERRUPT_STATUS, &interrupt_status);
    if (status != SYNTRA_STATUS_OK) return status;
    if (interrupt_status == 0U) return SYNTRA_STATUS_OK;

    status = virtio_mmio_mapped_write32(
        device, mapped_base, mapped_size,
        SYNTRA_VIRTIO_MMIO_INTERRUPT_ACK, interrupt_status);
    if (status != SYNTRA_STATUS_OK) return status;

    if ((interrupt_status & 1U) == 0U) return SYNTRA_STATUS_OK;

    memory = (volatile uint8_t *)queue->mapped_memory;
#if defined(__aarch64__)
    __asm__ volatile("dmb oshld" ::: "memory");
#endif
    used_index = virtio_mmio_load_volatile_le16(
        memory, queue->used_ring_offset + 2U);
    pending = (uint16_t)(used_index - queue->last_used_index);
    if (pending > queue->queue_size)
    {
        queue->irq_error_count++;
        return SYNTRA_STATUS_ERROR;
    }

    while (queue->last_used_index != used_index &&
           processed < queue->queue_size)
    {
        size_t used_element_offset =
            queue->used_ring_offset + 4U +
            ((size_t)(queue->last_used_index % queue->queue_size) *
             SYNTRA_VIRTIO_SPLIT_USED_ELEMENT_SIZE);
        uint32_t descriptor_id =
            virtio_mmio_load_volatile_le32(memory, used_element_offset);
        uint32_t event_length =
            virtio_mmio_load_volatile_le32(memory, used_element_offset + 4U);
        size_t event_offset;
        syntra_status_t event_status = SYNTRA_STATUS_OK;

        if (descriptor_id >= queue->queue_size)
        {
            queue->irq_error_count++;
            return SYNTRA_STATUS_ERROR;
        }

        event_offset = queue->event_buffer_offset +
            ((size_t)descriptor_id * SYNTRA_VIRTIO_INPUT_EVENT_SIZE);

        if (event_length != SYNTRA_VIRTIO_INPUT_EVENT_SIZE)
        {
            queue->dropped_event_count++;
        }
        else
        {
            syntra_input_event_t event;
            event.type = virtio_mmio_load_volatile_le16(memory, event_offset);
            event.code = virtio_mmio_load_volatile_le16(memory, event_offset + 2U);
            event.value = (int32_t)virtio_mmio_load_volatile_le32(
                memory, event_offset + 4U);
            event_status = syntra_input_process(input_state, &event);
            if (event_status == SYNTRA_STATUS_OK)
            {
                uint32_t head = queue->software_event_head;
                uint32_t tail = queue->software_event_tail;
                if ((uint32_t)(head - tail) <
                    SYNTRA_VIRTIO_INPUT_SOFTWARE_EVENT_QUEUE_SIZE)
                {
                    queue->software_events[
                        head % SYNTRA_VIRTIO_INPUT_SOFTWARE_EVENT_QUEUE_SIZE] =
                        event;
#if defined(__aarch64__)
                    __asm__ volatile("dmb oshst" ::: "memory");
#else
                    __asm__ volatile("" ::: "memory");
#endif
                    queue->software_event_head = head + 1U;
                }
                else if (queue->software_event_dropped < UINT32_MAX)
                {
                    queue->software_event_dropped++;
                }

                if (queue->event_count < UINT32_MAX) queue->event_count++;
                if (event.type == SYNTRA_INPUT_EV_KEY)
                {
                    if (queue->key_event_count < UINT32_MAX)
                        queue->key_event_count++;
                    if (event.code == 30U && event.value == 1 &&
                        queue->key_a_press_count < UINT32_MAX)
                        queue->key_a_press_count++;
                }
            }
            else if (queue->dropped_event_count < UINT32_MAX)
            {
                queue->dropped_event_count++;
            }
        }

        /* Recycle the completed descriptor into the available ring. */
        {
            uint16_t available_index = virtio_mmio_load_volatile_le16(
                memory, available_offset + 2U);
            size_t ring_offset = available_offset + 4U +
                ((size_t)(available_index % queue->queue_size) * 2U);
            virtio_mmio_store_volatile_le16(
                memory, ring_offset, (uint16_t)descriptor_id);
#if defined(__aarch64__)
            __asm__ volatile("dmb oshst" ::: "memory");
#endif
            virtio_mmio_store_volatile_le16(
                memory, available_offset + 2U,
                (uint16_t)(available_index + 1U));
        }

        queue->last_used_index++;
        processed++;
    }

#if defined(__aarch64__)
    __asm__ volatile("dmb oshst" ::: "memory");
#endif
    if (processed != 0U)
    {
        status = virtio_mmio_mapped_write32(
            device, mapped_base, mapped_size,
            SYNTRA_VIRTIO_MMIO_QUEUE_NOTIFY, 0U);
        if (status != SYNTRA_STATUS_OK) return status;
    }

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
