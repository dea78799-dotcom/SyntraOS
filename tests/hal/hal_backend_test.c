
#include "hal_backend.h"

#include <assert.h>
#include <stdio.h>

int main(void)
{
    syntra_hal_backend_t backend;
    int result = -1;

    /* Initialize software backend */

    assert(
        hal_backend_init(
            &backend,
            1U,
            "Software Backend",
            SYNTRA_HAL_BACKEND_SOFTWARE
        ) == SYNTRA_STATUS_OK
    );

    assert(backend.initialized == 1);
    assert(backend.backend_id == 1U);
    assert(backend.available == 1);

    /* Validate */

    assert(
        hal_backend_validate(&backend)
        == SYNTRA_STATUS_OK
    );

    /* Check availability */

    assert(
        hal_backend_is_available(
            &backend,
            &result
        ) == SYNTRA_STATUS_OK
    );

    assert(result == 1);

    /* Initialize hardware backend identifier */

    assert(
        hal_backend_init(
            &backend,
            2U,
            "ARM64 Backend",
            SYNTRA_HAL_BACKEND_ARM64
        ) == SYNTRA_STATUS_OK
    );

    assert(backend.initialized == 1);
    assert(backend.available == 0);

    assert(
        hal_backend_validate(&backend)
        == SYNTRA_STATUS_OK
    );

    assert(
        hal_backend_is_available(
            &backend,
            &result
        ) == SYNTRA_STATUS_OK
    );

    assert(result == 0);

    /* Invalid arguments */

    assert(
        hal_backend_init(
            &backend,
            0U,
            "Invalid",
            SYNTRA_HAL_BACKEND_SOFTWARE
        ) == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    assert(
        hal_backend_init(
            &backend,
            3U,
            NULL,
            SYNTRA_HAL_BACKEND_SOFTWARE
        ) == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    assert(
        hal_backend_init(
            &backend,
            3U,
            "Invalid",
            SYNTRA_HAL_BACKEND_UNKNOWN
        ) == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    assert(
        hal_backend_is_available(
            &backend,
            NULL
        ) == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    /* Reset */

    assert(
        hal_backend_reset(&backend)
        == SYNTRA_STATUS_OK
    );

    assert(backend.initialized == 0);
    assert(backend.available == 0);

    assert(
        hal_backend_validate(&backend)
        == SYNTRA_STATUS_ERROR
    );

    /* Null pointer checks */

    assert(
        hal_backend_reset(NULL)
        == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    assert(
        hal_backend_validate(NULL)
        == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    printf("HAL Hardware Backend V1 tests passed.\n");

    return 0;
}
