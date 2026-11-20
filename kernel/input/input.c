#include "input.h"

#include <limits.h>
#include <stddef.h>
#include <string.h>

static uint32_t clamp_pointer(int64_t value, uint32_t limit)
{
    if (value < 0) return 0U;
    if ((uint64_t)value >= (uint64_t)limit) return limit - 1U;
    return (uint32_t)value;
}

static int32_t saturating_add_i32(int32_t current, int32_t delta)
{
    if (delta > 0 && current > INT32_MAX - delta) return INT32_MAX;
    if (delta < 0 && current < INT32_MIN - delta) return INT32_MIN;
    return current + delta;
}

static uint32_t map_absolute_axis(
    int32_t value,
    int32_t minimum,
    int32_t maximum,
    uint32_t pixels)
{
    uint64_t numerator;
    uint64_t denominator;
    int64_t clamped = value;

    if (clamped < minimum) clamped = minimum;
    if (clamped > maximum) clamped = maximum;

    denominator = (uint64_t)((int64_t)maximum - (int64_t)minimum);
    numerator = (uint64_t)(clamped - (int64_t)minimum) *
                (uint64_t)(pixels - 1U);

    return (uint32_t)(numerator / denominator);
}

syntra_status_t syntra_input_init(
    syntra_input_state_t *state,
    uint32_t width,
    uint32_t height)
{
    if (state == NULL || width == 0U || height == 0U)
        return SYNTRA_STATUS_INVALID_ARGUMENT;

    memset(state, 0, sizeof(*state));
    state->width = width;
    state->height = height;
    state->pointer_x = width / 2U;
    state->pointer_y = height / 2U;
    state->initialized = 1;
    return SYNTRA_STATUS_OK;
}

syntra_status_t syntra_input_set_abs_range(
    syntra_input_state_t *state,
    int32_t x_min,
    int32_t x_max,
    int32_t y_min,
    int32_t y_max)
{
    if (state == NULL || state->initialized != 1 ||
        x_min >= x_max || y_min >= y_max)
        return SYNTRA_STATUS_INVALID_ARGUMENT;

    state->abs_x_min = x_min;
    state->abs_x_max = x_max;
    state->abs_y_min = y_min;
    state->abs_y_max = y_max;
    state->abs_range_configured = 1;
    return SYNTRA_STATUS_OK;
}

static syntra_status_t process_key(
    syntra_input_state_t *state,
    uint16_t code,
    int32_t value)
{
    uint64_t mask;
    unsigned int word;
    uint32_t button = 0U;

    if ((code > SYNTRA_INPUT_KEY_MAX &&
         code != SYNTRA_INPUT_BTN_LEFT &&
         code != SYNTRA_INPUT_BTN_RIGHT &&
         code != SYNTRA_INPUT_BTN_MIDDLE) ||
        (value != 0 && value != 1 && value != 2))
        return SYNTRA_STATUS_INVALID_ARGUMENT;

    if (value == 2)
    {
        if (state->key_repeat_count < UINT_MAX)
            state->key_repeat_count++;
        return SYNTRA_STATUS_OK;
    }

    if (code <= SYNTRA_INPUT_KEY_MAX)
    {
        word = (unsigned int)code / 64U;
        mask = (uint64_t)1U << ((unsigned int)code % 64U);
        if (value == 1) state->keys[word] |= mask;
        else state->keys[word] &= ~mask;
    }

    if (code == SYNTRA_INPUT_BTN_LEFT) button = SYNTRA_INPUT_BUTTON_LEFT;
    else if (code == SYNTRA_INPUT_BTN_RIGHT) button = SYNTRA_INPUT_BUTTON_RIGHT;
    else if (code == SYNTRA_INPUT_BTN_MIDDLE) button = SYNTRA_INPUT_BUTTON_MIDDLE;

    if (button != 0U)
    {
        if (value == 1) state->buttons |= button;
        else state->buttons &= ~button;
    }

    return SYNTRA_STATUS_OK;
}

