
#include "hal_capability_manager.h"

#include <stddef.h>
#include <string.h>

static int manager_is_valid(
    const syntra_hal_capability_manager_t *manager
)
{
    return manager != NULL &&
           manager->initialized == 1 &&
           manager->capability_count <=
               SYNTRA_HAL_MAX_CAPABILITIES;
}

static int find_capability_index(
    const syntra_hal_capability_manager_t *manager,
    unsigned int capability_id
)
{
    unsigned int i;

    for (i = 0U; i < manager->capability_count; ++i)
    {
        if (manager->capabilities[i].capability_id ==
            capability_id)
        {
            return (int)i;
        }
    }

    return -1;
}

syntra_status_t hal_capability_manager_init(
    syntra_hal_capability_manager_t *manager
)
{
    if (manager == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    memset(manager, 0, sizeof(*manager));
    manager->initialized = 1;

    return SYNTRA_STATUS_OK;
}

syntra_status_t hal_capability_manager_reset(
    syntra_hal_capability_manager_t *manager
)
{
    if (manager == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    memset(manager, 0, sizeof(*manager));

    return SYNTRA_STATUS_OK;
}

syntra_status_t hal_capability_manager_register(
    syntra_hal_capability_manager_t *manager,
    unsigned int capability_id,
    unsigned int device_id,
    const char *name
)
{
    syntra_hal_capability_t *capability;
    size_t name_length;

    if (manager == NULL || name == NULL ||
        capability_id == 0U || device_id == 0U)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (!manager_is_valid(manager))
    {
        return SYNTRA_STATUS_ERROR;
    }

    name_length = strlen(name);

    if (name_length == 0U ||
        name_length >= SYNTRA_HAL_CAPABILITY_NAME_MAX)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (find_capability_index(manager, capability_id) >= 0)
    {
        return SYNTRA_STATUS_ERROR;
    }

    if (manager->capability_count >=
        SYNTRA_HAL_MAX_CAPABILITIES)
    {
        return SYNTRA_STATUS_ERROR;
    }

    capability =
        &manager->capabilities[manager->capability_count];

    memset(capability, 0, sizeof(*capability));

    capability->capability_id = capability_id;
    capability->device_id = device_id;

    memcpy(
        capability->name,
        name,
        name_length + 1U
    );

    capability->initialized = 1;

    ++manager->capability_count;
    ++manager->update_count;

    return SYNTRA_STATUS_OK;
}

syntra_status_t hal_capability_manager_unregister(
    syntra_hal_capability_manager_t *manager,
    unsigned int capability_id
)
{
    int index;
    unsigned int i;

    if (manager == NULL || capability_id == 0U)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (!manager_is_valid(manager))
    {
        return SYNTRA_STATUS_ERROR;
    }

    index = find_capability_index(manager, capability_id);

    if (index < 0)
    {
        return SYNTRA_STATUS_ERROR;
    }

    for (i = (unsigned int)index;
         i + 1U < manager->capability_count;
         ++i)
    {
        manager->capabilities[i] =
            manager->capabilities[i + 1U];
    }

    --manager->capability_count;

    memset(
        &manager->capabilities[manager->capability_count],
        0,
        sizeof(syntra_hal_capability_t)
    );

    ++manager->update_count;

    return SYNTRA_STATUS_OK;
}

syntra_status_t hal_capability_manager_has(
    const syntra_hal_capability_manager_t *manager,
    unsigned int device_id,
    unsigned int capability_id,
    int *result
)
{
    int index;

    if (manager == NULL || result == NULL ||
        device_id == 0U || capability_id == 0U)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (!manager_is_valid(manager))
    {
        return SYNTRA_STATUS_ERROR;
    }

    *result = 0;

    index = find_capability_index(manager, capability_id);

    if (index >= 0 &&
        manager->capabilities[index].device_id == device_id)
    {
        *result = 1;
    }

    return SYNTRA_STATUS_OK;
}

syntra_status_t hal_capability_manager_get_count(
    const syntra_hal_capability_manager_t *manager,
    unsigned int *count
)
{
    if (manager == NULL || count == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (!manager_is_valid(manager))
    {
        return SYNTRA_STATUS_ERROR;
    }

    *count = manager->capability_count;

    return SYNTRA_STATUS_OK;
}

syntra_status_t hal_capability_manager_validate(
    const syntra_hal_capability_manager_t *manager
)
{
    unsigned int i;
    unsigned int j;

    if (manager == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (!manager_is_valid(manager))
    {
        return SYNTRA_STATUS_ERROR;
    }

    for (i = 0U; i < manager->capability_count; ++i)
    {
        const syntra_hal_capability_t *capability =
            &manager->capabilities[i];

        if (capability->initialized != 1 ||
            capability->capability_id == 0U ||
            capability->device_id == 0U ||
            capability->name[0] == '\0' ||
            memchr(
                capability->name,
                '\0',
                SYNTRA_HAL_CAPABILITY_NAME_MAX
            ) == NULL)
        {
            return SYNTRA_STATUS_ERROR;
        }

        for (j = i + 1U; j < manager->capability_count; ++j)
        {
            if (capability->capability_id ==
                manager->capabilities[j].capability_id)
            {
                return SYNTRA_STATUS_ERROR;
            }
        }
    }

    return SYNTRA_STATUS_OK;
}
