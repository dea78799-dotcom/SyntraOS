
#include "hal_backend_manager.h"

#include <stddef.h>
#include <string.h>

syntra_status_t hal_backend_manager_init(
    syntra_hal_backend_manager_t *manager
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

syntra_status_t hal_backend_manager_reset(
    syntra_hal_backend_manager_t *manager
)
{
    if (manager == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    memset(manager, 0, sizeof(*manager));

    return SYNTRA_STATUS_OK;
}

syntra_status_t hal_backend_manager_validate(
    const syntra_hal_backend_manager_t *manager
)
{
    unsigned int i;
    unsigned int j;
    unsigned int count = 0U;
    unsigned int available = 0U;
    unsigned int unavailable = 0U;

    if (manager == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (manager->initialized != 1 ||
        manager->dirty < 0 ||
        manager->dirty > 1)
    {
        return SYNTRA_STATUS_ERROR;
    }

    if (manager->backend_count >
        SYNTRA_HAL_BACKEND_MANAGER_MAX_BACKENDS)
    {
        return SYNTRA_STATUS_ERROR;
    }

    for (i = 0U;
         i < SYNTRA_HAL_BACKEND_MANAGER_MAX_BACKENDS;
         ++i)
    {
        syntra_hal_backend_t *backend = manager->backends[i];

        if (backend == NULL)
        {
            continue;
        }

        if (hal_backend_validate(backend) != SYNTRA_STATUS_OK)
        {
            return SYNTRA_STATUS_ERROR;
        }

        for (j = i + 1U;
             j < SYNTRA_HAL_BACKEND_MANAGER_MAX_BACKENDS;
             ++j)
        {
            syntra_hal_backend_t *other = manager->backends[j];

            if (other != NULL &&
                other->backend_id == backend->backend_id)
            {
                return SYNTRA_STATUS_ERROR;
            }
        }

        if (backend->available == 1)
        {
            ++available;
        }
        else
        {
            ++unavailable;
        }

        ++count;
    }

    if (count != manager->backend_count)
    {
        return SYNTRA_STATUS_ERROR;
    }

    /*
     * Statistics may be stale while dirty is set.
     * They must match the backend list after an update.
     */
    if (manager->dirty == 0 &&
        (manager->available_backends != available ||
         manager->unavailable_backends != unavailable))
    {
        return SYNTRA_STATUS_ERROR;
    }

    return SYNTRA_STATUS_OK;
}

syntra_status_t hal_backend_manager_register(
    syntra_hal_backend_manager_t *manager,
    syntra_hal_backend_t *backend
)
{
    unsigned int i;
    unsigned int empty_index =
        SYNTRA_HAL_BACKEND_MANAGER_MAX_BACKENDS;

    if (manager == NULL || backend == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (hal_backend_manager_validate(manager) != SYNTRA_STATUS_OK ||
        hal_backend_validate(backend) != SYNTRA_STATUS_OK)
    {
        return SYNTRA_STATUS_ERROR;
    }

    for (i = 0U;
         i < SYNTRA_HAL_BACKEND_MANAGER_MAX_BACKENDS;
         ++i)
    {
        if (manager->backends[i] == NULL)
        {
            if (empty_index ==
                SYNTRA_HAL_BACKEND_MANAGER_MAX_BACKENDS)
            {
                empty_index = i;
            }

            continue;
        }

        if (manager->backends[i]->backend_id ==
            backend->backend_id)
        {
            return SYNTRA_STATUS_ERROR;
        }
    }

    if (empty_index ==
        SYNTRA_HAL_BACKEND_MANAGER_MAX_BACKENDS)
    {
        return SYNTRA_STATUS_ERROR;
    }

    manager->backends[empty_index] = backend;
    ++manager->backend_count;
    ++manager->update_count;
    manager->dirty = 1;

    return SYNTRA_STATUS_OK;
}

syntra_status_t hal_backend_manager_unregister(
    syntra_hal_backend_manager_t *manager,
    unsigned int backend_id
)
{
    unsigned int i;

    if (manager == NULL || backend_id == 0U)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (hal_backend_manager_validate(manager) != SYNTRA_STATUS_OK)
    {
        return SYNTRA_STATUS_ERROR;
    }

    for (i = 0U;
         i < SYNTRA_HAL_BACKEND_MANAGER_MAX_BACKENDS;
         ++i)
    {
        if (manager->backends[i] != NULL &&
            manager->backends[i]->backend_id == backend_id)
        {
            manager->backends[i] = NULL;
            --manager->backend_count;
            ++manager->update_count;
            manager->dirty = 1;

            return SYNTRA_STATUS_OK;
        }
    }

    return SYNTRA_STATUS_ERROR;
}

syntra_status_t hal_backend_manager_find(
    const syntra_hal_backend_manager_t *manager,
    unsigned int backend_id,
    syntra_hal_backend_t **result
)
{
    unsigned int i;

    if (manager == NULL || result == NULL || backend_id == 0U)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    *result = NULL;

    if (hal_backend_manager_validate(manager) != SYNTRA_STATUS_OK)
    {
        return SYNTRA_STATUS_ERROR;
    }

    for (i = 0U;
         i < SYNTRA_HAL_BACKEND_MANAGER_MAX_BACKENDS;
         ++i)
    {
        if (manager->backends[i] != NULL &&
            manager->backends[i]->backend_id == backend_id)
        {
            *result = manager->backends[i];

            return SYNTRA_STATUS_OK;
        }
    }

    return SYNTRA_STATUS_ERROR;
}

syntra_status_t hal_backend_manager_get_count(
    const syntra_hal_backend_manager_t *manager,
    unsigned int *result
)
{
    if (manager == NULL || result == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (hal_backend_manager_validate(manager) != SYNTRA_STATUS_OK)
    {
        return SYNTRA_STATUS_ERROR;
    }

    *result = manager->backend_count;

    return SYNTRA_STATUS_OK;
}

syntra_status_t hal_backend_manager_update(
    syntra_hal_backend_manager_t *manager
)
{
    unsigned int i;
    unsigned int available = 0U;
    unsigned int unavailable = 0U;
    unsigned int count = 0U;

    if (manager == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (manager->initialized != 1 ||
        manager->backend_count >
        SYNTRA_HAL_BACKEND_MANAGER_MAX_BACKENDS)
    {
        return SYNTRA_STATUS_ERROR;
    }

    for (i = 0U;
         i < SYNTRA_HAL_BACKEND_MANAGER_MAX_BACKENDS;
         ++i)
    {
        syntra_hal_backend_t *backend = manager->backends[i];

        if (backend == NULL)
        {
            continue;
        }

        if (hal_backend_validate(backend) != SYNTRA_STATUS_OK)
        {
            return SYNTRA_STATUS_ERROR;
        }

        if (backend->available == 1)
        {
            ++available;
        }
        else
        {
            ++unavailable;
        }

        ++count;
    }

    if (count != manager->backend_count)
    {
        return SYNTRA_STATUS_ERROR;
    }

    manager->available_backends = available;
    manager->unavailable_backends = unavailable;
    manager->dirty = 0;

    return SYNTRA_STATUS_OK;
}
