#ifndef SYNTRA_GUI_DESKTOP_H
#define SYNTRA_GUI_DESKTOP_H

#include "framebuffer.h"

/* Draws a static, input-free desktop preview into a validated framebuffer. */
syntra_status_t syntra_desktop_render(syntra_framebuffer_t *framebuffer);

#endif
