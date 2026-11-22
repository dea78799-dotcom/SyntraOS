#include "ramfb.h"

#include <stdint.h>
#include <stddef.h>

#include "desktop.h"
#include "framebuffer.h"
#include "gui_runtime.h"
#include "uptime_panel.h"
#include "../virtio/virtio_mmio.h"

#define FW_CFG_FILE_DIR 0x0019U
#define FW_CFG_DMA_SELECT 0x00000008U
#define FW_CFG_DMA_READ 0x00000002U
#define FW_CFG_DMA_WRITE 0x00000010U
#define FW_CFG_DMA_ERROR 0x00000001U
#define FW_CFG_DMA_REGISTER_OFFSET 0x10U
#define FW_CFG_DMA_MIN_REGION_SIZE 0x18U
#define FW_CFG_DIRECTORY_HEADER_SIZE 4U
#define FW_CFG_DIRECTORY_ENTRY_SIZE 64U
#define FW_CFG_DIRECTORY_CAPACITY 4096U
#define FW_CFG_DMA_POLL_LIMIT 1000000U

typedef struct __attribute__((packed, aligned(16)))
{
    volatile uint32_t control_be;
    uint32_t length_be;
    uint64_t address_be;
} syntra_fw_cfg_dma_access_t;

/* QEMU marks the fw_cfg device dma-coherent in its ARM virt device tree. */
static syntra_fw_cfg_dma_access_t fw_cfg_dma_access
    __attribute__((aligned(16)));
static uint8_t fw_cfg_directory[FW_CFG_DIRECTORY_CAPACITY]
    __attribute__((aligned(16)));
static uint8_t ramfb_config[SYNTRA_RAMFB_CONFIG_SIZE]
    __attribute__((aligned(16)));
static uint32_t ramfb_pixels[SYNTRA_RAMFB_WIDTH * SYNTRA_RAMFB_HEIGHT]
    __attribute__((aligned(4096)));
static syntra_gui_runtime_t ramfb_gui_runtime;
static int ramfb_gui_runtime_ready;
static uint64_t ramfb_boot_counter;
static uint64_t ramfb_counter_frequency;

static uint64_t ramfb_read_counter(void)
{
#if defined(__aarch64__)
    uint64_t value;
    __asm__ volatile("mrs %0, cntpct_el0" : "=r"(value));
    return value;
#else
    return 0U;
#endif
}

static uint64_t ramfb_read_counter_frequency(void)
{
#if defined(__aarch64__)
    uint64_t value;
    __asm__ volatile("mrs %0, cntfrq_el0" : "=r"(value));
    return value;
#else
    return 0U;
#endif
}

static uint64_t ramfb_uptime_milliseconds(void)
{
    uint64_t now;
    uint64_t elapsed;
    if (ramfb_counter_frequency == 0U) return 0U;
    now = ramfb_read_counter();
    if (now < ramfb_boot_counter) return 0U;
    elapsed = now - ramfb_boot_counter;
    return (elapsed / ramfb_counter_frequency) * 1000ULL +
           ((elapsed % ramfb_counter_frequency) * 1000ULL) /
               ramfb_counter_frequency;
}

static uint64_t ramfb_uptime_seconds(void)
{
    uint64_t now;
    if (ramfb_counter_frequency == 0U) return 0U;
    now = ramfb_read_counter();
    if (now < ramfb_boot_counter) return 0U;
    return (now - ramfb_boot_counter) / ramfb_counter_frequency;
}

static uint16_t read_be16(const uint8_t *p)
{
    return (uint16_t)(((uint16_t)p[0] << 8U) | (uint16_t)p[1]);
}

static uint32_t read_be32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24U) | ((uint32_t)p[1] << 16U) |
           ((uint32_t)p[2] << 8U) | (uint32_t)p[3];
}

static void write_be32(uint8_t *p, uint32_t value)
{
    p[0] = (uint8_t)(value >> 24U);
    p[1] = (uint8_t)(value >> 16U);
    p[2] = (uint8_t)(value >> 8U);
    p[3] = (uint8_t)value;
}

