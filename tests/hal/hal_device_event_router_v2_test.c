
#include "hal_device_event_router.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static syntra_hal_device_event_t create_event(
    unsigned int sequence,
    syntra_hal_device_event_type_t type
)
{
    syntra_hal_device_event_t event;

    memset(&event, 0, sizeof(event));

    event.sequence = sequence;
    event.device_id = 100U;
    event.type = type;
    event.previous_state = SYNTRA_HAL_DEVICE_OFFLINE;
    event.current_state = SYNTRA_HAL_DEVICE_ONLINE;

    return event;
}

static void test_multi_target_routing(void)
{
    syntra_hal_device_event_router_t router;

    syntra_hal_device_event_queue_t queue_a;
    syntra_hal_device_event_queue_t queue_b;
    syntra_hal_device_event_queue_t queue_c;

    syntra_hal_device_event_t event;
    syntra_hal_device_event_t received;

    syntra_hal_device_event_queue_t *target = NULL;

    unsigned int target_count = 0U;

    unsigned int total_routes = 0U;
    unsigned int successful_routes = 0U;
    unsigned int failed_routes = 0U;
    unsigned int unhandled_events = 0U;

    unsigned int total_deliveries = 0U;
    unsigned int successful_deliveries = 0U;
    unsigned int failed_deliveries = 0U;
    unsigned int partially_routed_events = 0U;

    unsigned int i;

    event = create_event(
        1U,
        SYNTRA_HAL_DEVICE_EVENT_ADDED
    );

    assert(
        hal_device_event_router_init(&router) ==
        SYNTRA_STATUS_OK
    );

    assert(
        hal_device_event_queue_init(&queue_a) ==
        SYNTRA_STATUS_OK
    );

    assert(
        hal_device_event_queue_init(&queue_b) ==
        SYNTRA_STATUS_OK
    );

    assert(
        hal_device_event_queue_init(&queue_c) ==
        SYNTRA_STATUS_OK
    );

    /* Register two targets for one event type. */

    assert(
        hal_device_event_router_add_target(
            &router,
            SYNTRA_HAL_DEVICE_EVENT_ADDED,
            &queue_a
        ) == SYNTRA_STATUS_OK
    );

    assert(
        hal_device_event_router_add_target(
            &router,
            SYNTRA_HAL_DEVICE_EVENT_ADDED,
            &queue_b
        ) == SYNTRA_STATUS_OK
    );

    assert(
        hal_device_event_router_get_target_count(
            &router,
            SYNTRA_HAL_DEVICE_EVENT_ADDED,
            &target_count
        ) == SYNTRA_STATUS_OK
    );

    assert(target_count == 2U);

    /* Duplicate target must be rejected. */

    assert(
        hal_device_event_router_add_target(
            &router,
            SYNTRA_HAL_DEVICE_EVENT_ADDED,
            &queue_a
        ) == SYNTRA_STATUS_ERROR
    );

    /* Verify target enumeration. */

    assert(
        hal_device_event_router_get_target(
            &router,
            SYNTRA_HAL_DEVICE_EVENT_ADDED,
            0U,
            &target
        ) == SYNTRA_STATUS_OK
    );

    assert(target == &queue_a);

    assert(
        hal_device_event_router_get_target(
            &router,
            SYNTRA_HAL_DEVICE_EVENT_ADDED,
            1U,
            &target
        ) == SYNTRA_STATUS_OK
    );

    assert(target == &queue_b);

    /* Fill queue_b to force one delivery failure. */

    for (i = 0U;
         i < SYNTRA_HAL_DEVICE_EVENT_QUEUE_CAPACITY;
         i++)
    {
        assert(
            hal_device_event_queue_enqueue(
                &queue_b,
                &event
            ) == SYNTRA_STATUS_OK
        );
    }

    /* One target succeeds and one target fails. */

    assert(
        hal_device_event_router_route(
            &router,
            &event
        ) == SYNTRA_STATUS_OK
    );

    assert(
        hal_device_event_queue_get_count(
            &queue_a,
            &target_count
        ) == SYNTRA_STATUS_OK
    );

    assert(target_count == 1U);

    assert(
        hal_device_event_router_get_delivery_statistics(
            &router,
            &total_deliveries,
            &successful_deliveries,
            &failed_deliveries,
            &partially_routed_events
        ) == SYNTRA_STATUS_OK
    );

    assert(total_deliveries == 2U);
    assert(successful_deliveries == 1U);
    assert(failed_deliveries == 1U);
    assert(partially_routed_events == 1U);

    assert(
        hal_device_event_router_validate(&router) ==
        SYNTRA_STATUS_OK
    );

    /* Remove the failed target. */

    assert(
        hal_device_event_router_remove_target(
            &router,
            SYNTRA_HAL_DEVICE_EVENT_ADDED,
            &queue_b
        ) == SYNTRA_STATUS_OK
    );

    assert(
        hal_device_event_router_get_target_count(
            &router,
            SYNTRA_HAL_DEVICE_EVENT_ADDED,
            &target_count
        ) == SYNTRA_STATUS_OK
    );

    assert(target_count == 1U);

    /*
     * Reuse the removed target's slot.
     * Historical statistics must remain valid.
     */

    assert(
        hal_device_event_router_add_target(
            &router,
            SYNTRA_HAL_DEVICE_EVENT_ADDED,
            &queue_c
        ) == SYNTRA_STATUS_OK
    );

    event.sequence = 2U;

    assert(
        hal_device_event_router_route(
            &router,
            &event
        ) == SYNTRA_STATUS_OK
    );

    assert(
        hal_device_event_router_validate(&router) ==
        SYNTRA_STATUS_OK
    );

    /* Verify all delivery statistics. */

    assert(
        hal_device_event_router_get_delivery_statistics(
            &router,
            &total_deliveries,
            &successful_deliveries,
            &failed_deliveries,
            &partially_routed_events
        ) == SYNTRA_STATUS_OK
    );

    assert(total_deliveries == 4U);
    assert(successful_deliveries == 3U);
    assert(failed_deliveries == 1U);
    assert(partially_routed_events == 1U);

    /* Verify event-level statistics. */

    assert(
        hal_device_event_router_get_statistics(
            &router,
            &total_routes,
            &successful_routes,
            &failed_routes,
            &unhandled_events
        ) == SYNTRA_STATUS_OK
    );

    assert(total_routes == 2U);
    assert(successful_routes == 2U);
    assert(failed_routes == 0U);
    assert(unhandled_events == 0U);

    /* Verify queue_a received both events. */

    assert(
        hal_device_event_queue_dequeue(
            &queue_a,
            &received
        ) == SYNTRA_STATUS_OK
    );

    assert(received.sequence == 1U);

    assert(
        hal_device_event_queue_dequeue(
            &queue_a,
            &received
        ) == SYNTRA_STATUS_OK
    );

    assert(received.sequence == 2U);

    /* Remove all targets using the V1-compatible API. */

    assert(
        hal_device_event_router_unregister(
            &router,
            SYNTRA_HAL_DEVICE_EVENT_ADDED
        ) == SYNTRA_STATUS_OK
    );

    assert(
        hal_device_event_router_validate(&router) ==
        SYNTRA_STATUS_OK
    );

    /* Routing without targets must be unhandled. */

    event.sequence = 3U;

    assert(
        hal_device_event_router_route(
            &router,
            &event
        ) == SYNTRA_STATUS_NOT_IMPLEMENTED
    );

    assert(
        hal_device_event_router_get_statistics(
            &router,
            &total_routes,
            &successful_routes,
            &failed_routes,
            &unhandled_events
        ) == SYNTRA_STATUS_OK
    );

    assert(total_routes == 3U);
    assert(successful_routes == 2U);
    assert(failed_routes == 0U);
    assert(unhandled_events == 1U);

    assert(
        hal_device_event_router_validate(&router) ==
        SYNTRA_STATUS_OK
    );

    /* Reset must clear current and historical statistics. */

    assert(
        hal_device_event_router_reset(&router) ==
        SYNTRA_STATUS_OK
    );

    assert(
        hal_device_event_router_validate(&router) ==
        SYNTRA_STATUS_OK
    );

    assert(router.historical_routed_count == 0U);
    assert(router.historical_failure_count == 0U);
    assert(router.total_deliveries == 0U);
    assert(router.total_routes == 0U);
}

int main(void)
{
    test_multi_target_routing();

    printf(
        "HAL Device Event Router V2: ALL TESTS PASSED\n"
    );

    return 0;
}
