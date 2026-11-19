
#ifndef SYNTRA_HAL_DEVICE_DISCOVERY_H
#define SYNTRA_HAL_DEVICE_DISCOVERY_H

#include "hal_device.h"

#define SYNTRA_HAL_DEVICE_DISCOVERY_MAX_DEVICES 128U
#define SYNTRA_HAL_DEVICE_DISCOVERY_MAX_EVENTS 64U

typedef enum
{
    SYNTRA_HAL_DISCOVERY_EVENT_ADDED = 0,
    SYNTRA_HAL_DISCOVERY_EVENT_REMOVED,
    SYNTRA_HAL_DISCOVERY_EVENT_RESET
} syntra_hal_discovery_event_type_t;

typedef struct
{
    unsigned int sequence;
    unsigned int device_id;
    syntra_hal_discovery_event_type_t type;

} syntra_hal_discovery_event_t;

typedef struct
{
    unsigned int total_events;
    unsigned int added_events;
    unsigned int removed_events;
    unsigned int reset_events;

} syntra_hal_discovery_stats_t;

typedef struct
{
    unsigned int invalid_events;
    unsigned int duplicate_sequences;
    unsigned int out_of_order_events;
    unsigned int sequence_gaps;

} syntra_hal_discovery_validation_report_t;

typedef struct
{
    syntra_hal_device_t *devices[
        SYNTRA_HAL_DEVICE_DISCOVERY_MAX_DEVICES
    ];

    syntra_hal_discovery_event_t events[
        SYNTRA_HAL_DEVICE_DISCOVERY_MAX_EVENTS
    ];

    unsigned int device_count;
    unsigned int discovery_count;

    unsigned int event_count;
    unsigned int event_sequence;

    syntra_hal_discovery_stats_t stats;
    syntra_hal_discovery_validation_report_t validation_report;

    unsigned int update_count;

    int initialized;
    int dirty;

} syntra_hal_device_discovery_t;

syntra_status_t hal_device_discovery_init(
    syntra_hal_device_discovery_t *discovery
);

syntra_status_t hal_device_discovery_reset(
    syntra_hal_device_discovery_t *discovery
);

syntra_status_t hal_device_discovery_add(
    syntra_hal_device_discovery_t *discovery,
    syntra_hal_device_t *device
);

syntra_status_t hal_device_discovery_remove(
    syntra_hal_device_discovery_t *discovery,
    unsigned int device_id
);

syntra_status_t hal_device_discovery_find(
    const syntra_hal_device_discovery_t *discovery,
    unsigned int device_id,
    syntra_hal_device_t **result
);

syntra_status_t hal_device_discovery_get_count(
    const syntra_hal_device_discovery_t *discovery,
    unsigned int *result
);

syntra_status_t hal_device_discovery_get_event_count(
    const syntra_hal_device_discovery_t *discovery,
    unsigned int *result
);

syntra_status_t hal_device_discovery_get_event(
    const syntra_hal_device_discovery_t *discovery,
    unsigned int index,
    syntra_hal_discovery_event_t *result
);

syntra_status_t hal_device_discovery_find_event(
    const syntra_hal_device_discovery_t *discovery,
    unsigned int device_id,
    unsigned int occurrence,
    syntra_hal_discovery_event_t *result
);

syntra_status_t hal_device_discovery_get_event_by_type(
    const syntra_hal_device_discovery_t *discovery,
    syntra_hal_discovery_event_type_t type,
    unsigned int occurrence,
    syntra_hal_discovery_event_t *result
);

syntra_status_t hal_device_discovery_get_stats(
    const syntra_hal_device_discovery_t *discovery,
    syntra_hal_discovery_stats_t *result
);

syntra_status_t hal_device_discovery_update_stats(
    syntra_hal_device_discovery_t *discovery
);

syntra_status_t hal_device_discovery_get_validation_report(
    const syntra_hal_device_discovery_t *discovery,
    syntra_hal_discovery_validation_report_t *result
);

syntra_status_t hal_device_discovery_validate_history(
    syntra_hal_device_discovery_t *discovery
);

syntra_status_t hal_device_discovery_clear_events(
    syntra_hal_device_discovery_t *discovery
);

syntra_status_t hal_device_discovery_validate(
    const syntra_hal_device_discovery_t *discovery
);

#endif
