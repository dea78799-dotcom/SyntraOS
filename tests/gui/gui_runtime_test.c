#include "../../kernel/gui/gui_runtime.h"
#include "../../kernel/input/input.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>

static void send_event(syntra_gui_runtime_t *runtime,
                       uint16_t type, uint16_t code, int32_t value)
{
    syntra_input_event_t event = {type, code, value};
    assert(syntra_gui_runtime_process_event(runtime, &event) ==
           SYNTRA_STATUS_OK);
}

static void move_pointer(syntra_gui_runtime_t *runtime,
                        int32_t dx, int32_t dy)
{
    send_event(runtime, SYNTRA_INPUT_EV_REL, SYNTRA_INPUT_REL_X, dx);
    send_event(runtime, SYNTRA_INPUT_EV_REL, SYNTRA_INPUT_REL_Y, dy);
}

int main(void)
{
    syntra_gui_runtime_t runtime;
    syntra_framebuffer_t framebuffer;
    static uint32_t pixels[640U * 480U];
    const syntra_gui_window_t *launcher;
    uint32_t launcher_id, files_id;
    uint32_t redraw_before;

    assert(syntra_gui_runtime_init(&runtime, 640U, 480U) ==
           SYNTRA_STATUS_OK);
    assert(runtime.windows.count == 3U);
    launcher_id = runtime.windows.windows[2].id;
    files_id = runtime.windows.windows[0].id;

    /* Pointer starts at framebuffer center (320,240); move to title bar. */
    move_pointer(&runtime, -70, -170);
    send_event(&runtime, SYNTRA_INPUT_EV_KEY, SYNTRA_INPUT_BTN_LEFT, 1);
    assert(runtime.interaction.drag_window_id == launcher_id);
    move_pointer(&runtime, 50, 50);
    assert(syntra_gui_window_get(&runtime.windows, launcher_id, &launcher) ==
           SYNTRA_STATUS_OK);
    assert(launcher->x == 280U && launcher->y == 110U);
    send_event(&runtime, SYNTRA_INPUT_EV_KEY, SYNTRA_INPUT_BTN_LEFT, 0);
    assert(runtime.interaction.drag_window_id == 0U);

    /* Move to Files close affordance and click; this must remove the window. */
    move_pointer(&runtime, -21, 80);
    send_event(&runtime, SYNTRA_INPUT_EV_KEY, SYNTRA_INPUT_BTN_LEFT, 1);
    assert(syntra_gui_window_get(&runtime.windows, files_id, &launcher) ==
           SYNTRA_STATUS_INVALID_ARGUMENT);
    send_event(&runtime, SYNTRA_INPUT_EV_KEY, SYNTRA_INPUT_BTN_LEFT, 0);
    assert(runtime.windows.count == 2U);

    framebuffer.pixels = pixels;
    framebuffer.width = 640U;
    framebuffer.height = 480U;
    framebuffer.stride_pixels = 640U;
    redraw_before = runtime.redraw_count;
    assert(syntra_gui_runtime_render(&runtime, &framebuffer) ==
           SYNTRA_STATUS_OK);
    assert(runtime.redraw_count > redraw_before);
    assert(pixels[0] != 0U);

    /* Launcher Files tile focuses the existing Files window. */
    assert(syntra_gui_runtime_init(&runtime, 640U, 480U) ==
           SYNTRA_STATUS_OK);
    move_pointer(&runtime, -78, -148); /* center -> Files tile center */
    send_event(&runtime, SYNTRA_INPUT_EV_KEY, SYNTRA_INPUT_BTN_LEFT, 1);
    assert(syntra_gui_window_active(&runtime.windows)->app_type ==
           SYNTRA_GUI_APP_FILES);
    send_event(&runtime, SYNTRA_INPUT_EV_KEY, SYNTRA_INPUT_BTN_LEFT, 0);

    /* Renderer padding and inter-tile gaps are not clickable. */
    assert(syntra_gui_runtime_init(&runtime, 640U, 480U) ==
           SYNTRA_STATUS_OK);
    move_pointer(&runtime, -85, -150); /* launcher left padding */
    send_event(&runtime, SYNTRA_INPUT_EV_KEY, SYNTRA_INPUT_BTN_LEFT, 1);
    assert(runtime.windows.count == 3U);
    assert(syntra_gui_window_active(&runtime.windows)->app_type ==
           SYNTRA_GUI_APP_LAUNCHER);
    send_event(&runtime, SYNTRA_INPUT_EV_KEY, SYNTRA_INPUT_BTN_LEFT, 0);
    move_pointer(&runtime, 85, 0); /* inter-column gap */
    send_event(&runtime, SYNTRA_INPUT_EV_KEY, SYNTRA_INPUT_BTN_LEFT, 1);
    assert(runtime.windows.count == 3U);
    assert(syntra_gui_window_active(&runtime.windows)->app_type ==
           SYNTRA_GUI_APP_LAUNCHER);
    send_event(&runtime, SYNTRA_INPUT_EV_KEY, SYNTRA_INPUT_BTN_LEFT, 0);

    /* Settings tile also focuses its existing window without duplication. */
    move_pointer(&runtime, 42, 0); /* gap -> Settings tile */
    send_event(&runtime, SYNTRA_INPUT_EV_KEY, SYNTRA_INPUT_BTN_LEFT, 1);
    assert(syntra_gui_window_active(&runtime.windows)->app_type ==
           SYNTRA_GUI_APP_SETTINGS);
    assert(runtime.windows.count == 3U);
    send_event(&runtime, SYNTRA_INPUT_EV_KEY, SYNTRA_INPUT_BTN_LEFT, 0);

    /* Closing Files then choosing it from the launcher creates it again. */
    /* Launcher overlaps Files close control, so focus Files first. */
    move_pointer(&runtime, -262, 105); /* Files title bar */
    send_event(&runtime, SYNTRA_INPUT_EV_KEY, SYNTRA_INPUT_BTN_LEFT, 1);
    send_event(&runtime, SYNTRA_INPUT_EV_KEY, SYNTRA_INPUT_BTN_LEFT, 0);
    move_pointer(&runtime, 180, 0); /* Files close control */
    send_event(&runtime, SYNTRA_INPUT_EV_KEY, SYNTRA_INPUT_BTN_LEFT, 1);
    assert(runtime.windows.count == 2U);
    send_event(&runtime, SYNTRA_INPUT_EV_KEY, SYNTRA_INPUT_BTN_LEFT, 0);
    move_pointer(&runtime, -18, -105); /* launcher Files tile */
    /* Launcher is behind Settings; focus it before its tile receives input. */
    move_pointer(&runtime, -22, -20); /* launcher title bar */
    send_event(&runtime, SYNTRA_INPUT_EV_KEY, SYNTRA_INPUT_BTN_LEFT, 1);
    send_event(&runtime, SYNTRA_INPUT_EV_KEY, SYNTRA_INPUT_BTN_LEFT, 0);
    move_pointer(&runtime, 22, 20); /* Files tile */
    send_event(&runtime, SYNTRA_INPUT_EV_KEY, SYNTRA_INPUT_BTN_LEFT, 1);
    assert(runtime.windows.count == 3U);
    assert(syntra_gui_window_active(&runtime.windows)->app_type ==
           SYNTRA_GUI_APP_FILES);
    send_event(&runtime, SYNTRA_INPUT_EV_KEY, SYNTRA_INPUT_BTN_LEFT, 0);

    /* Close launcher, then restore it with the desktop dock home control. */
    move_pointer(&runtime, 131, -18); /* Launcher close control */
    send_event(&runtime, SYNTRA_INPUT_EV_KEY, SYNTRA_INPUT_BTN_LEFT, 1);
    assert(runtime.windows.count == 2U);
    send_event(&runtime, SYNTRA_INPUT_EV_KEY, SYNTRA_INPUT_BTN_LEFT, 0);
    move_pointer(&runtime, -373, 392); /* dock home */
    send_event(&runtime, SYNTRA_INPUT_EV_KEY, SYNTRA_INPUT_BTN_LEFT, 1);
    assert(runtime.windows.count == 3U);
    assert(syntra_gui_window_active(&runtime.windows)->app_type ==
           SYNTRA_GUI_APP_LAUNCHER);
    send_event(&runtime, SYNTRA_INPUT_EV_KEY, SYNTRA_INPUT_BTN_LEFT, 0);

    /* Compact framebuffer: dock opens launcher and tile opens a fit-sized app. */
    assert(syntra_gui_runtime_init(&runtime, 320U, 240U) ==
           SYNTRA_STATUS_OK);
    move_pointer(&runtime, -145, 105); /* compact dock */
    send_event(&runtime, SYNTRA_INPUT_EV_KEY, SYNTRA_INPUT_BTN_LEFT, 1);
    assert(runtime.windows.count == 1U);
    assert(syntra_gui_window_active(&runtime.windows)->app_type ==
           SYNTRA_GUI_APP_LAUNCHER);
    send_event(&runtime, SYNTRA_INPUT_EV_KEY, SYNTRA_INPUT_BTN_LEFT, 0);
    move_pointer(&runtime, 67, -145); /* compact Files tile center */
    send_event(&runtime, SYNTRA_INPUT_EV_KEY, SYNTRA_INPUT_BTN_LEFT, 1);
    assert(runtime.windows.count == 2U);
    assert(syntra_gui_window_active(&runtime.windows)->app_type ==
           SYNTRA_GUI_APP_FILES);
    assert(syntra_gui_window_active(&runtime.windows)->width == 240U);
    assert(syntra_gui_window_active(&runtime.windows)->height == 170U);
    send_event(&runtime, SYNTRA_INPUT_EV_KEY, SYNTRA_INPUT_BTN_LEFT, 0);

    /* Hover tracks the window under the pointer without focusing it. */
    {
        uint32_t files_id = syntra_gui_window_active(&runtime.windows)->id;
        assert(runtime.hover_target_id == files_id);
        send_event(&runtime, SYNTRA_INPUT_EV_REL, SYNTRA_INPUT_REL_WHEEL, -1);
        assert(syntra_gui_window_active(&runtime.windows)->app_scroll_offset == 1U);
        send_event(&runtime, SYNTRA_INPUT_EV_REL, SYNTRA_INPUT_REL_WHEEL, -5);
        assert(syntra_gui_window_active(&runtime.windows)->app_scroll_offset == 2U);
        send_event(&runtime, SYNTRA_INPUT_EV_REL, SYNTRA_INPUT_REL_WHEEL, 1);
        assert(syntra_gui_window_active(&runtime.windows)->app_scroll_offset == 1U);
        send_event(&runtime, SYNTRA_INPUT_EV_KEY, SYNTRA_INPUT_BTN_LEFT, 1);
        assert(syntra_gui_window_active(&runtime.windows)->app_selection == 1U);
        send_event(&runtime, SYNTRA_INPUT_EV_KEY, SYNTRA_INPUT_BTN_LEFT, 0);
        framebuffer.pixels = pixels;
        framebuffer.width = 320U;
        framebuffer.height = 240U;
        framebuffer.stride_pixels = 320U;
        assert(syntra_gui_runtime_render(&runtime, &framebuffer) ==
               SYNTRA_STATUS_OK);
        assert(pixels[40U * 320U + 10U] == 0xFF54D6E8U);
        assert(pixels[86U * 320U + 241U] == 0xFF54D6E8U);
    }

    /* A covered launcher must not intercept clicks from the front app. */
    assert(syntra_gui_runtime_init(&runtime, 640U, 480U) ==
           SYNTRA_STATUS_OK);
    assert(syntra_gui_window_close(
        &runtime.windows, runtime.windows.windows[1].id) == SYNTRA_STATUS_OK);
    /* Closing Settings compacts the array, so find Launcher by app type. */
    launcher_id = 0U;
    for (uint32_t index = 0U; index < runtime.windows.count; ++index)
    {
        if (runtime.windows.windows[index].app_type ==
            SYNTRA_GUI_APP_LAUNCHER)
        {
            launcher_id = runtime.windows.windows[index].id;
            break;
        }
    }
    assert(launcher_id != 0U);
    assert(syntra_gui_window_move(
        &runtime.windows, launcher_id, 30U, 200U) == SYNTRA_STATUS_OK);
    assert(syntra_gui_window_focus(
        &runtime.windows, runtime.windows.windows[0].id) == SYNTRA_STATUS_OK);
    move_pointer(&runtime, -194, -12); /* overlapping Settings tile */
    send_event(&runtime, SYNTRA_INPUT_EV_KEY, SYNTRA_INPUT_BTN_LEFT, 1);
    assert(runtime.windows.count == 2U);
    assert(syntra_gui_window_active(&runtime.windows)->app_type ==
           SYNTRA_GUI_APP_FILES);
    send_event(&runtime, SYNTRA_INPUT_EV_KEY, SYNTRA_INPUT_BTN_LEFT, 0);

    /* Files folder and Settings category clicks update app selection. */
    assert(syntra_gui_runtime_init(&runtime, 640U, 480U) ==
           SYNTRA_STATUS_OK);
    move_pointer(&runtime, -42, -140); /* Launcher Files tile */
    send_event(&runtime, SYNTRA_INPUT_EV_KEY, SYNTRA_INPUT_BTN_LEFT, 1);
    send_event(&runtime, SYNTRA_INPUT_EV_KEY, SYNTRA_INPUT_BTN_LEFT, 0);
    move_pointer(&runtime, -4, 174); /* Files list: DOWNLOADS row */
    send_event(&runtime, SYNTRA_INPUT_EV_KEY, SYNTRA_INPUT_BTN_LEFT, 1);
    assert(syntra_gui_window_active(&runtime.windows)->app_type ==
           SYNTRA_GUI_APP_FILES);
    assert(syntra_gui_window_active(&runtime.windows)->app_selection == 2U);
    send_event(&runtime, SYNTRA_INPUT_EV_KEY, SYNTRA_INPUT_BTN_LEFT, 0);
    move_pointer(&runtime, 176, -24); /* focus Settings */
    send_event(&runtime, SYNTRA_INPUT_EV_KEY, SYNTRA_INPUT_BTN_LEFT, 1);
    send_event(&runtime, SYNTRA_INPUT_EV_KEY, SYNTRA_INPUT_BTN_LEFT, 0);
    move_pointer(&runtime, 0, 42); /* Settings: ABOUT row */
    send_event(&runtime, SYNTRA_INPUT_EV_KEY, SYNTRA_INPUT_BTN_LEFT, 1);
    assert(syntra_gui_window_active(&runtime.windows)->app_type ==
           SYNTRA_GUI_APP_SETTINGS);
    assert(syntra_gui_window_active(&runtime.windows)->app_selection == 3U);
    send_event(&runtime, SYNTRA_INPUT_EV_KEY, SYNTRA_INPUT_BTN_LEFT, 0);

    /* Tiny launcher windows must not trigger tile-geometry underflow. */
    assert(syntra_gui_window_manager_init(&runtime.windows, 640U, 480U) ==
           SYNTRA_STATUS_OK);
    assert(syntra_input_init(&runtime.input, 640U, 480U) == SYNTRA_STATUS_OK);
    assert(syntra_gui_window_interaction_init(&runtime.interaction) ==
           SYNTRA_STATUS_OK);
    runtime.initialized = 1;
    assert(syntra_gui_window_open_app(
        &runtime.windows, 300U, 200U, 20U, 30U,
        SYNTRA_GUI_APP_LAUNCHER, &launcher_id) == SYNTRA_STATUS_OK);
    move_pointer(&runtime, -15, -15); /* inside tiny launcher surface */
    send_event(&runtime, SYNTRA_INPUT_EV_KEY, SYNTRA_INPUT_BTN_LEFT, 1);
    assert(runtime.windows.count == 1U);
    send_event(&runtime, SYNTRA_INPUT_EV_KEY, SYNTRA_INPUT_BTN_LEFT, 0);

    /* Right-click a window, render its menu, and close the target. */
    assert(syntra_gui_runtime_init(&runtime, 640U, 480U) ==
           SYNTRA_STATUS_OK);
    move_pointer(&runtime, 80, 60); /* inside Settings */
    send_event(&runtime, SYNTRA_INPUT_EV_KEY, SYNTRA_INPUT_BTN_RIGHT, 1);
    send_event(&runtime, SYNTRA_INPUT_EV_KEY, SYNTRA_INPUT_BTN_RIGHT, 0);
    assert(runtime.last_gesture.action == SYNTRA_GESTURE_CONTEXT_REQUEST);
    assert(runtime.context_menu_visible == 1U);
    assert(runtime.context_menu_target ==
           runtime.windows.windows[1].id);
    framebuffer.pixels = pixels;
    framebuffer.width = 640U;
    framebuffer.height = 480U;
    framebuffer.stride_pixels = 640U;
    assert(syntra_gui_runtime_render(&runtime, &framebuffer) ==
           SYNTRA_STATUS_OK);
    /* Select the third menu row: CLOSE (after minimize and maximize). */
    move_pointer(&runtime, 20, 59);
    send_event(&runtime, SYNTRA_INPUT_EV_KEY, SYNTRA_INPUT_BTN_LEFT, 1);
    assert(runtime.context_menu_visible == 0U);
    assert(runtime.windows.count == 2U);
    send_event(&runtime, SYNTRA_INPUT_EV_KEY, SYNTRA_INPUT_BTN_LEFT, 0);

    /* Right-click the desktop and use OPEN SETTINGS to recreate the app. */
    move_pointer(&runtime, 80, -259); /* empty desktop area at (500,100) */
    send_event(&runtime, SYNTRA_INPUT_EV_KEY, SYNTRA_INPUT_BTN_RIGHT, 1);
    send_event(&runtime, SYNTRA_INPUT_EV_KEY, SYNTRA_INPUT_BTN_RIGHT, 0);
    assert(runtime.context_menu_visible == 1U);
    assert(runtime.context_menu_target == UINT32_MAX);
    move_pointer(&runtime, 20, 44); /* second desktop menu row */
    send_event(&runtime, SYNTRA_INPUT_EV_KEY, SYNTRA_INPUT_BTN_LEFT, 1);
    assert(runtime.context_menu_visible == 0U);
    assert(runtime.windows.count == 3U);
    assert(syntra_gui_window_active(&runtime.windows)->app_type ==
           SYNTRA_GUI_APP_SETTINGS);
    send_event(&runtime, SYNTRA_INPUT_EV_KEY, SYNTRA_INPUT_BTN_LEFT, 0);

    /* Taskbar focuses open windows and restores minimized windows. */
    assert(syntra_gui_runtime_init(&runtime, 640U, 480U) ==
           SYNTRA_STATUS_OK);
    move_pointer(&runtime, -126, 219); /* first taskbar button: Files */
    send_event(&runtime, SYNTRA_INPUT_EV_KEY, SYNTRA_INPUT_BTN_LEFT, 1);
    assert(syntra_gui_window_active(&runtime.windows)->app_type ==
           SYNTRA_GUI_APP_FILES);
    send_event(&runtime, SYNTRA_INPUT_EV_KEY, SYNTRA_INPUT_BTN_LEFT, 0);
    move_pointer(&runtime, 45, -259); /* Files minimize titlebar control */
    send_event(&runtime, SYNTRA_INPUT_EV_KEY, SYNTRA_INPUT_BTN_LEFT, 1);
    assert(syntra_gui_window_active(&runtime.windows)->app_type ==
           SYNTRA_GUI_APP_LAUNCHER);
    assert(runtime.windows.windows[0].app_type == SYNTRA_GUI_APP_FILES);
    assert(runtime.windows.windows[0].minimized == 1U);
    send_event(&runtime, SYNTRA_INPUT_EV_KEY, SYNTRA_INPUT_BTN_LEFT, 0);
    move_pointer(&runtime, -45, 259); /* select minimized Files taskbar item */
    send_event(&runtime, SYNTRA_INPUT_EV_KEY, SYNTRA_INPUT_BTN_LEFT, 1);
    assert(syntra_gui_window_active(&runtime.windows)->app_type ==
           SYNTRA_GUI_APP_FILES);
    assert(syntra_gui_window_active(&runtime.windows)->minimized == 0U);
    send_event(&runtime, SYNTRA_INPUT_EV_KEY, SYNTRA_INPUT_BTN_LEFT, 0);
    framebuffer.pixels = pixels;
    framebuffer.width = 640U;
    framebuffer.height = 480U;
    framebuffer.stride_pixels = 640U;
    assert(syntra_gui_runtime_render(&runtime, &framebuffer) ==
           SYNTRA_STATUS_OK);
    assert(pixels[(size_t)447U * 640U + 340U] == 0xFF54D6E8U);
    /* Invalid dimensions must be rejected rather than creating partial state. */
    assert(syntra_gui_runtime_init(&runtime, 200U, 100U) ==
           SYNTRA_STATUS_INVALID_ARGUMENT);

    puts("SyntraOS persistent GUI runtime integration passed.");
    return 0;
}
