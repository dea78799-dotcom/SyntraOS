
#include "hal_device_discovery.h"

#include <stddef.h>

static syntra_status_t hal_device_discovery_record_event(
    syntra_hal_device_discovery_t *discovery,
    unsigned int device_id,
    syntra_hal_discovery_event_type_t type
)
{
    unsigned int index;

    if (discovery == NULL || !discovery->initialized)
    {
        return SYNTRA_STATUS_ERROR;
    }

    if (discovery->event_count >=
        SYNTRA_HAL_DEVICE_DISCOVERY_MAX_EVENTS)
    {
        for (index = 1U;
             index < SYNTRA_HAL_DEVICE_DISCOVERY_MAX_EVENTS;
             index++)
        {
            discovery->events[index - 1U] =
                discovery->events[index];
        }

        discovery->event_count--;
    }

    discovery->event_sequence++;

    discovery->events[discovery->event_count].sequence =
        discovery->event_sequence;

    discovery->events[discovery->event_count].device_id =
        device_id;

    discovery->events[discovery->event_count].type = type;

    discovery->event_count++;

    return hal_device_discovery_update_stats(discovery);
}

syntra_status_t hal_device_discovery_init(
    syntra_hal_device_discovery_t *discovery
)
{
    unsigned int index;

    if (discovery == NULL)
    {
        return SYNTRA_STATUS_ERROR;
    }

    for (index = 0U;
         index < SYNTRA_HAL_DEVICE_DISCOVERY_MAX_DEVICES;
         index++)
    {
        discovery->devices[index] = NULL;
    }

    for (index = 0U;
         index < SYNTRA_HAL_DEVICE_DISCOVERY_MAX_EVENTS;
         index++)
    {
        discovery->events[index].sequence = 0U;
        discovery->events[index].device_id = 0U;
        discovery->events[index].type =
            SYNTRA_HAL_DISCOVERY_EVENT_ADDED;
    }

    discovery->device_count = 0U;
    discovery->discovery_count = 0U;
    discovery->event_count = 0U;
    discovery->event_sequence = 0U;

    discovery->stats.total_events = 0U;
    discovery->stats.added_events = 0U;
    discovery->stats.removed_events = 0U;
    discovery->stats.reset_events = 0U;

    discovery->validation_report.invalid_events = 0U;
    discovery->validation_report.duplicate_sequences = 0U;
    discovery->validation_report.out_of_order_events = 0U;
    discovery->validation_report.sequence_gaps = 0U;

    discovery->update_count = 0U;
    discovery->initialized = 1;
    discovery->dirty = 0;

    return SYNTRA_STATUS_OK;
}

syntra_status_t hal_device_discovery_reset(
    syntra_hal_device_discovery_t *discovery
)
{
    if (discovery == NULL || !discovery->initialized)
    {
        return SYNTRA_STATUS_ERROR;
    }

    return hal_device_discovery_init(discovery);
}

syntra_status_t hal_device_discovery_add(
    syntra_hal_device_discovery_t *discovery,
    syntra_hal_device_t *device
)
{
    unsigned int index;

    if (discovery == NULL ||
        device == NULL ||
        !discovery->initialized ||
        !device->initialized)
    {
        return SYNTRA_STATUS_ERROR;
    }

    if (discovery->device_count >=
        SYNTRA_HAL_DEVICE_DISCOVERY_MAX_DEVICES)
    {
        return SYNTRA_STATUS_ERROR;
    }

    for (index = 0U; index < discovery->device_count; index++)
    {
        if (discovery->devices[index] == NULL ||
            discovery->devices[index]->device_id == device->device_id)
        {
            return SYNTRA_STATUS_ERROR;
        }
    }

    discovery->devices[discovery->device_count] = device;
    discovery->device_count++;
    discovery->discovery_count++;
    discovery->dirty = 1;

    return hal_device_discovery_record_event(
        discovery,
        device->device_id,
        SYNTRA_HAL_DISCOVERY_EVENT_ADDED
    );
}

syntra_status_t hal_device_discovery_remove(
    syntra_hal_device_discovery_t *discovery,
    unsigned int device_id
)
{
    unsigned int index;
    unsigned int next;

    if (discovery == NULL || !discovery->initialized)
    {
        return SYNTRA_STATUS_ERROR;
    }

    for (index = 0U; index < discovery->device_count; index++)
    {
        if (discovery->devices[index] == NULL)
        {
            return SYNTRA_STATUS_ERROR;
        }

        if (discovery->devices[index]->device_id == device_id)
        {
            for (next = index;
                 next + 1U < discovery->device_count;
                 next++)
            {
                discovery->devices[next] =
                    discovery->devices[next + 1U];
            }

            discovery->device_count--;
            discovery->devices[discovery->device_count] = NULL;
            discovery->dirty = 1;

            return hal_device_discovery_record_event(
                discovery,
                device_id,
                SYNTRA_HAL_DISCOVERY_EVENT_REMOVED
            );
        }
    }

    return SYNTRA_STATUS_ERROR;
}

