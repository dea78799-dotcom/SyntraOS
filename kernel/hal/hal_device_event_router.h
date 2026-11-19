
#ifndef SYNTRA_HAL_DEVICE_EVENT_ROUTER_H
#define SYNTRA_HAL_DEVICE_EVENT_ROUTER_H

#include "hal_device_event_queue.h"

#define SYNTRA_HAL_DEVICE_EVENT_ROUTER_MAX_ROUTES \
    SYNTRA_HAL_DEVICE_EVENT_TYPE_COUNT

#define SYNTRA_HAL_DEVICE_EVENT_ROUTER_MAX_TARGETS 8U

/*
 * Per-event-type monitoring statistics.
 */
typedef struct
{
    unsigned int total_routes;
    unsigned int successful_routes;
    unsigned int failed_routes;
    unsigned int unhandled_events;
    unsigned int partially_routed_events;

    unsigned int total_deliveries;
    unsigned int successful_deliveries;
    unsigned int failed_deliveries;

    syntra_status_t last_route_status;

    unsigned int update_count;

} syntra_hal_device_event_type_stats_t;

/*
 * Individual route target.
 */
typedef struct
{
    syntra_hal_device_event_queue_t *target_queue;

    unsigned int routed_count;
    unsigned int failure_count;

    int registered;

} syntra_hal_device_event_route_entry_t;

/*
 * Event Router V3.
 */
typedef struct
{
    syntra_hal_device_event_route_entry_t routes[
        SYNTRA_HAL_DEVICE_EVENT_ROUTER_MAX_ROUTES
    ][SYNTRA_HAL_DEVICE_EVENT_ROUTER_MAX_TARGETS];

    unsigned int target_counts[
        SYNTRA_HAL_DEVICE_EVENT_ROUTER_MAX_ROUTES
    ];

    /*
     * V2 registration statistics.
     */
    unsigned int registered_routes;
    unsigned int registered_targets;

    /*
     * V2 historical target statistics.
     */
    unsigned int historical_routed_count;
    unsigned int historical_failure_count;

    /*
     * Global event statistics.
     */
    unsigned int total_routes;
    unsigned int successful_routes;
    unsigned int failed_routes;
    unsigned int unhandled_events;
    unsigned int partially_routed_events;

    /*
     * Global delivery statistics.
     */
    unsigned int total_deliveries;
    unsigned int successful_deliveries;
    unsigned int failed_deliveries;

    /*
     * V3 per-event-type monitoring.
     */
    syntra_hal_device_event_type_stats_t type_statistics[
        SYNTRA_HAL_DEVICE_EVENT_TYPE_COUNT
    ];

    /*
     * General state.
     */
    unsigned int update_count;

    int initialized;
    int dirty;

} syntra_hal_device_event_router_t;


/* =========================================================
 * Lifecycle
 * ========================================================= */

syntra_status_t hal_device_event_router_init(
    syntra_hal_device_event_router_t *router
);

syntra_status_t hal_device_event_router_reset(
    syntra_hal_device_event_router_t *router
);


/* =========================================================
 * V1-compatible registration
 * ========================================================= */

syntra_status_t hal_device_event_router_register(
    syntra_hal_device_event_router_t *router,
    syntra_hal_device_event_type_t type,
    syntra_hal_device_event_queue_t *target_queue
);

syntra_status_t hal_device_event_router_unregister(
    syntra_hal_device_event_router_t *router,
    syntra_hal_device_event_type_t type
);


/* =========================================================
 * V2 multi-target registration
 * ========================================================= */

syntra_status_t hal_device_event_router_add_target(
    syntra_hal_device_event_router_t *router,
    syntra_hal_device_event_type_t type,
    syntra_hal_device_event_queue_t *target_queue
);

syntra_status_t hal_device_event_router_remove_target(
    syntra_hal_device_event_router_t *router,
    syntra_hal_device_event_type_t type,
    syntra_hal_device_event_queue_t *target_queue
);


/* =========================================================
 * Routing
 * ========================================================= */

syntra_status_t hal_device_event_router_route(
    syntra_hal_device_event_router_t *router,
    const syntra_hal_device_event_t *event
);


/* =========================================================
 * V1-compatible information
 * ========================================================= */

syntra_status_t hal_device_event_router_get_route(
    const syntra_hal_device_event_router_t *router,
    syntra_hal_device_event_type_t type,
    syntra_hal_device_event_queue_t **target_queue
);


/* =========================================================
 * V2 target information
 * ========================================================= */

syntra_status_t hal_device_event_router_get_target(
    const syntra_hal_device_event_router_t *router,
    syntra_hal_device_event_type_t type,
    unsigned int target_index,
    syntra_hal_device_event_queue_t **target_queue
);

syntra_status_t hal_device_event_router_get_target_count(
    const syntra_hal_device_event_router_t *router,
    syntra_hal_device_event_type_t type,
    unsigned int *target_count
);


/* =========================================================
 * V1-compatible global statistics
 * ========================================================= */

syntra_status_t hal_device_event_router_get_statistics(
    const syntra_hal_device_event_router_t *router,
    unsigned int *total_routes,
    unsigned int *successful_routes,
    unsigned int *failed_routes,
    unsigned int *unhandled_events
);


/* =========================================================
 * V2 global delivery statistics
 * ========================================================= */

syntra_status_t hal_device_event_router_get_delivery_statistics(
    const syntra_hal_device_event_router_t *router,
    unsigned int *total_deliveries,
    unsigned int *successful_deliveries,
    unsigned int *failed_deliveries,
    unsigned int *partially_routed_events
);


/* =========================================================
 * V3 per-event-type monitoring
 * ========================================================= */

syntra_status_t hal_device_event_router_get_type_statistics(
    const syntra_hal_device_event_router_t *router,
    syntra_hal_device_event_type_t type,
    syntra_hal_device_event_type_stats_t *statistics
);


/* =========================================================
 * Validation
 * ========================================================= */

syntra_status_t hal_device_event_router_validate(
    const syntra_hal_device_event_router_t *router
);

#endif
