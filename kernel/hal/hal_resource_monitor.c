
#include "hal_resource_monitor.h"

#include <stddef.h>

static int monitor_is_valid(
    const syntra_hal_resource_monitor_t *monitor
)
{
    return monitor != NULL &&
           monitor->initialized == 1;
}

syntra_status_t hal_resource_monitor_init(
    syntra_hal_resource_monitor_t *monitor
)
{
    if (monitor == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    monitor->total_resources = 0U;
    monitor->allocated_resources = 0U;
    monitor->available_resources = 0U;
    monitor->invalid_resources = 0U;
    monitor->update_count = 0U;
    monitor->initialized = 1;

    return SYNTRA_STATUS_SUCCESS;
}

syntra_status_t hal_resource_monitor_reset(
    syntra_hal_resource_monitor_t *monitor
)
{
    if (monitor == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    monitor->total_resources = 0U;
    monitor->allocated_resources = 0U;
    monitor->available_resources = 0U;
    monitor->invalid_resources = 0U;
    monitor->update_count = 0U;
    monitor->initialized = 0;

    return SYNTRA_STATUS_SUCCESS;
}

syntra_status_t hal_resource_monitor_update(
    syntra_hal_resource_monitor_t *monitor,
    const syntra_hal_resource_manager_t *manager
)
{
    unsigned int allocated = 0U;
    unsigned int available = 0U;
    unsigned int invalid = 0U;
    unsigned int i;

    if (monitor == NULL || manager == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (!monitor_is_valid(monitor))
    {
        return SYNTRA_STATUS_ERROR;
    }

    if (manager->initialized != 1 ||
        manager->resource_count > SYNTRA_HAL_MAX_RESOURCES)
    {
        return SYNTRA_STATUS_ERROR;
    }

    for (i = 0U; i < manager->resource_count; ++i)
    {
        const syntra_hal_resource_t *resource =
            &manager->resources[i];

        if (resource->initialized != 1 ||
            resource->resource_id == 0U)
        {
            ++invalid;
            continue;
        }

        if (resource->state == SYNTRA_HAL_RESOURCE_ALLOCATED)
        {
            if (resource->owner_device_id == 0U)
            {
                ++invalid;
            }
            else
            {
                ++allocated;
            }
        }
        else if (resource->state == SYNTRA_HAL_RESOURCE_AVAILABLE)
        {
            if (resource->owner_device_id != 0U)
            {
                ++invalid;
            }
            else
            {
                ++available;
            }
        }
        else
        {
            ++invalid;
        }
    }

    monitor->total_resources = manager->resource_count;
    monitor->allocated_resources = allocated;
    monitor->available_resources = available;
    monitor->invalid_resources = invalid;
    ++monitor->update_count;

    return SYNTRA_STATUS_SUCCESS;
}

syntra_status_t hal_resource_monitor_validate(
    const syntra_hal_resource_monitor_t *monitor
)
{
    if (monitor == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (!monitor_is_valid(monitor))
    {
        return SYNTRA_STATUS_ERROR;
    }

    if (monitor->allocated_resources >
        monitor->total_resources)
    {
        return SYNTRA_STATUS_ERROR;
    }

    if (monitor->available_resources >
        monitor->total_resources)
    {
        return SYNTRA_STATUS_ERROR;
    }

    if (monitor->allocated_resources +
        monitor->available_resources +
        monitor->invalid_resources !=
        monitor->total_resources)
    {
        return SYNTRA_STATUS_ERROR;
    }

    return SYNTRA_STATUS_SUCCESS;
}
