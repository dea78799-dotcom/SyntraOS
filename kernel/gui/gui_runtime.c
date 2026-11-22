#include "gui_runtime.h"

#include <stddef.h>
#include "desktop.h"
#include "window_renderer.h"

static uint32_t gesture_target_at(const syntra_gui_runtime_t *runtime,
                                  uint32_t x, uint32_t y)
{
    uint32_t index = runtime->windows.count;
    while (index > 0U)
    {
        const syntra_gui_window_t *window = &runtime->windows.windows[--index];
        if (x >= window->x && y >= window->y &&
            x - window->x < window->width &&
            y - window->y < window->height)
            return window->id;
    }
    return UINT32_MAX; /* Reserved ID: cannot collide with a managed window. */
}

static void update_hover(syntra_gui_runtime_t *runtime)
{
    runtime->hover_target_id = gesture_target_at(
        runtime, runtime->input.pointer_x, runtime->input.pointer_y);
}

static uint32_t files_visible_rows(const syntra_gui_window_t *window)
{
    uint32_t inner_height = window->height > 28U
        ? window->height - 28U : 0U;
    uint32_t row;
    uint32_t visible = 0U;
    for (row = 0U; row < SYNTRA_GUI_FILES_ENTRY_COUNT; ++row)
    {
        uint32_t row_y = 8U + row * 22U;
        if (row_y + 16U >= inner_height) break;
        visible++;
    }
    return visible;
}

static void apply_wheel_scroll(syntra_gui_runtime_t *runtime,
                               int64_t wheel_delta)
{
    uint32_t target = gesture_target_at(
        runtime, runtime->input.pointer_x, runtime->input.pointer_y);
    const syntra_gui_window_t *constant_window = NULL;
    syntra_gui_window_t *window = NULL;
    uint32_t visible_rows;
    uint32_t max_offset;
    uint64_t steps;

    if (wheel_delta == 0 || target == UINT32_MAX ||
        syntra_gui_window_get(&runtime->windows, target,
                              &constant_window) != SYNTRA_STATUS_OK)
        return;
    if (constant_window->app_type != SYNTRA_GUI_APP_FILES ||
        runtime->input.pointer_x < constant_window->x + 42U ||
        runtime->input.pointer_y < constant_window->y + 24U ||
        runtime->input.pointer_x >= constant_window->x +
                                    constant_window->width - 4U ||
        runtime->input.pointer_y >= constant_window->y +
                                    constant_window->height - 4U)
        return;

    window = &runtime->windows.windows[0];
    {
        uint32_t index;
        for (index = 0U; index < runtime->windows.count; ++index)
        {
            if (runtime->windows.windows[index].id == target)
            {
                window = &runtime->windows.windows[index];
                break;
            }
        }
    }

    visible_rows = files_visible_rows(window);
    if (visible_rows >= SYNTRA_GUI_FILES_ENTRY_COUNT) max_offset = 0U;
    else max_offset = SYNTRA_GUI_FILES_ENTRY_COUNT - visible_rows;

    if (wheel_delta > 0)
    {
        steps = (uint64_t)wheel_delta;
        if (steps > window->app_scroll_offset)
            steps = window->app_scroll_offset;
        window->app_scroll_offset -= (uint32_t)steps;
    }
    else
    {
        steps = (uint64_t)(-wheel_delta);
        if (steps > (uint64_t)(max_offset - window->app_scroll_offset))
            steps = (uint64_t)(max_offset - window->app_scroll_offset);
        window->app_scroll_offset += (uint32_t)steps;
    }
}

static syntra_status_t render_hover_outline(
    syntra_gui_runtime_t *runtime, syntra_framebuffer_t *framebuffer)
{
    const syntra_gui_window_t *window = NULL;
    uint32_t x, y, width, height;
    syntra_status_t status;

    if (runtime->hover_target_id == 0U ||
        runtime->hover_target_id == UINT32_MAX)
        return SYNTRA_STATUS_OK;
    status = syntra_gui_window_get(
        &runtime->windows, runtime->hover_target_id, &window);
    if (status != SYNTRA_STATUS_OK)
    {
        runtime->hover_target_id = UINT32_MAX;
        return SYNTRA_STATUS_OK;
    }
    x = window->x;
    y = window->y;
    width = window->width;
    height = window->height;
    if (width < 2U || height < 2U) return SYNTRA_STATUS_OK;

    status = syntra_framebuffer_fill_rect(
        framebuffer, x, y, width, 2U, 0xFF54D6E8U);
    if (status != SYNTRA_STATUS_OK) return status;
    status = syntra_framebuffer_fill_rect(
        framebuffer, x, y + height - 2U, width, 2U, 0xFF54D6E8U);
    if (status != SYNTRA_STATUS_OK) return status;
    status = syntra_framebuffer_fill_rect(
        framebuffer, x, y, 2U, height, 0xFF54D6E8U);
    if (status != SYNTRA_STATUS_OK) return status;
    return syntra_framebuffer_fill_rect(
        framebuffer, x + width - 2U, y, 2U, height, 0xFF54D6E8U);
}

