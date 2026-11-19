
#include "hal_device_event_router.h"

#include <limits.h>
#include <string.h>

static int event_type_is_valid(
    syntra_hal_device_event_type_t type
)
{
    return type >= SYNTRA_HAL_DEVICE_EVENT_ADDED &&
           type < SYNTRA_HAL_DEVICE_EVENT_TYPE_COUNT;
}

static int device_state_is_valid(
    syntra_hal_device_state_t state
)
{
    return state >= SYNTRA_HAL_DEVICE_OFFLINE &&
           state <= SYNTRA_HAL_DEVICE_ERROR;
}

static void increment_counter(unsigned int *counter)
{
    if (*counter < UINT_MAX)
    {
        (*counter)++;
    }
}

static int router_is_valid(
    const syntra_hal_device_event_router_t *router
)
{
    unsigned int i;
    unsigned int registered_count = 0U;

    if (router == NULL || router->initialized != 1)
    {
        return 0;
    }

    for (i = 0U;
         i < SYNTRA_HAL_DEVICE_EVENT_ROUTER_MAX_ROUTES;
         i++)
    {
        const syntra_hal_device_event_route_entry_t *entry =
            &router->routes[i];

        if (entry->registered != 0 &&
            entry->registered != 1)
        {
            return 0;
        }

        if (entry->registered == 1)
        {
            if (entry->target_queue == NULL)
            {
                return 0;
            }

            if (hal_device_event_queue_validate(
                    entry->target_queue
                ) != SYNTRA_STATUS_OK)
            {
                return 0;
            }

            if (entry->failure_count > entry->routed_count)
            {
                return 0;
            }

            registered_count++;
        }
        else if (entry->target_queue != NULL)
        {
            return 0;
        }
    }

    return registered_count == router->total_routes;
}

