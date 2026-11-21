#ifndef SYNTRA_GUI_DESKTOP_H
#define SYNTRA_GUI_DESKTOP_H

#include "framebuffer.h"

/*
 * Draws the static SyntraOS desktop shell. Selects a compact visual layout
 * for smaller framebuffers. Pointer, keyboard and touch input are not handled.
 */
syntra_status_t syntra_desktop_render(syntra_framebuffer_t *framebuffer);

#endif
