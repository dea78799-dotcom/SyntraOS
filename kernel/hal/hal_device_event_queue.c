
#include "hal_device_event_queue.h"

#include <stddef.h>
#include <string.h>

/*
 * SyntraOS HAL Device Event Queue
 * Version: 1.0.0
 */

static int queue_is_valid(
    const syntra_hal_device_event_queue_t *queue
)
{
    if (queue == NULL)
    {
        return 0;
    }

    if (queue->initialized != 1)
    {
        return 0;
    }

    if (queue->head >= SYNTRA_HAL_DEVICE_EVENT_QUEUE_CAPACITY)
    {
        return 0;
    }

    if (queue->tail >= SYNTRA_HAL_DEVICE_EVENT_QUEUE_CAPACITY)
    {
        return 0;
    }

    if (queue->count > SYNTRA_HAL_DEVICE_EVENT_QUEUE_CAPACITY)
    {
        return 0;
    }

    return 1;
}

syntra_status_t hal_device_event_queue_init(
    syntra_hal_device_event_queue_t *queue
)
{
    if (queue == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    memset(queue, 0, sizeof(*queue));

    queue->initialized = 1;
    queue->dirty = 0;

    return SYNTRA_STATUS_OK;
}

syntra_status_t hal_device_event_queue_reset(
    syntra_hal_device_event_queue_t *queue
)
{
    if (!queue_is_valid(queue))
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    return hal_device_event_queue_init(queue);
}

syntra_status_t hal_device_event_queue_enqueue(
    syntra_hal_device_event_queue_t *queue,
    const syntra_hal_device_event_t *event
)
{
    if (!queue_is_valid(queue) || event == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if ((int)event->type < 0 ||
        event->type >= SYNTRA_HAL_DEVICE_EVENT_TYPE_COUNT)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (queue->count >= SYNTRA_HAL_DEVICE_EVENT_QUEUE_CAPACITY)
    {
        queue->dropped_count++;
        queue->dirty = 1;

        return SYNTRA_STATUS_ERROR;
    }

    queue->events[queue->tail] = *event;

    queue->tail =
        (queue->tail + 1U) %
        SYNTRA_HAL_DEVICE_EVENT_QUEUE_CAPACITY;

    queue->count++;
    queue->enqueue_count++;
    queue->dirty = 1;

    return SYNTRA_STATUS_OK;
}

syntra_status_t hal_device_event_queue_dequeue(
    syntra_hal_device_event_queue_t *queue,
    syntra_hal_device_event_t *event
)
{
    if (!queue_is_valid(queue) || event == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (queue->count == 0U)
    {
        return SYNTRA_STATUS_ERROR;
    }

    *event = queue->events[queue->head];

    memset(
        &queue->events[queue->head],
        0,
        sizeof(queue->events[queue->head])
    );

    queue->head =
        (queue->head + 1U) %
        SYNTRA_HAL_DEVICE_EVENT_QUEUE_CAPACITY;

    queue->count--;
    queue->dequeue_count++;
    queue->dirty = 1;

    return SYNTRA_STATUS_OK;
}

syntra_status_t hal_device_event_queue_get_count(
    const syntra_hal_device_event_queue_t *queue,
    unsigned int *count
)
{
    if (!queue_is_valid(queue) || count == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    *count = queue->count;

    return SYNTRA_STATUS_OK;
}

syntra_status_t hal_device_event_queue_is_empty(
    const syntra_hal_device_event_queue_t *queue,
    int *is_empty
)
{
    if (!queue_is_valid(queue) || is_empty == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    *is_empty = (queue->count == 0U);

    return SYNTRA_STATUS_OK;
}

syntra_status_t hal_device_event_queue_is_full(
    const syntra_hal_device_event_queue_t *queue,
    int *is_full
)
{
    if (!queue_is_valid(queue) || is_full == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    *is_full =
        (queue->count == SYNTRA_HAL_DEVICE_EVENT_QUEUE_CAPACITY);

    return SYNTRA_STATUS_OK;
}

syntra_status_t hal_device_event_queue_clear(
    syntra_hal_device_event_queue_t *queue
)
{
    if (!queue_is_valid(queue))
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    memset(
        queue->events,
        0,
        sizeof(queue->events)
    );

    queue->head = 0U;
    queue->tail = 0U;
    queue->count = 0U;

    queue->dirty = 1;

    return SYNTRA_STATUS_OK;
}

syntra_status_t hal_device_event_queue_validate(
    const syntra_hal_device_event_queue_t *queue
)
{
    unsigned int expected_tail;

    if (!queue_is_valid(queue))
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    expected_tail =
        (queue->head + queue->count) %
        SYNTRA_HAL_DEVICE_EVENT_QUEUE_CAPACITY;

    if (queue->tail != expected_tail)
    {
        return SYNTRA_STATUS_ERROR;
    }

    return SYNTRA_STATUS_OK;
}
