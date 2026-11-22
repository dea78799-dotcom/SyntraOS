#include "gesture.h"

#include <stddef.h>

static uint64_t distance_squared(uint32_t ax, uint32_t ay,
                                 uint32_t bx, uint32_t by)
{
    uint64_t dx = ax > bx ? (uint64_t)(ax - bx) : (uint64_t)(bx - ax);
    uint64_t dy = ay > by ? (uint64_t)(ay - by) : (uint64_t)(by - ay);
    return dx * dx + dy * dy;
}

static uint32_t elapsed_ms(uint64_t now, uint64_t start)
{
    uint64_t elapsed = now >= start ? now - start : 0ULL;
    return elapsed > UINT32_MAX ? UINT32_MAX : (uint32_t)elapsed;
}

syntra_status_t syntra_gesture_init(syntra_gesture_t *gesture)
{
    if (gesture == NULL) return SYNTRA_STATUS_INVALID_ARGUMENT;
    gesture->active_button = 0U;
    gesture->target_id = 0U;
    gesture->start_x = gesture->start_y = 0U;
    gesture->last_x = gesture->last_y = 0U;
    gesture->last_click_target = 0U;
    gesture->press_started_ms = 0ULL;
    gesture->last_click_ms = 0ULL;
    gesture->pressed = 0U;
    gesture->long_press_fired = 0U;
    gesture->dragging = 0U;
    gesture->hold_reported = 0U;
    gesture->initialized = 1U;
    return SYNTRA_STATUS_OK;
}

static void set_result(syntra_gesture_result_t *r,
                       syntra_gesture_action_t action,
                       uint32_t target, uint32_t x, uint32_t y,
                       uint32_t duration)
{
    r->action = action;
    r->target_id = target;
    r->x = x;
    r->y = y;
    r->duration_ms = duration;
}

syntra_status_t syntra_gesture_update(
    syntra_gesture_t *g, uint32_t buttons, uint32_t target_id,
    uint32_t x, uint32_t y, uint64_t now_ms,
    syntra_gesture_result_t *r)
{
    uint32_t button;
    if (g == NULL || g->initialized != 1U || r == NULL)
        return SYNTRA_STATUS_INVALID_ARGUMENT;

    set_result(r, SYNTRA_GESTURE_NONE, target_id, x, y, 0U);
    button = (buttons & SYNTRA_GESTURE_BUTTON_LEFT) != 0U
        ? SYNTRA_GESTURE_BUTTON_LEFT
        : ((buttons & SYNTRA_GESTURE_BUTTON_RIGHT) != 0U
            ? SYNTRA_GESTURE_BUTTON_RIGHT : 0U);

    if (!g->pressed && button != 0U)
    {
        g->pressed = 1U;
        g->active_button = button;
        g->target_id = target_id;
        g->start_x = g->last_x = x;
        g->start_y = g->last_y = y;
        g->press_started_ms = now_ms;
        g->long_press_fired = 0U;
        g->dragging = 0U;
        g->hold_reported = 0U;
        set_result(r, SYNTRA_GESTURE_PRESS, target_id, x, y, 0U);
        return SYNTRA_STATUS_OK;
    }

    if (g->pressed && button == g->active_button)
    {
        uint32_t duration = elapsed_ms(now_ms, g->press_started_ms);
        uint64_t moved = distance_squared(x, y, g->start_x, g->start_y);
        g->last_x = x;
        g->last_y = y;

        if (!g->dragging && moved > (uint64_t)SYNTRA_GESTURE_DRAG_SLOP_PX *
                                      SYNTRA_GESTURE_DRAG_SLOP_PX &&
            g->active_button == SYNTRA_GESTURE_BUTTON_LEFT)
        {
            g->dragging = 1U;
            set_result(r, SYNTRA_GESTURE_DRAG_START, g->target_id,
                       x, y, duration);
            return SYNTRA_STATUS_OK;
        }
        if (g->dragging)
        {
            set_result(r, SYNTRA_GESTURE_DRAG_MOVE, g->target_id,
                       x, y, duration);
            return SYNTRA_STATUS_OK;
        }
        if (g->active_button == SYNTRA_GESTURE_BUTTON_LEFT &&
            !g->long_press_fired &&
            moved <= (uint64_t)SYNTRA_GESTURE_TAP_SLOP_PX *
                     SYNTRA_GESTURE_TAP_SLOP_PX &&
            duration >= SYNTRA_GESTURE_LONG_PRESS_MS)
        {
            g->long_press_fired = 1U;
            set_result(r, SYNTRA_GESTURE_LONG_PRESS, g->target_id,
                       x, y, duration);
            return SYNTRA_STATUS_OK;
        }
        if (!g->hold_reported && duration >= 100U)
        {
            g->hold_reported = 1U;
            set_result(r, SYNTRA_GESTURE_HOLD, g->target_id,
                       x, y, duration);
        }
        return SYNTRA_STATUS_OK;
    }

    if (g->pressed && button == 0U)
    {
        uint32_t duration = elapsed_ms(now_ms, g->press_started_ms);
        uint32_t old_target = g->target_id;
        uint32_t old_button = g->active_button;
        uint64_t moved = distance_squared(x, y, g->start_x, g->start_y);

        if (g->dragging)
            set_result(r, SYNTRA_GESTURE_DRAG_END, old_target, x, y, duration);
        else if (old_button == SYNTRA_GESTURE_BUTTON_RIGHT)
            set_result(r, SYNTRA_GESTURE_CONTEXT_REQUEST, old_target,
                       x, y, duration);
        else if (!g->long_press_fired &&
                 moved <= (uint64_t)SYNTRA_GESTURE_TAP_SLOP_PX *
                          SYNTRA_GESTURE_TAP_SLOP_PX &&
                 target_id == old_target)
        {
            if (g->last_click_target == old_target &&
                now_ms >= g->last_click_ms &&
                now_ms - g->last_click_ms <= SYNTRA_GESTURE_DOUBLE_CLICK_MS)
            {
                set_result(r, SYNTRA_GESTURE_DOUBLE_TAP, old_target,
                           x, y, duration);
                g->last_click_target = 0U;
                g->last_click_ms = 0ULL;
            }
            else
            {
                set_result(r, SYNTRA_GESTURE_TAP, old_target, x, y, duration);
                g->last_click_target = old_target;
                g->last_click_ms = now_ms;
            }
        }
        g->pressed = 0U;
        g->active_button = 0U;
        g->target_id = 0U;
        g->dragging = 0U;
        g->long_press_fired = 0U;
        g->hold_reported = 0U;
    }
    return SYNTRA_STATUS_OK;
}

syntra_status_t syntra_gesture_cancel(
    syntra_gesture_t *g, syntra_gesture_result_t *r)
{
    if (g == NULL || g->initialized != 1U || r == NULL)
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    set_result(r, g->pressed ? SYNTRA_GESTURE_CANCEL : SYNTRA_GESTURE_NONE,
               g->target_id, g->last_x, g->last_y, 0U);
    g->pressed = 0U;
    g->active_button = 0U;
    g->target_id = 0U;
    g->dragging = 0U;
    g->long_press_fired = 0U;
    g->hold_reported = 0U;
    g->last_click_target = 0U;
    g->last_click_ms = 0ULL;
    return SYNTRA_STATUS_OK;
}
