
#include "hal_device_discovery.h"

#include <stddef.h>

syntra_status_t hal_device_discovery_init(
    syntra_hal_device_discovery_t *discovery
)
{
    if (discovery == NULL)
    {
        return SYNTRA_STATUS_ERROR;
    }

    for (unsigned int i = 0U;
         i < SYNTRA_HAL_DEVICE_DISCOVERY_MAX_DEVICES;
         ++i)
    {
        discovery->devices[i] = NULL;
    }

    discovery->device_count = 0U;
    discovery->discovery_count = 0U;
    discovery->initialized = 1;
    discovery->dirty = 0;

    return SYNTRA_STATUS_OK;
}

syntra_status_t hal_device_discovery_reset(
    syntra_hal_device_discovery_t *discovery
)
{
    if (discovery == NULL)
    {
        return SYNTRA_STATUS_ERROR;
    }

    for (unsigned int i = 0U;
         i < SYNTRA_HAL_DEVICE_DISCOVERY_MAX_DEVICES;
         ++i)
    {
        discovery->devices[i] = NULL;
    }

    discovery->device_count = 0U;
    discovery->discovery_count = 0U;
    discovery->initialized = 1;
    discovery->dirty = 0;

    return SYNTRA_STATUS_OK;
}

syntra_status_t hal_device_discovery_add(
    syntra_hal_device_discovery_t *discovery,
    syntra_hal_device_t *device
)
{
    if (discovery == NULL || device == NULL)
    {
        return SYNTRA_STATUS_ERROR;
    }

    if (!discovery->initialized || !device->initialized)
    {
        return SYNTRA_STATUS_ERROR;
    }

    if (discovery->device_count >=
        SYNTRA_HAL_DEVICE_DISCOVERY_MAX_DEVICES)
    {
        return SYNTRA_STATUS_ERROR;
    }

    for (unsigned int i = 0U;
         i < discovery->device_count;
         ++i)
    {
        if (discovery->devices[i] == NULL)
        {
            return SYNTRA_STATUS_ERROR;
        }

        if (discovery->devices[i]->device_id == device->device_id)
        {
            return SYNTRA_STATUS_ERROR;
        }
    }

    discovery->devices[discovery->device_count] = device;
    discovery->device_count++;
    discovery->discovery_count++;
    discovery->dirty = 1;

    return SYNTRA_STATUS_OK;
}

syntra_status_t hal_device_discovery_remove(
    syntra_hal_device_discovery_t *discovery,
    unsigned int device_id
)
{
    if (discovery == NULL || !discovery->initialized)
    {
        return SYNTRA_STATUS_ERROR;
    }

    for (unsigned int i = 0U;
         i < discovery->device_count;
         ++i)
    {
        if (discovery->devices[i] == NULL)
        {
            return SYNTRA_STATUS_ERROR;
        }

        if (discovery->devices[i]->device_id == device_id)
        {
            for (unsigned int j = i;
                 j + 1U < discovery->device_count;
                 ++j)
            {
                discovery->devices[j] = discovery->devices[j + 1U];
            }

            discovery->device_count--;
            discovery->devices[discovery->device_count] = NULL;
            discovery->dirty = 1;

            return SYNTRA_STATUS_OK;
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
    if (discovery == NULL || result == NULL)
    {
        return SYNTRA_STATUS_ERROR;
    }

    if (!discovery->initialized)
    {
        return SYNTRA_STATUS_ERROR;
    }

    *result = NULL;

    for (unsigned int i = 0U;
         i < discovery->device_count;
         ++i)
    {
        if (discovery->devices[i] == NULL)
        {
            return SYNTRA_STATUS_ERROR;
        }

        if (discovery->devices[i]->device_id == device_id)
        {
            *result = discovery->devices[i];
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
    if (discovery == NULL || result == NULL)
    {
        return SYNTRA_STATUS_ERROR;
    }

    if (!discovery->initialized)
    {
        return SYNTRA_STATUS_ERROR;
    }

    *result = discovery->device_count;

    return SYNTRA_STATUS_OK;
}

syntra_status_t hal_device_discovery_validate(
    const syntra_hal_device_discovery_t *discovery
)
{
    if (discovery == NULL || !discovery->initialized)
    {
        return SYNTRA_STATUS_ERROR;
    }

    if (discovery->device_count >
        SYNTRA_HAL_DEVICE_DISCOVERY_MAX_DEVICES)
    {
        return SYNTRA_STATUS_ERROR;
    }

    for (unsigned int i = 0U;
         i < discovery->device_count;
         ++i)
    {
        if (discovery->devices[i] == NULL ||
            !discovery->devices[i]->initialized)
        {
            return SYNTRA_STATUS_ERROR;
        }

        for (unsigned int j = i + 1U;
             j < discovery->device_count;
             ++j)
        {
            if (discovery->devices[j] == NULL)
            {
                return SYNTRA_STATUS_ERROR;
            }

            if (discovery->devices[i]->device_id ==
                discovery->devices[j]->device_id)
            {
                return SYNTRA_STATUS_ERROR;
            }
        }
    }

    for (unsigned int i = discovery->device_count;
         i < SYNTRA_HAL_DEVICE_DISCOVERY_MAX_DEVICES;
         ++i)
    {
        if (discovery->devices[i] != NULL)
        {
            return SYNTRA_STATUS_ERROR;
        }
    }

    return SYNTRA_STATUS_OK;
}
