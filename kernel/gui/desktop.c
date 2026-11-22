#include "desktop.h"
#include "window_manager.h"
#include "window_renderer.h"

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

static syntra_status_t draw_text(
    syntra_framebuffer_t *framebuffer,
    uint32_t x,
    uint32_t y,
    const char *value,
    uint32_t foreground,
    uint32_t background,
    uint32_t scale)
{
    return syntra_framebuffer_draw_text(
        framebuffer, x, y, value, foreground, background, scale);
}

static syntra_status_t fill(
    syntra_framebuffer_t *framebuffer,
    uint32_t x,
    uint32_t y,
    uint32_t width,
    uint32_t height,
    uint32_t color)
{
    return syntra_framebuffer_fill_rect(
        framebuffer, x, y, width, height, color);
}

/*
 * Compact layout is used for small framebuffer sizes. It is a visual
 * adaptation only; input routing and touch interaction are not implemented.
 */
static syntra_status_t render_compact(syntra_framebuffer_t *framebuffer)
{
    const uint32_t width = framebuffer->width;
    const uint32_t height = framebuffer->height;
    const uint32_t dock_height = 26U;
    const uint32_t tile_width = (width - 48U) / 4U;
    const uint32_t tile_gap = 8U;
    const uint32_t tile_y = 112U;
    const uint32_t tile_height = 52U;
    const uint32_t tile_x[4] = {
        12U, 12U + tile_width + tile_gap,
        12U + 2U * (tile_width + tile_gap),
        12U + 3U * (tile_width + tile_gap)
    };
    const uint32_t tile_colors[4] = {
        COLOR_BLUE, COLOR_PURPLE, COLOR_GREEN, COLOR_ORANGE
    };
    const char *const tile_names[4] = {
        "APP", "FILES", "GAME", "SET"
    };
    syntra_status_t status;
    size_t index;

    status = fill(framebuffer, 0U, 0U, width, 24U, COLOR_PANEL);
    if (status != SYNTRA_STATUS_OK) return status;
    status = draw_text(framebuffer, 10U, 8U, "SYNTRAOS",
                       COLOR_WHITE, COLOR_PANEL, 1U);
    if (status != SYNTRA_STATUS_OK) return status;
    status = draw_text(framebuffer, width - 42U, 8U, "ARM64",
                       COLOR_MUTED, COLOR_PANEL, 1U);
    if (status != SYNTRA_STATUS_OK) return status;

    status = fill(framebuffer, 12U, 36U, width - 24U, 60U, COLOR_PANEL);
    if (status != SYNTRA_STATUS_OK) return status;
    status = fill(framebuffer, 12U, 36U, 3U, 60U, COLOR_CYAN);
    if (status != SYNTRA_STATUS_OK) return status;
    status = draw_text(framebuffer, 24U, 48U, "WELCOME",
                       COLOR_MUTED, COLOR_PANEL, 1U);
    if (status != SYNTRA_STATUS_OK) return status;
    status = draw_text(framebuffer, 24U, 66U, "SYNTRAOS",
                       COLOR_WHITE, COLOR_PANEL, 2U);
    if (status != SYNTRA_STATUS_OK) return status;

    for (index = 0U; index < 4U; ++index)
    {
        status = fill(framebuffer, tile_x[index], tile_y,
                      tile_width, tile_height, tile_colors[index]);
        if (status != SYNTRA_STATUS_OK) return status;
        status = draw_text(framebuffer, tile_x[index] + 7U, tile_y + 22U,
                           tile_names[index], COLOR_WHITE,
                           tile_colors[index], 1U);
        if (status != SYNTRA_STATUS_OK) return status;
    }

    status = fill(framebuffer, 0U, height - dock_height,
                  width, dock_height, COLOR_PANEL);
    if (status != SYNTRA_STATUS_OK) return status;
    status = fill(framebuffer, 8U, height - 32U, 24U, 24U, COLOR_CYAN);
    if (status != SYNTRA_STATUS_OK) return status;
    return draw_text(framebuffer, 28U, height - 18U, "DESKTOP PREVIEW",
                     COLOR_WHITE, COLOR_PANEL, 1U);
}

