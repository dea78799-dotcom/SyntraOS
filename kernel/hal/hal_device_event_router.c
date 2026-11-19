
#include "hal_device_event_router.h"

#include <limits.h>
#include <stddef.h>
#include <string.h>

/*
 * SyntraOS HAL Device Event Router
 * Version: 2.0.1
 */

static void counter_increment(unsigned int *counter)
{
    if (counter != NULL && *counter < UINT_MAX)
    {
        (*counter)++;
    }
}

static void counter_add(
    unsigned int *counter,
    unsigned int value
)
{
    if (counter == NULL)
    {
        return;
    }

    if (UINT_MAX - *counter < value)
    {
        *counter = UINT_MAX;
    }
    else
    {
        *counter += value;
    }
}

static int event_type_is_valid(
    syntra_hal_device_event_type_t type
)
{
    return (unsigned int)type <
        SYNTRA_HAL_DEVICE_EVENT_TYPE_COUNT;
}

static int event_is_valid(
    const syntra_hal_device_event_t *event
)
{
    if (event == NULL)
    {
        return 0;
    }

    if (event->sequence == 0U)
    {
        return 0;
    }

    if (!event_type_is_valid(event->type))
    {
        return 0;
    }

    if ((unsigned int)event->previous_state >
        (unsigned int)SYNTRA_HAL_DEVICE_ERROR)
    {
        return 0;
    }

    if ((unsigned int)event->current_state >
        (unsigned int)SYNTRA_HAL_DEVICE_ERROR)
    {
        return 0;
    }

    return 1;
}

static void retire_entry(
    syntra_hal_device_event_router_t *router,
    syntra_hal_device_event_route_entry_t *entry
)
{
    if (router == NULL || entry == NULL)
    {
        return;
    }

    counter_add(
        &router->historical_routed_count,
        entry->routed_count
    );

    counter_add(
        &router->historical_failure_count,
        entry->failure_count
    );

    entry->target_queue = NULL;
    entry->routed_count = 0U;
    entry->failure_count = 0U;
    entry->registered = 0;
}

static int router_is_valid(
    const syntra_hal_device_event_router_t *router
)
{
    unsigned int registered_routes = 0U;
    unsigned int registered_targets = 0U;
    unsigned int type_index;
    unsigned int target_index;

    if (router == NULL)
    {
        return 0;
    }

    if (router->initialized != 1)
    {
        return 0;
    }

    for (type_index = 0U;
         type_index < SYNTRA_HAL_DEVICE_EVENT_ROUTER_MAX_ROUTES;
         type_index++)
    {
        unsigned int count = 0U;

        for (target_index = 0U;
             target_index <
                 SYNTRA_HAL_DEVICE_EVENT_ROUTER_MAX_TARGETS;
             target_index++)
        {
            const syntra_hal_device_event_route_entry_t *entry =
                &router->routes[type_index][target_index];

            if (entry->registered == 1)
            {
                if (entry->target_queue == NULL)
                {
                    return 0;
                }

                if (entry->failure_count > entry->routed_count)
                {
                    return 0;
                }

                if (hal_device_event_queue_validate(
                        entry->target_queue
                    ) != SYNTRA_STATUS_OK)
                {
                    return 0;
                }

                count++;
                registered_targets++;
            }
            else if (entry->registered == 0)
            {
                if (entry->target_queue != NULL)
                {
                    return 0;
                }

                if (entry->routed_count != 0U ||
                    entry->failure_count != 0U)
                {
                    return 0;
                }
            }
            else
            {
                return 0;
            }
        }

        if (count != router->target_counts[type_index])
        {
            return 0;
        }

        if (count > 0U)
        {
            registered_routes++;
        }
    }

    if (registered_routes != router->registered_routes)
    {
        return 0;
    }

    if (registered_targets != router->registered_targets)
    {
        return 0;
    }

    if (router->registered_targets >
        SYNTRA_HAL_DEVICE_EVENT_ROUTER_MAX_ROUTES *
        SYNTRA_HAL_DEVICE_EVENT_ROUTER_MAX_TARGETS)
    {
        return 0;
    }

    if (router->historical_failure_count >
        router->historical_routed_count)
    {
        return 0;
    }

    return 1;
}

/* =========================================================
 * Lifecycle
 * ========================================================= */

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
    router->dirty = 0;

    return SYNTRA_STATUS_OK;
}

syntra_status_t hal_device_event_router_reset(
    syntra_hal_device_event_router_t *router
)
{
    if (!router_is_valid(router))
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    return hal_device_event_router_init(router);
}

