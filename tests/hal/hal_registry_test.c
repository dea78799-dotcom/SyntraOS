
#include "../../kernel/hal/hal_registry.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    syntra_hal_registry_t registry;

    syntra_hal_device_t timer;
    syntra_hal_device_t display;

    syntra_hal_device_t *found = NULL;

    unsigned int count = 0U;

    /* Initialize registry */

    assert(
        hal_registry_init(&registry)
        == SYNTRA_STATUS_OK
    );

    assert(registry.initialized == 1);
    assert(registry.device_count == 0U);

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

    assert(
        hal_registry_get_count(&registry, &count)
        == SYNTRA_STATUS_OK
    );

    assert(count == 2U);

    /* Find device by ID */

    assert(
        hal_registry_find_by_id(
            &registry,
            1U,
            &found
        ) == SYNTRA_STATUS_OK
    );

    assert(found == &timer);

    /* Find device by name */

    found = NULL;

    assert(
        hal_registry_find_by_name(
            &registry,
            "MainDisplay",
            &found
        ) == SYNTRA_STATUS_OK
    );

    assert(found == &display);

    /* Reject duplicate ID */

    syntra_hal_device_t duplicate_id;

    assert(
        hal_device_init(
            &duplicate_id,
            1U,
            "AnotherTimer",
            SYNTRA_HAL_DEVICE_TIMER
        ) == SYNTRA_STATUS_OK
    );

    assert(
        hal_registry_register(
            &registry,
            &duplicate_id
        ) == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    /* Reject duplicate name */

    syntra_hal_device_t duplicate_name;

    assert(
        hal_device_init(
            &duplicate_name,
            3U,
            "SystemTimer",
            SYNTRA_HAL_DEVICE_TIMER
        ) == SYNTRA_STATUS_OK
    );

    assert(
        hal_registry_register(
            &registry,
            &duplicate_name
        ) == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    /* Unregister device */

    assert(
        hal_registry_unregister(&registry, 1U)
        == SYNTRA_STATUS_OK
    );

    assert(
        hal_registry_get_count(&registry, &count)
        == SYNTRA_STATUS_OK
    );

    assert(count == 1U);

    found = NULL;

    assert(
        hal_registry_find_by_id(
            &registry,
            1U,
            &found
        ) == SYNTRA_STATUS_ERROR
    );

    /* Unregister missing device */

    assert(
        hal_registry_unregister(&registry, 99U)
        == SYNTRA_STATUS_ERROR
    );

    /* Invalid arguments */

    assert(
        hal_registry_register(NULL, &timer)
        == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    assert(
        hal_registry_register(&registry, NULL)
        == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    assert(
        hal_registry_find_by_id(
            &registry,
            2U,
            NULL
        ) == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    assert(
        hal_registry_find_by_name(
            &registry,
            NULL,
            &found
        ) == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    assert(
        hal_registry_get_count(&registry, NULL)
        == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    /* Reset registry */

    assert(
        hal_registry_reset(&registry)
        == SYNTRA_STATUS_OK
    );

    assert(registry.initialized == 1);
    assert(registry.device_count == 0U);

    printf("HAL Registry V1: all tests passed.\n");

    return 0;
}
