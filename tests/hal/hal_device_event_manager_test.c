
#include "hal_device_event_manager.h"

#include <assert.h>
#include <limits.h>
#include <stddef.h>

int main(void)
{
    syntra_hal_device_event_manager_t manager;

    syntra_hal_device_event_t event;
    syntra_hal_device_event_statistics_t statistics;
    syntra_hal_device_event_validation_report_t report;

    unsigned int count;
    unsigned int index;

    assert(
        hal_device_event_manager_init(&manager) ==
        SYNTRA_STATUS_OK
    );

    assert(manager.initialized == 1);
    assert(manager.event_count == 0U);

    assert(
        hal_device_event_manager_get_event_count(
            &manager,
            &count
        ) == SYNTRA_STATUS_OK
    );

    assert(count == 0U);

    assert(
        hal_device_event_manager_record(
            &manager,
            100U,
            SYNTRA_HAL_DEVICE_EVENT_ADDED,
            SYNTRA_HAL_DEVICE_OFFLINE,
            SYNTRA_HAL_DEVICE_ONLINE
        ) == SYNTRA_STATUS_OK
    );

    assert(
        hal_device_event_manager_record(
            &manager,
            100U,
            SYNTRA_HAL_DEVICE_EVENT_STATE_CHANGED,
            SYNTRA_HAL_DEVICE_OFFLINE,
            SYNTRA_HAL_DEVICE_ONLINE
        ) == SYNTRA_STATUS_OK
    );

    assert(
        hal_device_event_manager_record(
            &manager,
            100U,
            SYNTRA_HAL_DEVICE_EVENT_REMOVED,
            SYNTRA_HAL_DEVICE_ONLINE,
            SYNTRA_HAL_DEVICE_OFFLINE
        ) == SYNTRA_STATUS_OK
    );

    assert(
        hal_device_event_manager_get_event_count(
            &manager,
            &count
        ) == SYNTRA_STATUS_OK
    );

    assert(count == 3U);

    assert(
        hal_device_event_manager_get_event(
            &manager,
            0U,
            &event
        ) == SYNTRA_STATUS_OK
    );

    assert(event.sequence == 1U);
    assert(event.device_id == 100U);
    assert(event.type == SYNTRA_HAL_DEVICE_EVENT_ADDED);

    assert(
        hal_device_event_manager_get_statistics(
            &manager,
            &statistics
        ) == SYNTRA_STATUS_OK
    );

    assert(statistics.total_events == 3U);
    assert(statistics.added_events == 1U);
    assert(statistics.removed_events == 1U);
    assert(statistics.state_changed_events == 1U);
    assert(statistics.reset_events == 0U);

    assert(
        hal_device_event_manager_validate(&manager) ==
        SYNTRA_STATUS_OK
    );

    assert(
        hal_device_event_manager_validate_history(&manager) ==
        SYNTRA_STATUS_OK
    );

    assert(
        hal_device_event_manager_get_validation_report(
            &manager,
            &report
        ) == SYNTRA_STATUS_OK
    );

    assert(report.invalid_events == 0U);
    assert(report.duplicate_sequences == 0U);
    assert(report.out_of_order_sequences == 0U);
    assert(report.sequence_gaps == 0U);

    /* Test history capacity. */

    for (index = 0U; index < 70U; index++)
    {
        assert(
            hal_device_event_manager_record(
                &manager,
                200U + index,
                SYNTRA_HAL_DEVICE_EVENT_ADDED,
                SYNTRA_HAL_DEVICE_OFFLINE,
                SYNTRA_HAL_DEVICE_ONLINE
            ) == SYNTRA_STATUS_OK
        );
    }

    assert(
        hal_device_event_manager_get_event_count(
            &manager,
            &count
        ) == SYNTRA_STATUS_OK
    );

    assert(
        count == SYNTRA_HAL_DEVICE_EVENT_MANAGER_MAX_EVENTS
    );

    assert(
        hal_device_event_manager_get_event(
            &manager,
            0U,
            &event
        ) == SYNTRA_STATUS_OK
    );

    assert(event.sequence == 10U);

    assert(
        hal_device_event_manager_validate(&manager) ==
        SYNTRA_STATUS_OK
    );

    /* Test clearing history. */

    assert(
        hal_device_event_manager_clear_events(&manager) ==
        SYNTRA_STATUS_OK
    );

    assert(manager.event_count == 0U);
    assert(manager.event_sequence == 73U);

    assert(
        hal_device_event_manager_get_statistics(
            &manager,
            &statistics
        ) == SYNTRA_STATUS_OK
    );

    assert(statistics.total_events == 0U);

    /* Test invalid arguments. */

    assert(
        hal_device_event_manager_get_event_count(
            NULL,
            &count
        ) == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    assert(
        hal_device_event_manager_get_event_count(
            &manager,
            NULL
        ) == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    assert(
        hal_device_event_manager_record(
            &manager,
            1U,
            (syntra_hal_device_event_type_t)99,
            SYNTRA_HAL_DEVICE_OFFLINE,
            SYNTRA_HAL_DEVICE_ONLINE
        ) == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    assert(
        hal_device_event_manager_get_event(
            &manager,
            0U,
            &event
        ) == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    assert(
        hal_device_event_manager_reset(&manager) ==
        SYNTRA_STATUS_OK
    );

    assert(manager.initialized == 1);
    assert(manager.event_count == 0U);
    assert(manager.event_sequence == 0U);

    assert(
        hal_device_event_manager_validate(&manager) ==
        SYNTRA_STATUS_OK
    );

    assert(
        hal_device_event_manager_init(NULL) ==
        SYNTRA_STATUS_INVALID_ARGUMENT
    );

    return 0;
}