/* =========================================================
 * V2 Multi-target registration
 * ========================================================= */

syntra_status_t hal_device_event_router_add_target(
    syntra_hal_device_event_router_t *router,
    syntra_hal_device_event_type_t type,
    syntra_hal_device_event_queue_t *target_queue
)
{
    unsigned int type_index;
    unsigned int target_index;

    if (router == NULL || target_queue == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (!router_is_valid(router))
    {
        return SYNTRA_STATUS_ERROR;
    }

    if (!event_type_is_valid(type))
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (hal_device_event_queue_validate(target_queue) !=
        SYNTRA_STATUS_OK)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    type_index = (unsigned int)type;

    for (target_index = 0U;
         target_index <
             SYNTRA_HAL_DEVICE_EVENT_ROUTER_MAX_TARGETS;
         target_index++)
    {
        syntra_hal_device_event_route_entry_t *entry =
            &router->routes[type_index][target_index];

        if (entry->registered == 1 &&
            entry->target_queue == target_queue)
        {
            return SYNTRA_STATUS_ERROR;
        }
    }

    for (target_index = 0U;
         target_index <
             SYNTRA_HAL_DEVICE_EVENT_ROUTER_MAX_TARGETS;
         target_index++)
    {
        syntra_hal_device_event_route_entry_t *entry =
            &router->routes[type_index][target_index];

        if (entry->registered == 0)
        {
            int was_empty =
                router->target_counts[type_index] == 0U;

            entry->target_queue = target_queue;
            entry->routed_count = 0U;
            entry->failure_count = 0U;
            entry->registered = 1;

            counter_increment(
                &router->target_counts[type_index]
            );

            counter_increment(&router->registered_targets);

            if (was_empty)
            {
                counter_increment(&router->registered_routes);
            }

            counter_increment(&router->update_count);

            router->dirty = 1;

            return SYNTRA_STATUS_OK;
        }
    }

    return SYNTRA_STATUS_OUT_OF_MEMORY;
}

syntra_status_t hal_device_event_router_remove_target(
    syntra_hal_device_event_router_t *router,
    syntra_hal_device_event_type_t type,
    syntra_hal_device_event_queue_t *target_queue
)
{
    unsigned int type_index;
    unsigned int target_index;

    if (router == NULL || target_queue == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (!router_is_valid(router))
    {
        return SYNTRA_STATUS_ERROR;
    }

    if (!event_type_is_valid(type))
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    type_index = (unsigned int)type;

    for (target_index = 0U;
         target_index <
             SYNTRA_HAL_DEVICE_EVENT_ROUTER_MAX_TARGETS;
         target_index++)
    {
        syntra_hal_device_event_route_entry_t *entry =
            &router->routes[type_index][target_index];

        if (entry->registered == 1 &&
            entry->target_queue == target_queue)
        {
            retire_entry(router, entry);

            router->target_counts[type_index]--;
            router->registered_targets--;

            if (router->target_counts[type_index] == 0U)
            {
                router->registered_routes--;
            }

            counter_increment(&router->update_count);

            router->dirty = 1;

            return SYNTRA_STATUS_OK;
        }
    }

    return SYNTRA_STATUS_NOT_IMPLEMENTED;
}

/* =========================================================
 * V1-compatible registration
 * ========================================================= */

syntra_status_t hal_device_event_router_register(
    syntra_hal_device_event_router_t *router,
    syntra_hal_device_event_type_t type,
    syntra_hal_device_event_queue_t *target_queue
)
{
    /*
     * Validate arguments BEFORE checking duplicate routes.
     * This preserves the V1 API error contract.
     */

    if (router == NULL || target_queue == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (!event_type_is_valid(type))
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (!router_is_valid(router))
    {
        return SYNTRA_STATUS_ERROR;
    }

    if (router->target_counts[(unsigned int)type] != 0U)
    {
        return SYNTRA_STATUS_ERROR;
    }

    return hal_device_event_router_add_target(
        router,
        type,
        target_queue
    );
}

syntra_status_t hal_device_event_router_unregister(
    syntra_hal_device_event_router_t *router,
    syntra_hal_device_event_type_t type
)
{
    unsigned int type_index;
    unsigned int target_index;

    if (router == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (!router_is_valid(router))
    {
        return SYNTRA_STATUS_ERROR;
    }

    if (!event_type_is_valid(type))
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    type_index = (unsigned int)type;

    if (router->target_counts[type_index] == 0U)
    {
        return SYNTRA_STATUS_NOT_IMPLEMENTED;
    }

    for (target_index = 0U;
         target_index <
             SYNTRA_HAL_DEVICE_EVENT_ROUTER_MAX_TARGETS;
         target_index++)
    {
        syntra_hal_device_event_route_entry_t *entry =
            &router->routes[type_index][target_index];

        if (entry->registered == 1)
        {
            retire_entry(router, entry);
            router->registered_targets--;
        }
    }

    router->target_counts[type_index] = 0U;
    router->registered_routes--;

    counter_increment(&router->update_count);

    router->dirty = 1;

    return SYNTRA_STATUS_OK;
}

/* =========================================================
 * Routing
 * ========================================================= */

syntra_status_t hal_device_event_router_route(
    syntra_hal_device_event_router_t *router,
    const syntra_hal_device_event_t *event
)
{
    unsigned int type_index;
    unsigned int target_index;
    unsigned int attempted = 0U;
    unsigned int succeeded = 0U;
    unsigned int failed = 0U;

    syntra_status_t first_error = SYNTRA_STATUS_OK;

    if (router == NULL || event == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (!router_is_valid(router))
    {
        return SYNTRA_STATUS_ERROR;
    }

    if (!event_is_valid(event))
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    type_index = (unsigned int)event->type;

    counter_increment(&router->total_routes);
    counter_increment(&router->update_count);

    router->dirty = 1;

    if (router->target_counts[type_index] == 0U)
    {
        counter_increment(&router->unhandled_events);

        return SYNTRA_STATUS_NOT_IMPLEMENTED;
    }

    for (target_index = 0U;
         target_index <
             SYNTRA_HAL_DEVICE_EVENT_ROUTER_MAX_TARGETS;
         target_index++)
    {
        syntra_hal_device_event_route_entry_t *entry =
            &router->routes[type_index][target_index];

        syntra_status_t status;

        if (entry->registered != 1)
        {
            continue;
        }

        attempted++;

        counter_increment(&entry->routed_count);
        counter_increment(&router->total_deliveries);

        status = hal_device_event_queue_enqueue(
            entry->target_queue,
            event
        );

        if (status == SYNTRA_STATUS_OK)
        {
            succeeded++;

            counter_increment(
                &router->successful_deliveries
            );
        }
        else
        {
            failed++;

            counter_increment(&entry->failure_count);
            counter_increment(&router->failed_deliveries);

            if (first_error == SYNTRA_STATUS_OK)
            {
                first_error = status;
            }
        }
    }

    if (succeeded > 0U && failed > 0U)
    {
        counter_increment(&router->partially_routed_events);
    }

    if (attempted == 0U)
    {
        counter_increment(&router->unhandled_events);

        return SYNTRA_STATUS_NOT_IMPLEMENTED;
    }

    if (succeeded == 0U)
    {
        counter_increment(&router->failed_routes);

        return first_error;
    }

    counter_increment(&router->successful_routes);

    return SYNTRA_STATUS_OK;
}

/* =========================================================
 * V1-compatible information
 * ========================================================= */

syntra_status_t hal_device_event_router_get_route(
    const syntra_hal_device_event_router_t *router,
    syntra_hal_device_event_type_t type,
    syntra_hal_device_event_queue_t **target_queue
)
{
    unsigned int target_index;

    if (router == NULL || target_queue == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    *target_queue = NULL;

    if (!router_is_valid(router))
    {
        return SYNTRA_STATUS_ERROR;
    }

    if (!event_type_is_valid(type))
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    for (target_index = 0U;
         target_index <
             SYNTRA_HAL_DEVICE_EVENT_ROUTER_MAX_TARGETS;
         target_index++)
    {
        const syntra_hal_device_event_route_entry_t *entry =
            &router->routes[(unsigned int)type][target_index];

        if (entry->registered == 1)
        {
            *target_queue = entry->target_queue;

            return SYNTRA_STATUS_OK;
        }
    }

    return SYNTRA_STATUS_NOT_IMPLEMENTED;
}

/* =========================================================
 * V2 target information
 * ========================================================= */

syntra_status_t hal_device_event_router_get_target(
    const syntra_hal_device_event_router_t *router,
    syntra_hal_device_event_type_t type,
    unsigned int target_index,
    syntra_hal_device_event_queue_t **target_queue
)
{
    unsigned int current_index = 0U;
    unsigned int slot_index;

    if (router == NULL || target_queue == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    *target_queue = NULL;

    if (!router_is_valid(router))
    {
        return SYNTRA_STATUS_ERROR;
    }

    if (!event_type_is_valid(type))
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    for (slot_index = 0U;
         slot_index <
             SYNTRA_HAL_DEVICE_EVENT_ROUTER_MAX_TARGETS;
         slot_index++)
    {
        const syntra_hal_device_event_route_entry_t *entry =
            &router->routes[(unsigned int)type][slot_index];

        if (entry->registered == 1)
        {
            if (current_index == target_index)
            {
                *target_queue = entry->target_queue;

                return SYNTRA_STATUS_OK;
            }

            current_index++;
        }
    }

    return SYNTRA_STATUS_NOT_IMPLEMENTED;
}

syntra_status_t hal_device_event_router_get_target_count(
    const syntra_hal_device_event_router_t *router,
    syntra_hal_device_event_type_t type,
    unsigned int *target_count
)
{
    if (router == NULL || target_count == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    *target_count = 0U;

    if (!router_is_valid(router))
    {
        return SYNTRA_STATUS_ERROR;
    }

    if (!event_type_is_valid(type))
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    *target_count = router->target_counts[(unsigned int)type];

    return SYNTRA_STATUS_OK;
}

/* =========================================================
 * V1-compatible statistics
 * ========================================================= */

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

/* =========================================================
 * V2 delivery statistics
 * ========================================================= */

syntra_status_t hal_device_event_router_get_delivery_statistics(
    const syntra_hal_device_event_router_t *router,
    unsigned int *total_deliveries,
    unsigned int *successful_deliveries,
    unsigned int *failed_deliveries,
    unsigned int *partially_routed_events
)
{
    if (router == NULL ||
        total_deliveries == NULL ||
        successful_deliveries == NULL ||
        failed_deliveries == NULL ||
        partially_routed_events == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (!router_is_valid(router))
    {
        return SYNTRA_STATUS_ERROR;
    }

    *total_deliveries = router->total_deliveries;
    *successful_deliveries = router->successful_deliveries;
    *failed_deliveries = router->failed_deliveries;
    *partially_routed_events = router->partially_routed_events;

    return SYNTRA_STATUS_OK;
}

/* =========================================================
 * Validation
 * ========================================================= */

syntra_status_t hal_device_event_router_validate(
    const syntra_hal_device_event_router_t *router
)
{
    unsigned int type_index;
    unsigned int target_index;

    unsigned int total_entry_routes = 0U;
    unsigned int total_entry_failures = 0U;

    unsigned int event_accounted = 0U;
    unsigned int delivery_accounted = 0U;

    /*
     * NULL argument is an invalid caller argument,
     * not an internal router-state error.
     */
    if (router == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    /*
     * A non-NULL router with invalid internal state
     * returns SYNTRA_STATUS_ERROR.
     */
    if (!router_is_valid(router))
    {
        return SYNTRA_STATUS_ERROR;
    }

    total_entry_routes = router->historical_routed_count;
    total_entry_failures = router->historical_failure_count;

    for (type_index = 0U;
         type_index < SYNTRA_HAL_DEVICE_EVENT_ROUTER_MAX_ROUTES;
         type_index++)
    {
        for (target_index = 0U;
             target_index <
                 SYNTRA_HAL_DEVICE_EVENT_ROUTER_MAX_TARGETS;
             target_index++)
        {
            const syntra_hal_device_event_route_entry_t *entry =
                &router->routes[type_index][target_index];

            if (entry->registered == 1)
            {
                counter_add(
                    &total_entry_routes,
                    entry->routed_count
                );

                counter_add(
                    &total_entry_failures,
                    entry->failure_count
                );
            }
        }
    }

    if (total_entry_routes != router->total_deliveries)
    {
        return SYNTRA_STATUS_ERROR;
    }

    if (total_entry_failures != router->failed_deliveries)
    {
        return SYNTRA_STATUS_ERROR;
    }

    delivery_accounted = router->successful_deliveries;

    counter_add(
        &delivery_accounted,
        router->failed_deliveries
    );

    if (delivery_accounted != router->total_deliveries)
    {
        return SYNTRA_STATUS_ERROR;
    }

    event_accounted = router->successful_routes;

    counter_add(&event_accounted, router->failed_routes);
    counter_add(&event_accounted, router->unhandled_events);

    if (event_accounted != router->total_routes)
    {
        return SYNTRA_STATUS_ERROR;
    }

    if (router->partially_routed_events >
        router->successful_routes)
    {
        return SYNTRA_STATUS_ERROR;
    }

    return SYNTRA_STATUS_OK;
}
