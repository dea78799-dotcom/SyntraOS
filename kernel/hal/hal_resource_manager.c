
#include "hal_resource_manager.h"

#include <stddef.h>
#include <string.h>

static int resource_type_is_valid(
    syntra_hal_resource_type_t type
)
{
    return
        type >= SYNTRA_HAL_RESOURCE_GENERIC &&
        type <= SYNTRA_HAL_RESOURCE_DMA;
}

static int resource_state_is_valid(
    syntra_hal_resource_state_t state
)
{
    return
        state == SYNTRA_HAL_RESOURCE_AVAILABLE ||
        state == SYNTRA_HAL_RESOURCE_ALLOCATED;
}

static int manager_is_valid(
    const syntra_hal_resource_manager_t *manager
)
{
    return
        manager != NULL &&
        manager->initialized == 1 &&
        manager->resource_count <= SYNTRA_HAL_MAX_RESOURCES;
}

static int find_resource_index(
    const syntra_hal_resource_manager_t *manager,
    unsigned int resource_id
)
{
    unsigned int i;

    for (i = 0U; i < manager->resource_count; i++)
    {
        if (
            manager->resources[i].initialized == 1 &&
            manager->resources[i].resource_id == resource_id
        )
        {
            return (int)i;
        }
    }

    return -1;
}