syntra_status_t hal_device_discovery_find(
    const syntra_hal_device_discovery_t *discovery,
    unsigned int device_id,
    syntra_hal_device_t **result
)
{
    unsigned int index;

    if (result == NULL)
    {
        return SYNTRA_STATUS_ERROR;
    }

    *result = NULL;

    if (discovery == NULL || !discovery->initialized)
    {
        return SYNTRA_STATUS_ERROR;
    }

    for (index = 0U; index < discovery->device_count; index++)
    {
        if (discovery->devices[index] == NULL)
        {
            return SYNTRA_STATUS_ERROR;
        }

        if (discovery->devices[index]->device_id == device_id)
        {
            *result = discovery->devices[index];
            return SYNTRA_STATUS_OK;
        }
    }

    return SYNTRA_STATUS_ERROR;
}

syntra_status_t hal_device_discovery_get_count(
    const syntra_hal_device_discovery_t *discovery,
    unsigned int *result
)
{
    if (discovery == NULL ||
        result == NULL ||
        !discovery->initialized)
    {
        return SYNTRA_STATUS_ERROR;
    }

    *result = discovery->device_count;
    return SYNTRA_STATUS_OK;
}

syntra_status_t hal_device_discovery_get_event_count(
    const syntra_hal_device_discovery_t *discovery,
    unsigned int *result
)
{
    if (discovery == NULL ||
        result == NULL ||
        !discovery->initialized)
    {
        return SYNTRA_STATUS_ERROR;
    }

    *result = discovery->event_count;
    return SYNTRA_STATUS_OK;
}

syntra_status_t hal_device_discovery_get_event(
    const syntra_hal_device_discovery_t *discovery,
    unsigned int index,
    syntra_hal_discovery_event_t *result
)
{
    if (discovery == NULL ||
        result == NULL ||
        !discovery->initialized ||
        index >= discovery->event_count)
    {
        return SYNTRA_STATUS_ERROR;
    }

    *result = discovery->events[index];
    return SYNTRA_STATUS_OK;
}

syntra_status_t hal_device_discovery_find_event(
    const syntra_hal_device_discovery_t *discovery,
    unsigned int device_id,
    unsigned int occurrence,
    syntra_hal_discovery_event_t *result
)
{
    unsigned int index;
    unsigned int found = 0U;

    if (discovery == NULL ||
        result == NULL ||
        !discovery->initialized)
    {
        return SYNTRA_STATUS_ERROR;
    }

    for (index = 0U; index < discovery->event_count; index++)
    {
        if (discovery->events[index].device_id == device_id)
        {
            if (found == occurrence)
            {
                *result = discovery->events[index];
                return SYNTRA_STATUS_OK;
            }

            found++;
        }
    }

    return SYNTRA_STATUS_ERROR;
}

syntra_status_t hal_device_discovery_get_event_by_type(
    const syntra_hal_device_discovery_t *discovery,
    syntra_hal_discovery_event_type_t type,
    unsigned int occurrence,
    syntra_hal_discovery_event_t *result
)
{
    unsigned int index;
    unsigned int found = 0U;

    if (discovery == NULL ||
        result == NULL ||
        !discovery->initialized ||
        type > SYNTRA_HAL_DISCOVERY_EVENT_RESET)
    {
        return SYNTRA_STATUS_ERROR;
    }

    for (index = 0U; index < discovery->event_count; index++)
    {
        if (discovery->events[index].type == type)
        {
            if (found == occurrence)
            {
                *result = discovery->events[index];
                return SYNTRA_STATUS_OK;
            }

            found++;
        }
    }

    return SYNTRA_STATUS_ERROR;
}

syntra_status_t hal_device_discovery_get_stats(
    const syntra_hal_device_discovery_t *discovery,
    syntra_hal_discovery_stats_t *result
)
{
    if (discovery == NULL ||
        result == NULL ||
        !discovery->initialized)
    {
        return SYNTRA_STATUS_ERROR;
    }

    *result = discovery->stats;
    return SYNTRA_STATUS_OK;
}

syntra_status_t hal_device_discovery_update_stats(
    syntra_hal_device_discovery_t *discovery
)
{
    unsigned int index;

    if (discovery == NULL || !discovery->initialized)
    {
        return SYNTRA_STATUS_ERROR;
    }

    discovery->stats.total_events = discovery->event_count;
    discovery->stats.added_events = 0U;
    discovery->stats.removed_events = 0U;
    discovery->stats.reset_events = 0U;

    for (index = 0U; index < discovery->event_count; index++)
    {
        switch (discovery->events[index].type)
        {
            case SYNTRA_HAL_DISCOVERY_EVENT_ADDED:
                discovery->stats.added_events++;
                break;

            case SYNTRA_HAL_DISCOVERY_EVENT_REMOVED:
                discovery->stats.removed_events++;
                break;

            case SYNTRA_HAL_DISCOVERY_EVENT_RESET:
                discovery->stats.reset_events++;
                break;

            default:
                return SYNTRA_STATUS_ERROR;
        }
    }

    discovery->update_count++;
    discovery->dirty = 0;

    return SYNTRA_STATUS_OK;
}

