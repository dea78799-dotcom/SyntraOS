#include "desktop_session.h"

#include <stddef.h>

#define KEY_ENTER 28U
#define KEY_LEFT 105U
#define KEY_RIGHT 106U
#define KEY_UP 103U
#define KEY_DOWN 108U

static int point_in_app_tile(
    const syntra_desktop_session_t *session,
    uint32_t app,
    uint32_t x,
    uint32_t y)
{
    uint32_t tile_x;
    uint32_t tile_width;
    uint32_t tile_y;
    uint32_t tile_height;

    if (session->input.width < 640U || session->input.height < 480U)
    {
        tile_width = (session->input.width - 48U) / SYNTRA_DESKTOP_APP_COUNT;
        tile_x = 12U + app * (tile_width + 8U);
        tile_y = 112U;
        tile_height = 52U;
    }
    else
    {
        tile_x = 24U + app * 80U;
        tile_y = 180U;
        tile_width = 64U;
        tile_height = 64U;
    }

    return x >= tile_x && x < tile_x + tile_width &&
           y >= tile_y && y < tile_y + tile_height;
}

static void activate(syntra_desktop_session_t *session, uint32_t app)
{
    if (app >= SYNTRA_DESKTOP_APP_COUNT) return;
    session->focused_app = app;
    session->activated_app = (int32_t)app;
    session->activation_count++;
}

syntra_status_t syntra_desktop_session_init(
    syntra_desktop_session_t *session,
    uint32_t width,
    uint32_t height)
{
    syntra_status_t status;

    if (session == NULL || width < 320U || height < 240U)
        return SYNTRA_STATUS_INVALID_ARGUMENT;

    status = syntra_input_init(&session->input, width, height);
    if (status != SYNTRA_STATUS_OK) return status;

    session->focused_app = 0U;
    session->activated_app = -1;
    session->activation_count = 0U;
    session->initialized = 1;
    return SYNTRA_STATUS_OK;
}

syntra_status_t syntra_desktop_session_process(
    syntra_desktop_session_t *session,
    const syntra_input_event_t *event)
{
    uint32_t old_buttons;
    syntra_status_t status;

    if (session == NULL || session->initialized != 1 || event == NULL)
        return SYNTRA_STATUS_INVALID_ARGUMENT;

    old_buttons = session->input.buttons;
    status = syntra_input_process(&session->input, event);
    if (status != SYNTRA_STATUS_OK) return status;

    if (event->type == SYNTRA_INPUT_EV_KEY && event->value == 1)
    {
        if (event->code == KEY_LEFT || event->code == KEY_UP)
        {
            session->focused_app =
                (session->focused_app + SYNTRA_DESKTOP_APP_COUNT - 1U) %
                SYNTRA_DESKTOP_APP_COUNT;
        }
        else if (event->code == KEY_RIGHT || event->code == KEY_DOWN)
        {
            session->focused_app =
                (session->focused_app + 1U) % SYNTRA_DESKTOP_APP_COUNT;
        }
        else if (event->code == KEY_ENTER)
        {
            activate(session, session->focused_app);
        }
    }

    if (event->type == SYNTRA_INPUT_EV_KEY &&
        event->code == SYNTRA_INPUT_BTN_LEFT &&
        event->value == 1 &&
        (old_buttons & SYNTRA_INPUT_BUTTON_LEFT) == 0U)
    {
        uint32_t app;
        for (app = 0U; app < SYNTRA_DESKTOP_APP_COUNT; ++app)
        {
            if (point_in_app_tile(
                    session, app,
                    session->input.pointer_x,
                    session->input.pointer_y))
            {
                activate(session, app);
                break;
            }
        }
    }

    return SYNTRA_STATUS_OK;
}