syntra_status_t hal_resource_manager_init(
    syntra_hal_resource_manager_t *manager
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

syntra_status_t hal_resource_manager_reset(
    syntra_hal_resource_manager_t *manager
)
{
    if (manager == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    memset(manager, 0, sizeof(*manager));

    return SYNTRA_STATUS_OK;
}

syntra_status_t hal_resource_manager_register(
    syntra_hal_resource_manager_t *manager,
    unsigned int resource_id,
    const char *name,
    syntra_hal_resource_type_t type
)
{
    syntra_hal_resource_t resource;
    size_t name_length;

    if (
        !manager_is_valid(manager) ||
        resource_id == 0U ||
        name == NULL ||
        name[0] == '\0' ||
        !resource_type_is_valid(type)
    )
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    name_length = strlen(name);

    if (name_length >= SYNTRA_HAL_RESOURCE_NAME_MAX)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (find_resource_index(manager, resource_id) >= 0)
    {
        return SYNTRA_STATUS_ERROR;
    }

    if (manager->resource_count >= SYNTRA_HAL_MAX_RESOURCES)
    {
        return SYNTRA_STATUS_ERROR;
    }

    memset(&resource, 0, sizeof(resource));

    resource.resource_id = resource_id;

    memcpy(
        resource.name,
        name,
        name_length + 1U
    );

    resource.type = type;
    resource.state = SYNTRA_HAL_RESOURCE_AVAILABLE;
    resource.owner_device_id = 0U;
    resource.initialized = 1;

    manager->resources[manager->resource_count] = resource;

    manager->resource_count++;
    manager->available_count++;

    return SYNTRA_STATUS_OK;
}

syntra_status_t hal_resource_manager_unregister(
    syntra_hal_resource_manager_t *manager,
    unsigned int resource_id
)
{
    int index;
    unsigned int i;

    if (
        !manager_is_valid(manager) ||
        resource_id == 0U
    )
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    index = find_resource_index(manager, resource_id);

    if (index < 0)
    {
        return SYNTRA_STATUS_ERROR;
    }

    if (
        manager->resources[index].state ==
        SYNTRA_HAL_RESOURCE_ALLOCATED
    )
    {
        return SYNTRA_STATUS_ERROR;
    }

    if (manager->available_count == 0U)
    {
        return SYNTRA_STATUS_ERROR;
    }

    for (
        i = (unsigned int)index;
        i + 1U < manager->resource_count;
        i++
    )
    {
        manager->resources[i] = manager->resources[i + 1U];
    }

    manager->resource_count--;
    manager->available_count--;

    memset(
        &manager->resources[manager->resource_count],
        0,
        sizeof(manager->resources[0])
    );

    return SYNTRA_STATUS_OK;
}

syntra_status_t hal_resource_manager_allocate(
    syntra_hal_resource_manager_t *manager,
    unsigned int resource_id,
    unsigned int device_id
)
{
    int index;

    if (
        !manager_is_valid(manager) ||
        resource_id == 0U ||
        device_id == 0U
    )
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    index = find_resource_index(manager, resource_id);

    if (index < 0)
    {
        return SYNTRA_STATUS_ERROR;
    }

    if (
        manager->resources[index].state !=
        SYNTRA_HAL_RESOURCE_AVAILABLE
    )
    {
        return SYNTRA_STATUS_ERROR;
    }

    if (manager->available_count == 0U)
    {
        return SYNTRA_STATUS_ERROR;
    }

    manager->resources[index].state =
        SYNTRA_HAL_RESOURCE_ALLOCATED;

    manager->resources[index].owner_device_id = device_id;

    manager->allocated_count++;
    manager->available_count--;

    return SYNTRA_STATUS_OK;
}

syntra_status_t hal_resource_manager_release(
    syntra_hal_resource_manager_t *manager,
    unsigned int resource_id,
    unsigned int device_id
)
{
    int index;

    if (
        !manager_is_valid(manager) ||
        resource_id == 0U ||
        device_id == 0U
    )
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    index = find_resource_index(manager, resource_id);

    if (index < 0)
    {
        return SYNTRA_STATUS_ERROR;
    }

    if (
        manager->resources[index].state !=
        SYNTRA_HAL_RESOURCE_ALLOCATED
    )
    {
        return SYNTRA_STATUS_ERROR;
    }

    if (
        manager->resources[index].owner_device_id != device_id
    )
    {
        return SYNTRA_STATUS_ERROR;
    }

    if (manager->allocated_count == 0U)
    {
        return SYNTRA_STATUS_ERROR;
    }

    manager->resources[index].state =
        SYNTRA_HAL_RESOURCE_AVAILABLE;

    manager->resources[index].owner_device_id = 0U;

    manager->allocated_count--;
    manager->available_count++;

    return SYNTRA_STATUS_OK;
}

syntra_status_t hal_resource_manager_find(
    syntra_hal_resource_manager_t *manager,
    unsigned int resource_id,
    syntra_hal_resource_t **resource
)
{
    int index;

    if (
        !manager_is_valid(manager) ||
        resource_id == 0U ||
        resource == NULL
    )
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    *resource = NULL;

    index = find_resource_index(manager, resource_id);

    if (index < 0)
    {
        return SYNTRA_STATUS_ERROR;
    }

    *resource = &manager->resources[index];

    return SYNTRA_STATUS_OK;
}

syntra_status_t hal_resource_manager_get_count(
    const syntra_hal_resource_manager_t *manager,
    unsigned int *total,
    unsigned int *allocated,
    unsigned int *available
)
{
    if (
        !manager_is_valid(manager) ||
        total == NULL ||
        allocated == NULL ||
        available == NULL
    )
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    *total = manager->resource_count;
    *allocated = manager->allocated_count;
    *available = manager->available_count;

    return SYNTRA_STATUS_OK;
}

syntra_status_t hal_resource_manager_validate(
    const syntra_hal_resource_manager_t *manager
)
{
    unsigned int allocated = 0U;
    unsigned int available = 0U;
    unsigned int i;
    unsigned int j;

    if (!manager_is_valid(manager))
    {
        return SYNTRA_STATUS_ERROR;
    }

    for (i = 0U; i < manager->resource_count; i++)
    {
        const syntra_hal_resource_t *resource =
            &manager->resources[i];

        if (
            resource->initialized != 1 ||
            resource->resource_id == 0U ||
            resource->name[0] == '\0' ||
            !resource_type_is_valid(resource->type) ||
            !resource_state_is_valid(resource->state)
        )
        {
            return SYNTRA_STATUS_ERROR;
        }

        if (
            resource->state == SYNTRA_HAL_RESOURCE_ALLOCATED
        )
        {
            if (resource->owner_device_id == 0U)
            {
                return SYNTRA_STATUS_ERROR;
            }

            allocated++;
        }
        else
        {
            if (resource->owner_device_id != 0U)
            {
                return SYNTRA_STATUS_ERROR;
            }

            available++;
        }

        for (j = i + 1U; j < manager->resource_count; j++)
        {
            if (
                resource->resource_id ==
                manager->resources[j].resource_id
            )
            {
                return SYNTRA_STATUS_ERROR;
            }
        }
    }

    if (
        allocated != manager->allocated_count ||
        available != manager->available_count ||
        manager->resource_count !=
            manager->allocated_count + manager->available_count
    )
    {
        return SYNTRA_STATUS_ERROR;
    }

    return SYNTRA_STATUS_OK;
}
