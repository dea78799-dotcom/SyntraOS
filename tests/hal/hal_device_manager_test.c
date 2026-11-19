
#include "../../kernel/hal/hal_device_manager.h"

#include <assert.h>
#include <stdio.h>

int main(void)
{
    syntra_hal_registry_t registry;
    syntra_hal_device_manager_t manager;
    syntra_hal_device_manager_t info;

    syntra_hal_device_t timer;
    syntra_hal_device_t display;

    syntra_hal_device_t *found = NULL;

    syntra_hal_device_event_t event;
    syntra_hal_device_event_t history[
        SYNTRA_HAL_MAX_DEVICE_EVENTS
    ];

    unsigned int found_count = 0U;

    /* Initialize registry */

    assert(
        hal_registry_init(&registry)
        == SYNTRA_STATUS_OK
    );

    /* Initialize devices */

    assert(
        hal_device_init(
            &timer,
            1U,
            "SystemTimer",
            SYNTRA_HAL_DEVICE_TIMER
        ) == SYNTRA_STATUS_OK
    );

    assert(
        hal_device_init(
            &display,
            2U,
            "MainDisplay",
            SYNTRA_HAL_DEVICE_DISPLAY
        ) == SYNTRA_STATUS_OK
    );

    /* Register devices */

    assert(
        hal_registry_register(&registry, &timer)
        == SYNTRA_STATUS_OK
    );

    assert(
        hal_registry_register(&registry, &display)
        == SYNTRA_STATUS_OK
    );

    /* Initialize manager */

    assert(
        hal_device_manager_init(&manager, &registry)
        == SYNTRA_STATUS_OK
    );

    assert(manager.initialized == 1);
    assert(manager.total_devices == 2U);
    assert(manager.online_devices == 0U);
    assert(manager.offline_devices == 2U);
    assert(manager.error_devices == 0U);
    assert(manager.event_count == 0U);

    /* Validate manager */

    assert(
        hal_device_manager_validate(&manager)
        == SYNTRA_STATUS_OK
    );

    /* Start timer */

    assert(
        hal_device_manager_start(&manager, 1U)
        == SYNTRA_STATUS_OK
    );

    assert(manager.online_devices == 1U);
    assert(manager.event_count == 1U);

    /* Start display */

    assert(
        hal_device_manager_start(&manager, 2U)
        == SYNTRA_STATUS_OK
    );

    assert(manager.online_devices == 2U);
    assert(manager.event_count == 2U);

    /* Stop timer */

    assert(
        hal_device_manager_stop(&manager, 1U)
        == SYNTRA_STATUS_OK
    );

    assert(manager.offline_devices == 1U);
    assert(manager.event_count == 3U);

    /* Restart timer */

    assert(
        hal_device_manager_restart(&manager, 1U)
        == SYNTRA_STATUS_OK
    );

    assert(manager.online_devices == 2U);
    assert(manager.event_count == 4U);

    /* Find device */

    assert(
        hal_device_manager_get_device(
            &manager,
            1U,
            &found
        ) == SYNTRA_STATUS_OK
    );

    assert(found == &timer);

    /* Get manager information */

    assert(
        hal_device_manager_get_info(
            &manager,
            &info
        ) == SYNTRA_STATUS_OK
    );

    assert(info.total_devices == 2U);
    assert(info.online_devices == 2U);

    /* Read event */

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

    /* Find timer history */

    assert(
        hal_device_manager_find_events(
            &manager,
            1U,
            history,
            SYNTRA_HAL_MAX_DEVICE_EVENTS,
            &found_count
        ) == SYNTRA_STATUS_OK
    );

    assert(found_count == 3U);
    assert(history[0].type == SYNTRA_HAL_EVENT_STARTED);
    assert(history[1].type == SYNTRA_HAL_EVENT_STOPPED);
    assert(history[2].type == SYNTRA_HAL_EVENT_RESTARTED);

    /* Error state tracking */

    assert(
        hal_device_stop(&display)
        == SYNTRA_STATUS_OK
    );

    display.state = SYNTRA_HAL_DEVICE_ERROR;

    assert(
        hal_device_manager_refresh(&manager)
        == SYNTRA_STATUS_OK
    );

    assert(manager.online_devices == 1U);
    assert(manager.error_devices == 1U);

    assert(
        hal_device_manager_validate(&manager)
        == SYNTRA_STATUS_OK
    );

    /* Record error event */

    assert(
        hal_device_manager_record_event(
            &manager,
            2U,
            SYNTRA_HAL_EVENT_ERROR
        ) == SYNTRA_STATUS_OK
    );

    assert(manager.event_count == 5U);

    /* Event buffer capacity */

    assert(
        hal_device_manager_clear_events(&manager)
        == SYNTRA_STATUS_OK
    );

    for (unsigned int i = 0U;
         i < SYNTRA_HAL_MAX_DEVICE_EVENTS + 1U;
         i++)
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
        manager.event_count ==
        SYNTRA_HAL_MAX_DEVICE_EVENTS
    );

    assert(
        hal_device_manager_get_event(
            &manager,
            0U,
            &event
        ) == SYNTRA_STATUS_OK
    );

    assert(event.sequence == 2U);

    /* Insufficient history capacity */

    assert(
        hal_device_manager_find_events(
            &manager,
            1U,
            history,
            1U,
            &found_count
        ) == SYNTRA_STATUS_ERROR
    );

    assert(
        found_count ==
        SYNTRA_HAL_MAX_DEVICE_EVENTS
    );

    /* Missing device */

    assert(
        hal_device_manager_start(&manager, 99U)
        == SYNTRA_STATUS_ERROR
    );

    /* Invalid event */

    assert(
        hal_device_manager_record_event(
            &manager,
            1U,
            (syntra_hal_event_type_t)99
        ) == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    /* Invalid arguments */

    assert(
        hal_device_manager_init(NULL, &registry)
        == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    assert(
        hal_device_manager_init(&manager, NULL)
        == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    assert(
        hal_device_manager_get_event(
            &manager,
            0U,
            NULL
        ) == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    assert(
        hal_device_manager_find_events(
            &manager,
            1U,
            NULL,
            1U,
            &found_count
        ) == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    /* Reset manager */

    assert(
        hal_device_manager_reset(&manager)
        == SYNTRA_STATUS_OK
    );

    assert(manager.initialized == 0);
    assert(manager.event_count == 0U);

    printf("HAL Device Manager V3: all tests passed.\n");

    return 0;
}
