#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "input.h"

static void test_relative_pointer_and_clamping(void)
{
    syntra_input_state_t state;
    syntra_input_event_t event;

    assert(syntra_input_init(&state, 640U, 480U) == SYNTRA_STATUS_OK);
    assert(state.pointer_x == 320U && state.pointer_y == 240U);

    event.type = SYNTRA_INPUT_EV_REL;
    event.code = SYNTRA_INPUT_REL_X;
    event.value = 50;
    assert(syntra_input_process(&state, &event) == SYNTRA_STATUS_OK);
    assert(state.pointer_x == 370U);

    event.code = SYNTRA_INPUT_REL_Y;
    event.value = -500;
    assert(syntra_input_process(&state, &event) == SYNTRA_STATUS_OK);
    assert(state.pointer_y == 0U);

    event.code = SYNTRA_INPUT_REL_X;
    event.value = INT32_MAX;
    assert(syntra_input_process(&state, &event) == SYNTRA_STATUS_OK);
    assert(state.pointer_x == 639U);
}

static void test_buttons_and_keyboard(void)
{
    syntra_input_state_t state;
    syntra_input_event_t event = {SYNTRA_INPUT_EV_KEY, SYNTRA_INPUT_BTN_LEFT, 1};

    assert(syntra_input_init(&state, 640U, 480U) == SYNTRA_STATUS_OK);
    assert(syntra_input_process(&state, &event) == SYNTRA_STATUS_OK);
    assert((state.buttons & SYNTRA_INPUT_BUTTON_LEFT) != 0U);
    assert(syntra_input_key_down(&state, SYNTRA_INPUT_BTN_LEFT));

    event.value = 0;
    assert(syntra_input_process(&state, &event) == SYNTRA_STATUS_OK);
    assert((state.buttons & SYNTRA_INPUT_BUTTON_LEFT) == 0U);

    event.code = 30U;
    event.value = 1;
    assert(syntra_input_process(&state, &event) == SYNTRA_STATUS_OK);
    assert(syntra_input_key_down(&state, 30U));

    event.value = 2;
    assert(syntra_input_process(&state, &event) == SYNTRA_STATUS_OK);
    assert(state.key_repeat_count == 1U);
    assert(syntra_input_key_down(&state, 30U));

    event.value = 0;
    assert(syntra_input_process(&state, &event) == SYNTRA_STATUS_OK);
    assert(!syntra_input_key_down(&state, 30U));
}

static void test_touchscreen_contact_button(void)
{
    syntra_input_state_t state;
    syntra_input_event_t event = {
        SYNTRA_INPUT_EV_KEY, SYNTRA_INPUT_BTN_TOUCH, 1
    };

    assert(syntra_input_init(&state, 320U, 240U) == SYNTRA_STATUS_OK);
    assert(syntra_input_process(&state, &event) == SYNTRA_STATUS_OK);
    assert((state.buttons & SYNTRA_INPUT_BUTTON_LEFT) != 0U);
    assert(syntra_input_key_down(&state, SYNTRA_INPUT_BTN_TOUCH));

    event.value = 0;
    assert(syntra_input_process(&state, &event) == SYNTRA_STATUS_OK);
    assert((state.buttons & SYNTRA_INPUT_BUTTON_LEFT) == 0U);
    assert(!syntra_input_key_down(&state, SYNTRA_INPUT_BTN_TOUCH));
}

static void test_absolute_pointer_mapping(void)
{
    syntra_input_state_t state;
    syntra_input_event_t event = {SYNTRA_INPUT_EV_ABS, SYNTRA_INPUT_ABS_X, 0};

    assert(syntra_input_init(&state, 640U, 480U) == SYNTRA_STATUS_OK);
    assert(syntra_input_process(&state, &event) == SYNTRA_STATUS_INVALID_ARGUMENT);

    assert(syntra_input_set_abs_range(&state, 0, 32767, 0, 32767) == SYNTRA_STATUS_OK);
    assert(syntra_input_process(&state, &event) == SYNTRA_STATUS_OK);
    assert(state.pointer_x == 0U);

    event.value = 32767;
    assert(syntra_input_process(&state, &event) == SYNTRA_STATUS_OK);
    assert(state.pointer_x == 639U);

    event.code = SYNTRA_INPUT_ABS_Y;
    event.value = 16384;
    assert(syntra_input_process(&state, &event) == SYNTRA_STATUS_OK);
    assert(state.pointer_y >= 239U && state.pointer_y <= 240U);
}

static void test_wheel_sync_and_rejection(void)
{
    syntra_input_state_t state;
    syntra_input_event_t event = {SYNTRA_INPUT_EV_REL, SYNTRA_INPUT_REL_WHEEL, 2};

    assert(syntra_input_init(&state, 320U, 240U) == SYNTRA_STATUS_OK);
    assert(syntra_input_process(&state, &event) == SYNTRA_STATUS_OK);
    assert(state.wheel_y == 2);

    event.code = SYNTRA_INPUT_REL_HWHEEL;
    event.value = -1;
    assert(syntra_input_process(&state, &event) == SYNTRA_STATUS_OK);
    assert(state.wheel_x == -1);

    event.type = SYNTRA_INPUT_EV_SYN;
    event.code = SYNTRA_INPUT_SYN_REPORT;
    event.value = 0;
    assert(syntra_input_process(&state, &event) == SYNTRA_STATUS_OK);
    assert(state.frame_sequence == 1U);

    event.type = 99U;
    assert(syntra_input_process(&state, &event) == SYNTRA_STATUS_NOT_IMPLEMENTED);
    assert(state.rejected_event_count == 1U);
}

int main(void)
{
    syntra_input_state_t invalid;
    assert(syntra_input_init(NULL, 1U, 1U) == SYNTRA_STATUS_INVALID_ARGUMENT);
    assert(syntra_input_init(&invalid, 0U, 1U) == SYNTRA_STATUS_INVALID_ARGUMENT);

    test_relative_pointer_and_clamping();
    test_buttons_and_keyboard();
    test_touchscreen_contact_button();
    test_absolute_pointer_mapping();
    test_wheel_sync_and_rejection();

    puts("SyntraOS input core tests passed.");
    return 0;
}
