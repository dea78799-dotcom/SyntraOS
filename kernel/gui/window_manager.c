#include "window_manager.h"

#include <stddef.h>

static int window_bounds_valid(
    const syntra_gui_window_manager_t *manager,
    uint32_t x,
    uint32_t y,
    uint32_t width,
    uint32_t height)
{
    return width != 0U && height != 0U &&
           width <= manager->screen_width &&
           height <= manager->screen_height &&
           x <= manager->screen_width - width &&
           y <= manager->screen_height - height;
}

static int find_window(
    const syntra_gui_window_manager_t *manager,
    uint32_t window_id)
{
    uint32_t index;

    for (index = 0U; index < manager->count; ++index)
    {
        if (manager->windows[index].id == window_id)
            return (int)index;
    }
    return -1;
}

syntra_status_t syntra_gui_window_manager_init(
    syntra_gui_window_manager_t *manager,
    uint32_t screen_width,
    uint32_t screen_height)
{
    uint32_t index;

    if (manager == NULL || screen_width == 0U || screen_height == 0U)
        return SYNTRA_STATUS_INVALID_ARGUMENT;

    manager->screen_width = screen_width;
    manager->screen_height = screen_height;
    manager->next_id = 1U;
    manager->count = 0U;
    manager->initialized = 1;

    for (index = 0U; index < SYNTRA_GUI_MAX_WINDOWS; ++index)
    {
        manager->windows[index].id = 0U;
        manager->windows[index].x = 0U;
        manager->windows[index].y = 0U;
        manager->windows[index].width = 0U;
        manager->windows[index].height = 0U;
        manager->windows[index].app_type = SYNTRA_GUI_APP_GENERIC;
        manager->windows[index].app_selection = 0U;
        manager->windows[index].app_scroll_offset = 0U;
    }

    return SYNTRA_STATUS_OK;
}

syntra_status_t syntra_gui_window_open_app(
    syntra_gui_window_manager_t *manager,
    uint32_t x,
    uint32_t y,
    uint32_t width,
    uint32_t height,
    syntra_gui_app_type_t app_type,
    uint32_t *window_id)
{
    syntra_gui_window_t *window;

    if (manager == NULL || manager->initialized != 1 || window_id == NULL ||
        (app_type != SYNTRA_GUI_APP_GENERIC &&
         app_type != SYNTRA_GUI_APP_FILES &&
         app_type != SYNTRA_GUI_APP_SETTINGS &&
         app_type != SYNTRA_GUI_APP_LAUNCHER) ||
        !window_bounds_valid(manager, x, y, width, height))
        return SYNTRA_STATUS_INVALID_ARGUMENT;

    if (manager->count >= SYNTRA_GUI_MAX_WINDOWS)
        return SYNTRA_STATUS_OUT_OF_MEMORY;
    if (manager->next_id == 0U)
        return SYNTRA_STATUS_OUT_OF_MEMORY;

    window = &manager->windows[manager->count];
    window->id = manager->next_id;
    window->x = x;
    window->y = y;
    window->width = width;
    window->height = height;
    window->app_type = app_type;
    window->app_selection = 0U;
    window->app_scroll_offset = 0U;

    *window_id = manager->next_id;
    manager->next_id++;
    manager->count++;
    return SYNTRA_STATUS_OK;
}

syntra_status_t syntra_gui_window_open(
    syntra_gui_window_manager_t *manager,
    uint32_t x,
    uint32_t y,
    uint32_t width,
    uint32_t height,
    uint32_t *window_id)
{
    return syntra_gui_window_open_app(
        manager, x, y, width, height, SYNTRA_GUI_APP_GENERIC, window_id);
}

syntra_status_t syntra_gui_window_close(
    syntra_gui_window_manager_t *manager,
    uint32_t window_id)
{
    int found;
    uint32_t index;

    if (manager == NULL || manager->initialized != 1 || window_id == 0U)
        return SYNTRA_STATUS_INVALID_ARGUMENT;

    found = find_window(manager, window_id);
    if (found < 0) return SYNTRA_STATUS_INVALID_ARGUMENT;

    for (index = (uint32_t)found; index + 1U < manager->count; ++index)
        manager->windows[index] = manager->windows[index + 1U];

    manager->count--;
    manager->windows[manager->count].id = 0U;
    manager->windows[manager->count].x = 0U;
    manager->windows[manager->count].y = 0U;
    manager->windows[manager->count].width = 0U;
    manager->windows[manager->count].height = 0U;
    manager->windows[manager->count].app_type = SYNTRA_GUI_APP_GENERIC;
    manager->windows[manager->count].app_selection = 0U;
    manager->windows[manager->count].app_scroll_offset = 0U;
    return SYNTRA_STATUS_OK;
}

syntra_status_t syntra_gui_window_move(
    syntra_gui_window_manager_t *manager,
    uint32_t window_id,
    uint32_t x,
    uint32_t y)
{
    int found;
    syntra_gui_window_t *window;

    if (manager == NULL || manager->initialized != 1 || window_id == 0U)
        return SYNTRA_STATUS_INVALID_ARGUMENT;

    found = find_window(manager, window_id);
    if (found < 0) return SYNTRA_STATUS_INVALID_ARGUMENT;

    window = &manager->windows[(uint32_t)found];
    if (!window_bounds_valid(manager, x, y, window->width, window->height))
        return SYNTRA_STATUS_INVALID_ARGUMENT;

    window->x = x;
    window->y = y;
    return SYNTRA_STATUS_OK;
}

syntra_status_t syntra_gui_window_focus(
    syntra_gui_window_manager_t *manager,
    uint32_t window_id)
{
    int found;
    syntra_gui_window_t selected;
    uint32_t index;

    if (manager == NULL || manager->initialized != 1 || window_id == 0U)
        return SYNTRA_STATUS_INVALID_ARGUMENT;

    found = find_window(manager, window_id);
    if (found < 0) return SYNTRA_STATUS_INVALID_ARGUMENT;
    if ((uint32_t)found == manager->count - 1U) return SYNTRA_STATUS_OK;

    selected = manager->windows[(uint32_t)found];
    for (index = (uint32_t)found; index + 1U < manager->count; ++index)
        manager->windows[index] = manager->windows[index + 1U];
    manager->windows[manager->count - 1U] = selected;
    return SYNTRA_STATUS_OK;
}

syntra_status_t syntra_gui_window_get(
    const syntra_gui_window_manager_t *manager,
    uint32_t window_id,
    const syntra_gui_window_t **window)
{
    int found;

    if (manager == NULL || manager->initialized != 1 ||
        window_id == 0U || window == NULL)
        return SYNTRA_STATUS_INVALID_ARGUMENT;

    found = find_window(manager, window_id);
    if (found < 0) return SYNTRA_STATUS_INVALID_ARGUMENT;

    *window = &manager->windows[(uint32_t)found];
    return SYNTRA_STATUS_OK;
}

const syntra_gui_window_t *syntra_gui_window_active(
    const syntra_gui_window_manager_t *manager)
{
    if (manager == NULL || manager->initialized != 1 || manager->count == 0U)
        return NULL;

    return &manager->windows[manager->count - 1U];
}
