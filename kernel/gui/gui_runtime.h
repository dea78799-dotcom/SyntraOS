#ifndef SYNTRA_GUI_RUNTIME_H
#define SYNTRA_GUI_RUNTIME_H

#include "framebuffer.h"
#include "window_manager.h"
#include "window_interaction.h"
#include "../input/input.h"

typedef struct
{
    syntra_input_state_t input;
    syntra_gui_window_manager_t windows;
    syntra_gui_window_interaction_t interaction;
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

/* Render desktop plus the runtime's persistent windows into a framebuffer. */
syntra_status_t syntra_gui_runtime_render(
    syntra_gui_runtime_t *runtime,
    syntra_framebuffer_t *framebuffer);

#endif
