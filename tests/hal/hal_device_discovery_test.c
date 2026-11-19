
#include "hal_device_discovery.h"

#include <assert.h>
#include <stdio.h>

int main(void)
{
    syntra_hal_device_discovery_t discovery;
    syntra_hal_device_t device1;
    syntra_hal_device_t device2;
    syntra_hal_device_t *found = NULL;

    unsigned int count = 0U;

    /* Initialize devices */
    assert(
        hal_device_init(
            &device1,
            1U,
            "Device One",
            SYNTRA_HAL_DEVICE_TIMER
        ) == SYNTRA_STATUS_OK
    );

    assert(
        hal_device_init(
            &device2,
            2U,
            "Device Two",
            SYNTRA_HAL_DEVICE_TIMER
        ) == SYNTRA_STATUS_OK
    );

    /* Initialize discovery manager */
    assert(
        hal_device_discovery_init(&discovery) ==
        SYNTRA_STATUS_OK
    );

    assert(
        hal_device_discovery_validate(&discovery) ==
        SYNTRA_STATUS_OK
    );

    /* Add devices */
    assert(
        hal_device_discovery_add(&discovery, &device1) ==
        SYNTRA_STATUS_OK
    );

    assert(
        hal_device_discovery_add(&discovery, &device2) ==
        SYNTRA_STATUS_OK
    );

    /* Check count */
    assert(
        hal_device_discovery_get_count(&discovery, &count) ==
        SYNTRA_STATUS_OK
    );

    assert(count == 2U);
    assert(discovery.discovery_count == 2U);
    assert(discovery.dirty == 1);

    /* Find device */
    assert(
        hal_device_discovery_find(
            &discovery,
            1U,
            &found
        ) == SYNTRA_STATUS_OK
    );

    assert(found == &device1);

    /* Reject duplicate device ID */
    assert(
        hal_device_discovery_add(&discovery, &device1) ==
        SYNTRA_STATUS_ERROR
    );

    /* Reject missing device */
    assert(
        hal_device_discovery_find(
            &discovery,
            99U,
            &found
        ) == SYNTRA_STATUS_ERROR
    );

    assert(found == NULL);

    /* Remove device */
    assert(
        hal_device_discovery_remove(&discovery, 1U) ==
        SYNTRA_STATUS_OK
    );

    assert(discovery.device_count == 1U);

    assert(
        hal_device_discovery_find(
            &discovery,
            2U,
            &found
        ) == SYNTRA_STATUS_OK
    );

    assert(found == &device2);

    /* Validate after removal */
    assert(
        hal_device_discovery_validate(&discovery) ==
        SYNTRA_STATUS_OK
    );

    /* Reject removing a missing device */
    assert(
        hal_device_discovery_remove(&discovery, 99U) ==
        SYNTRA_STATUS_ERROR
    );

    /* Reset */
    assert(
        hal_device_discovery_reset(&discovery) ==
        SYNTRA_STATUS_OK
    );

    assert(discovery.device_count == 0U);
    assert(discovery.discovery_count == 0U);

    assert(
        hal_device_discovery_validate(&discovery) ==
        SYNTRA_STATUS_OK
    );

    /* Invalid arguments */
    assert(
        hal_device_discovery_init(NULL) ==
        SYNTRA_STATUS_ERROR
    );

    assert(
        hal_device_discovery_add(&discovery, NULL) ==
        SYNTRA_STATUS_ERROR
    );

    assert(
        hal_device_discovery_get_count(&discovery, NULL) ==
        SYNTRA_STATUS_ERROR
    );

    assert(
        hal_device_discovery_find(
            &discovery,
            1U,
            NULL
        ) == SYNTRA_STATUS_ERROR
    );

    assert(
        hal_device_discovery_validate(NULL) ==
        SYNTRA_STATUS_ERROR
    );

    assert(
        hal_device_discovery_remove(NULL, 1U) ==
        SYNTRA_STATUS_ERROR
    );

    puts("HAL Device Discovery V1 tests passed.");

    return 0;
}
