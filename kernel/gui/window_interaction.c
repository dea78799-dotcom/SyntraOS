#include "window_interaction.h"

#include <stddef.h>

#define TITLE_TOP_OFFSET 2U
#define TITLE_HEIGHT 20U
#define CLOSE_SIZE 12U
#define CLOSE_RIGHT_MARGIN 5U
#define CLOSE_TOP_OFFSET 2U
#define TOUCH_CONTROL_WIDTH 17U
#define TOUCH_CONTROL_HEIGHT 18U
#define CONTROL_GAP 5U
#define CONTROLS_MIN_WIDTH 120U

static int point_inside(
    uint32_t x, uint32_t y,
    uint32_t left, uint32_t top,
    uint32_t width, uint32_t height)
{
    return x >= left && y >= top &&
           x - left < width && y - top < height;
}

static int point_on_close_control(
    const syntra_gui_window_t *window,
    uint32_t x,
    uint32_t y)
{
    uint32_t close_x;

    if (window->width < 58U || window->height < 20U)
        return 0;

    close_x = window->x + window->width - CLOSE_SIZE - CLOSE_RIGHT_MARGIN;
    return point_inside(
        x, y, close_x - 2U,
        window->y + CLOSE_TOP_OFFSET,
        TOUCH_CONTROL_WIDTH, TOUCH_CONTROL_HEIGHT);
}

static int point_on_maximize_control(
    const syntra_gui_window_t *window, uint32_t x, uint32_t y)
{
    uint32_t control_x;
    if (window->width < CONTROLS_MIN_WIDTH || window->height < 20U)
        return 0;
    control_x = window->x + window->width - CLOSE_SIZE -
                CLOSE_RIGHT_MARGIN - CONTROL_GAP - CLOSE_SIZE;
    return point_inside(x, y, control_x - 2U,
                        window->y + CLOSE_TOP_OFFSET,
                        TOUCH_CONTROL_WIDTH, TOUCH_CONTROL_HEIGHT);
}

static int point_on_minimize_control(
    const syntra_gui_window_t *window, uint32_t x, uint32_t y)
{
    uint32_t control_x;
    if (window->width < CONTROLS_MIN_WIDTH || window->height < 20U)
        return 0;
    control_x = window->x + window->width - CLOSE_SIZE -
                CLOSE_RIGHT_MARGIN - 2U * (CONTROL_GAP + CLOSE_SIZE);
    return point_inside(x, y, control_x - 2U,
                        window->y + CLOSE_TOP_OFFSET,
                        TOUCH_CONTROL_WIDTH, TOUCH_CONTROL_HEIGHT);
}
static int point_on_title_bar(
    const syntra_gui_window_t *window,
    uint32_t x,
    uint32_t y)
{
    if (window->width <= 2U || window->height <= 2U)
        return 0;

    return point_inside(
        x, y,
        window->x + 2U,
        window->y + TITLE_TOP_OFFSET,
        window->width - 4U,
        (window->height - 2U < TITLE_HEIGHT)
            ? window->height - 2U : TITLE_HEIGHT);
}

syntra_status_t syntra_gui_window_interaction_init(
    syntra_gui_window_interaction_t *interaction)
{
    if (interaction == NULL)
        return SYNTRA_STATUS_INVALID_ARGUMENT;

    interaction->previous_buttons = 0U;
    interaction->drag_window_id = 0U;
    interaction->drag_offset_x = 0U;
    interaction->drag_offset_y = 0U;
    interaction->initialized = 1;
    return SYNTRA_STATUS_OK;
}

syntra_status_t syntra_gui_window_interaction_process(
    syntra_gui_window_interaction_t *interaction,
    syntra_gui_window_manager_t *manager,
    const syntra_input_state_t *input)
{
    uint32_t left_down;
    uint32_t left_was_down;
    syntra_status_t status;

    if (interaction == NULL || interaction->initialized != 1 ||
        manager == NULL || manager->initialized != 1 ||
        input == NULL || input->initialized != 1 ||
        manager->screen_width != input->width ||
        manager->screen_height != input->height)
        return SYNTRA_STATUS_INVALID_ARGUMENT;

    left_down = input->buttons & SYNTRA_INPUT_BUTTON_LEFT;
    left_was_down = interaction->previous_buttons &
                    SYNTRA_INPUT_BUTTON_LEFT;

    if (left_down != 0U && left_was_down == 0U)
    {
        uint32_t index = manager->count;
        uint32_t hit_id = 0U;

        interaction->drag_window_id = 0U;

        while (index > 0U)
        {
            const syntra_gui_window_t *window;
            --index;
            window = &manager->windows[index];
            if (window->minimized != 0U) continue;

            if (point_inside(input->pointer_x, input->pointer_y,
                             window->x, window->y,
                             window->width, window->height))
            {
                hit_id = window->id;
                break;
            }
        }

        if (hit_id != 0U)
        {
            const syntra_gui_window_t *window = NULL;

            status = syntra_gui_window_focus(manager, hit_id);
            if (status != SYNTRA_STATUS_OK) return status;

            status = syntra_gui_window_get(manager, hit_id, &window);
            if (status != SYNTRA_STATUS_OK) return status;

            if (point_on_close_control(window, input->pointer_x, input->pointer_y))
            {
                status = syntra_gui_window_close(manager, hit_id);
                if (status != SYNTRA_STATUS_OK) return status;
            }
            else if (point_on_maximize_control(window, input->pointer_x, input->pointer_y))
            {
                status = window->maximized != 0U
                    ? syntra_gui_window_restore(manager, hit_id)
                    : syntra_gui_window_maximize(manager, hit_id);
                if (status != SYNTRA_STATUS_OK) return status;
            }
            else if (point_on_minimize_control(window, input->pointer_x, input->pointer_y))
            {
                status = syntra_gui_window_minimize(manager, hit_id);
                if (status != SYNTRA_STATUS_OK) return status;
            }
            else if (point_on_title_bar(window, input->pointer_x, input->pointer_y) &&
                     window->maximized == 0U)
            {
                interaction->drag_window_id = hit_id;
                interaction->drag_offset_x = input->pointer_x - window->x;
                interaction->drag_offset_y = input->pointer_y - window->y;
            }
        }
    }

    if (left_down != 0U && interaction->drag_window_id != 0U)
    {
        const syntra_gui_window_t *window = NULL;
        uint32_t new_x;
        uint32_t new_y;
        uint32_t max_x;
        uint32_t max_y;

        status = syntra_gui_window_get(
            manager, interaction->drag_window_id, &window);
        if (status == SYNTRA_STATUS_INVALID_ARGUMENT)
        {
            interaction->drag_window_id = 0U;
        }
        else if (status != SYNTRA_STATUS_OK)
        {
            return status;
        }
        else
        {
            max_x = manager->screen_width - window->width;
            max_y = manager->screen_height - window->height;
            new_x = (input->pointer_x >= interaction->drag_offset_x)
                ? input->pointer_x - interaction->drag_offset_x : 0U;
            new_y = (input->pointer_y >= interaction->drag_offset_y)
                ? input->pointer_y - interaction->drag_offset_y : 0U;
            if (new_x > max_x) new_x = max_x;
            if (new_y > max_y) new_y = max_y;

            status = syntra_gui_window_move(
                manager, interaction->drag_window_id, new_x, new_y);
            if (status != SYNTRA_STATUS_OK) return status;
        }
    }

    if (left_down == 0U)
        interaction->drag_window_id = 0U;

    interaction->previous_buttons = input->buttons;
    return SYNTRA_STATUS_OK;
}
