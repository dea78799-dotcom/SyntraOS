#ifndef SYNTRA_GUI_UPTIME_PANEL_H
#define SYNTRA_GUI_UPTIME_PANEL_H

#include <stdint.h>
#include "framebuffer.h"
#include "../core/status.h"

/* Draw a compact uptime badge at the bottom-left of the desktop. */
syntra_status_t syntra_gui_render_uptime_panel(
    syntra_framebuffer_t *framebuffer,
    uint64_t elapsed_seconds);

#endif
