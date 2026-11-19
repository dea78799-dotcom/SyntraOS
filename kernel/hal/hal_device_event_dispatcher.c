
#include "hal_device_event_dispatcher.h"

#include <limits.h>
#include <stddef.h>
#include <string.h>

/*
 * SyntraOS HAL Device Event Dispatcher
 * Version: 1.0.0
 */

static int event_type_is_valid(
    syntra_hal_device_event_type_t type
)
{
    return (int)type >=
               (int)SYNTRA_HAL_DEVICE_EVENT_ADDED &&
           (int)type <
               (int)SYNTRA_HAL_DEVICE_EVENT_TYPE_COUNT;
}

static int device_state_is_valid(
    syntra_hal_device_state_t state
)
{
    return (int)state >=
               (int)SYNTRA_HAL_DEVICE_OFFLINE &&
           (int)state <=
               (int)SYNTRA_HAL_DEVICE_ERROR;
}

static void update_counter(unsigned int *counter)
{
    if (*counter < UINT_MAX)
    {
        (*counter)++;
    }
}

static int dispatcher_is_valid(
    const syntra_hal_device_event_dispatcher_t *dispatcher
)
{
    unsigned int index;

    if (dispatcher == NULL || dispatcher->initialized != 1)
    {
        return 0;
    }

    for (index = 0U;
         index < SYNTRA_HAL_DEVICE_EVENT_DISPATCHER_MAX_HANDLERS;
         index++)
    {
        const syntra_hal_device_event_handler_entry_t *entry =
            &dispatcher->handlers[index];

        if (entry->registered != 0 &&
            entry->registered != 1)
        {
            return 0;
        }

        if (entry->registered == 1 && entry->handler == NULL)
        {
            return 0;
        }

        if (entry->registered == 0 &&
            (entry->handler != NULL || entry->context != NULL))
        {
            return 0;
        }

        if (entry->failure_count > entry->dispatch_count)
        {
            return 0;
        }
    }

    return 1;
}

