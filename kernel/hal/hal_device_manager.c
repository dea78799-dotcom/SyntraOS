
#include "hal_device_manager.h"

#include <stddef.h>
#include <string.h>

/*
 * SyntraOS HAL Device Manager
 * Version: 4.0.0
 */

static int manager_is_valid(
    const syntra_hal_device_manager_t *manager
)
{
    if (manager == NULL)
    {
        return 0;
    }

    if (manager->initialized != 1)
    {
        return 0;
    }

    if (manager->registry == NULL)
    {
        return 0;
    }

    if (manager->registry->initialized != 1)
    {
        return 0;
    }

    if (manager->registry->device_count >
        SYNTRA_HAL_MAX_DEVICES)
    {
        return 0;
    }

    if (manager->event_count >
        SYNTRA_HAL_MAX_DEVICE_EVENTS)
    {
        return 0;
    }

    return 1;
}

static int event_type_is_valid(
    syntra_hal_event_type_t type
)
{
    return type >= SYNTRA_HAL_EVENT_STARTED &&
           type < SYNTRA_HAL_EVENT_TYPE_COUNT;
}

static void statistics_add_event(
    syntra_hal_event_statistics_t *statistics,
    syntra_hal_event_type_t type
)
{
    switch (type)
    {
        case SYNTRA_HAL_EVENT_STARTED:
            statistics->started++;
            break;

        case SYNTRA_HAL_EVENT_STOPPED:
            statistics->stopped++;
            break;

        case SYNTRA_HAL_EVENT_RESTARTED:
            statistics->restarted++;
            break;

        case SYNTRA_HAL_EVENT_ERROR:
            statistics->errors++;
            break;

        default:
            break;
    }

    statistics->total++;
}

static void statistics_reset(
    syntra_hal_event_statistics_t *statistics
)
{
    memset(statistics, 0, sizeof(*statistics));
}

syntra_status_t hal_device_manager_record_event(
    syntra_hal_device_manager_t *manager,
    unsigned int device_id,
    syntra_hal_event_type_t type
)
{
    unsigned int index;

    if (!manager_is_valid(manager))
    {
        return SYNTRA_STATUS_ERROR;
    }

    if (!event_type_is_valid(type))
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (device_id == 0U)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (manager->event_count <
        SYNTRA_HAL_MAX_DEVICE_EVENTS)
    {
        index = manager->event_count;
        manager->event_count++;
    }
    else
    {
        memmove(
            &manager->events[0],
            &manager->events[1],
            sizeof(manager->events[0]) *
            (SYNTRA_HAL_MAX_DEVICE_EVENTS - 1U)
        );

        index = SYNTRA_HAL_MAX_DEVICE_EVENTS - 1U;
    }

    manager->next_event_sequence++;

    manager->events[index].device_id = device_id;
    manager->events[index].type = type;
    manager->events[index].sequence =
        manager->next_event_sequence;

    manager->dirty = 1;

    return SYNTRA_STATUS_OK;
}

