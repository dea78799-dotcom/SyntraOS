#ifndef SYNTRA_GUI_DESKTOP_SESSION_H
#define SYNTRA_GUI_DESKTOP_SESSION_H

#include <stdint.h>

#include "../core/status.h"
#include "../input/input.h"

#define SYNTRA_DESKTOP_APP_COUNT 4U

typedef struct
{
    syntra_input_state_t input;
    uint32_t focused_app;
    int32_t activated_app;
    uint32_t activation_count;
    int initialized;
} syntra_desktop_session_t;

/* Initialize desktop interaction state for a virtual framebuffer. */
syntra_status_t syntra_desktop_session_init(
    syntra_desktop_session_t *session,
    uint32_t width,
    uint32_t height
);

/*
 * Consume one normalized event from an input backend.
 * This layer is transport-independent: QEMU VirtIO events can be passed
 * here after decoding, while host tests can inject deterministic events.
 */
syntra_status_t syntra_desktop_session_process(
    syntra_desktop_session_t *session,
    const syntra_input_event_t *event
);

#endif