static void write_be64(uint8_t *p, uint64_t value)
{
    write_be32(p, (uint32_t)(value >> 32U));
    write_be32(p + 4U, (uint32_t)value);
}

static uint32_t host_to_be32(uint32_t value)
{
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    return value;
#else
    return ((value & 0x000000FFU) << 24U) |
           ((value & 0x0000FF00U) << 8U) |
           ((value & 0x00FF0000U) >> 8U) |
           ((value & 0xFF000000U) >> 24U);
#endif
}

static uint64_t host_to_be64(uint64_t value)
{
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    return value;
#else
    return ((uint64_t)host_to_be32((uint32_t)value) << 32U) |
           (uint64_t)host_to_be32((uint32_t)(value >> 32U));
#endif
}

static uint32_t be32_to_host(uint32_t value)
{
    return host_to_be32(value);
}

static void syntra_dma_barrier(void)
{
#if defined(__aarch64__)
    __asm__ volatile("dsb sy" ::: "memory");
#else
    __asm__ volatile("" ::: "memory");
#endif
}

syntra_status_t syntra_ramfb_find_file(
    const uint8_t *directory, size_t directory_size,
    const char *name, uint16_t *selector, uint32_t *file_size)
{
    uint32_t count;
    size_t index;
    size_t required;
    size_t name_length;

    if (directory == NULL || name == NULL || selector == NULL ||
        file_size == NULL || directory_size < FW_CFG_DIRECTORY_HEADER_SIZE)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    count = read_be32(directory);
    if ((size_t)count >
        (SIZE_MAX - FW_CFG_DIRECTORY_HEADER_SIZE) / FW_CFG_DIRECTORY_ENTRY_SIZE)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    required = FW_CFG_DIRECTORY_HEADER_SIZE +
               (size_t)count * FW_CFG_DIRECTORY_ENTRY_SIZE;
    if (required > directory_size)
    {
        return SYNTRA_STATUS_ERROR;
    }

    name_length = 0U;
    while (name[name_length] != '\0')
    {
        if (name_length >= 55U) return SYNTRA_STATUS_INVALID_ARGUMENT;
        ++name_length;
    }

    for (index = 0U; index < (size_t)count; ++index)
    {
        const uint8_t *entry = directory + FW_CFG_DIRECTORY_HEADER_SIZE +
                               index * FW_CFG_DIRECTORY_ENTRY_SIZE;
        size_t stored_length = 0U;

        while (stored_length < 56U && entry[8U + stored_length] != 0U)
        {
            ++stored_length;
        }

        if (stored_length == name_length)
        {
            size_t character;
            for (character = 0U; character < name_length; ++character)
            {
                if (entry[8U + character] != (uint8_t)name[character]) break;
            }
            if (character == name_length)
            {
                *file_size = read_be32(entry);
                *selector = read_be16(entry + 4U);
                return SYNTRA_STATUS_OK;
            }
        }
    }

    return SYNTRA_STATUS_NOT_IMPLEMENTED;
}

syntra_status_t syntra_ramfb_encode_config(
    uint8_t output[SYNTRA_RAMFB_CONFIG_SIZE],
    uint64_t framebuffer_address,
    uint32_t width, uint32_t height, uint32_t stride)
{
    if (output == NULL || framebuffer_address == 0U ||
        width < 16U || height < 16U ||
        width > UINT32_MAX / 4U || stride < width * 4U ||
        (stride & 3U) != 0U ||
        (uint64_t)stride * (uint64_t)height > UINT32_MAX)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    write_be64(output, framebuffer_address);
    write_be32(output + 8U, SYNTRA_RAMFB_FOURCC_XRGB8888);
    write_be32(output + 12U, 0U);
    write_be32(output + 16U, width);
    write_be32(output + 20U, height);
    write_be32(output + 24U, stride);
    return SYNTRA_STATUS_OK;
}

