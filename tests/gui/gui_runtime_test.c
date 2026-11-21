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

    /* Invalid dimensions must be rejected rather than creating partial state. */
    assert(syntra_gui_runtime_init(&runtime, 200U, 100U) ==
           SYNTRA_STATUS_INVALID_ARGUMENT);

    puts("SyntraOS persistent GUI runtime integration passed.");
    return 0;
}
