#include <assert.h>
#include <stdio.h>
#include "gesture.h"

static syntra_gesture_t g;
static syntra_gesture_result_t r;

static void update(uint32_t buttons, uint32_t target,
                   uint32_t x, uint32_t y, uint64_t ms,
                   syntra_gesture_action_t expected)
{
    assert(syntra_gesture_update(&g, buttons, target, x, y, ms, &r)
           == SYNTRA_STATUS_OK);
    assert(r.action == expected);
}

int main(void)
{
    assert(syntra_gesture_init(&g) == SYNTRA_STATUS_OK);

    update(SYNTRA_GESTURE_BUTTON_LEFT, 7U, 20U, 20U, 1000U,
           SYNTRA_GESTURE_PRESS);
    update(SYNTRA_GESTURE_BUTTON_LEFT, 7U, 20U, 20U, 1100U,
           SYNTRA_GESTURE_HOLD);
    update(SYNTRA_GESTURE_BUTTON_LEFT, 7U, 20U, 20U, 1500U,
           SYNTRA_GESTURE_LONG_PRESS);
    update(SYNTRA_GESTURE_BUTTON_LEFT, 7U, 20U, 20U, 1700U,
           SYNTRA_GESTURE_NONE);
    update(0U, 7U, 20U, 20U, 1710U, SYNTRA_GESTURE_NONE);

    update(SYNTRA_GESTURE_BUTTON_LEFT, 7U, 20U, 20U, 2000U,
           SYNTRA_GESTURE_PRESS);
    update(0U, 7U, 20U, 20U, 2050U, SYNTRA_GESTURE_TAP);
    update(SYNTRA_GESTURE_BUTTON_LEFT, 7U, 20U, 20U, 2200U,
           SYNTRA_GESTURE_PRESS);
    update(0U, 7U, 20U, 20U, 2250U, SYNTRA_GESTURE_DOUBLE_TAP);

    update(SYNTRA_GESTURE_BUTTON_LEFT, 9U, 10U, 10U, 3000U,
           SYNTRA_GESTURE_PRESS);
    update(SYNTRA_GESTURE_BUTTON_LEFT, 9U, 20U, 10U, 3020U,
           SYNTRA_GESTURE_DRAG_START);
    update(SYNTRA_GESTURE_BUTTON_LEFT, 9U, 30U, 10U, 3030U,
           SYNTRA_GESTURE_DRAG_MOVE);
    update(0U, 9U, 30U, 10U, 3040U, SYNTRA_GESTURE_DRAG_END);

    update(SYNTRA_GESTURE_BUTTON_RIGHT, 4U, 2U, 2U, 4000U,
           SYNTRA_GESTURE_PRESS);
    update(0U, 4U, 2U, 2U, 4050U, SYNTRA_GESTURE_CONTEXT_REQUEST);

    update(SYNTRA_GESTURE_BUTTON_LEFT, 3U, 1U, 1U, 5000U,
           SYNTRA_GESTURE_PRESS);
    assert(syntra_gesture_cancel(&g, &r) == SYNTRA_STATUS_OK);
    assert(r.action == SYNTRA_GESTURE_CANCEL);

    puts("GUI gesture engine tests: PASS");
    return 0;
}
