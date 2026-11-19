
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

static void fill_queue(
    syntra_hal_device_event_queue_t *queue,
    const syntra_hal_device_event_t *event
)
{
    unsigned int i;

    for (i = 0U;
         i < SYNTRA_HAL_DEVICE_EVENT_QUEUE_CAPACITY;
         i++)
    {
        assert(
            hal_device_event_queue_enqueue(
                queue,
                event
            ) == SYNTRA_STATUS_OK
        );
    }
}

static void test_per_type_statistics(void)
{
    syntra_hal_device_event_router_t router;

    syntra_hal_device_event_queue_t queue_added_a;
    syntra_hal_device_event_queue_t queue_added_b;
    syntra_hal_device_event_queue_t queue_state;
    syntra_hal_device_event_queue_t queue_reset;

    syntra_hal_device_event_t event;

    syntra_hal_device_event_type_stats_t statistics;

    assert(
        hal_device_event_router_init(&router) ==
        SYNTRA_STATUS_OK
    );

    assert(
        hal_device_event_queue_init(&queue_added_a) ==
        SYNTRA_STATUS_OK
    );

    assert(
        hal_device_event_queue_init(&queue_added_b) ==
        SYNTRA_STATUS_OK
    );

    assert(
        hal_device_event_queue_init(&queue_state) ==
        SYNTRA_STATUS_OK
    );

    assert(
        hal_device_event_queue_init(&queue_reset) ==
        SYNTRA_STATUS_OK
    );

    /*
     * ADDED:
     * One target succeeds and one target fails.
     */

    assert(
        hal_device_event_router_add_target(
            &router,
            SYNTRA_HAL_DEVICE_EVENT_ADDED,
            &queue_added_a
        ) == SYNTRA_STATUS_OK
    );

    assert(
        hal_device_event_router_add_target(
            &router,
            SYNTRA_HAL_DEVICE_EVENT_ADDED,
            &queue_added_b
        ) == SYNTRA_STATUS_OK
    );

    event = create_event(
        1U,
        SYNTRA_HAL_DEVICE_EVENT_ADDED
    );

    fill_queue(&queue_added_b, &event);

    assert(
        hal_device_event_router_route(
            &router,
            &event
        ) == SYNTRA_STATUS_OK
    );

    assert(
        hal_device_event_router_get_type_statistics(
            &router,
            SYNTRA_HAL_DEVICE_EVENT_ADDED,
            &statistics
        ) == SYNTRA_STATUS_OK
    );

    assert(statistics.total_routes == 1U);
    assert(statistics.successful_routes == 1U);
    assert(statistics.failed_routes == 0U);
    assert(statistics.unhandled_events == 0U);
    assert(statistics.partially_routed_events == 1U);

    assert(statistics.total_deliveries == 2U);
    assert(statistics.successful_deliveries == 1U);
    assert(statistics.failed_deliveries == 1U);

    assert(
        statistics.last_route_status ==
        SYNTRA_STATUS_OK
    );

    /*
     * REMOVED:
     * No registered target.
     */

    event = create_event(
        2U,
        SYNTRA_HAL_DEVICE_EVENT_REMOVED
    );

    assert(
        hal_device_event_router_route(
            &router,
            &event
        ) == SYNTRA_STATUS_NOT_IMPLEMENTED
    );

    assert(
        hal_device_event_router_get_type_statistics(
            &router,
            SYNTRA_HAL_DEVICE_EVENT_REMOVED,
            &statistics
        ) == SYNTRA_STATUS_OK
    );

    assert(statistics.total_routes == 1U);
    assert(statistics.successful_routes == 0U);
    assert(statistics.failed_routes == 0U);
    assert(statistics.unhandled_events == 1U);
    assert(statistics.total_deliveries == 0U);

    assert(
        statistics.last_route_status ==
        SYNTRA_STATUS_NOT_IMPLEMENTED
    );

    /*
     * STATE_CHANGED:
     * Normal successful delivery.
     */

    assert(
        hal_device_event_router_add_target(
            &router,
            SYNTRA_HAL_DEVICE_EVENT_STATE_CHANGED,
            &queue_state
        ) == SYNTRA_STATUS_OK
    );

    event = create_event(
        3U,
        SYNTRA_HAL_DEVICE_EVENT_STATE_CHANGED
    );

    assert(
        hal_device_event_router_route(
            &router,
            &event
        ) == SYNTRA_STATUS_OK
    );

    assert(
        hal_device_event_router_get_type_statistics(
            &router,
            SYNTRA_HAL_DEVICE_EVENT_STATE_CHANGED,
            &statistics
        ) == SYNTRA_STATUS_OK
    );

    assert(statistics.total_routes == 1U);
    assert(statistics.successful_routes == 1U);
    assert(statistics.failed_routes == 0U);
    assert(statistics.unhandled_events == 0U);
    assert(statistics.total_deliveries == 1U);
    assert(statistics.successful_deliveries == 1U);
    assert(statistics.failed_deliveries == 0U);

    assert(
        statistics.last_route_status ==
        SYNTRA_STATUS_OK
    );

    /*
     * RESET:
     * All registered targets fail.
     */

    assert(
        hal_device_event_router_add_target(
            &router,
            SYNTRA_HAL_DEVICE_EVENT_RESET,
            &queue_reset
        ) == SYNTRA_STATUS_OK
    );

    event = create_event(
        4U,
        SYNTRA_HAL_DEVICE_EVENT_RESET
    );

    fill_queue(&queue_reset, &event);

    assert(
        hal_device_event_router_route(
            &router,
            &event
        ) == SYNTRA_STATUS_ERROR
    );

    assert(
        hal_device_event_router_get_type_statistics(
            &router,
            SYNTRA_HAL_DEVICE_EVENT_RESET,
            &statistics
        ) == SYNTRA_STATUS_OK
    );

    assert(statistics.total_routes == 1U);
    assert(statistics.successful_routes == 0U);
    assert(statistics.failed_routes == 1U);
    assert(statistics.unhandled_events == 0U);
    assert(statistics.total_deliveries == 1U);
    assert(statistics.successful_deliveries == 0U);
    assert(statistics.failed_deliveries == 1U);

    assert(
        statistics.last_route_status ==
        SYNTRA_STATUS_ERROR
    );

    /*
     * Global validation.
     */

    assert(
        hal_device_event_router_validate(&router) ==
        SYNTRA_STATUS_OK
    );

    /*
     * Invalid type must be rejected.
     */

    assert(
        hal_device_event_router_get_type_statistics(
            &router,
            SYNTRA_HAL_DEVICE_EVENT_TYPE_COUNT,
            &statistics
        ) == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    /*
     * Invalid output pointer must be rejected.
     */

    assert(
        hal_device_event_router_get_type_statistics(
            &router,
            SYNTRA_HAL_DEVICE_EVENT_ADDED,
            NULL
        ) == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    /*
     * Reset clears V3 statistics.
     */

    assert(
        hal_device_event_router_reset(&router) ==
        SYNTRA_STATUS_OK
    );

    assert(
        hal_device_event_router_get_type_statistics(
            &router,
            SYNTRA_HAL_DEVICE_EVENT_ADDED,
            &statistics
        ) == SYNTRA_STATUS_OK
    );

    assert(statistics.total_routes == 0U);
    assert(statistics.total_deliveries == 0U);
    assert(statistics.update_count == 0U);

    assert(
        statistics.last_route_status ==
        SYNTRA_STATUS_NOT_IMPLEMENTED
    );

    assert(
        hal_device_event_router_validate(&router) ==
        SYNTRA_STATUS_OK
    );
}

int main(void)
{
    test_per_type_statistics();

    printf(
        "HAL Device Event Router V3: ALL TESTS PASSED\n"
    );

    return 0;
}