syntra_status_t hal_device_discovery_get_validation_report(
    const syntra_hal_device_discovery_t *discovery,
    syntra_hal_discovery_validation_report_t *result
)
{
    if (discovery == NULL ||
        result == NULL ||
        !discovery->initialized)
    {
        return SYNTRA_STATUS_ERROR;
    }

    *result = discovery->validation_report;
    return SYNTRA_STATUS_OK;
}

syntra_status_t hal_device_discovery_validate_history(
    syntra_hal_device_discovery_t *discovery
)
{
    unsigned int index;
    unsigned int previous_sequence = 0U;

    if (discovery == NULL || !discovery->initialized)
    {
        return SYNTRA_STATUS_ERROR;
    }

    discovery->validation_report.invalid_events = 0U;
    discovery->validation_report.duplicate_sequences = 0U;
    discovery->validation_report.out_of_order_events = 0U;
    discovery->validation_report.sequence_gaps = 0U;

    for (index = 0U; index < discovery->event_count; index++)
    {
        const syntra_hal_discovery_event_t *event =
            &discovery->events[index];

        if (event->sequence == 0U ||
            event->sequence > discovery->event_sequence ||
            event->type > SYNTRA_HAL_DISCOVERY_EVENT_RESET)
        {
            discovery->validation_report.invalid_events++;
        }

        if (index > 0U)
        {
            if (event->sequence == previous_sequence)
            {
                discovery->validation_report.duplicate_sequences++;
            }

            if (event->sequence <= previous_sequence)
            {
                discovery->validation_report.out_of_order_events++;
            }
            else if (event->sequence > previous_sequence + 1U)
            {
                discovery->validation_report.sequence_gaps +=
                    event->sequence - previous_sequence - 1U;
            }
        }

        previous_sequence = event->sequence;
    }

    return SYNTRA_STATUS_OK;
}

syntra_status_t hal_device_discovery_clear_events(
    syntra_hal_device_discovery_t *discovery
)
{
    unsigned int index;

    if (discovery == NULL || !discovery->initialized)
    {
        return SYNTRA_STATUS_ERROR;
    }

    for (index = 0U;
         index < SYNTRA_HAL_DEVICE_DISCOVERY_MAX_EVENTS;
         index++)
    {
        discovery->events[index].sequence = 0U;
        discovery->events[index].device_id = 0U;
        discovery->events[index].type =
            SYNTRA_HAL_DISCOVERY_EVENT_ADDED;
    }

    discovery->event_count = 0U;
    discovery->dirty = 1;

    if (hal_device_discovery_update_stats(discovery) !=
        SYNTRA_STATUS_OK)
    {
        return SYNTRA_STATUS_ERROR;
    }

    return hal_device_discovery_validate_history(discovery);
}

syntra_status_t hal_device_discovery_validate(
    const syntra_hal_device_discovery_t *discovery
)
{
    unsigned int index;
    unsigned int other;
    unsigned int added = 0U;
    unsigned int removed = 0U;
    unsigned int reset = 0U;

    if (discovery == NULL || !discovery->initialized)
    {
        return SYNTRA_STATUS_ERROR;
    }

    if (discovery->device_count >
        SYNTRA_HAL_DEVICE_DISCOVERY_MAX_DEVICES ||
        discovery->event_count >
        SYNTRA_HAL_DEVICE_DISCOVERY_MAX_EVENTS)
    {
        return SYNTRA_STATUS_ERROR;
    }

    for (index = 0U; index < discovery->device_count; index++)
    {
        if (discovery->devices[index] == NULL ||
            !discovery->devices[index]->initialized)
        {
            return SYNTRA_STATUS_ERROR;
        }

        for (other = index + 1U;
             other < discovery->device_count;
             other++)
        {
            if (discovery->devices[other] == NULL ||
                discovery->devices[index]->device_id ==
                    discovery->devices[other]->device_id)
            {
                return SYNTRA_STATUS_ERROR;
            }
        }
    }

    for (index = discovery->device_count;
         index < SYNTRA_HAL_DEVICE_DISCOVERY_MAX_DEVICES;
         index++)
    {
        if (discovery->devices[index] != NULL)
        {
            return SYNTRA_STATUS_ERROR;
        }
    }

    for (index = 0U; index < discovery->event_count; index++)
    {
        if (discovery->events[index].sequence == 0U ||
            discovery->events[index].sequence >
                discovery->event_sequence)
        {
            return SYNTRA_STATUS_ERROR;
        }

        switch (discovery->events[index].type)
        {
            case SYNTRA_HAL_DISCOVERY_EVENT_ADDED:
                added++;
                break;

            case SYNTRA_HAL_DISCOVERY_EVENT_REMOVED:
                removed++;
                break;

            case SYNTRA_HAL_DISCOVERY_EVENT_RESET:
                reset++;
                break;

            default:
                return SYNTRA_STATUS_ERROR;
        }
    }

    if (discovery->stats.total_events != discovery->event_count ||
        discovery->stats.added_events != added ||
        discovery->stats.removed_events != removed ||
        discovery->stats.reset_events != reset)
    {
        return SYNTRA_STATUS_ERROR;
    }

    return SYNTRA_STATUS_OK;
}
