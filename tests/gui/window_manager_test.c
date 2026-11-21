#include "../../kernel/gui/window_manager.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

int main(void)
{
    syntra_gui_window_manager_t manager;
    const syntra_gui_window_t *window = NULL;
    uint32_t ids[SYNTRA_GUI_MAX_WINDOWS];
    uint32_t index;

    assert(syntra_gui_window_manager_init(&manager, 640U, 480U) ==
           SYNTRA_STATUS_OK);
    assert(manager.count == 0U);
    assert(syntra_gui_window_active(&manager) == NULL);

    assert(syntra_gui_window_open(&manager, 10U, 20U, 300U, 200U,
                                  &ids[0]) == SYNTRA_STATUS_OK);
    assert(ids[0] == 1U);
    assert(syntra_gui_window_open(&manager, 40U, 50U, 240U, 160U,
                                  &ids[1]) == SYNTRA_STATUS_OK);
    assert(ids[1] == 2U);
    assert(syntra_gui_window_active(&manager)->id == ids[1]);

    /* Focusing an older window raises it without changing its identity. */
    assert(syntra_gui_window_focus(&manager, ids[0]) == SYNTRA_STATUS_OK);
    assert(syntra_gui_window_active(&manager)->id == ids[0]);

    /* Move accepts only rectangles fully contained by the virtual screen. */
    assert(syntra_gui_window_move(&manager, ids[0], 100U, 120U) ==
           SYNTRA_STATUS_OK);
    assert(syntra_gui_window_get(&manager, ids[0], &window) ==
           SYNTRA_STATUS_OK);
    assert(window->x == 100U && window->y == 120U);
    assert(syntra_gui_window_move(&manager, ids[0], 400U, 300U) ==
           SYNTRA_STATUS_INVALID_ARGUMENT);
    assert(syntra_gui_window_get(&manager, ids[0], &window) ==
           SYNTRA_STATUS_OK);
    assert(window->x == 100U && window->y == 120U);

    /* Close removes the window and preserves the remaining active window. */
    assert(syntra_gui_window_close(&manager, ids[0]) == SYNTRA_STATUS_OK);
    assert(syntra_gui_window_get(&manager, ids[0], &window) ==
           SYNTRA_STATUS_INVALID_ARGUMENT);
    assert(syntra_gui_window_active(&manager)->id == ids[1]);

    assert(syntra_gui_window_close(&manager, ids[1]) == SYNTRA_STATUS_OK);
    assert(syntra_gui_window_active(&manager) == NULL);

    /* Invalid geometry and unknown IDs are rejected. */
    assert(syntra_gui_window_open(&manager, 0U, 0U, 0U, 10U,
                                  &ids[0]) == SYNTRA_STATUS_INVALID_ARGUMENT);
    assert(syntra_gui_window_open(&manager, 630U, 0U, 20U, 10U,
                                  &ids[0]) == SYNTRA_STATUS_INVALID_ARGUMENT);
    assert(syntra_gui_window_focus(&manager, 999U) ==
           SYNTRA_STATUS_INVALID_ARGUMENT);
    assert(syntra_gui_window_close(&manager, 999U) ==
           SYNTRA_STATUS_INVALID_ARGUMENT);

    /* Capacity is fixed and bounded; the ninth open is rejected cleanly. */
    for (index = 0U; index < SYNTRA_GUI_MAX_WINDOWS; ++index)
    {
        assert(syntra_gui_window_open(&manager, index * 2U, index * 2U,
                                      100U, 80U, &ids[index]) ==
               SYNTRA_STATUS_OK);
    }
    assert(manager.count == SYNTRA_GUI_MAX_WINDOWS);
    assert(syntra_gui_window_open(&manager, 0U, 0U, 100U, 80U,
                                  &ids[0]) == SYNTRA_STATUS_OUT_OF_MEMORY);
    assert(manager.count == SYNTRA_GUI_MAX_WINDOWS);

    puts("SyntraOS GUI window manager tests passed.");
    return 0;
}
