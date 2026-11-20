#ifndef SYNTRA_GUI_RAMFB_H
#define SYNTRA_GUI_RAMFB_H

#include <stddef.h>
#include <stdint.h>
#include "../core/status.h"

#define SYNTRA_RAMFB_WIDTH 640U
#define SYNTRA_RAMFB_HEIGHT 480U
#define SYNTRA_RAMFB_STRIDE (SYNTRA_RAMFB_WIDTH * 4U)
#define SYNTRA_RAMFB_BUFFER_SIZE ((size_t)SYNTRA_RAMFB_STRIDE * SYNTRA_RAMFB_HEIGHT)
#define SYNTRA_RAMFB_CONFIG_SIZE 28U
#define SYNTRA_RAMFB_FOURCC_XRGB8888 0x34325258U

/* Pure helpers are host-testable; the hardware initializer is ARM64/QEMU-specific. */
syntra_status_t syntra_ramfb_find_file(
    const uint8_t *directory, size_t directory_size,
    const char *name, uint16_t *selector, uint32_t *file_size);

syntra_status_t syntra_ramfb_encode_config(
    uint8_t output[SYNTRA_RAMFB_CONFIG_SIZE],
    uint64_t framebuffer_address,
    uint32_t width, uint32_t height, uint32_t stride);

/* Configure QEMU ramfb through the fw_cfg DMA interface and render the desktop. */
syntra_status_t syntra_ramfb_init(uintptr_t fw_cfg_base, size_t fw_cfg_size);

#endif
