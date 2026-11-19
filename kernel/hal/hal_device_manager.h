
#ifndef SYNTRA_HAL_DEVICE_MANAGER_H
#define SYNTRA_HAL_DEVICE_MANAGER_H

#include "hal_registry.h"
#include "hal_lifecycle.h"

/*
 * SyntraOS HAL Device Manager
 * Version: 4.0.0
 */

#define SYNTRA_HAL_MAX_DEVICE_EVENTS 64U

typedef enum
{
    SYNTRA_HAL_EVENT_STARTED = 0,
    SYNTRA_HAL_EVENT_STOPPED,
    SYNTRA_HAL_EVENT_RESTARTED,
    SYNTRA_HAL_EVENT_ERROR
} syntra_hal_event_type_t;

#define SYNTRA_HAL_EVENT_TYPE_COUNT 4U

typedef struct
{
    unsigned int device_id;
    syntra_hal_event_type_t type;
    unsigned int sequence;

} syntra_hal_device_event_t;

typedef struct
{
    unsigned int started;
    unsigned int stopped;
    unsigned int restarted;
    unsigned int errors;
    unsigned int total;

} syntra_hal_event_statistics_t;

typedef struct
{
    syntra_hal_registry_t *registry;

    unsigned int total_devices;
    unsigned int online_devices;
    unsigned int offline_devices;
    unsigned int error_devices;

    unsigned int update_count;

    syntra_hal_device_event_t events[
        SYNTRA_HAL_MAX_DEVICE_EVENTS
    ];

    unsigned int event_count;
    unsigned int next_event_sequence;

    int initialized;
    int dirty;

} syntra_hal_device_manager_t;

/* Manager lifecycle */

syntra_status_t hal_device_manager_init(
    syntra_hal_device_manager_t *manager,
    syntra_hal_registry_t *registry
);

syntra_status_t hal_device_manager_reset(
    syntra_hal_device_manager_t *manager
);

/* Device operations */

syntra_status_t hal_device_manager_start(
    syntra_hal_device_manager_t *manager,
    unsigned int device_id
);

syntra_status_t hal_device_manager_stop(
    syntra_hal_device_manager_t *manager,
    unsigned int device_id
);

syntra_status_t hal_device_manager_restart(
    syntra_hal_device_manager_t *manager,
    unsigned int device_id
);

/* Device information */

syntra_status_t hal_device_manager_get_device(
    const syntra_hal_device_manager_t *manager,
    unsigned int device_id,
    syntra_hal_device_t **device
);

syntra_status_t hal_device_manager_get_info(
    const syntra_hal_device_manager_t *manager,
    syntra_hal_device_manager_t *info
);

/* Validation and synchronization */

syntra_status_t hal_device_manager_validate(
    const syntra_hal_device_manager_t *manager
);

syntra_status_t hal_device_manager_refresh(
    syntra_hal_device_manager_t *manager
);

/* Event management */

syntra_status_t hal_device_manager_record_event(
    syntra_hal_device_manager_t *manager,
    unsigned int device_id,
    syntra_hal_event_type_t type
);

syntra_status_t hal_device_manager_get_event(
    const syntra_hal_device_manager_t *manager,
    unsigned int index,
    syntra_hal_device_event_t *event
);

syntra_status_t hal_device_manager_find_events(
    const syntra_hal_device_manager_t *manager,
    unsigned int device_id,
    syntra_hal_device_event_t *events,
    unsigned int capacity,
    unsigned int *found
);

syntra_status_t hal_device_manager_clear_events(
    syntra_hal_device_manager_t *manager
);

/* V4: Event filtering and statistics */

syntra_status_t hal_device_manager_get_event_statistics(
    const syntra_hal_device_manager_t *manager,
    syntra_hal_event_statistics_t *statistics
);

syntra_status_t hal_device_manager_find_events_by_type(
    const syntra_hal_device_manager_t *manager,
    syntra_hal_event_type_t type,
    syntra_hal_device_event_t *events,
    unsigned int capacity,
    unsigned int *found
);

syntra_status_t hal_device_manager_find_events_by_sequence(
    const syntra_hal_device_manager_t *manager,
    unsigned int first_sequence,
    unsigned int last_sequence,
    syntra_hal_device_event_t *events,
    unsigned int capacity,
    unsigned int *found
);

syntra_status_t hal_device_manager_get_device_event_statistics(
    const syntra_hal_device_manager_t *manager,
    unsigned int device_id,
    syntra_hal_event_statistics_t *statistics
);

#endif
