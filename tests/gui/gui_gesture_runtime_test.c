#include <assert.h>
#include <stdio.h>

#include "gui_runtime.h"

static void event_at(syntra_gui_runtime_t *runtime,
                     uint16_t type, uint16_t code, int32_t value,
                     uint64_t now_ms)
{
    syntra_input_event_t event = {type, code, value};
    assert(syntra_gui_runtime_process_event_at(runtime, &event, now_ms) ==
           SYNTRA_STATUS_OK);
}

int main(void)
{
    syntra_gui_runtime_t runtime;

    assert(syntra_gui_runtime_init(&runtime, 640U, 480U) ==
           SYNTRA_STATUS_OK);

    event_at(&runtime, SYNTRA_INPUT_EV_REL, SYNTRA_INPUT_REL_X, -170, 990U);
    event_at(&runtime, SYNTRA_INPUT_EV_REL, SYNTRA_INPUT_REL_Y, 10, 995U);
    event_at(&runtime, SYNTRA_INPUT_EV_KEY, SYNTRA_INPUT_BTN_LEFT, 1, 1000U);
    assert(runtime.last_gesture.action == SYNTRA_GESTURE_PRESS);
    assert(syntra_gui_runtime_tick(&runtime, 1100U) == SYNTRA_STATUS_OK);
    assert(runtime.last_gesture.action == SYNTRA_GESTURE_HOLD);
    assert(syntra_gui_runtime_tick(&runtime, 1500U) == SYNTRA_STATUS_OK);
    assert(runtime.last_gesture.action == SYNTRA_GESTURE_LONG_PRESS);
    assert(runtime.context_menu_visible == 1U);
    event_at(&runtime, SYNTRA_INPUT_EV_KEY, SYNTRA_INPUT_BTN_LEFT, 0, 1600U);
    assert(runtime.context_menu_visible == 1U);
    assert(runtime.last_gesture.action == SYNTRA_GESTURE_NONE);

    event_at(&runtime, SYNTRA_INPUT_EV_KEY, SYNTRA_INPUT_BTN_RIGHT, 1, 2000U);
    assert(runtime.last_gesture.action == SYNTRA_GESTURE_PRESS);
    event_at(&runtime, SYNTRA_INPUT_EV_KEY, SYNTRA_INPUT_BTN_RIGHT, 0, 2050U);
    assert(runtime.last_gesture.action == SYNTRA_GESTURE_CONTEXT_REQUEST);
    assert(runtime.context_menu_visible == 1U);

    /* Window context menu can minimize its target without destroying it. */
    {
        uint32_t target = runtime.context_menu_target;
        const syntra_gui_window_t *window = NULL;
        assert(target != UINT32_MAX && target != 0U);
        event_at(&runtime, SYNTRA_INPUT_EV_REL, SYNTRA_INPUT_REL_X, 10, 2060U);
        event_at(&runtime, SYNTRA_INPUT_EV_REL, SYNTRA_INPUT_REL_Y, 29, 2061U);
        event_at(&runtime, SYNTRA_INPUT_EV_KEY, SYNTRA_INPUT_BTN_LEFT, 1, 2062U);
        assert(runtime.context_menu_visible == 0U);
        assert(syntra_gui_window_get(&runtime.windows, target, &window) ==
               SYNTRA_STATUS_OK);
        assert(window->minimized == 1U);
        event_at(&runtime, SYNTRA_INPUT_EV_KEY, SYNTRA_INPUT_BTN_LEFT, 0, 2070U);
    }

    /* Desktop context menu can be dismissed by an outside primary click. */
    event_at(&runtime, SYNTRA_INPUT_EV_REL, SYNTRA_INPUT_REL_X, 340, 2100U);
    event_at(&runtime, SYNTRA_INPUT_EV_REL, SYNTRA_INPUT_REL_Y, -179, 2101U);
    event_at(&runtime, SYNTRA_INPUT_EV_KEY, SYNTRA_INPUT_BTN_RIGHT, 1, 2102U);
    event_at(&runtime, SYNTRA_INPUT_EV_KEY, SYNTRA_INPUT_BTN_RIGHT, 0, 2103U);
    assert(runtime.context_menu_visible == 1U);
    event_at(&runtime, SYNTRA_INPUT_EV_REL, SYNTRA_INPUT_REL_X, 100, 2110U);
    event_at(&runtime, SYNTRA_INPUT_EV_REL, SYNTRA_INPUT_REL_Y, 350, 2111U);
    event_at(&runtime, SYNTRA_INPUT_EV_KEY, SYNTRA_INPUT_BTN_LEFT, 1, 2112U);
    assert(runtime.context_menu_visible == 0U);
    event_at(&runtime, SYNTRA_INPUT_EV_KEY, SYNTRA_INPUT_BTN_LEFT, 0, 2120U);

    event_at(&runtime, SYNTRA_INPUT_EV_KEY, SYNTRA_INPUT_BTN_LEFT, 1, 3000U);
    event_at(&runtime, SYNTRA_INPUT_EV_KEY, SYNTRA_INPUT_BTN_LEFT, 0, 3050U);
    assert(runtime.last_gesture.action == SYNTRA_GESTURE_TAP);
    event_at(&runtime, SYNTRA_INPUT_EV_KEY, SYNTRA_INPUT_BTN_LEFT, 1, 3200U);
    event_at(&runtime, SYNTRA_INPUT_EV_KEY, SYNTRA_INPUT_BTN_LEFT, 0, 3250U);
    assert(runtime.last_gesture.action == SYNTRA_GESTURE_DOUBLE_TAP);

    /* Absolute touch coordinates plus BTN_TOUCH activate a launcher tile. */
    assert(syntra_gui_runtime_init(&runtime, 640U, 480U) ==
           SYNTRA_STATUS_OK);
    assert(syntra_input_set_abs_range(
        &runtime.input, 0, 639, 0, 479) == SYNTRA_STATUS_OK);
    event_at(&runtime, SYNTRA_INPUT_EV_ABS, SYNTRA_INPUT_ABS_X, 278, 4000U);
    event_at(&runtime, SYNTRA_INPUT_EV_ABS, SYNTRA_INPUT_ABS_Y, 100, 4001U);
    event_at(&runtime, SYNTRA_INPUT_EV_KEY, SYNTRA_INPUT_BTN_TOUCH, 1, 4002U);
    assert(syntra_gui_window_active(&runtime.windows)->app_type ==
           SYNTRA_GUI_APP_FILES);
    event_at(&runtime, SYNTRA_INPUT_EV_KEY, SYNTRA_INPUT_BTN_TOUCH, 0, 4050U);
    assert(runtime.last_gesture.action == SYNTRA_GESTURE_TAP);

    puts("GUI gesture runtime integration: PASS");
    return 0;
}