static syntra_status_t validate_event(
    const syntra_hal_device_event_t *event
)
{
    if (event == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (event->sequence == 0U ||
        !event_type_is_valid(event->type) ||
        !device_state_is_valid(event->previous_state) ||
        !device_state_is_valid(event->current_state))
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    return SYNTRA_STATUS_OK;
}

syntra_status_t hal_device_event_dispatcher_init(
    syntra_hal_device_event_dispatcher_t *dispatcher
)
{
    if (dispatcher == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    memset(dispatcher, 0, sizeof(*dispatcher));

    dispatcher->initialized = 1;
    dispatcher->dirty = 0;

    return SYNTRA_STATUS_OK;
}

syntra_status_t hal_device_event_dispatcher_reset(
    syntra_hal_device_event_dispatcher_t *dispatcher
)
{
    if (!dispatcher_is_valid(dispatcher))
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    return hal_device_event_dispatcher_init(dispatcher);
}

syntra_status_t hal_device_event_dispatcher_register(
    syntra_hal_device_event_dispatcher_t *dispatcher,
    syntra_hal_device_event_type_t type,
    syntra_hal_device_event_handler_t handler,
    void *context
)
{
    syntra_hal_device_event_handler_entry_t *entry;

    if (!dispatcher_is_valid(dispatcher) ||
        !event_type_is_valid(type) ||
        handler == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    entry = &dispatcher->handlers[(unsigned int)type];

    if (entry->registered == 1)
    {
        return SYNTRA_STATUS_ERROR;
    }

    entry->handler = handler;
    entry->context = context;
    entry->registered = 1;

    update_counter(&dispatcher->update_count);
    dispatcher->dirty = 1;

    return SYNTRA_STATUS_OK;
}

syntra_status_t hal_device_event_dispatcher_unregister(
    syntra_hal_device_event_dispatcher_t *dispatcher,
    syntra_hal_device_event_type_t type
)
{
    syntra_hal_device_event_handler_entry_t *entry;

    if (!dispatcher_is_valid(dispatcher) ||
        !event_type_is_valid(type))
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    entry = &dispatcher->handlers[(unsigned int)type];

    if (entry->registered != 1)
    {
        return SYNTRA_STATUS_ERROR;
    }

    /*
     * Preserve dispatch_count and failure_count.
     * They represent historical statistics for this event type.
     */

    entry->handler = NULL;
    entry->context = NULL;
    entry->registered = 0;

    update_counter(&dispatcher->update_count);
    dispatcher->dirty = 1;

    return SYNTRA_STATUS_OK;
}

syntra_status_t hal_device_event_dispatcher_dispatch(
    syntra_hal_device_event_dispatcher_t *dispatcher,
    const syntra_hal_device_event_t *event
)
{
    syntra_hal_device_event_handler_entry_t *entry;
    syntra_status_t status;

    if (!dispatcher_is_valid(dispatcher))
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    status = validate_event(event);

    if (status != SYNTRA_STATUS_OK)
    {
        return status;
    }

    entry = &dispatcher->handlers[(unsigned int)event->type];

    update_counter(&dispatcher->total_dispatches);

    if (entry->registered != 1)
    {
        update_counter(&dispatcher->unhandled_events);
        update_counter(&dispatcher->update_count);

        dispatcher->dirty = 1;

        return SYNTRA_STATUS_NOT_IMPLEMENTED;
    }

    status = entry->handler(event, entry->context);

    update_counter(&entry->dispatch_count);

    if (status == SYNTRA_STATUS_OK)
    {
        update_counter(&dispatcher->successful_dispatches);
    }
    else
    {
        update_counter(&dispatcher->failed_dispatches);
        update_counter(&entry->failure_count);
    }

    update_counter(&dispatcher->update_count);
    dispatcher->dirty = 1;

    return status;
}

syntra_status_t hal_device_event_dispatcher_process_next(
    syntra_hal_device_event_dispatcher_t *dispatcher,
    syntra_hal_device_event_queue_t *queue
)
{
    syntra_hal_device_event_t event;
    syntra_status_t status;

    if (!dispatcher_is_valid(dispatcher) || queue == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    status = hal_device_event_queue_dequeue(
        queue,
        &event
    );

    if (status != SYNTRA_STATUS_OK)
    {
        return status;
    }

    return hal_device_event_dispatcher_dispatch(
        dispatcher,
        &event
    );
}

syntra_status_t hal_device_event_dispatcher_process_all(
    syntra_hal_device_event_dispatcher_t *dispatcher,
    syntra_hal_device_event_queue_t *queue,
    unsigned int *processed
)
{
    unsigned int count = 0U;
    unsigned int remaining;
    syntra_status_t status;
    syntra_status_t first_error = SYNTRA_STATUS_OK;

    if (!dispatcher_is_valid(dispatcher) ||
        queue == NULL ||
        processed == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    *processed = 0U;

    for (;;)
    {
        status = hal_device_event_queue_get_count(
            queue,
            &remaining
        );

        if (status != SYNTRA_STATUS_OK)
        {
            *processed = count;
            return status;
        }

        if (remaining == 0U)
        {
            *processed = count;
            return first_error;
        }

        status = hal_device_event_dispatcher_process_next(
            dispatcher,
            queue
        );

        count++;

        if (status != SYNTRA_STATUS_OK &&
            first_error == SYNTRA_STATUS_OK)
        {
            first_error = status;
        }
    }
}

syntra_status_t hal_device_event_dispatcher_get_statistics(
    const syntra_hal_device_event_dispatcher_t *dispatcher,
    unsigned int *total_dispatches,
    unsigned int *successful_dispatches,
    unsigned int *failed_dispatches,
    unsigned int *unhandled_events
)
{
    if (!dispatcher_is_valid(dispatcher) ||
        total_dispatches == NULL ||
        successful_dispatches == NULL ||
        failed_dispatches == NULL ||
        unhandled_events == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    *total_dispatches = dispatcher->total_dispatches;
    *successful_dispatches = dispatcher->successful_dispatches;
    *failed_dispatches = dispatcher->failed_dispatches;
    *unhandled_events = dispatcher->unhandled_events;

    return SYNTRA_STATUS_OK;
}

syntra_status_t hal_device_event_dispatcher_validate(
    const syntra_hal_device_event_dispatcher_t *dispatcher
)
{
    unsigned int index;

    unsigned long long handler_dispatches = 0ULL;
    unsigned long long handler_failures = 0ULL;

    unsigned long long expected_total;
    unsigned long long expected_success;
    unsigned long long expected_failed;
    unsigned long long expected_unhandled;

    if (!dispatcher_is_valid(dispatcher))
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    for (index = 0U;
         index < SYNTRA_HAL_DEVICE_EVENT_DISPATCHER_MAX_HANDLERS;
         index++)
    {
        const syntra_hal_device_event_handler_entry_t *entry =
            &dispatcher->handlers[index];

        handler_dispatches += entry->dispatch_count;
        handler_failures += entry->failure_count;
    }

    expected_failed = handler_failures;

    if (expected_failed > UINT_MAX)
    {
        expected_failed = UINT_MAX;
    }

    if (expected_failed != dispatcher->failed_dispatches)
    {
        return SYNTRA_STATUS_ERROR;
    }

    expected_success = handler_dispatches - handler_failures;

    if (expected_success > UINT_MAX)
    {
        expected_success = UINT_MAX;
    }

    if (expected_success != dispatcher->successful_dispatches)
    {
        return SYNTRA_STATUS_ERROR;
    }

    expected_unhandled = dispatcher->unhandled_events;

    expected_total =
        (unsigned long long)dispatcher->successful_dispatches +
        dispatcher->failed_dispatches +
        expected_unhandled;

    if (expected_total > UINT_MAX)
    {
        expected_total = UINT_MAX;
    }

    if (expected_total != dispatcher->total_dispatches)
    {
        return SYNTRA_STATUS_ERROR;
    }

    return SYNTRA_STATUS_OK;
}
