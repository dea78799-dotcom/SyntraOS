#ifndef SYNTRA_GUI_GESTURE_H
#define SYNTRA_GUI_GESTURE_H

#include <stdint.h>
#include "../core/status.h"

#define SYNTRA_GESTURE_BUTTON_LEFT  (1U << 0)
#define SYNTRA_GESTURE_BUTTON_RIGHT (1U << 1)

#define SYNTRA_GESTURE_LONG_PRESS_MS 500ULL
#define SYNTRA_GESTURE_DOUBLE_CLICK_MS 400ULL
#define SYNTRA_GESTURE_TAP_SLOP_PX 8U
#define SYNTRA_GESTURE_DRAG_SLOP_PX 4U

typedef enum
{
    SYNTRA_GESTURE_NONE = 0,
    SYNTRA_GESTURE_PRESS,
    SYNTRA_GESTURE_HOLD,
    SYNTRA_GESTURE_TAP,
    SYNTRA_GESTURE_DOUBLE_TAP,
    SYNTRA_GESTURE_LONG_PRESS,
    SYNTRA_GESTURE_DRAG_START,
    SYNTRA_GESTURE_DRAG_MOVE,
    SYNTRA_GESTURE_DRAG_END,
    SYNTRA_GESTURE_CONTEXT_REQUEST,
    SYNTRA_GESTURE_CANCEL
} syntra_gesture_action_t;

typedef struct
{
    syntra_gesture_action_t action;
    uint32_t target_id;
    uint32_t x;
    uint32_t y;
    uint32_t duration_ms;
} syntra_gesture_result_t;

typedef struct
{
    uint32_t active_button;
    uint32_t target_id;
    uint32_t start_x;
    uint32_t start_y;
    uint32_t last_x;
    uint32_t last_y;
    uint32_t last_click_target;
    uint64_t press_started_ms;
    uint64_t last_click_ms;
    uint8_t pressed;
    uint8_t long_press_fired;
    uint8_t dragging;
    uint8_t hold_reported;
    uint8_t initialized;
} syntra_gesture_t;

syntra_status_t syntra_gesture_init(syntra_gesture_t *gesture);

syntra_status_t syntra_gesture_update(
    syntra_gesture_t *gesture,
    uint32_t buttons,
    uint32_t target_id,
    uint32_t x,
    uint32_t y,
    uint64_t now_ms,
    syntra_gesture_result_t *result);

syntra_status_t syntra_gesture_cancel(
    syntra_gesture_t *gesture,
    syntra_gesture_result_t *result);

#endif
