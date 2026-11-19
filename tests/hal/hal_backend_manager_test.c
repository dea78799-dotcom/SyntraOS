
#include "hal_backend_manager.h"

#include <assert.h>
#include <stdio.h>

int main(void)
{
    syntra_hal_backend_manager_t manager;
    syntra_hal_backend_t backend1;
    syntra_hal_backend_t backend2;
    syntra_hal_backend_t *found = NULL;
    unsigned int count = 0U;

    /* Initialize manager */

    assert(
        hal_backend_manager_init(&manager)
        == SYNTRA_STATUS_OK
    );

    assert(manager.initialized == 1);
    assert(manager.backend_count == 0U);

    /* Initialize backends */

    assert(
        hal_backend_init(
            &backend1,
            1U,
            "Software Backend",
            SYNTRA_HAL_BACKEND_SOFTWARE
        ) == SYNTRA_STATUS_OK
    );

    assert(
        hal_backend_init(
            &backend2,
            2U,
            "ARM64 Backend",
            SYNTRA_HAL_BACKEND_ARM64
        ) == SYNTRA_STATUS_OK
    );

    /* Register backends */

    assert(
        hal_backend_manager_register(
            &manager,
            &backend1
        ) == SYNTRA_STATUS_OK
    );

    assert(
        hal_backend_manager_register(
            &manager,
            &backend2
        ) == SYNTRA_STATUS_OK
    );

    assert(manager.backend_count == 2U);
    assert(manager.update_count == 2U);

    /* Find backend */

    assert(
        hal_backend_manager_find(
            &manager,
            1U,
            &found
        ) == SYNTRA_STATUS_OK
    );

    assert(found == &backend1);

    /* Get count */

    assert(
        hal_backend_manager_get_count(
            &manager,
            &count
        ) == SYNTRA_STATUS_OK
    );

    assert(count == 2U);

    /* Duplicate registration */

    assert(
        hal_backend_manager_register(
            &manager,
            &backend1
        ) == SYNTRA_STATUS_ERROR
    );

    assert(manager.backend_count == 2U);

    /* Unregister backend */

    assert(
        hal_backend_manager_unregister(
            &manager,
            1U
        ) == SYNTRA_STATUS_OK
    );

    assert(manager.backend_count == 1U);
    assert(manager.update_count == 3U);

    /* Find removed backend */

    found = NULL;

    assert(
        hal_backend_manager_find(
            &manager,
            1U,
            &found
        ) == SYNTRA_STATUS_ERROR
    );

    assert(found == NULL);

    /* Validate manager */

    assert(
        hal_backend_manager_validate(&manager)
        == SYNTRA_STATUS_OK
    );

    /* Invalid arguments */

    assert(
        hal_backend_manager_register(
            NULL,
            &backend1
        ) == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    assert(
        hal_backend_manager_unregister(
            &manager,
            0U
        ) == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    assert(
        hal_backend_manager_find(
            &manager,
            0U,
            &found
        ) == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    assert(
        hal_backend_manager_get_count(
            &manager,
            NULL
        ) == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    /* Reset manager */

    assert(
        hal_backend_manager_reset(&manager)
        == SYNTRA_STATUS_OK
    );

    assert(manager.initialized == 0);
    assert(manager.backend_count == 0U);

    assert(
        hal_backend_manager_validate(&manager)
        == SYNTRA_STATUS_ERROR
    );

    /* Null checks */

    assert(
        hal_backend_manager_init(NULL)
        == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    assert(
        hal_backend_manager_reset(NULL)
        == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    assert(
        hal_backend_manager_validate(NULL)
        == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    printf("HAL Backend Manager V1 tests passed.\n");

    return 0;
}