static syntra_status_t render_desktop(syntra_framebuffer_t *framebuffer)
{
    const uint32_t width = framebuffer->width;
    const uint32_t height = framebuffer->height;
    const uint32_t tile_x[4] = {24U, 104U, 184U, 264U};
    const uint32_t tile_colors[4] = {
        COLOR_BLUE, COLOR_PURPLE, COLOR_GREEN, COLOR_ORANGE
    };
    const char *const tile_names[4] = {
        "APPLICATIONS", "FILES", "GAMES", "SETTINGS"
    };
    const char *const tile_short_names[4] = {
        "APP", "FILES", "GAME", "SET"
    };
    syntra_status_t status;
    size_t index;

    /* Top navigation bar. */
    status = fill(framebuffer, 0U, 0U, width, 30U, COLOR_PANEL);
    if (status != SYNTRA_STATUS_OK) return status;
    status = fill(framebuffer, 14U, 8U, 12U, 14U, COLOR_CYAN);
    if (status != SYNTRA_STATUS_OK) return status;
    status = draw_text(framebuffer, 34U, 10U, "SYNTRAOS",
                       COLOR_WHITE, COLOR_PANEL, 1U);
    if (status != SYNTRA_STATUS_OK) return status;
    status = draw_text(framebuffer, 492U, 10U, "READY",
                       COLOR_GREEN, COLOR_PANEL, 1U);
    if (status != SYNTRA_STATUS_OK) return status;
    status = draw_text(framebuffer, 574U, 10U, "ARM64",
                       COLOR_MUTED, COLOR_PANEL, 1U);
    if (status != SYNTRA_STATUS_OK) return status;

    /* Welcome / identity card. */
    status = fill(framebuffer, 24U, 54U, width - 48U, 104U, COLOR_PANEL);
    if (status != SYNTRA_STATUS_OK) return status;
    status = fill(framebuffer, 24U, 54U, 4U, 104U, COLOR_CYAN);
    if (status != SYNTRA_STATUS_OK) return status;
    status = draw_text(framebuffer, 42U, 72U, "WELCOME TO",
                       COLOR_MUTED, COLOR_PANEL, 1U);
    if (status != SYNTRA_STATUS_OK) return status;
    status = draw_text(framebuffer, 42U, 91U, "SYNTRAOS",
                       COLOR_WHITE, COLOR_PANEL, 2U);
    if (status != SYNTRA_STATUS_OK) return status;
    status = draw_text(framebuffer, 42U, 128U, "A NEW COMPUTING EXPERIENCE",
                       COLOR_MUTED, COLOR_PANEL, 1U);
    if (status != SYNTRA_STATUS_OK) return status;

    status = fill(framebuffer, 454U, 72U, 142U, 68U,
                  COLOR_BACKGROUND_LIGHT);
    if (status != SYNTRA_STATUS_OK) return status;
    status = fill(framebuffer, 454U, 72U, 3U, 68U, COLOR_CYAN);
    if (status != SYNTRA_STATUS_OK) return status;
    status = draw_text(framebuffer, 472U, 91U, "ARM64",
                       COLOR_WHITE, COLOR_BACKGROUND_LIGHT, 2U);
    if (status != SYNTRA_STATUS_OK) return status;
    status = draw_text(framebuffer, 472U, 116U, "PREVIEW",
                       COLOR_MUTED, COLOR_BACKGROUND_LIGHT, 1U);
    if (status != SYNTRA_STATUS_OK) return status;

    /* Application launcher tiles. Keep the grid clear and evenly spaced. */
    for (index = 0U; index < 4U; ++index)
    {
        status = fill(framebuffer, tile_x[index], 180U, 64U, 64U,
                      tile_colors[index]);
        if (status != SYNTRA_STATUS_OK) return status;
        status = fill(framebuffer, tile_x[index] + 25U, 190U, 14U, 3U,
                      COLOR_WHITE);
        if (status != SYNTRA_STATUS_OK) return status;
        status = fill(framebuffer, tile_x[index] + 25U, 196U, 14U, 3U,
                      COLOR_WHITE);
        if (status != SYNTRA_STATUS_OK) return status;
        status = draw_text(framebuffer, tile_x[index] + 7U, 216U,
                           tile_short_names[index], COLOR_WHITE,
                           tile_colors[index], 1U);
        if (status != SYNTRA_STATUS_OK) return status;
        status = draw_text(framebuffer, tile_x[index], 252U,
                           tile_names[index], COLOR_WHITE,
                           COLOR_BACKGROUND_LIGHT, 1U);
        if (status != SYNTRA_STATUS_OK) return status;
    }

    /* System overview panel. This reports the current preview profile only. */
    status = fill(framebuffer, 360U, 180U, 256U, 150U, COLOR_PANEL);
    if (status != SYNTRA_STATUS_OK) return status;
    status = fill(framebuffer, 360U, 180U, 3U, 150U, COLOR_CYAN);
    if (status != SYNTRA_STATUS_OK) return status;
    status = draw_text(framebuffer, 378U, 194U, "SYSTEM OVERVIEW",
                       COLOR_WHITE, COLOR_PANEL, 1U);
    if (status != SYNTRA_STATUS_OK) return status;
    status = fill(framebuffer, 378U, 214U, 218U, 1U, COLOR_PANEL_EDGE);
    if (status != SYNTRA_STATUS_OK) return status;
    status = draw_text(framebuffer, 378U, 226U, "ARCH  ARM64",
                       COLOR_MUTED, COLOR_PANEL, 1U);
    if (status != SYNTRA_STATUS_OK) return status;
    status = draw_text(framebuffer, 378U, 252U, "DISPLAY  640 X 480",
                       COLOR_MUTED, COLOR_PANEL, 1U);
    if (status != SYNTRA_STATUS_OK) return status;
    status = draw_text(framebuffer, 378U, 278U, "MODE  PREVIEW",
                       COLOR_MUTED, COLOR_PANEL, 1U);
    if (status != SYNTRA_STATUS_OK) return status;
    status = fill(framebuffer, 378U, 304U, 7U, 7U, COLOR_GREEN);
    if (status != SYNTRA_STATUS_OK) return status;
    status = draw_text(framebuffer, 392U, 304U, "FRAMEBUFFER READY",
                       COLOR_GREEN, COLOR_PANEL, 1U);
    if (status != SYNTRA_STATUS_OK) return status;

    /* Bottom dock / persistent desktop identity. */
    status = fill(framebuffer, 0U, height - 42U, width, 42U, COLOR_PANEL);
    if (status != SYNTRA_STATUS_OK) return status;
    status = fill(framebuffer, 14U, height - 34U, 26U, 26U, COLOR_CYAN);
    if (status != SYNTRA_STATUS_OK) return status;
    status = draw_text(framebuffer, 52U, height - 26U, "SYNTRAOS DESKTOP",
                       COLOR_WHITE, COLOR_PANEL, 1U);
    if (status != SYNTRA_STATUS_OK) return status;
    status = draw_text(framebuffer, 520U, height - 26U, "BUILD PREVIEW",
                       COLOR_MUTED, COLOR_PANEL, 1U);
    if (status != SYNTRA_STATUS_OK) return status;
    return fill(framebuffer, width - 94U, height - 28U,
                70U, 2U, COLOR_PANEL_EDGE);
}

