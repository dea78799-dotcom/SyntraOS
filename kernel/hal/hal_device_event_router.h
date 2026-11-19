#ifndef SYNTRA_HAL_DEVICE_EVENT_ROUTER_H
#define SYNTRA_HAL_DEVICE_EVENT_ROUTER_H

#include "hal_device_event_queue.h"

#define SYNTRA_HAL_DEVICE_EVENT_ROUTER_MAX_ROUTES \
    SYNTRA_HAL_DEVICE_EVENT_TYPE_COUNT

typedef struct
{
    syntra_hal_device_event_queue_t *target_queue;

    unsigned int routed_count;
    unsigned int failure_count;

    int registered;

} syntra_hal_device_event_route_entry_t;

typedef struct
{
    syntra_hal_device_event_route_entry_t routes[
        SYNTRA_HAL_DEVICE_EVENT_ROUTER_MAX_ROUTES
    ];

    unsigned int registered_routes;
    unsigned int total_routes;
    unsigned int successful_routes;
    unsigned int failed_routes;
    unsigned int unhandled_events;

    unsigned int update_count;

    int initialized;
    int dirty;

} syntra_hal_device_event_router_t;

/* Lifecycle */

syntra_status_t hal_device_event_router_init(
    syntra_hal_device_event_router_t *router
);

syntra_status_t hal_device_event_router_reset(
    syntra_hal_device_event_router_t *router
);

/* Route registration */

syntra_status_t hal_device_event_router_register(
    syntra_hal_device_event_router_t *router,
    syntra_hal_device_event_type_t type,
    syntra_hal_device_event_queue_t *target_queue
);

syntra_status_t hal_device_event_router_unregister(
    syntra_hal_device_event_router_t *router,
    syntra_hal_device_event_type_t type
);

/* Event routing */

syntra_status_t hal_device_event_router_route(
    syntra_hal_device_event_router_t *router,
    const syntra_hal_device_event_t *event
);

/* Route information */

syntra_status_t hal_device_event_router_get_route(
    const syntra_hal_device_event_router_t *router,
    syntra_hal_device_event_type_t type,
    syntra_hal_device_event_queue_t **target_queue
);

syntra_status_t hal_device_event_router_get_statistics(
    const syntra_hal_device_event_router_t *router,
    unsigned int *total_routes,
    unsigned int *successful_routes,
    unsigned int *failed_routes,
    unsigned int *unhandled_events
);

/* Validation */

syntra_status_t hal_device_event_router_validate(
    const syntra_hal_device_event_router_t *router
);

#endif
