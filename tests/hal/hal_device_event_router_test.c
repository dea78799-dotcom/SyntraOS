
#include "hal_device_event_router.h"

#include <assert.h>
#include <stddef.h>

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

int main(void)
{
    syntra_hal_device_event_router_t router;

    syntra_hal_device_event_queue_t added_queue;
    syntra_hal_device_event_queue_t state_queue;

    syntra_hal_device_event_queue_t *target_queue = NULL;

    syntra_hal_device_event_t event;
    syntra_hal_device_event_t received_event;

    unsigned int total;
    unsigned int successful;
    unsigned int failed;
    unsigned int unhandled;
    unsigned int queue_count;

    /* Initialization */

    assert(
        hal_device_event_router_init(&router) ==
        SYNTRA_STATUS_OK
    );

    assert(router.initialized == 1);
    assert(router.registered_routes == 0U);
    assert(router.total_routes == 0U);

    assert(
        hal_device_event_router_validate(&router) ==
        SYNTRA_STATUS_OK
    );

    assert(
        hal_device_event_queue_init(&added_queue) ==
        SYNTRA_STATUS_OK
    );

    assert(
        hal_device_event_queue_init(&state_queue) ==
        SYNTRA_STATUS_OK
    );

    /* Register routes */

    assert(
        hal_device_event_router_register(
            &router,
            SYNTRA_HAL_DEVICE_EVENT_ADDED,
            &added_queue
        ) == SYNTRA_STATUS_OK
    );

    assert(
        hal_device_event_router_register(
            &router,
            SYNTRA_HAL_DEVICE_EVENT_STATE_CHANGED,
            &state_queue
        ) == SYNTRA_STATUS_OK
    );

    assert(router.registered_routes == 2U);

    assert(
        hal_device_event_router_validate(&router) ==
        SYNTRA_STATUS_OK
    );

    /* Duplicate registration */

    assert(
        hal_device_event_router_register(
            &router,
            SYNTRA_HAL_DEVICE_EVENT_ADDED,
            &added_queue
        ) == SYNTRA_STATUS_ERROR
    );

    /* Get registered route */

    assert(
        hal_device_event_router_get_route(
            &router,
            SYNTRA_HAL_DEVICE_EVENT_ADDED,
            &target_queue
        ) == SYNTRA_STATUS_OK
    );

    assert(target_queue == &added_queue);

    /* Get unregistered route */

    target_queue = &added_queue;

    assert(
        hal_device_event_router_get_route(
            &router,
            SYNTRA_HAL_DEVICE_EVENT_RESET,
            &target_queue
        ) == SYNTRA_STATUS_NOT_IMPLEMENTED
    );

    assert(target_queue == NULL);

    /* Successful routing: ADDED */

    event = create_event(
        1U,
        100U,
        SYNTRA_HAL_DEVICE_EVENT_ADDED
    );

    assert(
        hal_device_event_router_route(
            &router,
            &event
        ) == SYNTRA_STATUS_OK
    );

    /* Successful routing: STATE_CHANGED */

    event = create_event(
        2U,
        101U,
        SYNTRA_HAL_DEVICE_EVENT_STATE_CHANGED
    );

    assert(
        hal_device_event_router_route(
            &router,
            &event
        ) == SYNTRA_STATUS_OK
    );

    /* Unhandled event */

    event = create_event(
        3U,
        102U,
        SYNTRA_HAL_DEVICE_EVENT_RESET
    );

    assert(
        hal_device_event_router_route(
            &router,
            &event
        ) == SYNTRA_STATUS_NOT_IMPLEMENTED
    );

    /* Verify queue contents */

    assert(
        hal_device_event_queue_get_count(
            &added_queue,
            &queue_count
        ) == SYNTRA_STATUS_OK
    );

    assert(queue_count == 1U);

    assert(
        hal_device_event_queue_dequeue(
            &added_queue,
            &received_event
        ) == SYNTRA_STATUS_OK
    );

    assert(received_event.sequence == 1U);
    assert(received_event.device_id == 100U);
    assert(
        received_event.type ==
        SYNTRA_HAL_DEVICE_EVENT_ADDED
    );

    assert(
        hal_device_event_queue_get_count(
            &state_queue,
            &queue_count
        ) == SYNTRA_STATUS_OK
    );

    assert(queue_count == 1U);

    assert(
        hal_device_event_queue_dequeue(
            &state_queue,
            &received_event
        ) == SYNTRA_STATUS_OK
    );

    assert(received_event.sequence == 2U);
    assert(
        received_event.type ==
        SYNTRA_HAL_DEVICE_EVENT_STATE_CHANGED
    );

    /* Statistics */

    assert(
        hal_device_event_router_get_statistics(
            &router,
            &total,
            &successful,
            &failed,
            &unhandled
        ) == SYNTRA_STATUS_OK
    );

    assert(total == 3U);
    assert(successful == 2U);
    assert(failed == 0U);
    assert(unhandled == 1U);

    assert(
        hal_device_event_router_validate(&router) ==
        SYNTRA_STATUS_OK
    );

    /* Unregister route */

    assert(
        hal_device_event_router_unregister(
            &router,
            SYNTRA_HAL_DEVICE_EVENT_ADDED
        ) == SYNTRA_STATUS_OK
    );

    assert(router.registered_routes == 1U);

    assert(
        hal_device_event_router_unregister(
            &router,
            SYNTRA_HAL_DEVICE_EVENT_ADDED
        ) == SYNTRA_STATUS_NOT_IMPLEMENTED
    );

    assert(
        hal_device_event_router_validate(&router) ==
        SYNTRA_STATUS_OK
    );

    /* Route event without registered route */

    event = create_event(
        4U,
        103U,
        SYNTRA_HAL_DEVICE_EVENT_ADDED
    );

    assert(
        hal_device_event_router_route(
            &router,
            &event
        ) == SYNTRA_STATUS_NOT_IMPLEMENTED
    );

    /* Register route again */

    assert(
        hal_device_event_router_register(
            &router,
            SYNTRA_HAL_DEVICE_EVENT_ADDED,
            &added_queue
        ) == SYNTRA_STATUS_OK
    );

    assert(router.registered_routes == 2U);

    /* Route after re-registration */

    event = create_event(
        5U,
        104U,
        SYNTRA_HAL_DEVICE_EVENT_ADDED
    );

    assert(
        hal_device_event_router_route(
            &router,
            &event
        ) == SYNTRA_STATUS_OK
    );

    assert(
        hal_device_event_queue_dequeue(
            &added_queue,
            &received_event
        ) == SYNTRA_STATUS_OK
    );

    assert(received_event.sequence == 5U);

    /* Updated statistics */

    assert(
        hal_device_event_router_get_statistics(
            &router,
            &total,
            &successful,
            &failed,
            &unhandled
        ) == SYNTRA_STATUS_OK
    );

    assert(total == 5U);
    assert(successful == 3U);
    assert(failed == 0U);
    assert(unhandled == 2U);

    assert(
        hal_device_event_router_validate(&router) ==
        SYNTRA_STATUS_OK
    );

    /* Invalid event type */

    event = create_event(
        6U,
        105U,
        (syntra_hal_device_event_type_t)99
    );

    assert(
        hal_device_event_router_route(
            &router,
            &event
        ) == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    /* Invalid event state */

    event = create_event(
        7U,
        106U,
        SYNTRA_HAL_DEVICE_EVENT_ADDED
    );

    event.current_state =
        (syntra_hal_device_state_t)99;

    assert(
        hal_device_event_router_route(
            &router,
            &event
        ) == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    /* Invalid event sequence */

    event = create_event(
        0U,
        107U,
        SYNTRA_HAL_DEVICE_EVENT_ADDED
    );

    assert(
        hal_device_event_router_route(
            &router,
            &event
        ) == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    /* Invalid arguments */

    assert(
        hal_device_event_router_init(NULL) ==
        SYNTRA_STATUS_INVALID_ARGUMENT
    );

    assert(
        hal_device_event_router_reset(NULL) ==
        SYNTRA_STATUS_INVALID_ARGUMENT
    );

    assert(
        hal_device_event_router_register(
            NULL,
            SYNTRA_HAL_DEVICE_EVENT_ADDED,
            &added_queue
        ) == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    assert(
        hal_device_event_router_register(
            &router,
            SYNTRA_HAL_DEVICE_EVENT_ADDED,
            NULL
        ) == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    assert(
        hal_device_event_router_register(
            &router,
            (syntra_hal_device_event_type_t)99,
            &added_queue
        ) == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    assert(
        hal_device_event_router_unregister(
            NULL,
            SYNTRA_HAL_DEVICE_EVENT_ADDED
        ) == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    assert(
        hal_device_event_router_route(
            NULL,
            &event
        ) == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    assert(
        hal_device_event_router_route(
            &router,
            NULL
        ) == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    assert(
        hal_device_event_router_get_route(
            NULL,
            SYNTRA_HAL_DEVICE_EVENT_ADDED,
            &target_queue
        ) == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    assert(
        hal_device_event_router_get_route(
            &router,
            SYNTRA_HAL_DEVICE_EVENT_ADDED,
            NULL
        ) == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    assert(
        hal_device_event_router_get_statistics(
            NULL,
            &total,
            &successful,
            &failed,
            &unhandled
        ) == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    assert(
        hal_device_event_router_get_statistics(
            &router,
            NULL,
            &successful,
            &failed,
            &unhandled
        ) == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    assert(
        hal_device_event_router_validate(NULL) ==
        SYNTRA_STATUS_INVALID_ARGUMENT
    );

    /* Reset */

    assert(
        hal_device_event_router_reset(&router) ==
        SYNTRA_STATUS_OK
    );

    assert(router.initialized == 1);
    assert(router.registered_routes == 0U);
    assert(router.total_routes == 0U);
    assert(router.successful_routes == 0U);
    assert(router.failed_routes == 0U);
    assert(router.unhandled_events == 0U);

    assert(
        hal_device_event_router_validate(&router) ==
        SYNTRA_STATUS_OK
    );

    return 0;
}