static syntra_status_t fw_cfg_dma_transfer(
    uintptr_t fw_cfg_base, uint16_t selector,
    void *buffer, uint32_t length, int write_to_device)
{
    volatile uint64_t *dma_register =
        (volatile uint64_t *)(fw_cfg_base + FW_CFG_DMA_REGISTER_OFFSET);
    uint64_t descriptor_address;
    uint32_t command;
    uint32_t poll;

    if (buffer == NULL || length == 0U ||
        (uintptr_t)buffer > UINTPTR_MAX - (uintptr_t)length)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    command = ((uint32_t)selector << 16U) | FW_CFG_DMA_SELECT |
              (write_to_device ? FW_CFG_DMA_WRITE : FW_CFG_DMA_READ);
    fw_cfg_dma_access.control_be = host_to_be32(command);
    fw_cfg_dma_access.length_be = host_to_be32(length);
    fw_cfg_dma_access.address_be =
        host_to_be64((uint64_t)(uintptr_t)buffer);

    descriptor_address = (uint64_t)(uintptr_t)&fw_cfg_dma_access;
    syntra_dma_barrier();
    *dma_register = host_to_be64(descriptor_address);
    syntra_dma_barrier();

    for (poll = 0U; poll < FW_CFG_DMA_POLL_LIMIT; ++poll)
    {
        uint32_t control = be32_to_host(fw_cfg_dma_access.control_be);
        if (control == 0U) return SYNTRA_STATUS_OK;
        if ((control & FW_CFG_DMA_ERROR) != 0U) return SYNTRA_STATUS_ERROR;
    }

    return SYNTRA_STATUS_ERROR;
}

static syntra_status_t fw_cfg_read_directory(
    uintptr_t base, uint16_t *ramfb_selector, uint32_t *ramfb_file_size)
{
    uint8_t header[FW_CFG_DIRECTORY_HEADER_SIZE];
    uint32_t count;
    size_t directory_size;
    syntra_status_t status;

    status = fw_cfg_dma_transfer(base, FW_CFG_FILE_DIR, header,
                                 (uint32_t)sizeof(header), 0);
    if (status != SYNTRA_STATUS_OK) return status;

    count = read_be32(header);
    if ((size_t)count >
        (FW_CFG_DIRECTORY_CAPACITY - FW_CFG_DIRECTORY_HEADER_SIZE) /
            FW_CFG_DIRECTORY_ENTRY_SIZE)
    {
        return SYNTRA_STATUS_NOT_IMPLEMENTED;
    }

    directory_size = FW_CFG_DIRECTORY_HEADER_SIZE +
                     (size_t)count * FW_CFG_DIRECTORY_ENTRY_SIZE;
    status = fw_cfg_dma_transfer(base, FW_CFG_FILE_DIR, fw_cfg_directory,
                                 (uint32_t)directory_size, 0);
    if (status != SYNTRA_STATUS_OK) return status;

    return syntra_ramfb_find_file(
        fw_cfg_directory, directory_size, "etc/ramfb",
        ramfb_selector, ramfb_file_size);
}