static syntra_status_t update_gesture(syntra_gui_runtime_t *runtime,
                                      uint64_t now_ms)
{
    uint32_t buttons = 0U;
    uint32_t target;
    if ((runtime->input.buttons & SYNTRA_INPUT_BUTTON_LEFT) != 0U)
        buttons |= SYNTRA_GESTURE_BUTTON_LEFT;
    if ((runtime->input.buttons & SYNTRA_INPUT_BUTTON_RIGHT) != 0U)
        buttons |= SYNTRA_GESTURE_BUTTON_RIGHT;
    target = gesture_target_at(runtime, runtime->input.pointer_x,
                               runtime->input.pointer_y);
    runtime->gesture_time_ms = now_ms;
    return syntra_gesture_update(&runtime->gesture, buttons, target,
        runtime->input.pointer_x, runtime->input.pointer_y, now_ms,
        &runtime->last_gesture);
}


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
    status = syntra_gesture_init(&runtime->gesture);
    if (status != SYNTRA_STATUS_OK) return status;
    runtime->gesture_time_ms = 0ULL;
    runtime->context_menu_x = 0U;
    runtime->context_menu_y = 0U;
    runtime->context_menu_target = 0U;
    runtime->hover_target_id = UINT32_MAX;
    runtime->last_wheel_y = 0;
    runtime->context_menu_visible = 0U;
    runtime->last_gesture.action = SYNTRA_GESTURE_NONE;

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

    if (runtime->input.width < 640U || runtime->input.height < 480U)
    {
        if (app_type == SYNTRA_GUI_APP_FILES)
            status = syntra_gui_window_open_app(
                &runtime->windows, 10U, 40U, 240U, 170U,
                app_type, &new_id);
        else
            status = syntra_gui_window_open_app(
                &runtime->windows, 70U, 40U, 240U, 170U,
                app_type, &new_id);
    }
    else if (app_type == SYNTRA_GUI_APP_FILES)
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

    if (runtime->input.width < 640U || runtime->input.height < 480U)
    {
        if (pointer_x < 10U || pointer_x >= 20U ||
            pointer_y < runtime->input.height - 20U ||
            pointer_y >= runtime->input.height - 10U)
            return SYNTRA_STATUS_OK;
    }
    else if (pointer_x < 14U || pointer_x >= 40U ||
             pointer_y < runtime->input.height - 34U ||
             pointer_y >= runtime->input.height - 8U)
    {
        return SYNTRA_STATUS_OK;
    }

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

    if (runtime->input.width < 640U || runtime->input.height < 480U)
        return syntra_gui_window_open_app(
            &runtime->windows, 70U, 45U, 180U, 140U,
            SYNTRA_GUI_APP_LAUNCHER, &launcher_id);

    return syntra_gui_window_open_app(
        &runtime->windows, 230U, 60U, 180U, 140U,
        SYNTRA_GUI_APP_LAUNCHER, &launcher_id);
}

