
#include "hal_backend_manager.h"

#include <limits.h>
#include <stddef.h>
#include <string.h>

static syntra_status_t hal_backend_manager_record_event(
    syntra_hal_backend_manager_t *manager,
    unsigned int backend_id,
    syntra_hal_backend_event_type_t type
)
{
    unsigned int i;

    if (manager == NULL || backend_id == 0U)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (manager->event_sequence == UINT_MAX)
    {
        return SYNTRA_STATUS_ERROR;
    }

    if (manager->event_count >=
        SYNTRA_HAL_BACKEND_MANAGER_MAX_EVENTS)
    {
        for (i = 1U;
             i < SYNTRA_HAL_BACKEND_MANAGER_MAX_EVENTS;
             ++i)
        {
            manager->events[i - 1U] = manager->events[i];
        }

        --manager->event_count;
    }

    ++manager->event_sequence;

    manager->events[manager->event_count].sequence =
        manager->event_sequence;

    manager->events[manager->event_count].backend_id =
        backend_id;

    manager->events[manager->event_count].type = type;

    ++manager->event_count;

    return SYNTRA_STATUS_OK;
}

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
    unsigned int previous_sequence = 0U;

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
            SYNTRA_HAL_BACKEND_MANAGER_MAX_BACKENDS ||
        manager->event_count >
            SYNTRA_HAL_BACKEND_MANAGER_MAX_EVENTS ||
        manager->event_sequence < manager->event_count)
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

    if (manager->dirty == 0 &&
        (manager->available_backends != available ||
         manager->unavailable_backends != unavailable))
    {
        return SYNTRA_STATUS_ERROR;
    }

    for (i = 0U; i < manager->event_count; ++i)
    {
        const syntra_hal_backend_event_t *event =
            &manager->events[i];

        if (event->sequence == 0U ||
            event->sequence <= previous_sequence ||
            event->sequence > manager->event_sequence ||
            event->backend_id == 0U ||
            event->type < SYNTRA_HAL_BACKEND_EVENT_REGISTERED ||
            event->type > SYNTRA_HAL_BACKEND_EVENT_UNAVAILABLE)
        {
            return SYNTRA_STATUS_ERROR;
        }

        previous_sequence = event->sequence;
    }

    if (manager->event_count > 0U &&
        previous_sequence != manager->event_sequence)
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

    if (manager->event_sequence == UINT_MAX)
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

    return hal_backend_manager_record_event(
        manager,
        backend->backend_id,
        SYNTRA_HAL_BACKEND_EVENT_REGISTERED
    );
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

    if (manager->event_sequence == UINT_MAX)
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

            return hal_backend_manager_record_event(
                manager,
                backend_id,
                SYNTRA_HAL_BACKEND_EVENT_UNREGISTERED
            );
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

        ++count;
    }

    if (count != manager->backend_count ||
        UINT_MAX - manager->event_sequence < count)
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

        if (backend->available == 1)
        {
            ++available;

            if (hal_backend_manager_record_event(
                    manager,
                    backend->backend_id,
                    SYNTRA_HAL_BACKEND_EVENT_AVAILABLE
                ) != SYNTRA_STATUS_OK)
            {
                return SYNTRA_STATUS_ERROR;
            }
        }
        else
        {
            ++unavailable;

            if (hal_backend_manager_record_event(
                    manager,
                    backend->backend_id,
                    SYNTRA_HAL_BACKEND_EVENT_UNAVAILABLE
                ) != SYNTRA_STATUS_OK)
            {
                return SYNTRA_STATUS_ERROR;
            }
        }
    }

    manager->available_backends = available;
    manager->unavailable_backends = unavailable;
    manager->dirty = 0;

    return SYNTRA_STATUS_OK;
}

syntra_status_t hal_backend_manager_get_event_count(
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

    *result = manager->event_count;

    return SYNTRA_STATUS_OK;
}

syntra_status_t hal_backend_manager_get_event(
    const syntra_hal_backend_manager_t *manager,
    unsigned int index,
    syntra_hal_backend_event_t *result
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

    if (index >= manager->event_count)
    {
        return SYNTRA_STATUS_ERROR;
    }

    *result = manager->events[index];

    return SYNTRA_STATUS_OK;
}

syntra_status_t hal_backend_manager_clear_events(
    syntra_hal_backend_manager_t *manager
)
{
    if (manager == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (hal_backend_manager_validate(manager) != SYNTRA_STATUS_OK)
    {
        return SYNTRA_STATUS_ERROR;
    }

    memset(
        manager->events,
        0,
        sizeof(manager->events)
    );

    manager->event_count = 0U;

    return SYNTRA_STATUS_OK;
}
