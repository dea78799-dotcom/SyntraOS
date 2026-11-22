#ifndef SYNTRA_VIRTIO_MMIO_H
#define SYNTRA_VIRTIO_MMIO_H

#include <stddef.h>
#include <stdint.h>

#include "../core/status.h"
#include "../arch/arm64/boot/fdt.h"
#include "../input/input.h"

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
#define SYNTRA_VIRTIO_MMIO_QUEUE_SEL              0x030U
#define SYNTRA_VIRTIO_MMIO_QUEUE_NUM_MAX          0x034U
#define SYNTRA_VIRTIO_MMIO_QUEUE_NUM              0x038U
#define SYNTRA_VIRTIO_MMIO_QUEUE_ALIGN            0x03CU
#define SYNTRA_VIRTIO_MMIO_QUEUE_PFN              0x040U
#define SYNTRA_VIRTIO_MMIO_QUEUE_REGISTERS_SIZE   0x044U
#define SYNTRA_VIRTIO_MMIO_QUEUE_NOTIFY            0x050U
#define SYNTRA_VIRTIO_MMIO_INTERRUPT_STATUS       0x060U
#define SYNTRA_VIRTIO_MMIO_INTERRUPT_ACK          0x064U
#define SYNTRA_VIRTIO_MMIO_INTERRUPT_REGISTERS_SIZE 0x068U

/* VirtIO Input event-type feature bits (device feature word 0). */
#define SYNTRA_VIRTIO_INPUT_FEATURE_EV_SYN (1U << 0)
#define SYNTRA_VIRTIO_INPUT_FEATURE_EV_KEY (1U << 1)

#define SYNTRA_VIRTIO_INPUT_EVENT_QUEUE_SIZE 8U
#define SYNTRA_VIRTIO_INPUT_SOFTWARE_EVENT_QUEUE_SIZE 64U
#define SYNTRA_VIRTIO_INPUT_EVENT_SIZE 8U
#define SYNTRA_VIRTIO_SPLIT_DESC_SIZE 16U
#define SYNTRA_VIRTIO_SPLIT_USED_ELEMENT_SIZE 8U
#define SYNTRA_VIRTIO_SPLIT_DESC_F_WRITE 2U

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
    uint32_t interrupt_id;
    uint32_t interrupt_flags;
    int interrupt_valid;
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

typedef struct
{
    uintptr_t physical_base;
    volatile void *mapped_memory;
    size_t memory_size;
    size_t used_ring_offset;
    size_t event_buffer_offset;
    uint16_t queue_size;
    uint16_t last_used_index;
    volatile uint32_t event_count;
    volatile uint32_t key_event_count;
    volatile uint32_t key_a_press_count;
    volatile uint32_t dropped_event_count;
    volatile uint32_t irq_error_count;
    syntra_input_event_t software_events[SYNTRA_VIRTIO_INPUT_SOFTWARE_EVENT_QUEUE_SIZE];
    volatile uint32_t software_event_head;
    volatile uint32_t software_event_tail;
    volatile uint32_t software_event_dropped;
    int ready;
} syntra_virtio_input_queue_t;

syntra_status_t syntra_virtio_mmio_prepare_input_mapped(
    const syntra_virtio_mmio_device_t *device,
    volatile void *mapped_base,
    size_t mapped_size,
    const syntra_virtio_mmio_identity_t *identity
);


/*
 * Configure legacy MMIO event queue 0 using caller-supplied contiguous,
 * page-aligned RAM. The queue is published through QueuePFN but the device
 * is deliberately not advanced to DRIVER_OK and no IRQ is enabled.
 */
syntra_status_t syntra_virtio_mmio_setup_input_event_queue_mapped(
    const syntra_virtio_mmio_device_t *device,
    volatile void *mapped_base,
    size_t mapped_size,
    const syntra_virtio_mmio_identity_t *identity,
    void *queue_memory,
    uintptr_t queue_physical_base,
    size_t queue_memory_size,
    syntra_virtio_input_queue_t *queue
);


/* Start an already prepared legacy Input queue after its IRQ handler is ready. */
syntra_status_t syntra_virtio_mmio_start_input_mapped(
    const syntra_virtio_mmio_device_t *device,
    volatile void *mapped_base,
    size_t mapped_size,
    const syntra_virtio_mmio_identity_t *identity,
    const syntra_virtio_input_queue_t *queue
);

/*
 * SPSC event FIFO: IRQ handler is the producer, normal context is the consumer.
 * Events are copied and published in order; queue overflow drops newest events.
 */
syntra_status_t syntra_virtio_mmio_pop_input_event(
    syntra_virtio_input_queue_t *queue,
    syntra_input_event_t *event,
    int *available
);

/* Acknowledge a VirtIO Input interrupt, drain used entries, and recycle buffers. */
syntra_status_t syntra_virtio_mmio_handle_input_irq(
    const syntra_virtio_mmio_device_t *device,
    volatile void *mapped_base,
    size_t mapped_size,
    syntra_virtio_input_queue_t *queue,
    syntra_input_state_t *input_state
);

/* Decode a copied snapshot; no MMIO access or device mutation occurs here. */
syntra_status_t syntra_virtio_mmio_decode_identity(
    const syntra_virtio_mmio_register_snapshot_t *snapshot,
    syntra_virtio_mmio_identity_t *identity
);

#endif