syntra_status_t syntra_desktop_render(syntra_framebuffer_t *framebuffer)
{
    uint32_t width;
    uint32_t height;
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

    status = syntra_framebuffer_clear(framebuffer, COLOR_BACKGROUND);
    if (status != SYNTRA_STATUS_OK) return status;

    /* Layered wallpaper remains visible behind the desktop panels. */
    status = fill(framebuffer, 0U, height / 3U, width, height / 3U,
                  COLOR_BACKGROUND_LIGHT);
    if (status != SYNTRA_STATUS_OK) return status;

    if (width < 640U || height < 480U)
    {
        return render_compact(framebuffer);
    }

    status = fill(framebuffer, width / 4U, height / 5U,
                  width / 2U, height / 2U, COLOR_BACKGROUND);
    if (status != SYNTRA_STATUS_OK) return status;
    status = fill(framebuffer, width / 4U + 18U, height / 5U + 18U,
                  width / 2U - 36U, height / 2U - 36U,
                  COLOR_BACKGROUND_LIGHT);
    if (status != SYNTRA_STATUS_OK) return status;

    return render_desktop(framebuffer);
}

/*
 * GUI v2 focus feedback. This is deliberately a render overlay: input
 * transport remains separate, so the same session can be driven by host
 * tests or a future QEMU VirtIO input backend.
 */

