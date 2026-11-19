
#include "hal_device_event_router.h"

#include <limits.h>
#include <stddef.h>

static unsigned int saturating_increment(unsigned int value)
{
    if (value < UINT_MAX)
    {
        return value + 1U;
    }

    return value;
}

static unsigned int saturating_add(
    unsigned int left,
    unsigned int right
)
{
    if (UINT_MAX - left < right)
    {
        return UINT_MAX;
    }

    return left + right;
}

static int is_valid_event(const syntra_hal_device_event_t *event)
{
    if (event == NULL)
    {
        return 0;
    }

    if (event->sequence == 0U)
    {
        return 0;
    }

    if (event->type < SYNTRA_HAL_DEVICE_EVENT_ADDED ||
        event->type >= SYNTRA_HAL_DEVICE_EVENT_TYPE_COUNT)
    {
        return 0;
    }

    if (event->previous_state < SYNTRA_HAL_DEVICE_OFFLINE ||
        event->previous_state > SYNTRA_HAL_DEVICE_ERROR)
    {
        return 0;
    }

    if (event->current_state < SYNTRA_HAL_DEVICE_OFFLINE ||
        event->current_state > SYNTRA_HAL_DEVICE_ERROR)
    {
        return 0;
    }

    return 1;
}

static int router_is_valid(
    const syntra_hal_device_event_router_t *router
)
{
    unsigned int registered_count = 0U;
    unsigned int i;

    if (router == NULL || router->initialized != 1)
    {
        return 0;
    }

    for (i = 0U; i < SYNTRA_HAL_DEVICE_EVENT_ROUTER_MAX_ROUTES; ++i)
    {
        const syntra_hal_device_event_route_entry_t *entry =
            &router->routes[i];

        if (entry->registered != 0 && entry->registered != 1)
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

            registered_count++;
        }
        else if (entry->target_queue != NULL)
        {
            return 0;
        }
    }

    return registered_count == router->registered_routes;
}

syntra_status_t hal_device_event_router_init(
    syntra_hal_device_event_router_t *router
)
{
    unsigned int i;

    if (router == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    router->registered_routes = 0U;
    router->total_routes = 0U;
    router->successful_routes = 0U;
    router->failed_routes = 0U;
    router->unhandled_events = 0U;
    router->update_count = 0U;
    router->initialized = 1;
    router->dirty = 0;

    for (i = 0U; i < SYNTRA_HAL_DEVICE_EVENT_ROUTER_MAX_ROUTES; ++i)
    {
        router->routes[i].target_queue = NULL;
        router->routes[i].routed_count = 0U;
        router->routes[i].failure_count = 0U;
        router->routes[i].registered = 0;
    }

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
        return SYNTRA_STATUS_ERROR;
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

    if (router == NULL || target_queue == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (type < SYNTRA_HAL_DEVICE_EVENT_ADDED ||
        type >= SYNTRA_HAL_DEVICE_EVENT_TYPE_COUNT)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (!router_is_valid(router))
    {
        return SYNTRA_STATUS_ERROR;
    }

    if (hal_device_event_queue_validate(target_queue) != SYNTRA_STATUS_OK)
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

    router->registered_routes++;
    router->update_count = saturating_increment(router->update_count);
    router->dirty = 1;

    return SYNTRA_STATUS_OK;
}

syntra_status_t hal_device_event_router_unregister(
    syntra_hal_device_event_router_t *router,
    syntra_hal_device_event_type_t type
)
{
    syntra_hal_device_event_route_entry_t *entry;

    if (router == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (type < SYNTRA_HAL_DEVICE_EVENT_ADDED ||
        type >= SYNTRA_HAL_DEVICE_EVENT_TYPE_COUNT)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (!router_is_valid(router))
    {
        return SYNTRA_STATUS_ERROR;
    }

    entry = &router->routes[(unsigned int)type];

    if (entry->registered != 1)
    {
        return SYNTRA_STATUS_NOT_IMPLEMENTED;
    }

    entry->target_queue = NULL;
    entry->registered = 0;

    if (router->registered_routes > 0U)
    {
        router->registered_routes--;
    }

    router->update_count = saturating_increment(router->update_count);
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

    if (!is_valid_event(event))
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (!router_is_valid(router))
    {
        return SYNTRA_STATUS_ERROR;
    }

    entry = &router->routes[(unsigned int)event->type];

    router->total_routes =
        saturating_increment(router->total_routes);

    router->update_count =
        saturating_increment(router->update_count);

    router->dirty = 1;

    if (entry->registered != 1)
    {
        router->unhandled_events =
            saturating_increment(router->unhandled_events);

        return SYNTRA_STATUS_NOT_IMPLEMENTED;
    }

    entry->routed_count =
        saturating_increment(entry->routed_count);

    status = hal_device_event_queue_enqueue(
        entry->target_queue,
        event
    );

    if (status == SYNTRA_STATUS_OK)
    {
        router->successful_routes =
            saturating_increment(router->successful_routes);

        return SYNTRA_STATUS_OK;
    }

    entry->failure_count =
        saturating_increment(entry->failure_count);

    router->failed_routes =
        saturating_increment(router->failed_routes);

    return status;
}

syntra_status_t hal_device_event_router_get_route(
    const syntra_hal_device_event_router_t *router,
    syntra_hal_device_event_type_t type,
    syntra_hal_device_event_queue_t **target_queue
)
{
    const syntra_hal_device_event_route_entry_t *entry;

    if (router == NULL || target_queue == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    *target_queue = NULL;

    if (type < SYNTRA_HAL_DEVICE_EVENT_ADDED ||
        type >= SYNTRA_HAL_DEVICE_EVENT_TYPE_COUNT)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (!router_is_valid(router))
    {
        return SYNTRA_STATUS_ERROR;
    }

    entry = &router->routes[(unsigned int)type];

    if (entry->registered != 1)
    {
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
        return SYNTRA_STATUS_ERROR;
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
    unsigned int registered_count = 0U;
    unsigned int routed_count = 0U;
    unsigned int failure_count = 0U;
    unsigned int accounted_routes;
    unsigned int i;

    if (router == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (!router_is_valid(router))
    {
        return SYNTRA_STATUS_ERROR;
    }

    for (i = 0U; i < SYNTRA_HAL_DEVICE_EVENT_ROUTER_MAX_ROUTES; ++i)
    {
        const syntra_hal_device_event_route_entry_t *entry =
            &router->routes[i];

        if (entry->registered == 1)
        {
            registered_count++;
        }

        routed_count = saturating_add(
            routed_count,
            entry->routed_count
        );

        failure_count = saturating_add(
            failure_count,
            entry->failure_count
        );
    }

    if (registered_count != router->registered_routes)
    {
        return SYNTRA_STATUS_ERROR;
    }

    if (failure_count != router->failed_routes)
    {
        return SYNTRA_STATUS_ERROR;
    }

    if (routed_count != saturating_add(
            router->successful_routes,
            router->failed_routes
        ))
    {
        return SYNTRA_STATUS_ERROR;
    }

    accounted_routes = saturating_add(
        saturating_add(
            router->successful_routes,
            router->failed_routes
        ),
        router->unhandled_events
    );

    if (accounted_routes != router->total_routes)
    {
        return SYNTRA_STATUS_ERROR;
    }

    return SYNTRA_STATUS_OK;
}