syntra_status_t syntra_ramfb_init(uintptr_t fw_cfg_base, size_t fw_cfg_size)
{
    syntra_framebuffer_t framebuffer;
    uint16_t ramfb_selector;
    uint32_t ramfb_file_size;
    syntra_status_t status;

    if (fw_cfg_base == 0U || fw_cfg_size < FW_CFG_DMA_REGISTER_OFFSET + 8U)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    status = fw_cfg_read_directory(
        fw_cfg_base, &ramfb_selector, &ramfb_file_size);
    if (status != SYNTRA_STATUS_OK) return status;

    if (ramfb_file_size != SYNTRA_RAMFB_CONFIG_SIZE)
    {
        return SYNTRA_STATUS_ERROR;
    }

    framebuffer.pixels = ramfb_pixels;
    framebuffer.width = SYNTRA_RAMFB_WIDTH;
    framebuffer.height = SYNTRA_RAMFB_HEIGHT;
    framebuffer.stride_pixels = SYNTRA_RAMFB_WIDTH;

    ramfb_counter_frequency = ramfb_read_counter_frequency();
    ramfb_boot_counter = ramfb_read_counter();

    status = syntra_gui_runtime_init(
        &ramfb_gui_runtime, SYNTRA_RAMFB_WIDTH, SYNTRA_RAMFB_HEIGHT);
    if (status != SYNTRA_STATUS_OK) return status;
    status = syntra_gui_runtime_tick(
        &ramfb_gui_runtime, ramfb_uptime_milliseconds());
    if (status != SYNTRA_STATUS_OK) return status;
    status = syntra_gui_runtime_render(&ramfb_gui_runtime, &framebuffer);
    if (status != SYNTRA_STATUS_OK) return status;
    status = syntra_gui_render_uptime_panel(&framebuffer, ramfb_uptime_seconds());
    if (status != SYNTRA_STATUS_OK) return status;
    ramfb_gui_runtime_ready = 1;

    status = syntra_ramfb_encode_config(
        ramfb_config, (uint64_t)(uintptr_t)ramfb_pixels,
        SYNTRA_RAMFB_WIDTH, SYNTRA_RAMFB_HEIGHT, SYNTRA_RAMFB_STRIDE);
    if (status != SYNTRA_STATUS_OK) return status;

    syntra_dma_barrier();
    status = fw_cfg_dma_transfer(
        fw_cfg_base, ramfb_selector, ramfb_config,
        SYNTRA_RAMFB_CONFIG_SIZE, 1);
    syntra_dma_barrier();
    return status;
}

syntra_status_t syntra_ramfb_tick_gestures(void)
{
    if (!ramfb_gui_runtime_ready)
        return SYNTRA_STATUS_ERROR;

    return syntra_gui_runtime_tick(
        &ramfb_gui_runtime, ramfb_uptime_milliseconds());
}

syntra_status_t syntra_ramfb_refresh(void)
{
    syntra_framebuffer_t framebuffer;
    syntra_status_t status;

    if (!ramfb_gui_runtime_ready)
        return SYNTRA_STATUS_ERROR;

    framebuffer.pixels = ramfb_pixels;
    framebuffer.width = SYNTRA_RAMFB_WIDTH;
    framebuffer.height = SYNTRA_RAMFB_HEIGHT;
    framebuffer.stride_pixels = SYNTRA_RAMFB_WIDTH;

    status = syntra_gui_runtime_render(&ramfb_gui_runtime, &framebuffer);
    if (status != SYNTRA_STATUS_OK) return status;
    status = syntra_gui_render_uptime_panel(&framebuffer, ramfb_uptime_seconds());
    if (status == SYNTRA_STATUS_OK) syntra_dma_barrier();
    return status;
}

/*
 * Drain IRQ-produced VirtIO events in normal context. Rendering is performed
 * once after the batch, never from the interrupt handler. The boot idle loop
 * must call this function periodically after IRQs are enabled to make GUI
 * input interactive.
 */
syntra_status_t syntra_ramfb_process_input_events(
    syntra_virtio_input_queue_t *input_queue)
{
    syntra_input_event_t event;
    int available;
    int processed = 0;
    syntra_status_t status;

    if (input_queue == NULL || !ramfb_gui_runtime_ready)
        return SYNTRA_STATUS_INVALID_ARGUMENT;

    for (;;)
    {
        status = syntra_virtio_mmio_pop_input_event(
            input_queue, &event, &available);
        if (status != SYNTRA_STATUS_OK) return status;
        if (!available) break;

        status = syntra_gui_runtime_process_event_at(
            &ramfb_gui_runtime, &event, ramfb_uptime_milliseconds());
        if (status != SYNTRA_STATUS_OK) return status;
        processed = 1;
    }

    if (!processed) return SYNTRA_STATUS_OK;

    return syntra_ramfb_refresh();
}
