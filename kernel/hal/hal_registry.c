
#include "hal_registry.h"

#include <string.h>

static int registry_is_valid(
    const syntra_hal_registry_t *registry
)
{
    return registry != NULL &&
           registry->initialized == 1;
}

syntra_status_t hal_registry_init(
    syntra_hal_registry_t *registry
)
{
    if (registry == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    memset(registry, 0, sizeof(*registry));

    registry->initialized = 1;

    return SYNTRA_STATUS_OK;
}

syntra_status_t hal_registry_reset(
    syntra_hal_registry_t *registry
)
{
    return hal_registry_init(registry);
}

syntra_status_t hal_registry_register(
    syntra_hal_registry_t *registry,
    syntra_hal_device_t *device
)
{
    if (registry == NULL || device == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (!registry_is_valid(registry))
    {
        return SYNTRA_STATUS_ERROR;
    }

    if (device->initialized != 1 ||
        device->name[0] == '\0' ||
        device->type <= SYNTRA_HAL_DEVICE_UNKNOWN ||
        device->type > SYNTRA_HAL_DEVICE_INPUT)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (memchr(
            device->name,
            '\0',
            sizeof(device->name)
        ) == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (registry->device_count >=
        SYNTRA_HAL_REGISTRY_MAX_DEVICES)
    {
        return SYNTRA_STATUS_ERROR;
    }

    for (unsigned int i = 0U;
         i < registry->device_count;
         i++)
    {
        if (registry->devices[i] == NULL)
        {
            return SYNTRA_STATUS_ERROR;
        }

        if (registry->devices[i]->device_id ==
            device->device_id)
        {
            return SYNTRA_STATUS_INVALID_ARGUMENT;
        }

        if (strcmp(
                registry->devices[i]->name,
                device->name
            ) == 0)
        {
            return SYNTRA_STATUS_INVALID_ARGUMENT;
        }
    }

    registry->devices[registry->device_count] = device;
    registry->device_count++;

    return SYNTRA_STATUS_OK;
}

syntra_status_t hal_registry_unregister(
    syntra_hal_registry_t *registry,
    unsigned int device_id
)
{
    if (registry == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (!registry_is_valid(registry))
    {
        return SYNTRA_STATUS_ERROR;
    }

    for (unsigned int i = 0U;
         i < registry->device_count;
         i++)
    {
        if (registry->devices[i] == NULL)
        {
            return SYNTRA_STATUS_ERROR;
        }

        if (registry->devices[i]->device_id == device_id)
        {
            for (unsigned int j = i;
                 j + 1U < registry->device_count;
                 j++)
            {
                registry->devices[j] =
                    registry->devices[j + 1U];
            }

            registry->device_count--;

            registry->devices[registry->device_count] = NULL;

            return SYNTRA_STATUS_OK;
        }
    }

    return SYNTRA_STATUS_ERROR;
}

syntra_status_t hal_registry_find_by_id(
    const syntra_hal_registry_t *registry,
    unsigned int device_id,
    syntra_hal_device_t **device
)
{
    if (registry == NULL || device == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (!registry_is_valid(registry))
    {
        return SYNTRA_STATUS_ERROR;
    }

    for (unsigned int i = 0U;
         i < registry->device_count;
         i++)
    {
        if (registry->devices[i] == NULL)
        {
            return SYNTRA_STATUS_ERROR;
        }

        if (registry->devices[i]->device_id == device_id)
        {
            *device = registry->devices[i];

            return SYNTRA_STATUS_OK;
        }
    }

    return SYNTRA_STATUS_ERROR;
}

syntra_status_t hal_registry_find_by_name(
    const syntra_hal_registry_t *registry,
    const char *name,
    syntra_hal_device_t **device
)
{
    if (registry == NULL ||
        name == NULL ||
        name[0] == '\0' ||
        device == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (!registry_is_valid(registry))
    {
        return SYNTRA_STATUS_ERROR;
    }

    for (unsigned int i = 0U;
         i < registry->device_count;
         i++)
    {
        if (registry->devices[i] == NULL)
        {
            return SYNTRA_STATUS_ERROR;
        }

        if (strcmp(
                registry->devices[i]->name,
                name
            ) == 0)
        {
            *device = registry->devices[i];

            return SYNTRA_STATUS_OK;
        }
    }

    return SYNTRA_STATUS_ERROR;
}

syntra_status_t hal_registry_get_count(
    const syntra_hal_registry_t *registry,
    unsigned int *count
)
{
    if (registry == NULL || count == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (!registry_is_valid(registry))
    {
        return SYNTRA_STATUS_ERROR;
    }

    *count = registry->device_count;

    return SYNTRA_STATUS_OK;
}
