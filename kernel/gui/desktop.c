#include "desktop.h"

#define COLOR_BACKGROUND 0xFF101A35U
#define COLOR_BACKGROUND_LIGHT 0xFF1B2D55U
#define COLOR_PANEL 0xE8202D49U
#define COLOR_PANEL_EDGE 0xFF40577FU
#define COLOR_WHITE 0xFFF4F7FFU
#define COLOR_MUTED 0xFFB7C5E0U
#define COLOR_CYAN 0xFF54D6E8U
#define COLOR_BLUE 0xFF5578F2U
#define COLOR_PURPLE 0xFF9874F5U
#define COLOR_GREEN 0xFF58C99AU
#define COLOR_ORANGE 0xFFFFB66EU

syntra_status_t syntra_desktop_render(syntra_framebuffer_t *framebuffer)
{
    uint32_t width;
    uint32_t height;
    uint32_t center_x;
    syntra_status_t status = syntra_framebuffer_validate(framebuffer);

    if (status != SYNTRA_STATUS_OK)
    {
        return status;
    }

    width = framebuffer->width;
    height = framebuffer->height;
    if (width < 320U || height < 240U)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    center_x = width / 2U;

    status = syntra_framebuffer_clear(framebuffer, COLOR_BACKGROUND);
    if (status != SYNTRA_STATUS_OK) return status;

    /* Layered wallpaper geometry. */
    status = syntra_framebuffer_fill_rect(
        framebuffer, 0U, height / 3U, width, height / 3U,
        COLOR_BACKGROUND_LIGHT);
    if (status != SYNTRA_STATUS_OK) return status;

    status = syntra_framebuffer_fill_rect(
        framebuffer, center_x / 2U, height / 5U,
        width / 2U, height / 2U, COLOR_BACKGROUND);
    if (status != SYNTRA_STATUS_OK) return status;

    status = syntra_framebuffer_fill_rect(
        framebuffer, center_x / 2U + 18U, height / 5U + 18U,
        width / 2U - 36U, height / 2U - 36U, COLOR_BACKGROUND_LIGHT);
    if (status != SYNTRA_STATUS_OK) return status;

    /* Top status bar. */
    status = syntra_framebuffer_fill_rect(
        framebuffer, 0U, 0U, width, 30U, COLOR_PANEL);
    if (status != SYNTRA_STATUS_OK) return status;
    status = syntra_framebuffer_draw_text(
        framebuffer, 14U, 10U, "SYNTRAOS",
        COLOR_WHITE, COLOR_PANEL, 1U);
    if (status != SYNTRA_STATUS_OK) return status;
    status = syntra_framebuffer_draw_text(
        framebuffer, width - 112U, 10U, "DESKTOP",
        COLOR_MUTED, COLOR_PANEL, 1U);
    if (status != SYNTRA_STATUS_OK) return status;

    /* Main welcome card. */
    status = syntra_framebuffer_fill_rect(
        framebuffer, 24U, 54U, width - 48U, 104U, COLOR_PANEL);
    if (status != SYNTRA_STATUS_OK) return status;
    status = syntra_framebuffer_fill_rect(
        framebuffer, 24U, 54U, 4U, 104U, COLOR_CYAN);
    if (status != SYNTRA_STATUS_OK) return status;
    status = syntra_framebuffer_draw_text(
        framebuffer, 42U, 76U, "WELCOME TO",
        COLOR_MUTED, COLOR_PANEL, 2U);
    if (status != SYNTRA_STATUS_OK) return status;
    status = syntra_framebuffer_draw_text(
        framebuffer, 42U, 101U, "SYNTRAOS",
        COLOR_WHITE, COLOR_PANEL, 2U);
    if (status != SYNTRA_STATUS_OK) return status;

    /* Four application tiles; input/window management comes in later stages. */
    status = syntra_framebuffer_fill_rect(
        framebuffer, 24U, 180U, 64U, 64U, COLOR_BLUE);
    if (status != SYNTRA_STATUS_OK) return status;
    status = syntra_framebuffer_fill_rect(
        framebuffer, 104U, 180U, 64U, 64U, COLOR_PURPLE);
    if (status != SYNTRA_STATUS_OK) return status;
    status = syntra_framebuffer_fill_rect(
        framebuffer, 184U, 180U, 64U, 64U, COLOR_GREEN);
    if (status != SYNTRA_STATUS_OK) return status;
    status = syntra_framebuffer_fill_rect(
        framebuffer, 264U, 180U, 64U, 64U, COLOR_ORANGE);
    if (status != SYNTRA_STATUS_OK) return status;

    status = syntra_framebuffer_draw_text(
        framebuffer, 37U, 205U, "APP",
        COLOR_WHITE, COLOR_BLUE, 1U);
    if (status != SYNTRA_STATUS_OK) return status;
    status = syntra_framebuffer_draw_text(
        framebuffer, 117U, 205U, "FILES",
        COLOR_WHITE, COLOR_PURPLE, 1U);
    if (status != SYNTRA_STATUS_OK) return status;
    status = syntra_framebuffer_draw_text(
        framebuffer, 197U, 205U, "GAME",
        COLOR_WHITE, COLOR_GREEN, 1U);
    if (status != SYNTRA_STATUS_OK) return status;
    status = syntra_framebuffer_draw_text(
        framebuffer, 277U, 205U, "SET",
        COLOR_WHITE, COLOR_ORANGE, 1U);
    if (status != SYNTRA_STATUS_OK) return status;

    /* Bottom dock. */
    status = syntra_framebuffer_fill_rect(
        framebuffer, 0U, height - 42U, width, 42U, COLOR_PANEL);
    if (status != SYNTRA_STATUS_OK) return status;
    status = syntra_framebuffer_fill_rect(
        framebuffer, 14U, height - 34U, 26U, 26U, COLOR_CYAN);
    if (status != SYNTRA_STATUS_OK) return status;
    status = syntra_framebuffer_draw_text(
        framebuffer, 52U, height - 26U, "SYNTRAOS PREVIEW",
        COLOR_WHITE, COLOR_PANEL, 1U);
    if (status != SYNTRA_STATUS_OK) return status;
    status = syntra_framebuffer_fill_rect(
        framebuffer, width - 94U, height - 28U, 70U, 2U, COLOR_PANEL_EDGE);
    return status;
}
