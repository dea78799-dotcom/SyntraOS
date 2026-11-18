
#include "../../kernel/hal/hal_lifecycle.h"

#include <assert.h>
#include <stdio.h>

int main(void)
{
    syntra_hal_device_t device;

    int online = 0;
    int offline = 0;

    /* Initialize device */

    assert(
        hal_device_init(
            &device,
            1U,
            "TestDevice",
            SYNTRA_HAL_DEVICE_TIMER
        ) == SYNTRA_STATUS_OK
    );

    /* Initial state */

    assert(
        hal_device_is_offline(&device, &offline)
        == SYNTRA_STATUS_OK
    );

    assert(offline == 1);

    /* Start device */

    assert(
        hal_device_start(&device)
        == SYNTRA_STATUS_OK
    );

    assert(
        hal_device_is_online(&device, &online)
        == SYNTRA_STATUS_OK
    );

    assert(online == 1);

    /* Start an already online device */

    assert(
        hal_device_start(&device)
        == SYNTRA_STATUS_OK
    );

    /* Stop device */

    assert(
        hal_device_stop(&device)
        == SYNTRA_STATUS_OK
    );

    assert(
        hal_device_is_offline(&device, &offline)
        == SYNTRA_STATUS_OK
    );

    assert(offline == 1);

    /* Stop an already offline device */

    assert(
        hal_device_stop(&device)
        == SYNTRA_STATUS_OK
    );

    /* Restart device */

    assert(
        hal_device_restart(&device)
        == SYNTRA_STATUS_OK
    );

    assert(
        hal_device_is_online(&device, &online)
        == SYNTRA_STATUS_OK
    );

    assert(online == 1);

    /* Reject error state */

    assert(
        hal_device_set_state(
            &device,
            SYNTRA_HAL_DEVICE_ERROR
        ) == SYNTRA_STATUS_OK
    );

    assert(
        hal_device_start(&device)
        == SYNTRA_STATUS_ERROR
    );

    assert(
        hal_device_stop(&device)
        == SYNTRA_STATUS_ERROR
    );

    assert(
        hal_device_restart(&device)
        == SYNTRA_STATUS_ERROR
    );

    /* Invalid arguments */

    assert(
        hal_device_start(NULL)
        == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    assert(
        hal_device_stop(NULL)
        == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    assert(
        hal_device_restart(NULL)
        == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    assert(
        hal_device_is_online(NULL, &online)
        == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    assert(
        hal_device_is_online(&device, NULL)
        == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    assert(
        hal_device_is_offline(NULL, &offline)
        == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    assert(
        hal_device_is_offline(&device, NULL)
        == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    printf("HAL Lifecycle V1: all tests passed.\n");

    return 0;
}
