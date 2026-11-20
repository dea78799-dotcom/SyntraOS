#ifndef SYNTRA_GUI_FRAMEBUFFER_H
#define SYNTRA_GUI_FRAMEBUFFER_H

#include <stddef.h>
#include <stdint.h>

#include "../core/status.h"

/* Pixel storage is caller-owned and uses 0xAARRGGBB format. */
typedef struct
{
    uint32_t *pixels;
    uint32_t width;
    uint32_t height;
    size_t stride_pixels;
} syntra_framebuffer_t;

syntra_status_t syntra_framebuffer_validate(
    const syntra_framebuffer_t *framebuffer
);

syntra_status_t syntra_framebuffer_clear(
    syntra_framebuffer_t *framebuffer,
    uint32_t color
);

syntra_status_t syntra_framebuffer_fill_rect(
    syntra_framebuffer_t *framebuffer,
    uint32_t x,
    uint32_t y,
    uint32_t width,
    uint32_t height,
    uint32_t color
);

syntra_status_t syntra_framebuffer_draw_text(
    syntra_framebuffer_t *framebuffer,
    uint32_t x,
    uint32_t y,
    const char *text,
    uint32_t foreground,
    uint32_t background,
    uint32_t scale
);

#endif
