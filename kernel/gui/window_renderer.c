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


static syntra_status_t draw_app_surface(
    syntra_framebuffer_t *framebuffer,
    const syntra_gui_window_t *window)
{
    const uint32_t left = window->x + 4U;
    const uint32_t top = window->y + 24U;
    const uint32_t inner_width = window->width > 8U ? window->width - 8U : 0U;
    const uint32_t inner_height = window->height > 28U ? window->height - 28U : 0U;
    uint32_t row_y;
    syntra_status_t status;

    if (inner_width < 40U || inner_height < 24U)
        return SYNTRA_STATUS_OK;

    if (window->app_type == SYNTRA_GUI_APP_FILES)
    {
        static const char *const names[] = {
            "HOME", "DOCUMENTS", "DOWNLOADS", "PICTURES"
        };
        uint32_t row;
        status = syntra_framebuffer_fill_rect(
            framebuffer, left, top, inner_width, inner_height, 0xFF0C1428U);
        if (status != SYNTRA_STATUS_OK) return status;
        status = syntra_framebuffer_fill_rect(
            framebuffer, left, top, 34U, inner_height, 0xFF1A2948U);
        if (status != SYNTRA_STATUS_OK) return status;
        status = syntra_framebuffer_draw_text(
            framebuffer, left + 5U, top + 6U, "FILES",
            COLOR_WINDOW_TEXT, 0xFF1A2948U, 1U);
        if (status != SYNTRA_STATUS_OK) return status;
        for (row = 0U; row < 4U; ++row)
        {
            row_y = top + 8U + row * 22U;
            if (row_y + 16U >= top + inner_height) break;
            status = syntra_framebuffer_fill_rect(
                framebuffer, left + 42U, row_y, 12U, 9U, 0xFF54D6E8U);
            if (status != SYNTRA_STATUS_OK) return status;
            status = syntra_framebuffer_fill_rect(
                framebuffer, left + 42U, row_y + 3U, 12U, 9U, 0xFF3B9FB5U);
            if (status != SYNTRA_STATUS_OK) return status;
            status = syntra_framebuffer_draw_text(
                framebuffer, left + 62U, row_y + 2U, names[row],
                COLOR_WINDOW_TEXT, 0xFF0C1428U, 1U);
            if (status != SYNTRA_STATUS_OK) return status;
        }
        return SYNTRA_STATUS_OK;
    }

    if (window->app_type == SYNTRA_GUI_APP_SETTINGS)
    {
        static const char *const names[] = {
            "DISPLAY", "INPUT", "SYSTEM", "ABOUT"
        };
        uint32_t row;
        status = syntra_framebuffer_fill_rect(
            framebuffer, left, top, inner_width, inner_height, 0xFF0C1428U);
        if (status != SYNTRA_STATUS_OK) return status;
        for (row = 0U; row < 4U; ++row)
        {
            row_y = top + 5U + row * 22U;
            if (row_y + 18U >= top + inner_height) break;
            status = syntra_framebuffer_fill_rect(
                framebuffer, left + 5U, row_y, inner_width - 10U, 17U,
                0xFF1A2948U);
            if (status != SYNTRA_STATUS_OK) return status;
            status = syntra_framebuffer_fill_rect(
                framebuffer, left + 8U, row_y + 4U, 5U, 9U,
                0xFF54D6E8U);
            if (status != SYNTRA_STATUS_OK) return status;
            status = syntra_framebuffer_draw_text(
                framebuffer, left + 20U, row_y + 5U, names[row],
                COLOR_WINDOW_TEXT, 0xFF1A2948U, 1U);
            if (status != SYNTRA_STATUS_OK) return status;
        }
        return SYNTRA_STATUS_OK;
    }

    if (window->app_type == SYNTRA_GUI_APP_LAUNCHER)
    {
        static const char *const names[] = {"FILES", "SET", "APP", "INFO"};
        static const uint32_t colors[] = {
            0xFF54D6E8U, 0xFF58C99AU, 0xFF9874F5U, 0xFFFFB66EU
        };
        uint32_t tile_width = (inner_width - 12U) / 2U;
        uint32_t row;
        for (row = 0U; row < 4U; ++row)
        {
            uint32_t x = left + 4U + (row % 2U) * (tile_width + 4U);
            row_y = top + 4U + (row / 2U) * 32U;
            if (row_y + 26U >= top + inner_height) break;
            status = syntra_framebuffer_fill_rect(
                framebuffer, x, row_y, tile_width, 24U, colors[row]);
            if (status != SYNTRA_STATUS_OK) return status;
            status = syntra_framebuffer_draw_text(
                framebuffer, x + 5U, row_y + 9U, names[row],
                COLOR_WINDOW_TEXT, colors[row], 1U);
            if (status != SYNTRA_STATUS_OK) return status;
        }
    }
    return SYNTRA_STATUS_OK;
}

static syntra_status_t draw_window(
    syntra_framebuffer_t *framebuffer,
    const syntra_gui_window_t *window,
    int active)
{
    uint32_t title_height;
    uint32_t title_color;
    uint32_t close_x;
    const char *title_text = "WINDOW";
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
    if (window->app_type == SYNTRA_GUI_APP_FILES) title_text = "FILES";
    else if (window->app_type == SYNTRA_GUI_APP_SETTINGS) title_text = "SETTINGS";
    else if (window->app_type == SYNTRA_GUI_APP_LAUNCHER) title_text = "LAUNCHER";

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
            window->y + 7U, title_text,
            COLOR_WINDOW_TEXT, title_color, 1U);
        if (status != SYNTRA_STATUS_OK) return status;
    }

    if (window->app_type != SYNTRA_GUI_APP_GENERIC)
        return draw_app_surface(framebuffer, window);

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
