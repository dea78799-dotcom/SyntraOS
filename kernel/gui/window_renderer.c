#include "window_renderer.h"

#define WINDOW_BORDER 2U
#define WINDOW_TITLE_HEIGHT 20U
#define WINDOW_CLOSE_SIZE 12U

#define COLOR_WINDOW_BODY 0xFF101A35U
#define COLOR_WINDOW_BORDER 0xFF5578F2U
#define COLOR_WINDOW_ACTIVE 0xFF1479D4U
#define COLOR_WINDOW_INACTIVE 0xFF263D66U
#define COLOR_WINDOW_TEXT 0xFFF4F7FFU
#define COLOR_WINDOW_CLOSE 0xFFB83D55U

static syntra_status_t draw_window(
    syntra_framebuffer_t *framebuffer,
    const syntra_gui_window_t *window,
    int active)
{
    uint32_t title_height;
    uint32_t title_color;
    uint32_t close_x;
    syntra_status_t status;

    status = syntra_framebuffer_fill_rect(
        framebuffer, window->x, window->y,
        window->width, window->height, COLOR_WINDOW_BORDER);
    if (status != SYNTRA_STATUS_OK) return status;

    if (window->width <= 2U * WINDOW_BORDER ||
        window->height <= 2U * WINDOW_BORDER)
        return SYNTRA_STATUS_OK;

    status = syntra_framebuffer_fill_rect(
        framebuffer,
        window->x + WINDOW_BORDER,
        window->y + WINDOW_BORDER,
        window->width - 2U * WINDOW_BORDER,
        window->height - 2U * WINDOW_BORDER,
        COLOR_WINDOW_BODY);
    if (status != SYNTRA_STATUS_OK) return status;

    title_height = window->height - 2U * WINDOW_BORDER;
    if (title_height > WINDOW_TITLE_HEIGHT)
        title_height = WINDOW_TITLE_HEIGHT;
    title_color = active ? COLOR_WINDOW_ACTIVE : COLOR_WINDOW_INACTIVE;

    status = syntra_framebuffer_fill_rect(
        framebuffer,
        window->x + WINDOW_BORDER,
        window->y + WINDOW_BORDER,
        window->width - 2U * WINDOW_BORDER,
        title_height,
        title_color);
    if (status != SYNTRA_STATUS_OK) return status;

    if (window->width >= 58U && title_height >= 16U)
    {
        close_x = window->x + window->width - WINDOW_CLOSE_SIZE - 5U;
        status = syntra_framebuffer_fill_rect(
            framebuffer, close_x,
            window->y + WINDOW_BORDER + 2U,
            WINDOW_CLOSE_SIZE, WINDOW_CLOSE_SIZE,
            COLOR_WINDOW_CLOSE);
        if (status != SYNTRA_STATUS_OK) return status;

        status = syntra_framebuffer_draw_text(
            framebuffer, close_x + 3U,
            window->y + WINDOW_BORDER + 4U,
            "X", COLOR_WINDOW_TEXT, COLOR_WINDOW_CLOSE, 1U);
        if (status != SYNTRA_STATUS_OK) return status;

        status = syntra_framebuffer_draw_text(
            framebuffer, window->x + 7U,
            window->y + 7U, "WINDOW",
            COLOR_WINDOW_TEXT, title_color, 1U);
        if (status != SYNTRA_STATUS_OK) return status;
    }

    return SYNTRA_STATUS_OK;
}

syntra_status_t syntra_gui_render_windows(
    syntra_framebuffer_t *framebuffer,
    const syntra_gui_window_manager_t *manager)
{
    uint32_t index;
    syntra_status_t status;

    status = syntra_framebuffer_validate(framebuffer);
    if (status != SYNTRA_STATUS_OK) return status;

    if (manager == NULL || manager->initialized != 1 ||
        manager->count > SYNTRA_GUI_MAX_WINDOWS ||
        manager->screen_width != framebuffer->width ||
        manager->screen_height != framebuffer->height)
        return SYNTRA_STATUS_INVALID_ARGUMENT;

    /* The manager stores windows from back to front. */
    for (index = 0U; index < manager->count; ++index)
    {
        const syntra_gui_window_t *window = &manager->windows[index];
        const int active = (index + 1U == manager->count);

        if (window->id == 0U || window->width == 0U ||
            window->height == 0U ||
            window->width > framebuffer->width ||
            window->height > framebuffer->height ||
            window->x > framebuffer->width - window->width ||
            window->y > framebuffer->height - window->height)
            return SYNTRA_STATUS_INVALID_ARGUMENT;

        status = draw_window(framebuffer, window, active);
        if (status != SYNTRA_STATUS_OK) return status;
    }

    return SYNTRA_STATUS_OK;
}
