#ifndef SYNTRA_GUI_WINDOW_MANAGER_H
#define SYNTRA_GUI_WINDOW_MANAGER_H

#include <stdint.h>
#include "../core/status.h"

#define SYNTRA_GUI_MAX_WINDOWS 8U

typedef enum
{
    SYNTRA_GUI_APP_GENERIC = 0U,
    SYNTRA_GUI_APP_FILES = 1U,
    SYNTRA_GUI_APP_SETTINGS = 2U,
    SYNTRA_GUI_APP_LAUNCHER = 3U
} syntra_gui_app_type_t;

typedef struct
{
    uint32_t id;
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;
    syntra_gui_app_type_t app_type;
    uint32_t app_selection;
} syntra_gui_window_t;

/*
 * Fixed-capacity window manager. The windows array is ordered from back
 * to front; the final entry is the focused/frontmost window.
 */
typedef struct
{
    uint32_t screen_width;
    uint32_t screen_height;
    uint32_t next_id;
    uint32_t count;
    int initialized;
    syntra_gui_window_t windows[SYNTRA_GUI_MAX_WINDOWS];
} syntra_gui_window_manager_t;

syntra_status_t syntra_gui_window_manager_init(
    syntra_gui_window_manager_t *manager,
    uint32_t screen_width,
    uint32_t screen_height);

syntra_status_t syntra_gui_window_open_app(
    syntra_gui_window_manager_t *manager,
    uint32_t x,
    uint32_t y,
    uint32_t width,
    uint32_t height,
    syntra_gui_app_type_t app_type,
    uint32_t *window_id);

syntra_status_t syntra_gui_window_open(
    syntra_gui_window_manager_t *manager,
    uint32_t x,
    uint32_t y,
    uint32_t width,
    uint32_t height,
    uint32_t *window_id);

syntra_status_t syntra_gui_window_close(
    syntra_gui_window_manager_t *manager,
    uint32_t window_id);

syntra_status_t syntra_gui_window_move(
    syntra_gui_window_manager_t *manager,
    uint32_t window_id,
    uint32_t x,
    uint32_t y);

syntra_status_t syntra_gui_window_focus(
    syntra_gui_window_manager_t *manager,
    uint32_t window_id);

syntra_status_t syntra_gui_window_get(
    const syntra_gui_window_manager_t *manager,
    uint32_t window_id,
    const syntra_gui_window_t **window);

const syntra_gui_window_t *syntra_gui_window_active(
    const syntra_gui_window_manager_t *manager);

#endif
