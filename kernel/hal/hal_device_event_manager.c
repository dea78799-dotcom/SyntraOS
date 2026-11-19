
#include "hal_device_event_manager.h"

#include <limits.h>
#include <stddef.h>
#include <string.h>

static int event_type_is_valid(
    syntra_hal_device_event_type_t type
)
{
    return (int)type >=
               (int)SYNTRA_HAL_DEVICE_EVENT_ADDED &&
           (int)type <
               (int)SYNTRA_HAL_DEVICE_EVENT_TYPE_COUNT;
}

static int device_state_is_valid(
    syntra_hal_device_state_t state
)
{
    return (int)state >=
               (int)SYNTRA_HAL_DEVICE_OFFLINE &&
           (int)state <=
               (int)SYNTRA_HAL_DEVICE_ERROR;
}

static int manager_is_valid(
    const syntra_hal_device_event_manager_t *manager
)
{
    if (manager == NULL || manager->initialized != 1)
    {
        return 0;
    }

    if (manager->event_count >
        SYNTRA_HAL_DEVICE_EVENT_MANAGER_MAX_EVENTS)
    {
        return 0;
    }

    return 1;
}

static void update_counter(unsigned int *counter)
{
    if (*counter < UINT_MAX)
    {
        (*counter)++;
    }
}

static void clear_validation_report(
    syntra_hal_device_event_validation_report_t *report
)
{
    report->invalid_events = 0U;
    report->duplicate_sequences = 0U;
    report->out_of_order_sequences = 0U;
    report->sequence_gaps = 0U;
}

