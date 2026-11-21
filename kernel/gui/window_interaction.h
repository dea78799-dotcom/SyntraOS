#ifndef SYNTRA_GUI_WINDOW_INTERACTION_H
#define SYNTRA_GUI_WINDOW_INTERACTION_H

#include <stdint.h>
#include "window_manager.h"
#include "../input/input.h"

typedef struct
{
    uint32_t previous_buttons;
    uint32_t drag_window_id;
    uint32_t drag_offset_x;
    uint32_t drag_offset_y;
    int initialized;
} syntra_gui_window_interaction_t;

syntra_status_t syntra_gui_window_interaction_init(
    syntra_gui_window_interaction_t *interaction);

syntra_status_t syntra_gui_window_interaction_process(
    syntra_gui_window_interaction_t *interaction,
    syntra_gui_window_manager_t *manager,
    const syntra_input_state_t *input);

#endif
