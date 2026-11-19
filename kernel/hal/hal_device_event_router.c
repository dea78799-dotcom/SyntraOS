#include "hal_device_event_router.h"

#include <limits.h>
#include <stddef.h>
#include <string.h>

/*
 * SyntraOS HAL Device Event Router
 * Version: 5.0.0
 */

/* =========================================================
 * Counter utilities
 * ========================================================= */

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

/* =========================================================
 * Basic validation
 * ========================================================= */

static int event_type_is_valid(
    syntra_hal_device_event_type_t type
)
{
    return (unsigned int)type <
        SYNTRA_HAL_DEVICE_EVENT_TYPE_COUNT;
}

static int status_is_valid(syntra_status_t status)
{
    switch (status)
    {
        case SYNTRA_STATUS_OK:
        case SYNTRA_STATUS_ERROR:
        case SYNTRA_STATUS_INVALID_ARGUMENT:
        case SYNTRA_STATUS_NOT_IMPLEMENTED:
        case SYNTRA_STATUS_OUT_OF_MEMORY:
        case SYNTRA_STATUS_PERMISSION_DENIED:
            return 1;

        default:
            return 0;
    }
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

/* =========================================================
 * Statistics validation
 * ========================================================= */

static int type_statistics_are_valid(
    const syntra_hal_device_event_type_stats_t *statistics
)
{
    unsigned int accounted_routes = 0U;
    unsigned int accounted_deliveries = 0U;

    if (statistics == NULL)
    {
        return 0;
    }

    if (!status_is_valid(statistics->last_route_status))
    {
        return 0;
    }

    counter_add(
        &accounted_routes,
        statistics->successful_routes
    );

    counter_add(
        &accounted_routes,
        statistics->failed_routes
    );

    counter_add(
        &accounted_routes,
        statistics->unhandled_events
    );

    if (accounted_routes != statistics->total_routes)
    {
        return 0;
    }

    counter_add(
        &accounted_deliveries,
        statistics->successful_deliveries
    );

    counter_add(
        &accounted_deliveries,
        statistics->failed_deliveries
    );

    if (accounted_deliveries != statistics->total_deliveries)
    {
        return 0;
    }

    if (statistics->partially_routed_events >
        statistics->successful_routes)
    {
        return 0;
    }

    if (statistics->partially_routed_events >
        statistics->total_routes)
    {
        return 0;
    }

    return 1;
}

/* =========================================================
 * Statistics update helpers
 * ========================================================= */

static void record_route_start(
    syntra_hal_device_event_router_t *router,
    syntra_hal_device_event_type_stats_t *statistics
)
{
    counter_increment(&router->total_routes);
    counter_increment(&router->update_count);

    counter_increment(&statistics->total_routes);
    counter_increment(&statistics->update_count);

    router->dirty = 1;
}

static void record_unhandled_event(
    syntra_hal_device_event_router_t *router,
    syntra_hal_device_event_type_stats_t *statistics
)
{
    counter_increment(&router->unhandled_events);
    counter_increment(&statistics->unhandled_events);

    statistics->last_route_status =
        SYNTRA_STATUS_NOT_IMPLEMENTED;
}

static void record_delivery(
    syntra_hal_device_event_router_t *router,
    syntra_hal_device_event_type_stats_t *statistics,
    syntra_hal_device_event_route_entry_t *entry,
    syntra_status_t status
)
{
    counter_increment(&entry->routed_count);

    counter_increment(&router->total_deliveries);
    counter_increment(&statistics->total_deliveries);

    if (status == SYNTRA_STATUS_OK)
    {
        counter_increment(&router->successful_deliveries);
        counter_increment(&statistics->successful_deliveries);
    }
    else
    {
        counter_increment(&entry->failure_count);

        counter_increment(&router->failed_deliveries);
        counter_increment(&statistics->failed_deliveries);
    }
}

static void record_partial_route(
    syntra_hal_device_event_router_t *router,
    syntra_hal_device_event_type_stats_t *statistics
)
{
    counter_increment(&router->partially_routed_events);
    counter_increment(&statistics->partially_routed_events);
}

static void record_successful_route(
    syntra_hal_device_event_router_t *router,
    syntra_hal_device_event_type_stats_t *statistics
)
{
    counter_increment(&router->successful_routes);
    counter_increment(&statistics->successful_routes);

    statistics->last_route_status = SYNTRA_STATUS_OK;
}

static void record_failed_route(
    syntra_hal_device_event_router_t *router,
    syntra_hal_device_event_type_stats_t *statistics,
    syntra_status_t status
)
{
    counter_increment(&router->failed_routes);
    counter_increment(&statistics->failed_routes);

    statistics->last_route_status = status;
}

/* =========================================================
 * Entry management
 * ========================================================= */

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

/* =========================================================
 * Router integrity validation
 * ========================================================= */

static int router_is_valid(
    const syntra_hal_device_event_router_t *router
)
{
    unsigned int type_index;
    unsigned int target_index;

    unsigned int registered_routes = 0U;
    unsigned int registered_targets = 0U;

    unsigned int total_routes = 0U;
    unsigned int successful_routes = 0U;
    unsigned int failed_routes = 0U;
    unsigned int unhandled_events = 0U;
    unsigned int partially_routed_events = 0U;

    unsigned int total_deliveries = 0U;
    unsigned int successful_deliveries = 0U;
    unsigned int failed_deliveries = 0U;

    unsigned int historical_routed = 0U;
    unsigned int historical_failures = 0U;

    unsigned int active_routed = 0U;
    unsigned int active_failures = 0U;

    unsigned int accounted_deliveries = 0U;
    unsigned int accounted_routes = 0U;

    if (router == NULL)
    {
        return 0;
    }

    if (router->initialized != 1)
    {
        return 0;
    }

    if (router->dirty != 0 && router->dirty != 1)
    {
        return 0;
    }

    if (router->registered_routes >
        SYNTRA_HAL_DEVICE_EVENT_ROUTER_MAX_ROUTES)
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

    for (type_index = 0U;
         type_index < SYNTRA_HAL_DEVICE_EVENT_TYPE_COUNT;
         type_index++)
    {
        const syntra_hal_device_event_type_stats_t *statistics =
            &router->type_statistics[type_index];

        unsigned int actual_targets = 0U;

        if (!type_statistics_are_valid(statistics))
        {
            return 0;
        }

        if (router->target_counts[type_index] >
            SYNTRA_HAL_DEVICE_EVENT_ROUTER_MAX_TARGETS)
        {
            return 0;
        }

        for (target_index = 0U;
             target_index <
                 SYNTRA_HAL_DEVICE_EVENT_ROUTER_MAX_TARGETS;
             target_index++)
        {
            const syntra_hal_device_event_route_entry_t *entry =
                &router->routes[type_index][target_index];

            if (entry->registered != 0 &&
                entry->registered != 1)
            {
                return 0;
            }

            if (entry->registered == 1)
            {
                unsigned int duplicate_index;

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

                for (duplicate_index = target_index + 1U;
                     duplicate_index <
                         SYNTRA_HAL_DEVICE_EVENT_ROUTER_MAX_TARGETS;
                     duplicate_index++)
                {
                    const syntra_hal_device_event_route_entry_t *other =
                        &router->routes[type_index][duplicate_index];

                    if (other->registered == 1 &&
                        other->target_queue == entry->target_queue)
                    {
                        return 0;
                    }
                }

                actual_targets++;

                counter_increment(&registered_targets);

                counter_add(
                    &active_routed,
                    entry->routed_count
                );

                counter_add(
                    &active_failures,
                    entry->failure_count
                );
            }
            else
            {
                if (entry->target_queue != NULL ||
                    entry->routed_count != 0U ||
                    entry->failure_count != 0U)
                {
                    return 0;
                }
            }
        }

        if (actual_targets != router->target_counts[type_index])
        {
            return 0;
        }

        if (actual_targets > 0U)
        {
            counter_increment(&registered_routes);
        }

        counter_add(
            &total_routes,
            statistics->total_routes
        );

        counter_add(
            &successful_routes,
            statistics->successful_routes
        );

        counter_add(
            &failed_routes,
            statistics->failed_routes
        );

        counter_add(
            &unhandled_events,
            statistics->unhandled_events
        );

        counter_add(
            &partially_routed_events,
            statistics->partially_routed_events
        );

        counter_add(
            &total_deliveries,
            statistics->total_deliveries
        );

        counter_add(
            &successful_deliveries,
            statistics->successful_deliveries
        );

        counter_add(
            &failed_deliveries,
            statistics->failed_deliveries
        );
    }

    if (registered_routes != router->registered_routes)
    {
        return 0;
    }

    if (registered_targets != router->registered_targets)
    {
        return 0;
    }

    if (total_routes != router->total_routes ||
        successful_routes != router->successful_routes ||
        failed_routes != router->failed_routes ||
        unhandled_events != router->unhandled_events ||
        partially_routed_events != router->partially_routed_events)
    {
        return 0;
    }

    if (total_deliveries != router->total_deliveries ||
        successful_deliveries != router->successful_deliveries ||
        failed_deliveries != router->failed_deliveries)
    {
        return 0;
    }

    counter_add(
        &historical_routed,
        router->historical_routed_count
    );

    counter_add(
        &historical_failures,
        router->historical_failure_count
    );

    counter_add(&historical_routed, active_routed);
    counter_add(&historical_failures, active_failures);

    if (historical_routed != router->total_deliveries ||
        historical_failures != router->failed_deliveries)
    {
        return 0;
    }

    if (router->successful_deliveries >
        router->total_deliveries)
    {
        return 0;
    }

    if (router->failed_deliveries >
        router->total_deliveries)
    {
        return 0;
    }

    counter_add(
        &accounted_deliveries,
        router->successful_deliveries
    );

    counter_add(
        &accounted_deliveries,
        router->failed_deliveries
    );

    if (accounted_deliveries != router->total_deliveries)
    {
        return 0;
    }

    counter_add(
        &accounted_routes,
        router->successful_routes
    );

    counter_add(
        &accounted_routes,
        router->failed_routes
    );

    counter_add(
        &accounted_routes,
        router->unhandled_events
    );

    if (accounted_routes != router->total_routes)
    {
        return 0;
    }

    if (router->partially_routed_events >
        router->successful_routes)
    {
        return 0;
    }

    return 1;
}

/* =========================================================
 * Initialization
 * ========================================================= */

syntra_status_t hal_device_event_router_init(
    syntra_hal_device_event_router_t *router
)
{
    unsigned int index;

    if (router == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    memset(router, 0, sizeof(*router));

    for (index = 0U;
         index < SYNTRA_HAL_DEVICE_EVENT_TYPE_COUNT;
         index++)
    {
        router->type_statistics[index].last_route_status =
            SYNTRA_STATUS_NOT_IMPLEMENTED;
    }

    router->initialized = 1;
    router->dirty = 0;

    return SYNTRA_STATUS_OK;
}

/* =========================================================
 * Reset
 * ========================================================= */

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

/* =========================================================
 * Target registration
 * ========================================================= */

syntra_status_t hal_device_event_router_add_target(
    syntra_hal_device_event_router_t *router,
    syntra_hal_device_event_type_t type,
    syntra_hal_device_event_queue_t *target_queue
)
{
    unsigned int index;
    unsigned int free_index =
        SYNTRA_HAL_DEVICE_EVENT_ROUTER_MAX_TARGETS;

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

    for (index = 0U;
         index < SYNTRA_HAL_DEVICE_EVENT_ROUTER_MAX_TARGETS;
         index++)
    {
        syntra_hal_device_event_route_entry_t *entry =
            &router->routes[(unsigned int)type][index];

        if (entry->registered == 1)
        {
            if (entry->target_queue == target_queue)
            {
                return SYNTRA_STATUS_ERROR;
            }
        }
        else if (free_index ==
                 SYNTRA_HAL_DEVICE_EVENT_ROUTER_MAX_TARGETS)
        {
            free_index = index;
        }
    }

    if (free_index ==
        SYNTRA_HAL_DEVICE_EVENT_ROUTER_MAX_TARGETS)
    {
        return SYNTRA_STATUS_OUT_OF_MEMORY;
    }

    {
        syntra_hal_device_event_route_entry_t *entry =
            &router->routes[(unsigned int)type][free_index];

        entry->target_queue = target_queue;
        entry->routed_count = 0U;
        entry->failure_count = 0U;
        entry->registered = 1;
    }

    counter_increment(
        &router->target_counts[(unsigned int)type]
    );

    counter_increment(&router->registered_targets);

    if (router->target_counts[(unsigned int)type] == 1U)
    {
        counter_increment(&router->registered_routes);
    }

    counter_increment(&router->update_count);
    router->dirty = 1;

    return SYNTRA_STATUS_OK;
}

syntra_status_t hal_device_event_router_register(
    syntra_hal_device_event_router_t *router,
    syntra_hal_device_event_type_t type,
    syntra_hal_device_event_queue_t *target_queue
)
{
    return hal_device_event_router_add_target(
        router,
        type,
        target_queue
    );
}

/* =========================================================
 * Target removal
 * ========================================================= */

syntra_status_t hal_device_event_router_remove_target(
    syntra_hal_device_event_router_t *router,
    syntra_hal_device_event_type_t type,
    syntra_hal_device_event_queue_t *target_queue
)
{
    unsigned int index;

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

    for (index = 0U;
         index < SYNTRA_HAL_DEVICE_EVENT_ROUTER_MAX_TARGETS;
         index++)
    {
        syntra_hal_device_event_route_entry_t *entry =
            &router->routes[(unsigned int)type][index];

        if (entry->registered == 1 &&
            entry->target_queue == target_queue)
        {
            retire_entry(router, entry);

            if (router->target_counts[(unsigned int)type] > 0U)
            {
                router->target_counts[(unsigned int)type]--;
            }

            if (router->registered_targets > 0U)
            {
                router->registered_targets--;
            }

            if (router->target_counts[(unsigned int)type] == 0U &&
                router->registered_routes > 0U)
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

syntra_status_t hal_device_event_router_unregister(
    syntra_hal_device_event_router_t *router,
    syntra_hal_device_event_type_t type
)
{
    unsigned int index;
    unsigned int removed_count = 0U;

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

    for (index = 0U;
         index < SYNTRA_HAL_DEVICE_EVENT_ROUTER_MAX_TARGETS;
         index++)
    {
        syntra_hal_device_event_route_entry_t *entry =
            &router->routes[(unsigned int)type][index];

        if (entry->registered == 1)
        {
            retire_entry(router, entry);

            if (router->target_counts[(unsigned int)type] > 0U)
            {
                router->target_counts[(unsigned int)type]--;
            }

            if (router->registered_targets > 0U)
            {
                router->registered_targets--;
            }

            counter_increment(&removed_count);
        }
    }

    if (removed_count == 0U)
    {
        return SYNTRA_STATUS_NOT_IMPLEMENTED;
    }

    if (router->registered_routes > 0U)
    {
        router->registered_routes--;
    }

    counter_increment(&router->update_count);
    router->dirty = 1;

    return SYNTRA_STATUS_OK;
}

/* =========================================================
 * Event routing
 * ========================================================= */

syntra_status_t hal_device_event_router_route(
    syntra_hal_device_event_router_t *router,
    const syntra_hal_device_event_t *event
)
{
    unsigned int index;
    unsigned int target_count;

    unsigned int successful_count = 0U;
    unsigned int failed_count = 0U;

    syntra_status_t first_error = SYNTRA_STATUS_ERROR;

    syntra_hal_device_event_type_stats_t *statistics;

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

    statistics =
        &router->type_statistics[(unsigned int)event->type];

    record_route_start(router, statistics);

    target_count =
        router->target_counts[(unsigned int)event->type];

    if (target_count == 0U)
    {
        record_unhandled_event(router, statistics);

        return SYNTRA_STATUS_NOT_IMPLEMENTED;
    }

    for (index = 0U;
         index < SYNTRA_HAL_DEVICE_EVENT_ROUTER_MAX_TARGETS;
         index++)
    {
        syntra_hal_device_event_route_entry_t *entry =
            &router->routes[(unsigned int)event->type][index];

        syntra_status_t status;

        if (entry->registered != 1)
        {
            continue;
        }

        status = hal_device_event_queue_enqueue(
            entry->target_queue,
            event
        );

        record_delivery(
            router,
            statistics,
            entry,
            status
        );

        if (status == SYNTRA_STATUS_OK)
        {
            counter_increment(&successful_count);
        }
        else
        {
            if (failed_count == 0U)
            {
                first_error = status;
            }

            counter_increment(&failed_count);
        }
    }

    if (successful_count > 0U)
    {
        record_successful_route(router, statistics);

        if (failed_count > 0U)
        {
            record_partial_route(router, statistics);
        }

        return SYNTRA_STATUS_OK;
    }

    record_failed_route(
        router,
        statistics,
        first_error
    );

    return first_error;
}

/* =========================================================
 * Statistics query
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

    *total_routes = 0U;
    *successful_routes = 0U;
    *failed_routes = 0U;
    *unhandled_events = 0U;

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

syntra_status_t hal_device_event_router_get_route(
    const syntra_hal_device_event_router_t *router,
    syntra_hal_device_event_type_t type,
    syntra_hal_device_event_queue_t **target_queue
)
{
    unsigned int index;

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

    for (index = 0U;
         index < SYNTRA_HAL_DEVICE_EVENT_ROUTER_MAX_TARGETS;
         index++)
    {
        const syntra_hal_device_event_route_entry_t *entry =
            &router->routes[(unsigned int)type][index];

        if (entry->registered == 1)
        {
            *target_queue = entry->target_queue;
            return SYNTRA_STATUS_OK;
        }
    }

    return SYNTRA_STATUS_NOT_IMPLEMENTED;
}

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

    *total_deliveries = 0U;
    *successful_deliveries = 0U;
    *failed_deliveries = 0U;
    *partially_routed_events = 0U;

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

syntra_status_t hal_device_event_router_get_type_statistics(
    const syntra_hal_device_event_router_t *router,
    syntra_hal_device_event_type_t type,
    syntra_hal_device_event_type_stats_t *statistics
)
{
    if (router == NULL || statistics == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    memset(statistics, 0, sizeof(*statistics));

    if (!router_is_valid(router))
    {
        return SYNTRA_STATUS_ERROR;
    }

    if (!event_type_is_valid(type))
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    *statistics = router->type_statistics[(unsigned int)type];

    return SYNTRA_STATUS_OK;
}

/* =========================================================
 * Target query
 * ========================================================= */

syntra_status_t hal_device_event_router_get_target_count(
    const syntra_hal_device_event_router_t *router,
    syntra_hal_device_event_type_t type,
    unsigned int *count
)
{
    if (count == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    *count = 0U;

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

    *count = router->target_counts[(unsigned int)type];

    return SYNTRA_STATUS_OK;
}

syntra_status_t hal_device_event_router_get_target(
    const syntra_hal_device_event_router_t *router,
    syntra_hal_device_event_type_t type,
    unsigned int target_index,
    syntra_hal_device_event_queue_t **target_queue
)
{
    unsigned int index;
    unsigned int current_target = 0U;

    if (target_queue == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    *target_queue = NULL;

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

    if (target_index >=
        router->target_counts[(unsigned int)type])
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    for (index = 0U;
         index < SYNTRA_HAL_DEVICE_EVENT_ROUTER_MAX_TARGETS;
         index++)
    {
        const syntra_hal_device_event_route_entry_t *entry =
            &router->routes[(unsigned int)type][index];

        if (entry->registered == 1)
        {
            if (current_target == target_index)
            {
                *target_queue = entry->target_queue;

                return SYNTRA_STATUS_OK;
            }

            counter_increment(&current_target);
        }
    }

    return SYNTRA_STATUS_ERROR;
}

/* =========================================================
 * Router validation
 * ========================================================= */

syntra_status_t hal_device_event_router_validate(
    const syntra_hal_device_event_router_t *router
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

    return SYNTRA_STATUS_OK;
}
