
#include "../../kernel/hal/hal_device.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    syntra_hal_device_t device;
    syntra_hal_device_state_t state;

    /* Initialize device */

    assert(
        hal_device_init(
            &device,
            1U,
            "TestTimer",
            SYNTRA_HAL_DEVICE_TIMER
        ) == SYNTRA_STATUS_OK
    );

    assert(device.initialized == 1);
    assert(device.device_id == 1U);
    assert(strcmp(device.name, "TestTimer") == 0);
    assert(device.type == SYNTRA_HAL_DEVICE_TIMER);
    assert(device.state == SYNTRA_HAL_DEVICE_OFFLINE);

    /* Change device state */

    assert(
        hal_device_set_state(
            &device,
            SYNTRA_HAL_DEVICE_ONLINE
        ) == SYNTRA_STATUS_OK
    );

    /* Read device state */

    assert(
        hal_device_get_state(&device, &state)
        == SYNTRA_STATUS_OK
    );

    assert(state == SYNTRA_HAL_DEVICE_ONLINE);

    /* Set error state */

    assert(
        hal_device_set_state(
            &device,
            SYNTRA_HAL_DEVICE_ERROR
        ) == SYNTRA_STATUS_OK
    );

    assert(
        hal_device_get_state(&device, &state)
        == SYNTRA_STATUS_OK
    );

    assert(state == SYNTRA_HAL_DEVICE_ERROR);

    /* Invalid arguments */

    assert(
        hal_device_init(
            NULL,
            1U,
            "Test",
            SYNTRA_HAL_DEVICE_TIMER
        ) == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    assert(
        hal_device_init(
            &device,
            1U,
            NULL,
            SYNTRA_HAL_DEVICE_TIMER
        ) == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    assert(
        hal_device_init(
            &device,
            1U,
            "",
            SYNTRA_HAL_DEVICE_TIMER
        ) == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    assert(
        hal_device_init(
            &device,
            1U,
            "Invalid",
            SYNTRA_HAL_DEVICE_UNKNOWN
        ) == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    assert(
        hal_device_get_state(&device, &state)
        == SYNTRA_STATUS_ERROR
    );

    assert(
        hal_device_get_state(NULL, &state)
        == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    assert(
        hal_device_get_state(&device, NULL)
        == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    printf("HAL Device Interface V1: all tests passed.\n");

    return 0;
}
