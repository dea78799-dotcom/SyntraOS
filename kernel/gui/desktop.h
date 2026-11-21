#ifndef SYNTRA_GUI_DESKTOP_H
#define SYNTRA_GUI_DESKTOP_H

#include "framebuffer.h"
#include "desktop_session.h"

/*
 * Draws the static SyntraOS desktop shell. Selects a compact visual layout
 * for smaller framebuffers. Pointer, keyboard and touch input are not handled.
 */
syntra_status_t syntra_desktop_render(syntra_framebuffer_t *framebuffer);

/* GUI v5 composed preview: desktop plus typed app windows on large displays. */
syntra_status_t syntra_desktop_render_gui_v5(
    syntra_framebuffer_t *framebuffer);

/* Render the desktop and draw a visible focus outline around the focused app. */
syntra_status_t syntra_desktop_render_session(
    syntra_framebuffer_t *framebuffer,
    const syntra_desktop_session_t *session);

#endif
