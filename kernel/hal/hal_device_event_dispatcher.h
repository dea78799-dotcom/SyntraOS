#ifndef SYNTRA_HAL_DEVICE_EVENT_DISPATCHER_H
#define SYNTRA_HAL_DEVICE_EVENT_DISPATCHER_H

#include "hal_device_event_queue.h"

#define SYNTRA_HAL_DEVICE_EVENT_DISPATCHER_MAX_HANDLERS \
    SYNTRA_HAL_DEVICE_EVENT_TYPE_COUNT

typedef syntra_status_t (*syntra_hal_device_event_handler_t)(
    const syntra_hal_device_event_t *event,
    void *context
);

typedef struct
{
    syntra_hal_device_event_handler_t handler;
    void *context;

    unsigned int dispatch_count;
    unsigned int failure_count;

    int registered;

} syntra_hal_device_event_handler_entry_t;

typedef struct
{
    syntra_hal_device_event_handler_entry_t handlers[
        SYNTRA_HAL_DEVICE_EVENT_DISPATCHER_MAX_HANDLERS
    ];

    unsigned int total_dispatches;
    unsigned int successful_dispatches;
    unsigned int failed_dispatches;
    unsigned int unhandled_events;

    unsigned int update_count;

    int initialized;
    int dirty;

} syntra_hal_device_event_dispatcher_t;

/* Lifecycle */

syntra_status_t hal_device_event_dispatcher_init(
    syntra_hal_device_event_dispatcher_t *dispatcher
);

syntra_status_t hal_device_event_dispatcher_reset(
    syntra_hal_device_event_dispatcher_t *dispatcher
);

/* Handler registration */

syntra_status_t hal_device_event_dispatcher_register(
    syntra_hal_device_event_dispatcher_t *dispatcher,
    syntra_hal_device_event_type_t type,
    syntra_hal_device_event_handler_t handler,
    void *context
);

syntra_status_t hal_device_event_dispatcher_unregister(
    syntra_hal_device_event_dispatcher_t *dispatcher,
    syntra_hal_device_event_type_t type
);

/* Event dispatching */

syntra_status_t hal_device_event_dispatcher_dispatch(
    syntra_hal_device_event_dispatcher_t *dispatcher,
    const syntra_hal_device_event_t *event
);

syntra_status_t hal_device_event_dispatcher_process_next(
    syntra_hal_device_event_dispatcher_t *dispatcher,
    syntra_hal_device_event_queue_t *queue
);

syntra_status_t hal_device_event_dispatcher_process_all(
    syntra_hal_device_event_dispatcher_t *dispatcher,
    syntra_hal_device_event_queue_t *queue,
    unsigned int *processed
);

/* Information */

syntra_status_t hal_device_event_dispatcher_get_statistics(
    const syntra_hal_device_event_dispatcher_t *dispatcher,
    unsigned int *total_dispatches,
    unsigned int *successful_dispatches,
    unsigned int *failed_dispatches,
    unsigned int *unhandled_events
);

/* Validation */

syntra_status_t hal_device_event_dispatcher_validate(
    const syntra_hal_device_event_dispatcher_t *dispatcher
);

#endif
