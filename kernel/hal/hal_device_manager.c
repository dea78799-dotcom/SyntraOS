
#include "hal_device_manager.h"

#include <stddef.h>
#include <string.h>

syntra_status_t hal_device_manager_init(
    syntra_hal_device_manager_t *manager,
    syntra_hal_registry_t *registry
)
{
    if (manager == NULL || registry == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (registry->initialized != 1 ||
        registry->device_count > SYNTRA_HAL_REGISTRY_MAX_DEVICES)
    {
        return SYNTRA_STATUS_ERROR;
    }

    memset(manager, 0, sizeof(*manager));

    manager->registry = registry;
    manager->initialized = 1;

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

syntra_status_t hal_device_manager_start(
    syntra_hal_device_manager_t *manager,
    unsigned int device_id
)
{
    syntra_hal_device_t *device = NULL;

    if (manager == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (manager->initialized != 1 ||
        manager->registry == NULL)
    {
        return SYNTRA_STATUS_ERROR;
    }

    if (hal_registry_find_by_id(
            manager->registry,
            device_id,
            &device
        ) != SYNTRA_STATUS_OK)
    {
        return SYNTRA_STATUS_ERROR;
    }

    if (hal_device_start(device) != SYNTRA_STATUS_OK)
    {
        return SYNTRA_STATUS_ERROR;
    }

    return hal_device_manager_refresh(manager);
}

syntra_status_t hal_device_manager_stop(
    syntra_hal_device_manager_t *manager,
    unsigned int device_id
)
{
    syntra_hal_device_t *device = NULL;

    if (manager == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (manager->initialized != 1 ||
        manager->registry == NULL)
    {
        return SYNTRA_STATUS_ERROR;
    }

    if (hal_registry_find_by_id(
            manager->registry,
            device_id,
            &device
        ) != SYNTRA_STATUS_OK)
    {
        return SYNTRA_STATUS_ERROR;
    }

    if (hal_device_stop(device) != SYNTRA_STATUS_OK)
    {
        return SYNTRA_STATUS_ERROR;
    }

    return hal_device_manager_refresh(manager);
}

syntra_status_t hal_device_manager_restart(
    syntra_hal_device_manager_t *manager,
    unsigned int device_id
)
{
    syntra_hal_device_t *device = NULL;

    if (manager == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (manager->initialized != 1 ||
        manager->registry == NULL)
    {
        return SYNTRA_STATUS_ERROR;
    }

    if (hal_registry_find_by_id(
            manager->registry,
            device_id,
            &device
        ) != SYNTRA_STATUS_OK)
    {
        return SYNTRA_STATUS_ERROR;
    }

    if (hal_device_restart(device) != SYNTRA_STATUS_OK)
    {
        return SYNTRA_STATUS_ERROR;
    }

    return hal_device_manager_refresh(manager);
}

syntra_status_t hal_device_manager_get_device(
    const syntra_hal_device_manager_t *manager,
    unsigned int device_id,
    syntra_hal_device_t **device
)
{
    if (manager == NULL || device == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (manager->initialized != 1 ||
        manager->registry == NULL)
    {
        return SYNTRA_STATUS_ERROR;
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
    if (manager == NULL || info == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (manager->initialized != 1 ||
        manager->registry == NULL)
    {
        return SYNTRA_STATUS_ERROR;
    }

    *info = *manager;

    return SYNTRA_STATUS_OK;
}

syntra_status_t hal_device_manager_refresh(
    syntra_hal_device_manager_t *manager
)
{
    if (manager == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (manager->initialized != 1 ||
        manager->registry == NULL)
    {
        return SYNTRA_STATUS_ERROR;
    }

    if (manager->registry->initialized != 1 ||
        manager->registry->device_count >
            SYNTRA_HAL_REGISTRY_MAX_DEVICES)
    {
        return SYNTRA_STATUS_ERROR;
    }

    unsigned int online = 0U;
    unsigned int offline = 0U;

    for (unsigned int i = 0U;
         i < manager->registry->device_count;
         i++)
    {
        syntra_hal_device_t *device =
            manager->registry->devices[i];

        if (device == NULL ||
            device->initialized != 1)
        {
            return SYNTRA_STATUS_ERROR;
        }

        if (device->state == SYNTRA_HAL_DEVICE_ONLINE)
        {
            online++;
        }
        else if (device->state == SYNTRA_HAL_DEVICE_OFFLINE)
        {
            offline++;
        }
        else
        {
            return SYNTRA_STATUS_ERROR;
        }
    }

    manager->total_devices =
        manager->registry->device_count;

    manager->online_devices = online;
    manager->offline_devices = offline;

    return SYNTRA_STATUS_OK;
}