syntra_status_t hal_device_manager_init(
    syntra_hal_device_manager_t *manager,
    syntra_hal_registry_t *registry
)
{
    if (manager == NULL || registry == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (registry->initialized != 1)
    {
        return SYNTRA_STATUS_ERROR;
    }

    if (registry->device_count > SYNTRA_HAL_MAX_DEVICES)
    {
        return SYNTRA_STATUS_ERROR;
    }

    memset(manager, 0, sizeof(*manager));

    manager->registry = registry;
    manager->initialized = 1;
    manager->dirty = 1;

    return hal_device_manager_refresh(manager);
}

syntra_status_t hal_device_manager_reset(
    syntra_hal_device_manager_t *manager
)
{
    if (manager == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    memset(manager, 0, sizeof(*manager));

    return SYNTRA_STATUS_OK;
}

syntra_status_t hal_device_manager_validate(
    const syntra_hal_device_manager_t *manager
)
{
    unsigned int i;
    syntra_hal_device_t *device;

    if (!manager_is_valid(manager))
    {
        return SYNTRA_STATUS_ERROR;
    }

    for (i = 0U;
         i < manager->registry->device_count;
         i++)
    {
        device = manager->registry->devices[i];

        if (device == NULL)
        {
            return SYNTRA_STATUS_ERROR;
        }

        if (device->state != SYNTRA_HAL_DEVICE_ONLINE &&
            device->state != SYNTRA_HAL_DEVICE_OFFLINE &&
            device->state != SYNTRA_HAL_DEVICE_ERROR)
        {
            return SYNTRA_STATUS_ERROR;
        }
    }

    for (i = 0U; i < manager->event_count; i++)
    {
        if (manager->events[i].device_id == 0U ||
            !event_type_is_valid(manager->events[i].type) ||
            manager->events[i].sequence == 0U)
        {
            return SYNTRA_STATUS_ERROR;
        }
    }

    return SYNTRA_STATUS_OK;
}

syntra_status_t hal_device_manager_refresh(
    syntra_hal_device_manager_t *manager
)
{
    unsigned int i;
    syntra_hal_device_t *device;

    if (!manager_is_valid(manager))
    {
        return SYNTRA_STATUS_ERROR;
    }

    manager->total_devices = 0U;
    manager->online_devices = 0U;
    manager->offline_devices = 0U;
    manager->error_devices = 0U;

    for (i = 0U;
         i < manager->registry->device_count;
         i++)
    {
        device = manager->registry->devices[i];

        if (device == NULL)
        {
            return SYNTRA_STATUS_ERROR;
        }

        manager->total_devices++;

        switch (device->state)
        {
            case SYNTRA_HAL_DEVICE_ONLINE:
                manager->online_devices++;
                break;

            case SYNTRA_HAL_DEVICE_OFFLINE:
                manager->offline_devices++;
                break;

            case SYNTRA_HAL_DEVICE_ERROR:
                manager->error_devices++;
                break;

            default:
                return SYNTRA_STATUS_ERROR;
        }
    }

    manager->update_count++;
    manager->dirty = 0;

    return SYNTRA_STATUS_OK;
}

syntra_status_t hal_device_manager_start(
    syntra_hal_device_manager_t *manager,
    unsigned int device_id
)
{
    syntra_hal_device_t *device = NULL;
    syntra_status_t status;

    if (!manager_is_valid(manager))
    {
        return SYNTRA_STATUS_ERROR;
    }

    status = hal_device_manager_get_device(
        manager,
        device_id,
        &device
    );

    if (status != SYNTRA_STATUS_OK)
    {
        return status;
    }

    status = hal_device_start(device);

    if (status == SYNTRA_STATUS_OK)
    {
        (void)hal_device_manager_record_event(
            manager,
            device_id,
            SYNTRA_HAL_EVENT_STARTED
        );
    }
    else
    {
        (void)hal_device_manager_record_event(
            manager,
            device_id,
            SYNTRA_HAL_EVENT_ERROR
        );
    }

    (void)hal_device_manager_refresh(manager);

    return status;
}

syntra_status_t hal_device_manager_stop(
    syntra_hal_device_manager_t *manager,
    unsigned int device_id
)
{
    syntra_hal_device_t *device = NULL;
    syntra_status_t status;

    if (!manager_is_valid(manager))
    {
        return SYNTRA_STATUS_ERROR;
    }

    status = hal_device_manager_get_device(
        manager,
        device_id,
        &device
    );

    if (status != SYNTRA_STATUS_OK)
    {
        return status;
    }

    status = hal_device_stop(device);

    if (status == SYNTRA_STATUS_OK)
    {
        (void)hal_device_manager_record_event(
            manager,
            device_id,
            SYNTRA_HAL_EVENT_STOPPED
        );
    }
    else
    {
        (void)hal_device_manager_record_event(
            manager,
            device_id,
            SYNTRA_HAL_EVENT_ERROR
        );
    }

    (void)hal_device_manager_refresh(manager);

    return status;
}

syntra_status_t hal_device_manager_restart(
    syntra_hal_device_manager_t *manager,
    unsigned int device_id
)
{
    syntra_hal_device_t *device = NULL;
    syntra_status_t status;

    if (!manager_is_valid(manager))
    {
        return SYNTRA_STATUS_ERROR;
    }

    status = hal_device_manager_get_device(
        manager,
        device_id,
        &device
    );

    if (status != SYNTRA_STATUS_OK)
    {
        return status;
    }

    status = hal_device_restart(device);

    if (status == SYNTRA_STATUS_OK)
    {
        (void)hal_device_manager_record_event(
            manager,
            device_id,
            SYNTRA_HAL_EVENT_RESTARTED
        );
    }
    else
    {
        (void)hal_device_manager_record_event(
            manager,
            device_id,
            SYNTRA_HAL_EVENT_ERROR
        );
    }

    (void)hal_device_manager_refresh(manager);

    return status;
}

syntra_status_t hal_device_manager_get_device(
    const syntra_hal_device_manager_t *manager,
    unsigned int device_id,
    syntra_hal_device_t **device
)
{
    if (!manager_is_valid(manager) || device == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    return hal_registry_find_by_id(
        manager->registry,
        device_id,
        device
    );
}

syntra_status_t hal_device_manager_get_info(
    const syntra_hal_device_manager_t *manager,
    syntra_hal_device_manager_t *info
)
{
    if (!manager_is_valid(manager) || info == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    *info = *manager;

    return SYNTRA_STATUS_OK;
}

syntra_status_t hal_device_manager_get_event(
    const syntra_hal_device_manager_t *manager,
    unsigned int index,
    syntra_hal_device_event_t *event
)
{
    if (!manager_is_valid(manager) || event == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (index >= manager->event_count)
    {
        return SYNTRA_STATUS_ERROR;
    }

    *event = manager->events[index];

    return SYNTRA_STATUS_OK;
}

syntra_status_t hal_device_manager_find_events(
    const syntra_hal_device_manager_t *manager,
    unsigned int device_id,
    syntra_hal_device_event_t *events,
    unsigned int capacity,
    unsigned int *found
)
{
    unsigned int i;
    unsigned int count = 0U;

    if (!manager_is_valid(manager) ||
        found == NULL ||
        (capacity > 0U && events == NULL))
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    *found = 0U;

    for (i = 0U; i < manager->event_count; i++)
    {
        if (manager->events[i].device_id == device_id)
        {
            if (count < capacity)
            {
                events[count] = manager->events[i];
            }

            count++;
        }
    }

    *found = count;

    if (count > capacity)
    {
        return SYNTRA_STATUS_ERROR;
    }

    return SYNTRA_STATUS_OK;
}

syntra_status_t hal_device_manager_clear_events(
    syntra_hal_device_manager_t *manager
)
{
    if (!manager_is_valid(manager))
    {
        return SYNTRA_STATUS_ERROR;
    }

    memset(
        manager->events,
        0,
        sizeof(manager->events)
    );

    manager->event_count = 0U;
    manager->next_event_sequence = 0U;
    manager->dirty = 1;

    return SYNTRA_STATUS_OK;
}

syntra_status_t hal_device_manager_get_event_statistics(
    const syntra_hal_device_manager_t *manager,
    syntra_hal_event_statistics_t *statistics
)
{
    unsigned int i;

    if (!manager_is_valid(manager) || statistics == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    statistics_reset(statistics);

    for (i = 0U; i < manager->event_count; i++)
    {
        statistics_add_event(
            statistics,
            manager->events[i].type
        );
    }

    return SYNTRA_STATUS_OK;
}

syntra_status_t hal_device_manager_find_events_by_type(
    const syntra_hal_device_manager_t *manager,
    syntra_hal_event_type_t type,
    syntra_hal_device_event_t *events,
    unsigned int capacity,
    unsigned int *found
)
{
    unsigned int i;
    unsigned int count = 0U;

    if (!manager_is_valid(manager) ||
        found == NULL ||
        (capacity > 0U && events == NULL) ||
        !event_type_is_valid(type))
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    *found = 0U;

    for (i = 0U; i < manager->event_count; i++)
    {
        if (manager->events[i].type == type)
        {
            if (count < capacity)
            {
                events[count] = manager->events[i];
            }

            count++;
        }
    }

    *found = count;

    if (count > capacity)
    {
        return SYNTRA_STATUS_ERROR;
    }

    return SYNTRA_STATUS_OK;
}

syntra_status_t hal_device_manager_find_events_by_sequence(
    const syntra_hal_device_manager_t *manager,
    unsigned int first_sequence,
    unsigned int last_sequence,
    syntra_hal_device_event_t *events,
    unsigned int capacity,
    unsigned int *found
)
{
    unsigned int i;
    unsigned int count = 0U;

    if (!manager_is_valid(manager) ||
        found == NULL ||
        (capacity > 0U && events == NULL) ||
        first_sequence == 0U ||
        last_sequence < first_sequence)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    *found = 0U;

    for (i = 0U; i < manager->event_count; i++)
    {
        unsigned int sequence = manager->events[i].sequence;

        if (sequence >= first_sequence &&
            sequence <= last_sequence)
        {
            if (count < capacity)
            {
                events[count] = manager->events[i];
            }

            count++;
        }
    }

    *found = count;

    if (count > capacity)
    {
        return SYNTRA_STATUS_ERROR;
    }

    return SYNTRA_STATUS_OK;
}

syntra_status_t hal_device_manager_get_device_event_statistics(
    const syntra_hal_device_manager_t *manager,
    unsigned int device_id,
    syntra_hal_event_statistics_t *statistics
)
{
    unsigned int i;

    if (!manager_is_valid(manager) ||
        statistics == NULL ||
        device_id == 0U)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    statistics_reset(statistics);

    for (i = 0U; i < manager->event_count; i++)
    {
        if (manager->events[i].device_id == device_id)
        {
            statistics_add_event(
                statistics,
                manager->events[i].type
            );
        }
    }

    return SYNTRA_STATUS_OK;
}
