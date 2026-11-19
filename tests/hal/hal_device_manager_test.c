
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
    assert(manager.dirty == 0);
    assert(manager.update_count == 1U);

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
    assert(manager.offline_devices == 1U);
    assert(manager.update_count == 2U);

    /* Start display */

    assert(
        hal_device_manager_start(&manager, 2U)
        == SYNTRA_STATUS_OK
    );

    assert(manager.online_devices == 2U);
    assert(manager.offline_devices == 0U);

    /* Stop timer */

    assert(
        hal_device_manager_stop(&manager, 1U)
        == SYNTRA_STATUS_OK
    );

    assert(manager.online_devices == 1U);
    assert(manager.offline_devices == 1U);

    /* Restart timer */

    assert(
        hal_device_manager_restart(&manager, 1U)
        == SYNTRA_STATUS_OK
    );

    assert(manager.online_devices == 2U);
    assert(manager.offline_devices == 0U);

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
    assert(info.offline_devices == 0U);

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
    assert(manager.offline_devices == 0U);
    assert(manager.error_devices == 1U);

    assert(
        hal_device_manager_validate(&manager)
        == SYNTRA_STATUS_OK
    );

    /* Missing device */

    assert(
        hal_device_manager_start(&manager, 99U)
        == SYNTRA_STATUS_ERROR
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
        hal_device_manager_start(NULL, 1U)
        == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    assert(
        hal_device_manager_stop(NULL, 1U)
        == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    assert(
        hal_device_manager_restart(NULL, 1U)
        == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    assert(
        hal_device_manager_get_device(
            &manager,
            1U,
            NULL
        ) == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    assert(
        hal_device_manager_get_info(
            &manager,
            NULL
        ) == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    /* Reset manager */

    assert(
        hal_device_manager_reset(&manager)
        == SYNTRA_STATUS_OK
    );

    assert(manager.initialized == 0);

    printf("HAL Device Manager V2: all tests passed.\n");

    return 0;
}
