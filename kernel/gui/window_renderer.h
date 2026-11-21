#ifndef SYNTRA_GUI_WINDOW_RENDERER_H
#define SYNTRA_GUI_WINDOW_RENDERER_H

#include "framebuffer.h"
#include "window_manager.h"

/*
 * Draws managed windows back-to-front over an already-rendered desktop.
 * This is a visual layer only; it does not process pointer or keyboard input.
 */
syntra_status_t syntra_gui_render_windows(
    syntra_framebuffer_t *framebuffer,
    const syntra_gui_window_manager_t *manager);

#endif
