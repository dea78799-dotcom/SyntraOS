
#include "hal_device_manager.h"

#include <assert.h>
#include <stddef.h>

int main(void)
{
    syntra_hal_registry_t registry;
    syntra_hal_device_manager_t manager;
    syntra_hal_device_manager_t info;

    syntra_hal_device_t device1;
    syntra_hal_device_t device2;
    syntra_hal_device_t *found_device = NULL;

    syntra_hal_device_event_t event;
    syntra_hal_device_event_t events[16];

    syntra_hal_event_statistics_t statistics;

    unsigned int count = 0U;
    unsigned int found = 0U;
    unsigned int i;

    /* Initialize devices */

    assert(
        hal_device_init(
            &device1,
            1U,
            "Timer",
            SYNTRA_HAL_DEVICE_TIMER
        ) == SYNTRA_STATUS_OK
    );

    assert(
        hal_device_init(
            &device2,
            2U,
            "Generic",
            SYNTRA_HAL_DEVICE_TIMER
        ) == SYNTRA_STATUS_OK
    );

    /* Initialize registry */

    assert(
        hal_registry_init(&registry)
        == SYNTRA_STATUS_OK
    );

    assert(
        hal_registry_register(&registry, &device1)
        == SYNTRA_STATUS_OK
    );

    assert(
        hal_registry_register(&registry, &device2)
        == SYNTRA_STATUS_OK
    );

    assert(registry.device_count == 2U);

    /* Initialize manager */

    assert(
        hal_device_manager_init(
            &manager,
            &registry
        ) == SYNTRA_STATUS_OK
    );

    assert(manager.initialized == 1);
    assert(manager.registry == &registry);
    assert(manager.total_devices == 2U);

    /* Validate and refresh */

    assert(
        hal_device_manager_validate(&manager)
        == SYNTRA_STATUS_OK
    );

    assert(
        hal_device_manager_refresh(&manager)
        == SYNTRA_STATUS_OK
    );

    assert(manager.dirty == 0);

    /* Get device */

    assert(
        hal_device_manager_get_device(
            &manager,
            1U,
            &found_device
        ) == SYNTRA_STATUS_OK
    );

    assert(found_device == &device1);

    /* Get manager information */

    assert(
        hal_device_manager_get_info(
            &manager,
            &info
        ) == SYNTRA_STATUS_OK
    );

    assert(info.total_devices == 2U);

    /* Record events */

    assert(
        hal_device_manager_record_event(
            &manager,
            1U,
            SYNTRA_HAL_EVENT_STARTED
        ) == SYNTRA_STATUS_OK
    );

    assert(
        hal_device_manager_record_event(
            &manager,
            1U,
            SYNTRA_HAL_EVENT_STOPPED
        ) == SYNTRA_STATUS_OK
    );

    assert(
        hal_device_manager_record_event(
            &manager,
            2U,
            SYNTRA_HAL_EVENT_RESTARTED
        ) == SYNTRA_STATUS_OK
    );

    assert(
        hal_device_manager_record_event(
            &manager,
            2U,
            SYNTRA_HAL_EVENT_ERROR
        ) == SYNTRA_STATUS_OK
    );

    assert(manager.event_count == 4U);

    /* Check event */

    assert(
        hal_device_manager_get_event(
            &manager,
            0U,
            &event
        ) == SYNTRA_STATUS_OK
    );

    assert(event.device_id == 1U);
    assert(event.type == SYNTRA_HAL_EVENT_STARTED);
    assert(event.sequence == 1U);

    /* Find events by device */

    assert(
        hal_device_manager_find_events(
            &manager,
            1U,
            events,
            16U,
            &found
        ) == SYNTRA_STATUS_OK
    );

    assert(found == 2U);

    /* Event statistics */

    assert(
        hal_device_manager_get_event_statistics(
            &manager,
            &statistics
        ) == SYNTRA_STATUS_OK
    );

    assert(statistics.started == 1U);
    assert(statistics.stopped == 1U);
    assert(statistics.restarted == 1U);
    assert(statistics.errors == 1U);
    assert(statistics.total == 4U);

    /* Find events by type */

    assert(
        hal_device_manager_find_events_by_type(
            &manager,
            SYNTRA_HAL_EVENT_STARTED,
            events,
            16U,
            &found
        ) == SYNTRA_STATUS_OK
    );

    assert(found == 1U);
    assert(events[0].device_id == 1U);

    /* Find events by sequence */

    assert(
        hal_device_manager_find_events_by_sequence(
            &manager,
            2U,
            3U,
            events,
            16U,
            &found
        ) == SYNTRA_STATUS_OK
    );

    assert(found == 2U);
    assert(events[0].sequence == 2U);
    assert(events[1].sequence == 3U);

    /* Device event statistics */

    assert(
        hal_device_manager_get_device_event_statistics(
            &manager,
            1U,
            &statistics
        ) == SYNTRA_STATUS_OK
    );

    assert(statistics.started == 1U);
    assert(statistics.stopped == 1U);
    assert(statistics.total == 2U);

    /* Clear events */

    assert(
        hal_device_manager_clear_events(&manager)
        == SYNTRA_STATUS_OK
    );

    assert(manager.event_count == 0U);

    /* Test event history capacity */

    for (i = 0U; i < 70U; i++)
    {
        assert(
            hal_device_manager_record_event(
                &manager,
                1U,
                SYNTRA_HAL_EVENT_STARTED
            ) == SYNTRA_STATUS_OK
        );
    }

    assert(
        manager.event_count
        == SYNTRA_HAL_MAX_DEVICE_EVENTS
    );

    assert(
        hal_device_manager_get_event(
            &manager,
            0U,
            &event
        ) == SYNTRA_STATUS_OK
    );

    /* Six oldest events are discarded. */
    assert(event.sequence == 7U);

    /* Invalid arguments */

    assert(
        hal_device_manager_init(NULL, &registry)
        == SYNTRA_STATUS_ERROR
    );

    assert(
        hal_device_manager_init(&manager, NULL)
        == SYNTRA_STATUS_ERROR
    );

    assert(
        hal_device_manager_reset(NULL)
        == SYNTRA_STATUS_ERROR
    );

    assert(
        hal_device_manager_validate(NULL)
        == SYNTRA_STATUS_ERROR
    );

    assert(
        hal_device_manager_refresh(NULL)
        == SYNTRA_STATUS_ERROR
    );

    assert(
        hal_device_manager_get_device(
            &manager,
            1U,
            NULL
        ) == SYNTRA_STATUS_ERROR
    );

    assert(
        hal_device_manager_get_event(
            &manager,
            0U,
            NULL
        ) == SYNTRA_STATUS_ERROR
    );

    assert(
        hal_device_manager_get_event(
            &manager,
            manager.event_count,
            &event
        ) == SYNTRA_STATUS_ERROR
    );

    assert(
        hal_device_manager_get_event_statistics(
            &manager,
            NULL
        ) == SYNTRA_STATUS_ERROR
    );

    assert(
        hal_device_manager_clear_events(NULL)
        == SYNTRA_STATUS_ERROR
    );

    return 0;
}
