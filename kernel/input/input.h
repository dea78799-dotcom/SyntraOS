#ifndef SYNTRA_INPUT_H
#define SYNTRA_INPUT_H

#include <stdint.h>
#include "../core/status.h"

/* VirtIO input uses the Linux input-event type and code namespace. */
#define SYNTRA_INPUT_EV_SYN 0U
#define SYNTRA_INPUT_EV_KEY 1U
#define SYNTRA_INPUT_EV_REL 2U
#define SYNTRA_INPUT_EV_ABS 3U

#define SYNTRA_INPUT_SYN_REPORT 0U
#define SYNTRA_INPUT_SYN_MT_REPORT 2U

#define SYNTRA_INPUT_REL_X 0U
#define SYNTRA_INPUT_REL_Y 1U
#define SYNTRA_INPUT_REL_HWHEEL 6U
#define SYNTRA_INPUT_REL_WHEEL 8U

#define SYNTRA_INPUT_ABS_X 0U
#define SYNTRA_INPUT_ABS_Y 1U

#define SYNTRA_INPUT_BTN_LEFT 0x110U
#define SYNTRA_INPUT_BTN_RIGHT 0x111U
#define SYNTRA_INPUT_BTN_MIDDLE 0x112U

#define SYNTRA_INPUT_KEY_MAX 255U

#define SYNTRA_INPUT_BUTTON_LEFT   (1U << 0)
#define SYNTRA_INPUT_BUTTON_RIGHT  (1U << 1)
#define SYNTRA_INPUT_BUTTON_MIDDLE (1U << 2)

typedef struct
{
    uint16_t type;
    uint16_t code;
    int32_t value;
} syntra_input_event_t;

typedef struct
{
    uint32_t width;
    uint32_t height;
    uint32_t pointer_x;
    uint32_t pointer_y;
    uint32_t buttons;

    uint64_t keys[4];

    int32_t wheel_x;
    int32_t wheel_y;

    int32_t abs_x_min;
    int32_t abs_x_max;
    int32_t abs_y_min;
    int32_t abs_y_max;
    int abs_range_configured;

    uint32_t event_count;
    uint32_t frame_sequence;
    uint32_t key_repeat_count;
    uint32_t rejected_event_count;

    int initialized;
} syntra_input_state_t;

/* Initialize an input state for the target framebuffer dimensions. */
syntra_status_t syntra_input_init(
    syntra_input_state_t *state,
    uint32_t width,
    uint32_t height
);

/* Configure the absolute-axis ranges reported by the input device. */
syntra_status_t syntra_input_set_abs_range(
    syntra_input_state_t *state,
    int32_t x_min,
    int32_t x_max,
    int32_t y_min,
    int32_t y_max
);

/* Consume one decoded VirtIO/Linux input event. */
syntra_status_t syntra_input_process(
    syntra_input_state_t *state,
    const syntra_input_event_t *event
);

int syntra_input_key_down(
    const syntra_input_state_t *state,
    uint16_t key_code
);

#endif