syntra_status_t syntra_input_process(
    syntra_input_state_t *state,
    const syntra_input_event_t *event)
{
    syntra_status_t status = SYNTRA_STATUS_OK;

    if (state == NULL || state->initialized != 1 || event == NULL)
        return SYNTRA_STATUS_INVALID_ARGUMENT;

    switch (event->type)
    {
        case SYNTRA_INPUT_EV_SYN:
            if (event->code == SYNTRA_INPUT_SYN_REPORT)
            {
                if (state->frame_sequence < UINT_MAX)
                    state->frame_sequence++;
            }
            else if (event->code != SYNTRA_INPUT_SYN_MT_REPORT)
            {
                status = SYNTRA_STATUS_NOT_IMPLEMENTED;
            }
            break;

        case SYNTRA_INPUT_EV_KEY:
            status = process_key(state, event->code, event->value);
            break;

        case SYNTRA_INPUT_EV_REL:
            if (event->code == SYNTRA_INPUT_REL_X)
            {
                state->pointer_x = clamp_pointer(
                    (int64_t)state->pointer_x + event->value,
                    state->width);
            }
            else if (event->code == SYNTRA_INPUT_REL_Y)
            {
                state->pointer_y = clamp_pointer(
                    (int64_t)state->pointer_y + event->value,
                    state->height);
            }
            else if (event->code == SYNTRA_INPUT_REL_WHEEL)
            {
                state->wheel_y = saturating_add_i32(
                    state->wheel_y, event->value);
            }
            else if (event->code == SYNTRA_INPUT_REL_HWHEEL)
            {
                state->wheel_x = saturating_add_i32(
                    state->wheel_x, event->value);
            }
            else
            {
                status = SYNTRA_STATUS_NOT_IMPLEMENTED;
            }
            break;

        case SYNTRA_INPUT_EV_ABS:
            if (state->abs_range_configured != 1)
            {
                status = SYNTRA_STATUS_INVALID_ARGUMENT;
            }
            else if (event->code == SYNTRA_INPUT_ABS_X)
            {
                state->pointer_x = map_absolute_axis(
                    event->value, state->abs_x_min, state->abs_x_max,
                    state->width);
            }
            else if (event->code == SYNTRA_INPUT_ABS_Y)
            {
                state->pointer_y = map_absolute_axis(
                    event->value, state->abs_y_min, state->abs_y_max,
                    state->height);
            }
            else
            {
                status = SYNTRA_STATUS_NOT_IMPLEMENTED;
            }
            break;

        default:
            status = SYNTRA_STATUS_NOT_IMPLEMENTED;
            break;
    }

    if (status == SYNTRA_STATUS_OK)
    {
        if (state->event_count < UINT_MAX) state->event_count++;
    }
    else if (state->rejected_event_count < UINT_MAX)
    {
        state->rejected_event_count++;
    }

    return status;
}

int syntra_input_key_down(
    const syntra_input_state_t *state,
    uint16_t key_code)
{
    uint64_t mask;

    if (state == NULL || state->initialized != 1)
        return 0;

    if (key_code == SYNTRA_INPUT_BTN_LEFT)
        return (state->buttons & SYNTRA_INPUT_BUTTON_LEFT) != 0U;
    if (key_code == SYNTRA_INPUT_BTN_RIGHT)
        return (state->buttons & SYNTRA_INPUT_BUTTON_RIGHT) != 0U;
    if (key_code == SYNTRA_INPUT_BTN_MIDDLE)
        return (state->buttons & SYNTRA_INPUT_BUTTON_MIDDLE) != 0U;
    if (key_code > SYNTRA_INPUT_KEY_MAX)
        return 0;

    mask = (uint64_t)1U << ((unsigned int)key_code % 64U);
    return (state->keys[(unsigned int)key_code / 64U] & mask) != 0U;
}
