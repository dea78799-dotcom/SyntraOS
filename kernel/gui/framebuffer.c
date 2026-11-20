#include "framebuffer.h"

#include <stdint.h>
#include <stddef.h>

#define SYNTRA_FONT_WIDTH 5U
#define SYNTRA_FONT_HEIGHT 7U
#define SYNTRA_FONT_GLYPH_COUNT 36U

static const uint8_t syntra_font[SYNTRA_FONT_GLYPH_COUNT][SYNTRA_FONT_HEIGHT] = {
    {14,17,17,31,17,17,17}, {30,17,17,30,17,17,30},
    {14,17,16,16,16,17,14}, {30,17,17,17,17,17,30},
    {31,16,16,30,16,16,31}, {31,16,16,30,16,16,16},
    {14,17,16,23,17,17,15}, {17,17,17,31,17,17,17},
    {14,4,4,4,4,4,14}, {7,2,2,2,18,18,12},
    {17,18,20,24,20,18,17}, {16,16,16,16,16,16,31},
    {17,27,21,21,17,17,17}, {17,25,21,19,17,17,17},
    {14,17,17,17,17,17,14}, {30,17,17,30,16,16,16},
    {14,17,17,17,21,18,13}, {30,17,17,30,20,18,17},
    {15,16,16,14,1,1,30}, {31,4,4,4,4,4,4},
    {17,17,17,17,17,17,14}, {17,17,17,17,17,10,4},
    {17,17,17,21,21,21,10}, {17,17,10,4,10,17,17},
    {17,17,10,4,4,4,4}, {31,1,2,4,8,16,31},
    {14,17,19,21,25,17,14}, {4,12,4,4,4,4,14},
    {14,17,1,2,4,8,31}, {30,1,1,14,1,1,30},
    {2,6,10,18,31,2,2}, {31,16,16,30,1,1,30},
    {14,16,16,30,17,17,14}, {31,1,2,4,8,8,8},
    {14,17,17,14,17,17,14}, {14,17,17,15,1,1,14}
};

static int syntra_font_index(char character)
{
    if (character >= 'a' && character <= 'z')
    {
        character = (char)(character - 'a' + 'A');
    }
    if (character >= 'A' && character <= 'Z')
    {
        return (int)(character - 'A');
    }
    if (character >= '0' && character <= '9')
    {
        return 26 + (int)(character - '0');
    }
    return -1;
}

syntra_status_t syntra_framebuffer_validate(
    const syntra_framebuffer_t *framebuffer
)
{
    size_t last_row_offset;

    if (framebuffer == NULL || framebuffer->pixels == NULL ||
        framebuffer->width == 0U || framebuffer->height == 0U ||
        framebuffer->stride_pixels < framebuffer->width ||
        framebuffer->width > SIZE_MAX / sizeof(uint32_t))
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if ((size_t)(framebuffer->height - 1U) >
        (SIZE_MAX - framebuffer->width) / framebuffer->stride_pixels)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    last_row_offset =
        (size_t)(framebuffer->height - 1U) * framebuffer->stride_pixels;
    if (last_row_offset > SIZE_MAX / sizeof(uint32_t) -
                          framebuffer->width)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    return SYNTRA_STATUS_OK;
}

syntra_status_t syntra_framebuffer_clear(
    syntra_framebuffer_t *framebuffer,
    uint32_t color
)
{
    uint32_t y;
    syntra_status_t status = syntra_framebuffer_validate(framebuffer);

    if (status != SYNTRA_STATUS_OK)
    {
        return status;
    }

    for (y = 0U; y < framebuffer->height; ++y)
    {
        size_t x;
        uint32_t *row =
            framebuffer->pixels + (size_t)y * framebuffer->stride_pixels;
        for (x = 0U; x < framebuffer->width; ++x)
        {
            row[x] = color;
        }
    }

    return SYNTRA_STATUS_OK;
}

syntra_status_t syntra_framebuffer_fill_rect(
    syntra_framebuffer_t *framebuffer,
    uint32_t x,
    uint32_t y,
    uint32_t width,
    uint32_t height,
    uint32_t color
)
{
    uint32_t row_index;
    syntra_status_t status = syntra_framebuffer_validate(framebuffer);

    if (status != SYNTRA_STATUS_OK)
    {
        return status;
    }

    if (width == 0U || height == 0U ||
        x >= framebuffer->width || y >= framebuffer->height)
    {
        return SYNTRA_STATUS_OK;
    }

    if (width > framebuffer->width - x)
    {
        width = framebuffer->width - x;
    }
    if (height > framebuffer->height - y)
    {
        height = framebuffer->height - y;
    }

    for (row_index = 0U; row_index < height; ++row_index)
    {
        size_t column;
        uint32_t *row = framebuffer->pixels +
            (size_t)(y + row_index) * framebuffer->stride_pixels + x;
        for (column = 0U; column < width; ++column)
        {
            row[column] = color;
        }
    }

    return SYNTRA_STATUS_OK;
}

syntra_status_t syntra_framebuffer_draw_text(
    syntra_framebuffer_t *framebuffer,
    uint32_t x,
    uint32_t y,
    const char *text,
    uint32_t foreground,
    uint32_t background,
    uint32_t scale
)
{
    uint32_t cursor_x = x;
    syntra_status_t status = syntra_framebuffer_validate(framebuffer);

    if (status != SYNTRA_STATUS_OK || text == NULL || scale == 0U ||
        scale > UINT32_MAX / SYNTRA_FONT_HEIGHT ||
        x > UINT32_MAX - SYNTRA_FONT_WIDTH * scale ||
        y > UINT32_MAX - SYNTRA_FONT_HEIGHT * scale)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    while (*text != '\0')
    {
        int glyph_index = syntra_font_index(*text);
        uint32_t glyph_y;

        if (cursor_x >= framebuffer->width || y >= framebuffer->height)
        {
            break;
        }

        if (*text == ' ')
        {
            if (cursor_x > UINT32_MAX - 4U * scale) break;
            cursor_x += 4U * scale;
            ++text;
            continue;
        }

        for (glyph_y = 0U; glyph_y < SYNTRA_FONT_HEIGHT; ++glyph_y)
        {
            uint32_t glyph_x;
            for (glyph_x = 0U; glyph_x < SYNTRA_FONT_WIDTH; ++glyph_x)
            {
                uint32_t color = background;
                if (glyph_index >= 0 &&
                    (syntra_font[glyph_index][glyph_y] &
                     (uint8_t)(1U << (SYNTRA_FONT_WIDTH - 1U - glyph_x))) != 0U)
                {
                    color = foreground;
                }

                status = syntra_framebuffer_fill_rect(
                    framebuffer,
                    cursor_x + glyph_x * scale,
                    y + glyph_y * scale,
                    scale,
                    scale,
                    color
                );
                if (status != SYNTRA_STATUS_OK)
                {
                    return status;
                }
            }
        }

        if (cursor_x > UINT32_MAX - 6U * scale) break;
        cursor_x += 6U * scale;
        ++text;
    }

    return SYNTRA_STATUS_OK;
}
