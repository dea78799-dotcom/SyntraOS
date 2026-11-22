#include "window_renderer.h"

#define WINDOW_BORDER 2U
#define WINDOW_TITLE_HEIGHT 20U
#define WINDOW_CLOSE_SIZE 12U
#define WINDOW_CONTROL_GAP 5U
#define WINDOW_CONTROLS_MIN_WIDTH 120U

#define COLOR_WINDOW_BODY 0xFF101A35U
#define COLOR_WINDOW_BORDER 0xFF5578F2U
#define COLOR_WINDOW_ACTIVE 0xFF1479D4U
#define COLOR_WINDOW_INACTIVE 0xFF263D66U
#define COLOR_WINDOW_TEXT 0xFFF4F7FFU
#define COLOR_WINDOW_CLOSE 0xFFB83D55U
#define COLOR_WINDOW_MINIMIZE 0xFFE5A84BU
#define COLOR_WINDOW_MAXIMIZE 0xFF54D6E8U


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
        static const char *const names[SYNTRA_GUI_FILES_ENTRY_COUNT] = {
            "HOME", "DOCUMENTS", "DOWNLOADS", "PICTURES",
            "MUSIC", "VIDEOS", "PROJECTS", "SYSTEM"
        };
        uint32_t row;
        uint32_t visible_rows = 0U;
        uint32_t scroll_offset = window->app_scroll_offset;
        status = syntra_framebuffer_fill_rect(
            framebuffer, left, top, inner_width, inner_height, 0xFF0C1428U);
        if (status != SYNTRA_STATUS_OK) return status;
        if (inner_width < 64U ||
            scroll_offset >= SYNTRA_GUI_FILES_ENTRY_COUNT)
            return SYNTRA_STATUS_OK;
        status = syntra_framebuffer_fill_rect(
            framebuffer, left, top, 34U, inner_height, 0xFF1A2948U);
        if (status != SYNTRA_STATUS_OK) return status;
        status = syntra_framebuffer_draw_text(
            framebuffer, left + 5U, top + 6U, "FILES",
            COLOR_WINDOW_TEXT, 0xFF1A2948U, 1U);
        if (status != SYNTRA_STATUS_OK) return status;

        for (row = 0U; row < SYNTRA_GUI_FILES_ENTRY_COUNT; ++row)
        {
            row_y = top + 8U + row * 22U;
            if (row_y + 16U >= top + inner_height) break;
            visible_rows++;
        }

        for (row = 0U; row < visible_rows; ++row)
        {
            uint32_t entry = scroll_offset + row;
            row_y = top + 8U + row * 22U;
            if (entry >= SYNTRA_GUI_FILES_ENTRY_COUNT) break;
            if (window->app_selection == entry)
            {
                status = syntra_framebuffer_fill_rect(
                    framebuffer, left + 38U, row_y - 2U,
                    inner_width - 42U, 18U, 0xFF25466FU);
                if (status != SYNTRA_STATUS_OK) return status;
            }
            status = syntra_framebuffer_fill_rect(
                framebuffer, left + 42U, row_y, 12U, 9U, 0xFF54D6E8U);
            if (status != SYNTRA_STATUS_OK) return status;
            status = syntra_framebuffer_fill_rect(
                framebuffer, left + 42U, row_y + 3U, 12U, 9U, 0xFF3B9FB5U);
            if (status != SYNTRA_STATUS_OK) return status;
            status = syntra_framebuffer_draw_text(
                framebuffer, left + 62U, row_y + 2U, names[entry],
                COLOR_WINDOW_TEXT,
                window->app_selection == entry ? 0xFF25466FU : 0xFF0C1428U,
                1U);
            if (status != SYNTRA_STATUS_OK) return status;
        }

        if (visible_rows < SYNTRA_GUI_FILES_ENTRY_COUNT)
        {
            uint32_t track_x = left + inner_width - 5U;
            uint32_t track_y = top + 6U;
            uint32_t track_height = inner_height - 12U;
            uint32_t thumb_height = (uint32_t)(
                ((uint64_t)track_height * visible_rows) /
                SYNTRA_GUI_FILES_ENTRY_COUNT);
            uint32_t max_offset =
                SYNTRA_GUI_FILES_ENTRY_COUNT - visible_rows;
            uint32_t thumb_y;
            if (scroll_offset > max_offset) scroll_offset = max_offset;
            if (thumb_height < 8U) thumb_height = 8U;
            if (thumb_height > track_height) thumb_height = track_height;
            thumb_y = track_y;
            if (max_offset != 0U)
                thumb_y += (uint32_t)(
                    ((uint64_t)(track_height - thumb_height) *
                     scroll_offset) / max_offset);
            status = syntra_framebuffer_fill_rect(
                framebuffer, track_x, track_y, 3U, track_height,
                0xFF263D66U);
            if (status != SYNTRA_STATUS_OK) return status;
            status = syntra_framebuffer_fill_rect(
                framebuffer, track_x, thumb_y, 3U, thumb_height,
                0xFF54D6E8U);
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
        static const char *const descriptions[] = {
            "SCREEN AND DISPLAY OPTIONS",
            "KEYBOARD AND POINTER OPTIONS",
            "DEVICE AND OS INFORMATION",
            "SYNTRAOS GUI PREVIEW"
        };
        uint32_t selected = window->app_selection < 4U
            ? window->app_selection : 0U;
        for (row = 0U; row < 4U; ++row)
        {
            row_y = top + 5U + row * 22U;
            if (row_y + 18U >= top + inner_height) break;
            status = syntra_framebuffer_fill_rect(
                framebuffer, left + 5U, row_y, inner_width - 10U, 17U,
                window->app_selection == row ? 0xFF25466FU : 0xFF1A2948U);
            if (status != SYNTRA_STATUS_OK) return status;
            status = syntra_framebuffer_fill_rect(
                framebuffer, left + 8U, row_y + 4U, 5U, 9U,
                0xFF54D6E8U);
            if (status != SYNTRA_STATUS_OK) return status;
            status = syntra_framebuffer_draw_text(
                framebuffer, left + 20U, row_y + 5U, names[row],
                COLOR_WINDOW_TEXT,
                window->app_selection == row ? 0xFF25466FU : 0xFF1A2948U,
                1U);
            if (status != SYNTRA_STATUS_OK) return status;
        }
        if (inner_height >= 112U)
        {
            uint32_t panel_y = top + 98U;
            status = syntra_framebuffer_fill_rect(
                framebuffer, left + 5U, panel_y, inner_width - 10U,
                inner_height - 103U, 0xFF1A2948U);
            if (status != SYNTRA_STATUS_OK) return status;
            status = syntra_framebuffer_fill_rect(
                framebuffer, left + 5U, panel_y, 3U,
                inner_height - 103U, 0xFF54D6E8U);
            if (status != SYNTRA_STATUS_OK) return status;
            status = syntra_framebuffer_draw_text(
                framebuffer, left + 13U, panel_y + 8U, names[selected],
                COLOR_WINDOW_TEXT, 0xFF1A2948U, 1U);
            if (status != SYNTRA_STATUS_OK) return status;
            status = syntra_framebuffer_draw_text(
                framebuffer, left + 13U, panel_y + 27U, descriptions[selected],
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
    uint32_t maximize_x;
    uint32_t minimize_x;
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
            framebuffer, close_x, window->y + WINDOW_BORDER + 2U,
            WINDOW_CLOSE_SIZE, WINDOW_CLOSE_SIZE, COLOR_WINDOW_CLOSE);
        if (status != SYNTRA_STATUS_OK) return status;
        status = syntra_framebuffer_draw_text(
            framebuffer, close_x + 3U, window->y + WINDOW_BORDER + 4U,
            "X", COLOR_WINDOW_TEXT, COLOR_WINDOW_CLOSE, 1U);
        if (status != SYNTRA_STATUS_OK) return status;

        if (window->width >= WINDOW_CONTROLS_MIN_WIDTH)
        {
            maximize_x = close_x - WINDOW_CONTROL_GAP - WINDOW_CLOSE_SIZE;
            minimize_x = maximize_x - WINDOW_CONTROL_GAP - WINDOW_CLOSE_SIZE;
            status = syntra_framebuffer_fill_rect(
                framebuffer, maximize_x, window->y + WINDOW_BORDER + 2U,
                WINDOW_CLOSE_SIZE, WINDOW_CLOSE_SIZE, COLOR_WINDOW_MAXIMIZE);
            if (status != SYNTRA_STATUS_OK) return status;
            status = syntra_framebuffer_fill_rect(
                framebuffer, maximize_x + 3U, window->y + WINDOW_BORDER + 5U,
                6U, 6U, title_color);
            if (status != SYNTRA_STATUS_OK) return status;
            status = syntra_framebuffer_fill_rect(
                framebuffer, minimize_x, window->y + WINDOW_BORDER + 2U,
                WINDOW_CLOSE_SIZE, WINDOW_CLOSE_SIZE, COLOR_WINDOW_MINIMIZE);
            if (status != SYNTRA_STATUS_OK) return status;
            status = syntra_framebuffer_fill_rect(
                framebuffer, minimize_x + 3U, window->y + WINDOW_BORDER + 7U,
                6U, 2U, title_color);
            if (status != SYNTRA_STATUS_OK) return status;
        }

        status = syntra_framebuffer_draw_text(
            framebuffer, window->x + 7U, window->y + 7U, title_text,
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

        if (window->minimized != 0U) continue;
        if (window->id == 0U || window->width == 0U ||
            window->height == 0U ||
            window->width > framebuffer->width ||
            window->height > framebuffer->height ||
            (window->app_type != SYNTRA_GUI_APP_GENERIC &&
             window->app_type != SYNTRA_GUI_APP_FILES &&
             window->app_type != SYNTRA_GUI_APP_SETTINGS &&
             window->app_type != SYNTRA_GUI_APP_LAUNCHER) ||
            window->x > framebuffer->width - window->width ||
            window->y > framebuffer->height - window->height)
            return SYNTRA_STATUS_INVALID_ARGUMENT;

        status = draw_window(framebuffer, window, active);
        if (status != SYNTRA_STATUS_OK) return status;
    }

    return SYNTRA_STATUS_OK;
}
