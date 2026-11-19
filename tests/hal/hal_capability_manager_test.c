
#include "hal_capability_manager.h"

#include <assert.h>
#include <stdio.h>

int main(void)
{
    syntra_hal_capability_manager_t manager;
    unsigned int count = 0U;
    int result = 0;

    /* Initialization */

    assert(
        hal_capability_manager_init(&manager)
        == SYNTRA_STATUS_OK
    );

    assert(
        hal_capability_manager_validate(&manager)
        == SYNTRA_STATUS_OK
    );

    /* Register capabilities */

    assert(
        hal_capability_manager_register(
            &manager,
            1U,
            100U,
            "Memory"
        ) == SYNTRA_STATUS_OK
    );

    assert(
        hal_capability_manager_register(
            &manager,
            2U,
            100U,
            "Interrupt"
        ) == SYNTRA_STATUS_OK
    );

    assert(
        hal_capability_manager_register(
            &manager,
            3U,
            200U,
            "DMA"
        ) == SYNTRA_STATUS_OK
    );

    /* Count */

    assert(
        hal_capability_manager_get_count(
            &manager,
            &count
        ) == SYNTRA_STATUS_OK
    );

    assert(count == 3U);

    /* Capability lookup */

    assert(
        hal_capability_manager_has(
            &manager,
            100U,
            1U,
            &result
        ) == SYNTRA_STATUS_OK
    );

    assert(result == 1);

    assert(
        hal_capability_manager_has(
            &manager,
            200U,
            1U,
            &result
        ) == SYNTRA_STATUS_OK
    );

    assert(result == 0);

    assert(
        hal_capability_manager_has(
            &manager,
            200U,
            3U,
            &result
        ) == SYNTRA_STATUS_OK
    );

    assert(result == 1);

    /* Duplicate registration */

    assert(
        hal_capability_manager_register(
            &manager,
            1U,
            300U,
            "Duplicate"
        ) == SYNTRA_STATUS_ERROR
    );

    /* Unregister */

    assert(
        hal_capability_manager_unregister(
            &manager,
            2U
        ) == SYNTRA_STATUS_OK
    );

    assert(
        hal_capability_manager_get_count(
            &manager,
            &count
        ) == SYNTRA_STATUS_OK
    );

    assert(count == 2U);

    assert(
        hal_capability_manager_has(
            &manager,
            100U,
            2U,
            &result
        ) == SYNTRA_STATUS_OK
    );

    assert(result == 0);

    /* Missing capability */

    assert(
        hal_capability_manager_unregister(
            &manager,
            99U
        ) == SYNTRA_STATUS_ERROR
    );

    /* Invalid arguments */

    assert(
        hal_capability_manager_register(
            &manager,
            0U,
            100U,
            "Invalid"
        ) == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    assert(
        hal_capability_manager_register(
            &manager,
            4U,
            0U,
            "Invalid"
        ) == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    assert(
        hal_capability_manager_register(
            &manager,
            4U,
            100U,
            NULL
        ) == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    assert(
        hal_capability_manager_get_count(
            &manager,
            NULL
        ) == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    assert(
        hal_capability_manager_has(
            &manager,
            100U,
            1U,
            NULL
        ) == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    /* Validation */

    assert(
        hal_capability_manager_validate(&manager)
        == SYNTRA_STATUS_OK
    );

    /* Update counter */

    assert(manager.update_count == 4U);

    /* Reset */

    assert(
        hal_capability_manager_reset(&manager)
        == SYNTRA_STATUS_OK
    );

    assert(manager.initialized == 0);
    assert(manager.capability_count == 0U);

    assert(
        hal_capability_manager_validate(&manager)
        == SYNTRA_STATUS_ERROR
    );

    printf("HAL Capability Manager V1 tests passed.\n");

    return 0;
}
