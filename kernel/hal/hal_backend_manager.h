
#ifndef SYNTRA_HAL_BACKEND_MANAGER_H
#define SYNTRA_HAL_BACKEND_MANAGER_H

#include "hal_backend.h"

#define SYNTRA_HAL_BACKEND_MANAGER_MAX_BACKENDS 32U
#define SYNTRA_HAL_BACKEND_MANAGER_MAX_EVENTS 64U

typedef enum
{
    SYNTRA_HAL_BACKEND_EVENT_REGISTERED = 0,
    SYNTRA_HAL_BACKEND_EVENT_UNREGISTERED,
    SYNTRA_HAL_BACKEND_EVENT_AVAILABLE,
    SYNTRA_HAL_BACKEND_EVENT_UNAVAILABLE,
    SYNTRA_HAL_BACKEND_EVENT_TYPE_COUNT
} syntra_hal_backend_event_type_t;

typedef struct
{
    unsigned int sequence;
    unsigned int backend_id;
    syntra_hal_backend_event_type_t type;

} syntra_hal_backend_event_t;

typedef struct
{
    syntra_hal_backend_t *backends[
        SYNTRA_HAL_BACKEND_MANAGER_MAX_BACKENDS
    ];

    syntra_hal_backend_event_t events[
        SYNTRA_HAL_BACKEND_MANAGER_MAX_EVENTS
    ];

    unsigned int backend_count;
    unsigned int available_backends;
    unsigned int unavailable_backends;

    unsigned int update_count;

    unsigned int event_count;
    unsigned int event_sequence;

    unsigned int event_type_counts[
        SYNTRA_HAL_BACKEND_EVENT_TYPE_COUNT
    ];

    int initialized;
    int dirty;

} syntra_hal_backend_manager_t;

syntra_status_t hal_backend_manager_init(
    syntra_hal_backend_manager_t *manager
);

syntra_status_t hal_backend_manager_reset(
    syntra_hal_backend_manager_t *manager
);

syntra_status_t hal_backend_manager_register(
    syntra_hal_backend_manager_t *manager,
    syntra_hal_backend_t *backend
);

syntra_status_t hal_backend_manager_unregister(
    syntra_hal_backend_manager_t *manager,
    unsigned int backend_id
);

syntra_status_t hal_backend_manager_find(
    const syntra_hal_backend_manager_t *manager,
    unsigned int backend_id,
    syntra_hal_backend_t **result
);

syntra_status_t hal_backend_manager_get_count(
    const syntra_hal_backend_manager_t *manager,
    unsigned int *result
);

syntra_status_t hal_backend_manager_update(
    syntra_hal_backend_manager_t *manager
);

syntra_status_t hal_backend_manager_validate(
    const syntra_hal_backend_manager_t *manager
);

syntra_status_t hal_backend_manager_get_event_count(
    const syntra_hal_backend_manager_t *manager,
    unsigned int *result
);

syntra_status_t hal_backend_manager_get_event(
    const syntra_hal_backend_manager_t *manager,
    unsigned int index,
    syntra_hal_backend_event_t *result
);

syntra_status_t hal_backend_manager_clear_events(
    syntra_hal_backend_manager_t *manager
);

syntra_status_t hal_backend_manager_filter_events(
    const syntra_hal_backend_manager_t *manager,
    unsigned int backend_id,
    syntra_hal_backend_event_type_t type,
    syntra_hal_backend_event_t *results,
    unsigned int capacity,
    unsigned int *result_count
);

syntra_status_t hal_backend_manager_get_event_statistics(
    const syntra_hal_backend_manager_t *manager,
    unsigned int *results,
    unsigned int capacity
);

syntra_status_t hal_backend_manager_validate_history(
    const syntra_hal_backend_manager_t *manager
);

#endif