/* Select visible Files entries and Settings categories on click. */
static syntra_status_t handle_app_surface_click(
    syntra_gui_runtime_t *runtime,
    uint32_t pointer_x,
    uint32_t pointer_y)
{
    syntra_gui_window_t *window;
    uint32_t left, top, inner_width, inner_height, row;
    if (runtime->windows.count == 0U) return SYNTRA_STATUS_OK;
    window = &runtime->windows.windows[runtime->windows.count - 1U];
    left = window->x + 4U;
    top = window->y + 24U;
    inner_width = window->width > 8U ? window->width - 8U : 0U;
    inner_height = window->height > 28U ? window->height - 28U : 0U;
    if (pointer_x < left || pointer_y < top ||
        pointer_x >= left + inner_width || pointer_y >= top + inner_height)
        return SYNTRA_STATUS_OK;

    if (window->app_type == SYNTRA_GUI_APP_FILES)
    {
        if (pointer_x < left + 38U) return SYNTRA_STATUS_OK;
        for (row = 0U; row < SYNTRA_GUI_FILES_ENTRY_COUNT; ++row)
        {
            uint32_t entry = window->app_scroll_offset + row;
            uint32_t row_y = top + 8U + row * 22U;
            if (entry >= SYNTRA_GUI_FILES_ENTRY_COUNT ||
                row_y + 16U >= top + inner_height)
                break;
            if (pointer_y >= row_y && pointer_y < row_y + 16U)
            {
                window->app_selection = entry;
                break;
            }
        }
    }
    else if (window->app_type == SYNTRA_GUI_APP_SETTINGS)
    {
        for (row = 0U; row < 4U; ++row)
        {
            uint32_t row_y = top + 5U + row * 22U;
            if (row_y + 18U >= top + inner_height) break;
            if (pointer_x >= left + 5U && pointer_x < left + inner_width - 5U &&
                pointer_y >= row_y && pointer_y < row_y + 17U)
            {
                window->app_selection = row;
                break;
            }
        }
    }
    return SYNTRA_STATUS_OK;
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
    /*
     * Only dispatch launcher tiles when Launcher is the frontmost window.
     * Otherwise a window dragged over another app could steal clicks from
     * the app that actually received the pointer hit-test.
     */
    if (launcher == NULL ||
        launcher->width <= 20U ||
        launcher->height <= 28U ||
        syntra_gui_window_active(&runtime->windows) == NULL ||
        syntra_gui_window_active(&runtime->windows)->app_type !=
            SYNTRA_GUI_APP_LAUNCHER ||
        pointer_x < launcher->x + 4U ||
        pointer_y < launcher->y + 24U)
        return SYNTRA_STATUS_OK;

    /*
     * Match renderer geometry exactly: the first tile starts four pixels
     * inside the app surface, with a four-pixel gap between columns/rows.
     */
    if (pointer_x < launcher->x + 8U ||
        pointer_y < launcher->y + 28U)
        return SYNTRA_STATUS_OK;
    local_x = pointer_x - (launcher->x + 8U);
    local_y = pointer_y - (launcher->y + 28U);
    tile_width = (launcher->width - 20U) / 2U;
    if (local_x >= 2U * tile_width + 4U || local_y >= 60U)
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

#define SYNTRA_CONTEXT_MENU_WIDTH 136U
#define SYNTRA_CONTEXT_MENU_HEIGHT 72U

static void open_context_menu(syntra_gui_runtime_t *runtime,
                              uint32_t x, uint32_t y, uint32_t target)
{
    uint32_t max_x = runtime->input.width - SYNTRA_CONTEXT_MENU_WIDTH;
    uint32_t max_y = runtime->input.height - SYNTRA_CONTEXT_MENU_HEIGHT;
    runtime->context_menu_x = x > max_x ? max_x : x;
    runtime->context_menu_y = y > max_y ? max_y : y;
    runtime->context_menu_target = target;
    runtime->context_menu_visible = 1U;
}

static syntra_status_t handle_context_menu_click(
    syntra_gui_runtime_t *runtime, uint32_t x, uint32_t y)
{
    uint32_t menu_x = runtime->context_menu_x;
    uint32_t menu_y = runtime->context_menu_y;
    uint32_t target = runtime->context_menu_target;
    uint32_t row;
    syntra_status_t status = SYNTRA_STATUS_OK;

    if (x >= menu_x && x < menu_x + SYNTRA_CONTEXT_MENU_WIDTH &&
        y >= menu_y + 22U && y < menu_y + 67U)
    {
        row = (y - menu_y - 22U) / 15U;
        if (row < 2U)
        {
            if (target == UINT32_MAX)
                status = activate_launcher_tile(runtime, row);
            else if (row == 0U)
                status = syntra_gui_window_focus(&runtime->windows, target);
            else
                status = syntra_gui_window_close(&runtime->windows, target);
        }
    }

    runtime->context_menu_visible = 0U;
    runtime->context_menu_target = 0U;
    return status;
}

static syntra_status_t render_context_menu(
    syntra_gui_runtime_t *runtime, syntra_framebuffer_t *framebuffer)
{
    static const char *const window_items[] = {"FOCUS", "CLOSE", "DISMISS"};
    static const char *const desktop_items[] = {
        "OPEN FILES", "OPEN SETTINGS", "DISMISS"
    };
    const char *const *items = runtime->context_menu_target == UINT32_MAX
        ? desktop_items : window_items;
    uint32_t x = runtime->context_menu_x;
    uint32_t y = runtime->context_menu_y;
    uint32_t row;
    syntra_status_t status;

    status = syntra_framebuffer_fill_rect(
        framebuffer, x, y, SYNTRA_CONTEXT_MENU_WIDTH,
        SYNTRA_CONTEXT_MENU_HEIGHT, 0xFF6D8DB8U);
    if (status != SYNTRA_STATUS_OK) return status;
    status = syntra_framebuffer_fill_rect(
        framebuffer, x + 2U, y + 2U, SYNTRA_CONTEXT_MENU_WIDTH - 4U,
        SYNTRA_CONTEXT_MENU_HEIGHT - 4U, 0xFF101A2CU);
    if (status != SYNTRA_STATUS_OK) return status;
    status = syntra_framebuffer_fill_rect(
        framebuffer, x + 2U, y + 2U, SYNTRA_CONTEXT_MENU_WIDTH - 4U,
        17U, 0xFF263D66U);
    if (status != SYNTRA_STATUS_OK) return status;
    status = syntra_framebuffer_draw_text(
        framebuffer, x + 8U, y + 7U,
        runtime->context_menu_target == UINT32_MAX ? "DESKTOP" : "WINDOW",
        0xFFF4F7FFU, 0xFF263D66U, 1U);
    if (status != SYNTRA_STATUS_OK) return status;

    for (row = 0U; row < 3U; ++row)
    {
        uint32_t row_y = y + 22U + row * 15U;
        status = syntra_framebuffer_draw_text(
            framebuffer, x + 8U, row_y + 4U, items[row],
            row == 2U ? 0xFF9BAAC2U : 0xFFF4F7FFU,
            0xFF101A2CU, 1U);
        if (status != SYNTRA_STATUS_OK) return status;
    }
    return SYNTRA_STATUS_OK;
}

syntra_status_t syntra_gui_runtime_process_event_at(
    syntra_gui_runtime_t *runtime,
    const syntra_input_event_t *event,
    uint64_t now_ms)
{
    syntra_status_t status;
    uint32_t old_buttons;

    if (runtime == NULL || runtime->initialized != 1 || event == NULL)
        return SYNTRA_STATUS_INVALID_ARGUMENT;

    int64_t wheel_delta;
    old_buttons = runtime->input.buttons;
    status = syntra_input_process(&runtime->input, event);
    if (status != SYNTRA_STATUS_OK) return status;

    wheel_delta = (int64_t)runtime->input.wheel_y -
                  (int64_t)runtime->last_wheel_y;
    runtime->last_wheel_y = runtime->input.wheel_y;

    /* A visible menu owns the next primary click, including outside dismissal. */
    if (runtime->context_menu_visible != 0U &&
        event->type == SYNTRA_INPUT_EV_KEY &&
        event->code == SYNTRA_INPUT_BTN_LEFT && event->value == 1 &&
        (old_buttons & SYNTRA_INPUT_BUTTON_LEFT) == 0U)
        return handle_context_menu_click(
            runtime, runtime->input.pointer_x, runtime->input.pointer_y);

    status = syntra_gui_window_interaction_process(
        &runtime->interaction, &runtime->windows, &runtime->input);
    if (status != SYNTRA_STATUS_OK) return status;

    update_hover(runtime);
    if (runtime->context_menu_visible == 0U)
        apply_wheel_scroll(runtime, wheel_delta);

    status = update_gesture(runtime, now_ms);
    if (status != SYNTRA_STATUS_OK) return status;

    if (runtime->last_gesture.action == SYNTRA_GESTURE_CONTEXT_REQUEST)
        open_context_menu(runtime, runtime->last_gesture.x,
                          runtime->last_gesture.y,
                          runtime->last_gesture.target_id);

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
        status = handle_app_surface_click(
            runtime, runtime->input.pointer_x, runtime->input.pointer_y);
        if (status != SYNTRA_STATUS_OK) return status;
    }

    return SYNTRA_STATUS_OK;
}

syntra_status_t syntra_gui_runtime_process_event(
    syntra_gui_runtime_t *runtime,
    const syntra_input_event_t *event)
{
    uint64_t now_ms;
    if (runtime == NULL || runtime->initialized != 1)
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    now_ms = (uint64_t)runtime->input.event_count * 10ULL;
    return syntra_gui_runtime_process_event_at(runtime, event, now_ms);
}

syntra_status_t syntra_gui_runtime_tick(
    syntra_gui_runtime_t *runtime,
    uint64_t now_ms)
{
    if (runtime == NULL || runtime->initialized != 1)
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    return update_gesture(runtime, now_ms);
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

    status = render_hover_outline(runtime, framebuffer);
    if (status != SYNTRA_STATUS_OK) return status;

    if (runtime->context_menu_visible != 0U)
    {
        status = render_context_menu(runtime, framebuffer);
        if (status != SYNTRA_STATUS_OK) return status;
    }

    if (runtime->redraw_count < UINT32_MAX)
        runtime->redraw_count++;
    return SYNTRA_STATUS_OK;
}
