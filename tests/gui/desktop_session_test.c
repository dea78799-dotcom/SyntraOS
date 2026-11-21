#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "desktop_session.h"

static void test_keyboard_navigation_and_activation(void)
{
    syntra_desktop_session_t session;
    syntra_input_event_t event = {SYNTRA_INPUT_EV_KEY, 106U, 1};

    assert(syntra_desktop_session_init(&session, 640U, 480U) ==
           SYNTRA_STATUS_OK);
    assert(session.focused_app == 0U);
    assert(session.activated_app == -1);

    assert(syntra_desktop_session_process(&session, &event) ==
           SYNTRA_STATUS_OK);
    assert(session.focused_app == 1U);

    event.code = 28U;
    assert(syntra_desktop_session_process(&session, &event) ==
           SYNTRA_STATUS_OK);
    assert(session.activated_app == 1);
    assert(session.activation_count == 1U);

    event.code = 105U;
    assert(syntra_desktop_session_process(&session, &event) ==
           SYNTRA_STATUS_OK);
    assert(session.focused_app == 0U);
}

static void test_relative_mouse_click(void)
{
    syntra_desktop_session_t session;
    syntra_input_event_t event = {SYNTRA_INPUT_EV_REL, SYNTRA_INPUT_REL_X, 0};

    assert(syntra_desktop_session_init(&session, 640U, 480U) ==
           SYNTRA_STATUS_OK);

    /* Move the virtual pointer from the center into launcher tile 3. */
    event.value = -110;
    assert(syntra_desktop_session_process(&session, &event) ==
           SYNTRA_STATUS_OK);
    event.code = SYNTRA_INPUT_REL_Y;
    event.value = -30;
    assert(syntra_desktop_session_process(&session, &event) ==
           SYNTRA_STATUS_OK);
    assert(session.input.pointer_x == 210U);
    assert(session.input.pointer_y == 210U);

    event.type = SYNTRA_INPUT_EV_KEY;
    event.code = SYNTRA_INPUT_BTN_LEFT;
    event.value = 1;
    assert(syntra_desktop_session_process(&session, &event) ==
           SYNTRA_STATUS_OK);
    assert(session.activated_app == 2);
    assert(session.activation_count == 1U);

    /* Holding the button must not repeatedly activate the same tile. */
    assert(syntra_desktop_session_process(&session, &event) ==
           SYNTRA_STATUS_OK);
    assert(session.activation_count == 1U);
}

static void test_compact_layout_hit_testing(void)
{
    syntra_desktop_session_t session;
    syntra_input_event_t event = {SYNTRA_INPUT_EV_REL, SYNTRA_INPUT_REL_X, 0};

    assert(syntra_desktop_session_init(&session, 320U, 240U) ==
           SYNTRA_STATUS_OK);

    /* Center pointer (160,120) is inside compact tile 2 (x=164..232). */
    event.type = SYNTRA_INPUT_EV_KEY;
    event.code = SYNTRA_INPUT_BTN_LEFT;
    event.value = 1;
    assert(syntra_desktop_session_process(&session, &event) ==
           SYNTRA_STATUS_OK);
    assert(session.activated_app == -1);

    event.value = 0;
    assert(syntra_desktop_session_process(&session, &event) ==
           SYNTRA_STATUS_OK);
    event.type = SYNTRA_INPUT_EV_REL;
    event.code = SYNTRA_INPUT_REL_X;
    event.value = 8;
    assert(syntra_desktop_session_process(&session, &event) ==
           SYNTRA_STATUS_OK);
    event.code = SYNTRA_INPUT_REL_Y;
    event.value = 0;
    assert(syntra_desktop_session_process(&session, &event) ==
           SYNTRA_STATUS_OK);
    event.type = SYNTRA_INPUT_EV_KEY;
    event.code = SYNTRA_INPUT_BTN_LEFT;
    event.value = 1;
    assert(syntra_desktop_session_process(&session, &event) ==
           SYNTRA_STATUS_OK);
    assert(session.activated_app == 2);
}

int main(void)
{
    syntra_desktop_session_t invalid;
    assert(syntra_desktop_session_init(NULL, 640U, 480U) ==
           SYNTRA_STATUS_INVALID_ARGUMENT);
    assert(syntra_desktop_session_init(&invalid, 0U, 480U) ==
           SYNTRA_STATUS_INVALID_ARGUMENT);
    assert(syntra_desktop_session_init(&invalid, 319U, 240U) ==
           SYNTRA_STATUS_INVALID_ARGUMENT);

    test_keyboard_navigation_and_activation();
    test_relative_mouse_click();
    test_compact_layout_hit_testing();

    puts("SyntraOS virtual desktop interaction tests passed.");
    return 0;
}