syntra_status_t syntra_desktop_gui_v5_init_windows(
    syntra_gui_window_manager_t *manager,
    uint32_t width,
    uint32_t height)
{
    uint32_t files_id;
    uint32_t settings_id;
    uint32_t launcher_id;
    syntra_status_t status;

    if (manager == NULL || width < 640U || height < 480U)
        return SYNTRA_STATUS_INVALID_ARGUMENT;

    status = syntra_gui_window_manager_init(manager, width, height);
    if (status != SYNTRA_STATUS_OK) return status;

    status = syntra_gui_window_open_app(
        manager, 20U, 190U, 270U, 230U,
        SYNTRA_GUI_APP_FILES, &files_id);
    if (status != SYNTRA_STATUS_OK) return status;

    status = syntra_gui_window_open_app(
        manager, 330U, 190U, 270U, 230U,
        SYNTRA_GUI_APP_SETTINGS, &settings_id);
    if (status != SYNTRA_STATUS_OK) return status;

    status = syntra_gui_window_open_app(
        manager, 230U, 60U, 180U, 140U,
        SYNTRA_GUI_APP_LAUNCHER, &launcher_id);
    if (status != SYNTRA_STATUS_OK) return status;

    return SYNTRA_STATUS_OK;
}

syntra_status_t syntra_desktop_render_gui_v5(
    syntra_framebuffer_t *framebuffer)
{
    syntra_gui_window_manager_t manager;
    syntra_status_t status;

    status = syntra_framebuffer_validate(framebuffer);
    if (status != SYNTRA_STATUS_OK) return status;

    status = syntra_desktop_render(framebuffer);
    if (status != SYNTRA_STATUS_OK) return status;

    /* Keep the compact mobile preview uncluttered on small framebuffers. */
    if (framebuffer->width < 640U || framebuffer->height < 480U)
        return SYNTRA_STATUS_OK;

    status = syntra_desktop_gui_v5_init_windows(
        &manager, framebuffer->width, framebuffer->height);
    if (status != SYNTRA_STATUS_OK) return status;

    return syntra_gui_render_windows(framebuffer, &manager);
}

syntra_status_t syntra_desktop_render_session(
    syntra_framebuffer_t *framebuffer,
    const syntra_desktop_session_t *session)
{
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;
    const uint32_t border = 2U;
    const uint32_t color = 0xFFFFFFFFU;
    syntra_status_t status;

    if (session == NULL || session->initialized != 1 ||
        session->focused_app >= SYNTRA_DESKTOP_APP_COUNT)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    status = syntra_desktop_render(framebuffer);
    if (status != SYNTRA_STATUS_OK) return status;

    if (framebuffer->width < 640U || framebuffer->height < 480U)
    {
        width = (framebuffer->width - 48U) / SYNTRA_DESKTOP_APP_COUNT;
        height = 52U;
        x = 12U + session->focused_app * (width + 8U);
        y = 112U;
    }
    else
    {
        width = 64U;
        height = 64U;
        x = 24U + session->focused_app * 80U;
        y = 180U;
    }

    status = syntra_framebuffer_fill_rect(
        framebuffer, x, y, width, border, color);
    if (status != SYNTRA_STATUS_OK) return status;
    status = syntra_framebuffer_fill_rect(
        framebuffer, x, y + height - border, width, border, color);
    if (status != SYNTRA_STATUS_OK) return status;
    status = syntra_framebuffer_fill_rect(
        framebuffer, x, y, border, height, color);
    if (status != SYNTRA_STATUS_OK) return status;
    return syntra_framebuffer_fill_rect(
        framebuffer, x + width - border, y, border, height, color);
}