syntra_status_t hal_device_event_router_init(
    syntra_hal_device_event_router_t *router
)
{
    if (router == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    memset(router, 0, sizeof(*router));

    router->initialized = 1;

    return SYNTRA_STATUS_OK;
}

syntra_status_t hal_device_event_router_reset(
    syntra_hal_device_event_router_t *router
)
{
    if (router == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (!router_is_valid(router))
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    return hal_device_event_router_init(router);
}

syntra_status_t hal_device_event_router_register(
    syntra_hal_device_event_router_t *router,
    syntra_hal_device_event_type_t type,
    syntra_hal_device_event_queue_t *target_queue
)
{
    syntra_hal_device_event_route_entry_t *entry;

    if (router == NULL ||
        target_queue == NULL ||
        !event_type_is_valid(type))
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (!router_is_valid(router))
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (hal_device_event_queue_validate(
            target_queue
        ) != SYNTRA_STATUS_OK)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    entry = &router->routes[(unsigned int)type];

    if (entry->registered == 1)
    {
        return SYNTRA_STATUS_ERROR;
    }

    entry->target_queue = target_queue;
    entry->registered = 1;

    increment_counter(&router->total_routes);
    increment_counter(&router->update_count);

    router->dirty = 1;

    return SYNTRA_STATUS_OK;
}

syntra_status_t hal_device_event_router_unregister(
    syntra_hal_device_event_router_t *router,
    syntra_hal_device_event_type_t type
)
{
    syntra_hal_device_event_route_entry_t *entry;

    if (router == NULL || !event_type_is_valid(type))
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (!router_is_valid(router))
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    entry = &router->routes[(unsigned int)type];

    if (entry->registered != 1)
    {
        return SYNTRA_STATUS_ERROR;
    }

    entry->target_queue = NULL;
    entry->registered = 0;

    if (router->total_routes > 0U)
    {
        router->total_routes--;
    }

    increment_counter(&router->update_count);

    router->dirty = 1;

    return SYNTRA_STATUS_OK;
}

syntra_status_t hal_device_event_router_route(
    syntra_hal_device_event_router_t *router,
    const syntra_hal_device_event_t *event
)
{
    syntra_hal_device_event_route_entry_t *entry;
    syntra_status_t status;

    if (router == NULL || event == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (!router_is_valid(router))
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (event->sequence == 0U ||
        !event_type_is_valid(event->type) ||
        !device_state_is_valid(event->previous_state) ||
        !device_state_is_valid(event->current_state))
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    entry = &router->routes[(unsigned int)event->type];

    if (entry->registered != 1)
    {
        increment_counter(&router->unhandled_events);
        increment_counter(&router->update_count);

        router->dirty = 1;

        return SYNTRA_STATUS_NOT_IMPLEMENTED;
    }

    status = hal_device_event_queue_enqueue(
        entry->target_queue,
        event
    );

    increment_counter(&router->total_routes);
    increment_counter(&entry->routed_count);

    if (status == SYNTRA_STATUS_OK)
    {
        increment_counter(&router->successful_routes);
    }
    else
    {
        increment_counter(&router->failed_routes);
        increment_counter(&entry->failure_count);
    }

    increment_counter(&router->update_count);

    router->dirty = 1;

    return status;
}

syntra_status_t hal_device_event_router_get_route(
    const syntra_hal_device_event_router_t *router,
    syntra_hal_device_event_type_t type,
    syntra_hal_device_event_queue_t **target_queue
)
{
    const syntra_hal_device_event_route_entry_t *entry;

    if (router == NULL ||
        target_queue == NULL ||
        !event_type_is_valid(type))
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (!router_is_valid(router))
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    entry = &router->routes[(unsigned int)type];

    if (entry->registered != 1)
    {
        *target_queue = NULL;
        return SYNTRA_STATUS_NOT_IMPLEMENTED;
    }

    *target_queue = entry->target_queue;

    return SYNTRA_STATUS_OK;
}

syntra_status_t hal_device_event_router_get_statistics(
    const syntra_hal_device_event_router_t *router,
    unsigned int *total_routes,
    unsigned int *successful_routes,
    unsigned int *failed_routes,
    unsigned int *unhandled_events
)
{
    if (router == NULL ||
        total_routes == NULL ||
        successful_routes == NULL ||
        failed_routes == NULL ||
        unhandled_events == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (!router_is_valid(router))
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    *total_routes = router->total_routes;
    *successful_routes = router->successful_routes;
    *failed_routes = router->failed_routes;
    *unhandled_events = router->unhandled_events;

    return SYNTRA_STATUS_OK;
}

syntra_status_t hal_device_event_router_validate(
    const syntra_hal_device_event_router_t *router
)
{
    unsigned int i;
    unsigned long long routed_count = 0ULL;
    unsigned long long failure_count = 0ULL;
    unsigned int expected_success;
    unsigned int expected_total;

    if (router == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (!router_is_valid(router))
    {
        return SYNTRA_STATUS_ERROR;
    }

    for (i = 0U;
         i < SYNTRA_HAL_DEVICE_EVENT_ROUTER_MAX_ROUTES;
         i++)
    {
        const syntra_hal_device_event_route_entry_t *entry =
            &router->routes[i];

        routed_count += entry->routed_count;
        failure_count += entry->failure_count;
    }

    expected_total =
        routed_count > UINT_MAX
            ? UINT_MAX
            : (unsigned int)routed_count;

    expected_success =
        routed_count >= failure_count
            ? (unsigned int)(
                routed_count - failure_count > UINT_MAX
                    ? UINT_MAX
                    : routed_count - failure_count
              )
            : 0U;

    if (router->total_routes != expected_total ||
        router->failed_routes != (failure_count > UINT_MAX
            ? UINT_MAX
            : (unsigned int)failure_count) ||
        router->successful_routes != expected_success)
    {
        return SYNTRA_STATUS_ERROR;
    }

    if (router->successful_routes > router->total_routes ||
        router->failed_routes > router->total_routes)
    {
        return SYNTRA_STATUS_ERROR;
    }

    return SYNTRA_STATUS_OK;
}
