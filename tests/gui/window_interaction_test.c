#include "../../kernel/gui/window_interaction.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

static void set_pointer(syntra_input_state_t *input, uint32_t x, uint32_t y,
                        uint32_t buttons)
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
    uint32_t back_id;
    uint32_t front_id;

    assert(syntra_gui_window_manager_init(&manager, 640U, 480U) ==
           SYNTRA_STATUS_OK);
    assert(syntra_gui_window_interaction_init(&interaction) ==
           SYNTRA_STATUS_OK);
    assert(syntra_input_init(&input, 640U, 480U) == SYNTRA_STATUS_OK);

    assert(syntra_gui_window_open(&manager, 20U, 30U, 180U, 120U,
                                  &back_id) == SYNTRA_STATUS_OK);
    assert(syntra_gui_window_open(&manager, 60U, 50U, 180U, 120U,
                                  &front_id) == SYNTRA_STATUS_OK);

    /* Clicking an overlapping point focuses the topmost window. */
    set_pointer(&input, 35U, 38U, SYNTRA_INPUT_BUTTON_LEFT);
    assert(syntra_gui_window_interaction_process(
               &interaction, &manager, &input) == SYNTRA_STATUS_OK);
    assert(syntra_gui_window_active(&manager)->id == back_id);

    /* Release, then press the title bar to begin dragging. */
    set_pointer(&input, 35U, 38U, 0U);
    assert(syntra_gui_window_interaction_process(
               &interaction, &manager, &input) == SYNTRA_STATUS_OK);
    set_pointer(&input, 35U, 38U, SYNTRA_INPUT_BUTTON_LEFT);
    assert(syntra_gui_window_interaction_process(
               &interaction, &manager, &input) == SYNTRA_STATUS_OK);
    assert(interaction.drag_window_id == back_id);

    /* Pointer motion while held moves the window while preserving grab offset. */
    set_pointer(&input, 95U, 88U, SYNTRA_INPUT_BUTTON_LEFT);
    assert(syntra_gui_window_interaction_process(
               &interaction, &manager, &input) == SYNTRA_STATUS_OK);
    assert(syntra_gui_window_get(&manager, back_id, &window) ==
           SYNTRA_STATUS_OK);
    assert(window->x == 80U && window->y == 80U);

    /* Dragging beyond the right/bottom edges clamps to the screen boundary. */
    set_pointer(&input, 639U, 479U, SYNTRA_INPUT_BUTTON_LEFT);
    assert(syntra_gui_window_interaction_process(
               &interaction, &manager, &input) == SYNTRA_STATUS_OK);
    assert(syntra_gui_window_get(&manager, back_id, &window) ==
           SYNTRA_STATUS_OK);
    assert(window->x == 460U && window->y == 360U);

    set_pointer(&input, 639U, 479U, 0U);
    assert(syntra_gui_window_interaction_process(
               &interaction, &manager, &input) == SYNTRA_STATUS_OK);
    assert(interaction.drag_window_id == 0U);

    /* The close target matches the renderer's close-control rectangle. */
    set_pointer(&input, 460U + 180U - 11U, 360U + 10U,
                SYNTRA_INPUT_BUTTON_LEFT);
    assert(syntra_gui_window_interaction_process(
               &interaction, &manager, &input) == SYNTRA_STATUS_OK);
    assert(syntra_gui_window_get(&manager, back_id, &window) ==
           SYNTRA_STATUS_INVALID_ARGUMENT);
    assert(manager.count == 1U);
    assert(syntra_gui_window_active(&manager)->id == front_id);
    set_pointer(&input, 639U, 479U, 0U);
    assert(syntra_gui_window_interaction_process(
        &interaction, &manager, &input) == SYNTRA_STATUS_OK);

    /* Direct titlebar controls maximize, restore and minimize windows. */
    {
        uint32_t control_id;
        assert(syntra_gui_window_open(&manager, 100U, 100U, 180U, 120U,
                                     &control_id) == SYNTRA_STATUS_OK);
        set_pointer(&input, 246U, 110U, SYNTRA_INPUT_BUTTON_LEFT);
        assert(syntra_gui_window_interaction_process(
            &interaction, &manager, &input) == SYNTRA_STATUS_OK);
        assert(syntra_gui_window_get(&manager, control_id, &window) ==
               SYNTRA_STATUS_OK);
        assert(window->maximized == 1U && window->x == 0U &&
               window->y == 0U && window->width == 640U &&
               window->height == 480U);

        set_pointer(&input, 606U, 10U, 0U);
        assert(syntra_gui_window_interaction_process(
            &interaction, &manager, &input) == SYNTRA_STATUS_OK);
        set_pointer(&input, 606U, 10U, SYNTRA_INPUT_BUTTON_LEFT);
        assert(syntra_gui_window_interaction_process(
            &interaction, &manager, &input) == SYNTRA_STATUS_OK);
        assert(syntra_gui_window_get(&manager, control_id, &window) ==
               SYNTRA_STATUS_OK);
        assert(window->maximized == 0U && window->x == 100U &&
               window->y == 100U && window->width == 180U &&
               window->height == 120U);

        set_pointer(&input, 606U, 110U, 0U);
        assert(syntra_gui_window_interaction_process(
            &interaction, &manager, &input) == SYNTRA_STATUS_OK);
        set_pointer(&input, 229U, 110U, SYNTRA_INPUT_BUTTON_LEFT);
        assert(syntra_gui_window_interaction_process(
            &interaction, &manager, &input) == SYNTRA_STATUS_OK);
        assert(syntra_gui_window_get(&manager, control_id, &window) ==
               SYNTRA_STATUS_OK);
        assert(window->minimized == 1U);
        assert(syntra_gui_window_active(&manager)->id == front_id);
        assert(syntra_gui_window_restore(&manager, control_id) ==
               SYNTRA_STATUS_OK);
        assert(syntra_gui_window_active(&manager)->id == control_id);
    }
    /* Invalid dimension pairing is rejected without mutating the manager. */
    assert(syntra_input_init(&input, 320U, 240U) == SYNTRA_STATUS_OK);
    assert(syntra_gui_window_interaction_process(
               &interaction, &manager, &input) ==
           SYNTRA_STATUS_INVALID_ARGUMENT);
    assert(manager.count == 1U);

    puts("SyntraOS GUI window interaction tests passed.");
    return 0;
}
