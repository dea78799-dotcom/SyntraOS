
#ifndef SYNTRA_HAL_DEVICE_EVENT_QUEUE_H
#define SYNTRA_HAL_DEVICE_EVENT_QUEUE_H

#include "hal_device_event_manager.h"

#define SYNTRA_HAL_DEVICE_EVENT_QUEUE_CAPACITY 64U

typedef struct
{
    syntra_hal_device_event_t events[
        SYNTRA_HAL_DEVICE_EVENT_QUEUE_CAPACITY
    ];

    unsigned int head;
    unsigned int tail;
    unsigned int count;

    unsigned int enqueue_count;
    unsigned int dequeue_count;
    unsigned int dropped_count;

    int initialized;
    int dirty;

} syntra_hal_device_event_queue_t;

/* Lifecycle */

syntra_status_t hal_device_event_queue_init(
    syntra_hal_device_event_queue_t *queue
);

syntra_status_t hal_device_event_queue_reset(
    syntra_hal_device_event_queue_t *queue
);

/* Queue operations */

syntra_status_t hal_device_event_queue_enqueue(
    syntra_hal_device_event_queue_t *queue,
    const syntra_hal_device_event_t *event
);

syntra_status_t hal_device_event_queue_dequeue(
    syntra_hal_device_event_queue_t *queue,
    syntra_hal_device_event_t *event
);

/* Queue information */

syntra_status_t hal_device_event_queue_get_count(
    const syntra_hal_device_event_queue_t *queue,
    unsigned int *count
);

syntra_status_t hal_device_event_queue_is_empty(
    const syntra_hal_device_event_queue_t *queue,
    int *is_empty
);

syntra_status_t hal_device_event_queue_is_full(
    const syntra_hal_device_event_queue_t *queue,
    int *is_full
);

/* Queue management */

syntra_status_t hal_device_event_queue_clear(
    syntra_hal_device_event_queue_t *queue
);

syntra_status_t hal_device_event_queue_validate(
    const syntra_hal_device_event_queue_t *queue
);

#endif