syntra_status_t hal_device_event_manager_init(
    syntra_hal_device_event_manager_t *manager
)
{
    if (manager == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    memset(manager, 0, sizeof(*manager));

    manager->initialized = 1;
    manager->dirty = 0;

    return SYNTRA_STATUS_OK;
}

syntra_status_t hal_device_event_manager_reset(
    syntra_hal_device_event_manager_t *manager
)
{
    if (!manager_is_valid(manager))
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    return hal_device_event_manager_init(manager);
}

syntra_status_t hal_device_event_manager_record(
    syntra_hal_device_event_manager_t *manager,
    unsigned int device_id,
    syntra_hal_device_event_type_t type,
    syntra_hal_device_state_t previous_state,
    syntra_hal_device_state_t current_state
)
{
    unsigned int index;

    if (!manager_is_valid(manager) ||
        !event_type_is_valid(type) ||
        !device_state_is_valid(previous_state) ||
        !device_state_is_valid(current_state))
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (manager->event_sequence == UINT_MAX)
    {
        return SYNTRA_STATUS_ERROR;
    }

    if (manager->event_count >=
        SYNTRA_HAL_DEVICE_EVENT_MANAGER_MAX_EVENTS)
    {
        for (index = 1U;
             index < SYNTRA_HAL_DEVICE_EVENT_MANAGER_MAX_EVENTS;
             index++)
        {
            manager->events[index - 1U] =
                manager->events[index];
        }

        manager->event_count--;
    }

    manager->event_sequence++;

    manager->events[manager->event_count].sequence =
        manager->event_sequence;

    manager->events[manager->event_count].device_id =
        device_id;

    manager->events[manager->event_count].type = type;

    manager->events[manager->event_count].previous_state =
        previous_state;

    manager->events[manager->event_count].current_state =
        current_state;

    manager->event_count++;

    manager->dirty = 1;

    return hal_device_event_manager_update_statistics(manager);
}

syntra_status_t hal_device_event_manager_get_event_count(
    const syntra_hal_device_event_manager_t *manager,
    unsigned int *count
)
{
    if (!manager_is_valid(manager) || count == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    *count = manager->event_count;

    return SYNTRA_STATUS_OK;
}

syntra_status_t hal_device_event_manager_get_event(
    const syntra_hal_device_event_manager_t *manager,
    unsigned int index,
    syntra_hal_device_event_t *event
)
{
    if (!manager_is_valid(manager) ||
        event == NULL ||
        index >= manager->event_count)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    *event = manager->events[index];

    return SYNTRA_STATUS_OK;
}

syntra_status_t hal_device_event_manager_clear_events(
    syntra_hal_device_event_manager_t *manager
)
{
    if (!manager_is_valid(manager))
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    memset(
        manager->events,
        0,
        sizeof(manager->events)
    );

    manager->event_count = 0U;
    manager->dirty = 1;

    return hal_device_event_manager_update_statistics(manager);
}

syntra_status_t hal_device_event_manager_update_statistics(
    syntra_hal_device_event_manager_t *manager
)
{
    unsigned int index;

    if (!manager_is_valid(manager))
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    manager->statistics.total_events = manager->event_count;
    manager->statistics.added_events = 0U;
    manager->statistics.removed_events = 0U;
    manager->statistics.state_changed_events = 0U;
    manager->statistics.reset_events = 0U;

    for (index = 0U; index < manager->event_count; index++)
    {
        switch (manager->events[index].type)
        {
            case SYNTRA_HAL_DEVICE_EVENT_ADDED:
                update_counter(
                    &manager->statistics.added_events
                );
                break;

            case SYNTRA_HAL_DEVICE_EVENT_REMOVED:
                update_counter(
                    &manager->statistics.removed_events
                );
                break;

            case SYNTRA_HAL_DEVICE_EVENT_STATE_CHANGED:
                update_counter(
                    &manager->statistics.state_changed_events
                );
                break;

            case SYNTRA_HAL_DEVICE_EVENT_RESET:
                update_counter(
                    &manager->statistics.reset_events
                );
                break;

            default:
                return SYNTRA_STATUS_ERROR;
        }
    }

    update_counter(&manager->update_count);

    manager->dirty = 0;

    return SYNTRA_STATUS_OK;
}

syntra_status_t hal_device_event_manager_get_statistics(
    const syntra_hal_device_event_manager_t *manager,
    syntra_hal_device_event_statistics_t *statistics
)
{
    if (!manager_is_valid(manager) || statistics == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    *statistics = manager->statistics;

    return SYNTRA_STATUS_OK;
}

syntra_status_t hal_device_event_manager_validate_history(
    syntra_hal_device_event_manager_t *manager
)
{
    unsigned int index;
    unsigned int previous_sequence = 0U;

    if (!manager_is_valid(manager))
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    clear_validation_report(&manager->validation_report);

    for (index = 0U; index < manager->event_count; index++)
    {
        const syntra_hal_device_event_t *event =
            &manager->events[index];

        if (event->sequence == 0U ||
            event->sequence > manager->event_sequence ||
            !event_type_is_valid(event->type) ||
            !device_state_is_valid(event->previous_state) ||
            !device_state_is_valid(event->current_state))
        {
            update_counter(
                &manager->validation_report.invalid_events
            );
        }

        if (index > 0U)
        {
            if (event->sequence == previous_sequence)
            {
                update_counter(
                    &manager->validation_report.duplicate_sequences
                );
            }

            if (event->sequence <= previous_sequence)
            {
                update_counter(
                    &manager->validation_report.out_of_order_sequences
                );
            }
            else if (previous_sequence != UINT_MAX &&
                     event->sequence > previous_sequence + 1U)
            {
                unsigned int gap =
                    event->sequence - previous_sequence - 1U;

                if (UINT_MAX -
                    manager->validation_report.sequence_gaps < gap)
                {
                    manager->validation_report.sequence_gaps =
                        UINT_MAX;
                }
                else
                {
                    manager->validation_report.sequence_gaps += gap;
                }
            }
        }

        previous_sequence = event->sequence;
    }

    return SYNTRA_STATUS_OK;
}

syntra_status_t hal_device_event_manager_get_validation_report(
    const syntra_hal_device_event_manager_t *manager,
    syntra_hal_device_event_validation_report_t *report
)
{
    if (!manager_is_valid(manager) || report == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    *report = manager->validation_report;

    return SYNTRA_STATUS_OK;
}

syntra_status_t hal_device_event_manager_validate(
    const syntra_hal_device_event_manager_t *manager
)
{
    unsigned int index;
    unsigned int added = 0U;
    unsigned int removed = 0U;
    unsigned int state_changed = 0U;
    unsigned int reset = 0U;

    if (!manager_is_valid(manager))
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    for (index = 0U; index < manager->event_count; index++)
    {
        const syntra_hal_device_event_t *event =
            &manager->events[index];

        if (event->sequence == 0U ||
            event->sequence > manager->event_sequence ||
            !event_type_is_valid(event->type) ||
            !device_state_is_valid(event->previous_state) ||
            !device_state_is_valid(event->current_state))
        {
            return SYNTRA_STATUS_ERROR;
        }

        switch (event->type)
        {
            case SYNTRA_HAL_DEVICE_EVENT_ADDED:
                update_counter(&added);
                break;

            case SYNTRA_HAL_DEVICE_EVENT_REMOVED:
                update_counter(&removed);
                break;

            case SYNTRA_HAL_DEVICE_EVENT_STATE_CHANGED:
                update_counter(&state_changed);
                break;

            case SYNTRA_HAL_DEVICE_EVENT_RESET:
                update_counter(&reset);
                break;

            default:
                return SYNTRA_STATUS_ERROR;
        }

        if (index > 0U &&
            event->sequence <= manager->events[index - 1U].sequence)
        {
            return SYNTRA_STATUS_ERROR;
        }
    }

    if (manager->statistics.total_events != manager->event_count ||
        manager->statistics.added_events != added ||
        manager->statistics.removed_events != removed ||
        manager->statistics.state_changed_events != state_changed ||
        manager->statistics.reset_events != reset)
    {
        return SYNTRA_STATUS_ERROR;
    }

    return SYNTRA_STATUS_OK;
}
