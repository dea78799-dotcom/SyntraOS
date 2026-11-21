#include "../../kernel/gui/desktop.h"
#include "../../kernel/gui/window_interaction.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>

static void pointer(syntra_input_state_t *input,
                   uint32_t x, uint32_t y, uint32_t buttons)
{
    input->pointer_x = x;
    input->pointer_y = y;
    input->buttons = buttons;
}

int main(void)
{
    syntra_gui_window_manager_t manager;
    syntra_gui_window_interaction_t interaction;
    syntra_input_state_t input;
    const syntra_gui_window_t *window = NULL;
    uint32_t files_id, settings_id, launcher_id;

    assert(syntra_desktop_gui_v5_init_windows(&manager, 640U, 480U) ==
           SYNTRA_STATUS_OK);
    assert(manager.count == 3U);
    files_id = manager.windows[0].id;
    settings_id = manager.windows[1].id;
    launcher_id = manager.windows[2].id;
    assert(manager.windows[0].app_type == SYNTRA_GUI_APP_FILES);
    assert(manager.windows[1].app_type == SYNTRA_GUI_APP_SETTINGS);
    assert(manager.windows[2].app_type == SYNTRA_GUI_APP_LAUNCHER);
    assert(syntra_gui_window_active(&manager)->id == launcher_id);

    assert(syntra_gui_window_interaction_init(&interaction) ==
           SYNTRA_STATUS_OK);
    assert(syntra_input_init(&input, 640U, 480U) == SYNTRA_STATUS_OK);

    /* Drag Launcher using its title bar, preserving the grab offset. */
    pointer(&input, 250U, 70U, SYNTRA_INPUT_BUTTON_LEFT);
    assert(syntra_gui_window_interaction_process(
        &interaction, &manager, &input) == SYNTRA_STATUS_OK);
    assert(interaction.drag_window_id == launcher_id);
    pointer(&input, 300U, 120U, SYNTRA_INPUT_BUTTON_LEFT);
    assert(syntra_gui_window_interaction_process(
        &interaction, &manager, &input) == SYNTRA_STATUS_OK);
    assert(syntra_gui_window_get(&manager, launcher_id, &window) ==
           SYNTRA_STATUS_OK);
    assert(window->x == 280U && window->y == 110U);
    pointer(&input, 300U, 120U, 0U);
    assert(syntra_gui_window_interaction_process(
        &interaction, &manager, &input) == SYNTRA_STATUS_OK);

    /* Close Files after Launcher has been moved away from the target. */
    pointer(&input, 279U, 200U, SYNTRA_INPUT_BUTTON_LEFT);
    assert(syntra_gui_window_interaction_process(
        &interaction, &manager, &input) == SYNTRA_STATUS_OK);
    assert(syntra_gui_window_get(&manager, files_id, &window) ==
           SYNTRA_STATUS_INVALID_ARGUMENT);
    assert(manager.count == 2U);
    assert(syntra_gui_window_get(&manager, settings_id, &window) ==
           SYNTRA_STATUS_OK);
    assert(syntra_gui_window_active(&manager)->id == launcher_id ||
           syntra_gui_window_active(&manager)->id == settings_id);

    puts("SyntraOS GUI v5 composed-window interaction integration passed.");
    return 0;
}
