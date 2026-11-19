
#ifndef SYNTRA_HAL_DEVICE_EVENT_MANAGER_H
#define SYNTRA_HAL_DEVICE_EVENT_MANAGER_H

#include "hal_device.h"

#define SYNTRA_HAL_DEVICE_EVENT_MANAGER_MAX_EVENTS 64U

typedef enum
{
    SYNTRA_HAL_DEVICE_EVENT_ADDED = 0,
    SYNTRA_HAL_DEVICE_EVENT_REMOVED,
    SYNTRA_HAL_DEVICE_EVENT_STATE_CHANGED,
    SYNTRA_HAL_DEVICE_EVENT_RESET,
    SYNTRA_HAL_DEVICE_EVENT_TYPE_COUNT
} syntra_hal_device_event_type_t;

typedef struct
{
    unsigned int sequence;
    unsigned int device_id;

    syntra_hal_device_event_type_t type;

    syntra_hal_device_state_t previous_state;
    syntra_hal_device_state_t current_state;

} syntra_hal_device_event_t;

typedef struct
{
    unsigned int total_events;
    unsigned int added_events;
    unsigned int removed_events;
    unsigned int state_changed_events;
    unsigned int reset_events;

} syntra_hal_device_event_statistics_t;

typedef struct
{
    unsigned int invalid_events;
    unsigned int duplicate_sequences;
    unsigned int out_of_order_sequences;
    unsigned int sequence_gaps;

} syntra_hal_device_event_validation_report_t;

typedef struct
{
    syntra_hal_device_event_t events[
        SYNTRA_HAL_DEVICE_EVENT_MANAGER_MAX_EVENTS
    ];

    unsigned int event_count;
    unsigned int event_sequence;

    syntra_hal_device_event_statistics_t statistics;

    syntra_hal_device_event_validation_report_t validation_report;

    unsigned int update_count;

    int initialized;
    int dirty;

} syntra_hal_device_event_manager_t;

/* Lifecycle */

syntra_status_t hal_device_event_manager_init(
    syntra_hal_device_event_manager_t *manager
);

syntra_status_t hal_device_event_manager_reset(
    syntra_hal_device_event_manager_t *manager
);

/* Event recording */

syntra_status_t hal_device_event_manager_record(
    syntra_hal_device_event_manager_t *manager,
    unsigned int device_id,
    syntra_hal_device_event_type_t type,
    syntra_hal_device_state_t previous_state,
    syntra_hal_device_state_t current_state
);

/* Event history */

syntra_status_t hal_device_event_manager_get_event_count(
    const syntra_hal_device_event_manager_t *manager,
    unsigned int *count
);

syntra_status_t hal_device_event_manager_get_event(
    const syntra_hal_device_event_manager_t *manager,
    unsigned int index,
    syntra_hal_device_event_t *event
);

syntra_status_t hal_device_event_manager_clear_events(
    syntra_hal_device_event_manager_t *manager
);

/* Statistics */

syntra_status_t hal_device_event_manager_update_statistics(
    syntra_hal_device_event_manager_t *manager
);

syntra_status_t hal_device_event_manager_get_statistics(
    const syntra_hal_device_event_manager_t *manager,
    syntra_hal_device_event_statistics_t *statistics
);

/* Validation */

syntra_status_t hal_device_event_manager_validate(
    const syntra_hal_device_event_manager_t *manager
);

syntra_status_t hal_device_event_manager_validate_history(
    syntra_hal_device_event_manager_t *manager
);

syntra_status_t hal_device_event_manager_get_validation_report(
    const syntra_hal_device_event_manager_t *manager,
    syntra_hal_device_event_validation_report_t *report
);

#endif
