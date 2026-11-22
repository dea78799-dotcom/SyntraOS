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

    event_at(&runtime, SYNTRA_INPUT_EV_KEY, SYNTRA_INPUT_BTN_LEFT, 1, 1000U);
    assert(runtime.last_gesture.action == SYNTRA_GESTURE_PRESS);
    assert(syntra_gui_runtime_tick(&runtime, 1100U) == SYNTRA_STATUS_OK);
    assert(runtime.last_gesture.action == SYNTRA_GESTURE_HOLD);
    assert(syntra_gui_runtime_tick(&runtime, 1500U) == SYNTRA_STATUS_OK);
    assert(runtime.last_gesture.action == SYNTRA_GESTURE_LONG_PRESS);
    event_at(&runtime, SYNTRA_INPUT_EV_KEY, SYNTRA_INPUT_BTN_LEFT, 0, 1600U);
    assert(runtime.last_gesture.action == SYNTRA_GESTURE_NONE);

    event_at(&runtime, SYNTRA_INPUT_EV_KEY, SYNTRA_INPUT_BTN_RIGHT, 1, 2000U);
    assert(runtime.last_gesture.action == SYNTRA_GESTURE_PRESS);
    event_at(&runtime, SYNTRA_INPUT_EV_KEY, SYNTRA_INPUT_BTN_RIGHT, 0, 2050U);
    assert(runtime.last_gesture.action == SYNTRA_GESTURE_CONTEXT_REQUEST);
    assert(runtime.context_menu_visible == 1U);

    /* The open context menu consumes and dismisses an outside primary click. */
    event_at(&runtime, SYNTRA_INPUT_EV_REL, SYNTRA_INPUT_REL_X, 180, 2100U);
    event_at(&runtime, SYNTRA_INPUT_EV_REL, SYNTRA_INPUT_REL_Y, 160, 2101U);
    event_at(&runtime, SYNTRA_INPUT_EV_KEY, SYNTRA_INPUT_BTN_LEFT, 1, 2102U);
    assert(runtime.context_menu_visible == 0U);
    event_at(&runtime, SYNTRA_INPUT_EV_KEY, SYNTRA_INPUT_BTN_LEFT, 0, 2110U);

    event_at(&runtime, SYNTRA_INPUT_EV_KEY, SYNTRA_INPUT_BTN_LEFT, 1, 3000U);
    event_at(&runtime, SYNTRA_INPUT_EV_KEY, SYNTRA_INPUT_BTN_LEFT, 0, 3050U);
    assert(runtime.last_gesture.action == SYNTRA_GESTURE_TAP);
    event_at(&runtime, SYNTRA_INPUT_EV_KEY, SYNTRA_INPUT_BTN_LEFT, 1, 3200U);
    event_at(&runtime, SYNTRA_INPUT_EV_KEY, SYNTRA_INPUT_BTN_LEFT, 0, 3250U);
    assert(runtime.last_gesture.action == SYNTRA_GESTURE_DOUBLE_TAP);

    puts("GUI gesture runtime integration: PASS");
    return 0;
}
