#ifndef SYNTRA_GUI_RUNTIME_H
#define SYNTRA_GUI_RUNTIME_H

#include "framebuffer.h"
#include "window_manager.h"
#include "window_interaction.h"
#include "../input/input.h"
#include "gesture.h"

typedef struct
{
    syntra_input_state_t input;
    syntra_gui_window_manager_t windows;
    syntra_gui_window_interaction_t interaction;
    syntra_gesture_t gesture;
    syntra_gesture_result_t last_gesture;
    uint64_t gesture_time_ms;
    uint32_t redraw_count;
    int initialized;
} syntra_gui_runtime_t;

/* Persistent GUI runtime; owns normalized input, window state and drag state. */
syntra_status_t syntra_gui_runtime_init(
    syntra_gui_runtime_t *runtime,
    uint32_t width,
    uint32_t height);

/* Process one ordered input event outside IRQ context. */
syntra_status_t syntra_gui_runtime_process_event(
    syntra_gui_runtime_t *runtime,
    const syntra_input_event_t *event);

/* Timestamped input path; now_ms must be monotonic milliseconds. */
syntra_status_t syntra_gui_runtime_process_event_at(
    syntra_gui_runtime_t *runtime,
    const syntra_input_event_t *event,
    uint64_t now_ms);

/* Advance hold/long-press recognition even when no new input arrives. */
syntra_status_t syntra_gui_runtime_tick(
    syntra_gui_runtime_t *runtime,
    uint64_t now_ms);

/* Render desktop plus the runtime's persistent windows into a framebuffer. */
syntra_status_t syntra_gui_runtime_render(
    syntra_gui_runtime_t *runtime,
    syntra_framebuffer_t *framebuffer);

#endif
