
#include "hal_device_event_dispatcher.h"

#include <assert.h>
#include <stddef.h>

static unsigned int callback_count;
static unsigned int callback_fail_count;

static syntra_hal_device_event_t create_event(
    unsigned int sequence,
    unsigned int device_id,
    syntra_hal_device_event_type_t type
)
{
    syntra_hal_device_event_t event = {0};

    event.sequence = sequence;
    event.device_id = device_id;
    event.type = type;
    event.previous_state = SYNTRA_HAL_DEVICE_OFFLINE;
    event.current_state = SYNTRA_HAL_DEVICE_ONLINE;

    return event;
}

static syntra_status_t successful_handler(
    const syntra_hal_device_event_t *event,
    void *context
)
{
    unsigned int *received = (unsigned int *)context;

    assert(event != NULL);

    callback_count++;

    if (received != NULL)
    {
        (*received)++;
    }

    return SYNTRA_STATUS_OK;
}

static syntra_status_t failing_handler(
    const syntra_hal_device_event_t *event,
    void *context
)
{
    (void)context;

    assert(event != NULL);

    callback_fail_count++;

    return SYNTRA_STATUS_ERROR;
}

int main(void)
{
    syntra_hal_device_event_dispatcher_t dispatcher;
    syntra_hal_device_event_queue_t queue;
    syntra_hal_device_event_t event;

    unsigned int received = 0U;
    unsigned int processed = 0U;

    unsigned int total;
    unsigned int successful;
    unsigned int failed;
    unsigned int unhandled;

    callback_count = 0U;
    callback_fail_count = 0U;

    assert(
        hal_device_event_dispatcher_init(&dispatcher) ==
        SYNTRA_STATUS_OK
    );

    assert(dispatcher.initialized == 1);

    assert(
        hal_device_event_dispatcher_validate(&dispatcher) ==
        SYNTRA_STATUS_OK
    );

    assert(
        hal_device_event_queue_init(&queue) ==
        SYNTRA_STATUS_OK
    );

    /* Handler registration */

    assert(
        hal_device_event_dispatcher_register(
            &dispatcher,
            SYNTRA_HAL_DEVICE_EVENT_ADDED,
            successful_handler,
            &received
        ) == SYNTRA_STATUS_OK
    );

    assert(
        hal_device_event_dispatcher_register(
            &dispatcher,
            SYNTRA_HAL_DEVICE_EVENT_ADDED,
            successful_handler,
            &received
        ) == SYNTRA_STATUS_ERROR
    );

    assert(
        hal_device_event_dispatcher_register(
            &dispatcher,
            SYNTRA_HAL_DEVICE_EVENT_REMOVED,
            failing_handler,
            NULL
        ) == SYNTRA_STATUS_OK
    );

    assert(
        hal_device_event_dispatcher_validate(&dispatcher) ==
        SYNTRA_STATUS_OK
    );

    /* Successful dispatch */

    event = create_event(
        1U,
        100U,
        SYNTRA_HAL_DEVICE_EVENT_ADDED
    );

    assert(
        hal_device_event_dispatcher_dispatch(
            &dispatcher,
            &event
        ) == SYNTRA_STATUS_OK
    );

    assert(callback_count == 1U);
    assert(received == 1U);

    /* Failed callback */

    event = create_event(
        2U,
        101U,
        SYNTRA_HAL_DEVICE_EVENT_REMOVED
    );

    assert(
        hal_device_event_dispatcher_dispatch(
            &dispatcher,
            &event
        ) == SYNTRA_STATUS_ERROR
    );

    assert(callback_fail_count == 1U);

    /* Unhandled event */

    event = create_event(
        3U,
        102U,
        SYNTRA_HAL_DEVICE_EVENT_RESET
    );

    assert(
        hal_device_event_dispatcher_dispatch(
            &dispatcher,
            &event
        ) == SYNTRA_STATUS_NOT_IMPLEMENTED
    );

    /* Statistics */

    assert(
        hal_device_event_dispatcher_get_statistics(
            &dispatcher,
            &total,
            &successful,
            &failed,
            &unhandled
        ) == SYNTRA_STATUS_OK
    );

    assert(total == 3U);
    assert(successful == 1U);
    assert(failed == 1U);
    assert(unhandled == 1U);

    assert(
        hal_device_event_dispatcher_validate(&dispatcher) ==
        SYNTRA_STATUS_OK
    );

    /* Unregister and preserve historical statistics */

    assert(
        hal_device_event_dispatcher_unregister(
            &dispatcher,
            SYNTRA_HAL_DEVICE_EVENT_ADDED
        ) == SYNTRA_STATUS_OK
    );

    assert(
        hal_device_event_dispatcher_unregister(
            &dispatcher,
            SYNTRA_HAL_DEVICE_EVENT_ADDED
        ) == SYNTRA_STATUS_ERROR
    );

    assert(
        hal_device_event_dispatcher_validate(&dispatcher) ==
        SYNTRA_STATUS_OK
    );

    /* Register again */

    assert(
        hal_device_event_dispatcher_register(
            &dispatcher,
            SYNTRA_HAL_DEVICE_EVENT_ADDED,
            successful_handler,
            &received
        ) == SYNTRA_STATUS_OK
    );

    event = create_event(
        4U,
        103U,
        SYNTRA_HAL_DEVICE_EVENT_ADDED
    );

    assert(
        hal_device_event_dispatcher_dispatch(
            &dispatcher,
            &event
        ) == SYNTRA_STATUS_OK
    );

    assert(received == 2U);

    /* Process one queue event */

    event = create_event(
        5U,
        104U,
        SYNTRA_HAL_DEVICE_EVENT_ADDED
    );

    assert(
        hal_device_event_queue_enqueue(
            &queue,
            &event
        ) == SYNTRA_STATUS_OK
    );

    assert(
        hal_device_event_dispatcher_process_next(
            &dispatcher,
            &queue
        ) == SYNTRA_STATUS_OK
    );

    assert(received == 3U);

    /* Process all queue events */

    event = create_event(
        6U,
        105U,
        SYNTRA_HAL_DEVICE_EVENT_ADDED
    );

    assert(
        hal_device_event_queue_enqueue(
            &queue,
            &event
        ) == SYNTRA_STATUS_OK
    );

    event = create_event(
        7U,
        106U,
        SYNTRA_HAL_DEVICE_EVENT_RESET
    );

    assert(
        hal_device_event_queue_enqueue(
            &queue,
            &event
        ) == SYNTRA_STATUS_OK
    );

    assert(
        hal_device_event_dispatcher_process_all(
            &dispatcher,
            &queue,
            &processed
        ) == SYNTRA_STATUS_NOT_IMPLEMENTED
    );

    assert(processed == 2U);

    assert(
        hal_device_event_queue_get_count(
            &queue,
            &processed
        ) == SYNTRA_STATUS_OK
    );

    assert(processed == 0U);

    assert(
        hal_device_event_dispatcher_validate(&dispatcher) ==
        SYNTRA_STATUS_OK
    );

    /* Empty queue */

    assert(
        hal_device_event_dispatcher_process_all(
            &dispatcher,
            &queue,
            &processed
        ) == SYNTRA_STATUS_OK
    );

    assert(processed == 0U);

    /* Invalid event */

    event = create_event(
        8U,
        107U,
        (syntra_hal_device_event_type_t)99
    );

    assert(
        hal_device_event_dispatcher_dispatch(
            &dispatcher,
            &event
        ) == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    event = create_event(
        9U,
        108U,
        SYNTRA_HAL_DEVICE_EVENT_ADDED
    );

    event.current_state =
        (syntra_hal_device_state_t)99;

    assert(
        hal_device_event_dispatcher_dispatch(
            &dispatcher,
            &event
        ) == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    event = create_event(
        0U,
        109U,
        SYNTRA_HAL_DEVICE_EVENT_ADDED
    );

    assert(
        hal_device_event_dispatcher_dispatch(
            &dispatcher,
            &event
        ) == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    /* Invalid arguments */

    assert(
        hal_device_event_dispatcher_init(NULL) ==
        SYNTRA_STATUS_INVALID_ARGUMENT
    );

    assert(
        hal_device_event_dispatcher_reset(NULL) ==
        SYNTRA_STATUS_INVALID_ARGUMENT
    );

    assert(
        hal_device_event_dispatcher_register(
            NULL,
            SYNTRA_HAL_DEVICE_EVENT_ADDED,
            successful_handler,
            NULL
        ) == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    assert(
        hal_device_event_dispatcher_register(
            &dispatcher,
            SYNTRA_HAL_DEVICE_EVENT_ADDED,
            NULL,
            NULL
        ) == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    assert(
        hal_device_event_dispatcher_register(
            &dispatcher,
            (syntra_hal_device_event_type_t)99,
            successful_handler,
            NULL
        ) == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    assert(
        hal_device_event_dispatcher_unregister(
            NULL,
            SYNTRA_HAL_DEVICE_EVENT_ADDED
        ) == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    assert(
        hal_device_event_dispatcher_dispatch(
            NULL,
            &event
        ) == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    assert(
        hal_device_event_dispatcher_dispatch(
            &dispatcher,
            NULL
        ) == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    assert(
        hal_device_event_dispatcher_process_next(
            NULL,
            &queue
        ) == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    assert(
        hal_device_event_dispatcher_process_next(
            &dispatcher,
            NULL
        ) == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    assert(
        hal_device_event_dispatcher_process_all(
            &dispatcher,
            &queue,
            NULL
        ) == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    assert(
        hal_device_event_dispatcher_get_statistics(
            NULL,
            &total,
            &successful,
            &failed,
            &unhandled
        ) == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    assert(
        hal_device_event_dispatcher_get_statistics(
            &dispatcher,
            NULL,
            &successful,
            &failed,
            &unhandled
        ) == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    assert(
        hal_device_event_dispatcher_validate(NULL) ==
        SYNTRA_STATUS_INVALID_ARGUMENT
    );

    /* Reset */

    assert(
        hal_device_event_dispatcher_reset(&dispatcher) ==
        SYNTRA_STATUS_OK
    );

    assert(dispatcher.initialized == 1);
    assert(dispatcher.total_dispatches == 0U);
    assert(dispatcher.successful_dispatches == 0U);
    assert(dispatcher.failed_dispatches == 0U);
    assert(dispatcher.unhandled_events == 0U);

    assert(
        hal_device_event_dispatcher_validate(&dispatcher) ==
        SYNTRA_STATUS_OK
    );

    return 0;
}
