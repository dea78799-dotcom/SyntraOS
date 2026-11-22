#include "gui_runtime.h"

#include <stddef.h>
#include "desktop.h"
#include "window_renderer.h"

syntra_status_t syntra_gui_runtime_init(
    syntra_gui_runtime_t *runtime,
    uint32_t width,
    uint32_t height)
{
    syntra_status_t status;

    if (runtime == NULL || width < 320U || height < 240U)
        return SYNTRA_STATUS_INVALID_ARGUMENT;

    runtime->initialized = 0;
    runtime->redraw_count = 0U;

    status = syntra_input_init(&runtime->input, width, height);
    if (status != SYNTRA_STATUS_OK) return status;

    if (width >= 640U && height >= 480U)
    {
        status = syntra_desktop_gui_v5_init_windows(
            &runtime->windows, width, height);
    }
    else
    {
        status = syntra_gui_window_manager_init(
            &runtime->windows, width, height);
    }
    if (status != SYNTRA_STATUS_OK) return status;

    status = syntra_gui_window_interaction_init(&runtime->interaction);
    if (status != SYNTRA_STATUS_OK) return status;

    runtime->initialized = 1;
    return SYNTRA_STATUS_OK;
}

/* Launcher tile activation is handled in normal input context. */
static syntra_status_t activate_launcher_tile(
    syntra_gui_runtime_t *runtime,
    uint32_t tile)
{
    syntra_gui_app_type_t app_type;
    uint32_t index;
    uint32_t existing_id = 0U;
    uint32_t new_id;
    syntra_status_t status;

    if (tile == 0U) app_type = SYNTRA_GUI_APP_FILES;
    else if (tile == 1U) app_type = SYNTRA_GUI_APP_SETTINGS;
    else return SYNTRA_STATUS_OK; /* APP/INFO are reserved surfaces. */

    for (index = 0U; index < runtime->windows.count; ++index)
    {
        if (runtime->windows.windows[index].app_type == app_type)
        {
            existing_id = runtime->windows.windows[index].id;
            break;
        }
    }

    if (existing_id != 0U)
        return syntra_gui_window_focus(&runtime->windows, existing_id);

    if (app_type == SYNTRA_GUI_APP_FILES)
        status = syntra_gui_window_open_app(
            &runtime->windows, 30U, 190U, 270U, 230U,
            app_type, &new_id);
    else
        status = syntra_gui_window_open_app(
            &runtime->windows, 340U, 190U, 270U, 230U,
            app_type, &new_id);

    return status;
}

/* The desktop dock's cyan home button restores the launcher. */
static syntra_status_t handle_launcher_dock_click(
    syntra_gui_runtime_t *runtime,
    uint32_t pointer_x,
    uint32_t pointer_y)
{
    uint32_t index;
    uint32_t launcher_id = 0U;

    if (pointer_x < 14U || pointer_x >= 40U ||
        pointer_y < runtime->input.height - 34U ||
        pointer_y >= runtime->input.height - 8U)
        return SYNTRA_STATUS_OK;

    for (index = 0U; index < runtime->windows.count; ++index)
    {
        if (runtime->windows.windows[index].app_type ==
            SYNTRA_GUI_APP_LAUNCHER)
        {
            launcher_id = runtime->windows.windows[index].id;
            break;
        }
    }

    if (launcher_id != 0U)
        return syntra_gui_window_focus(&runtime->windows, launcher_id);

    return syntra_gui_window_open_app(
        &runtime->windows, 230U, 60U, 180U, 140U,
        SYNTRA_GUI_APP_LAUNCHER, &launcher_id);
}

static syntra_status_t handle_launcher_click(
    syntra_gui_runtime_t *runtime,
    uint32_t pointer_x,
    uint32_t pointer_y)
{
    uint32_t index;
    const syntra_gui_window_t *launcher = NULL;
    uint32_t tile_width;
    uint32_t tile_x;
    uint32_t tile_y;
    uint32_t local_x;
    uint32_t local_y;
    uint32_t tile;
    
    for (index = 0U; index < runtime->windows.count; ++index)
    {
        if (runtime->windows.windows[index].app_type ==
            SYNTRA_GUI_APP_LAUNCHER)
        {
            launcher = &runtime->windows.windows[index];
            break;
        }
    }
    if (launcher == NULL || pointer_x < launcher->x + 4U ||
        pointer_y < launcher->y + 24U)
        return SYNTRA_STATUS_OK;

    local_x = pointer_x - (launcher->x + 8U);
    local_y = pointer_y - (launcher->y + 28U);
    tile_width = (launcher->width - 20U) / 2U;
    if (local_x >= 2U * tile_width + 4U || local_y >= 64U)
        return SYNTRA_STATUS_OK;

    tile_x = local_x / (tile_width + 4U);
    tile_y = local_y / 32U;
    if (tile_x > 1U || tile_y > 1U ||
        (local_x % (tile_width + 4U)) >= tile_width ||
        (local_y % 32U) >= 24U)
        return SYNTRA_STATUS_OK;
    tile = tile_y * 2U + tile_x;
    return activate_launcher_tile(runtime, tile);
}

syntra_status_t syntra_gui_runtime_process_event(
    syntra_gui_runtime_t *runtime,
    const syntra_input_event_t *event)
{
    syntra_status_t status;
    uint32_t old_buttons;

    if (runtime == NULL || runtime->initialized != 1 || event == NULL)
        return SYNTRA_STATUS_INVALID_ARGUMENT;

    old_buttons = runtime->input.buttons;
    status = syntra_input_process(&runtime->input, event);
    if (status != SYNTRA_STATUS_OK) return status;

    status = syntra_gui_window_interaction_process(
        &runtime->interaction, &runtime->windows, &runtime->input);
    if (status != SYNTRA_STATUS_OK) return status;

    if (event->type == SYNTRA_INPUT_EV_KEY &&
        event->code == SYNTRA_INPUT_BTN_LEFT && event->value == 1 &&
        (old_buttons & SYNTRA_INPUT_BUTTON_LEFT) == 0U)
    {
        status = handle_launcher_dock_click(
            runtime, runtime->input.pointer_x, runtime->input.pointer_y);
        if (status != SYNTRA_STATUS_OK) return status;
        status = handle_launcher_click(
            runtime, runtime->input.pointer_x, runtime->input.pointer_y);
        if (status != SYNTRA_STATUS_OK) return status;
    }

    return SYNTRA_STATUS_OK;
}

syntra_status_t syntra_gui_runtime_render(
    syntra_gui_runtime_t *runtime,
    syntra_framebuffer_t *framebuffer)
{
    syntra_status_t status;

    if (runtime == NULL || runtime->initialized != 1 ||
        framebuffer == NULL ||
        framebuffer->width != runtime->input.width ||
        framebuffer->height != runtime->input.height)
        return SYNTRA_STATUS_INVALID_ARGUMENT;

    status = syntra_desktop_render(framebuffer);
    if (status != SYNTRA_STATUS_OK) return status;

    if (runtime->windows.count != 0U)
    {
        status = syntra_gui_render_windows(framebuffer, &runtime->windows);
        if (status != SYNTRA_STATUS_OK) return status;
    }

    if (runtime->redraw_count < UINT32_MAX)
        runtime->redraw_count++;
    return SYNTRA_STATUS_OK;
}
