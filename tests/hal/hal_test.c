
#include "../../kernel/hal/hal.h"

#include <assert.h>
#include <stdio.h>

int main(void)
{
    int initialized = 0;

    /* Initial state */

    assert(
        hal_is_initialized(&initialized)
        == SYNTRA_STATUS_OK
    );

    assert(initialized == 0);

    /* Initialization */

    assert(hal_init() == SYNTRA_STATUS_OK);

    assert(
        hal_is_initialized(&initialized)
        == SYNTRA_STATUS_OK
    );

    assert(initialized == 1);

    /* Repeated initialization */

    assert(hal_init() == SYNTRA_STATUS_OK);

    assert(
        hal_is_initialized(&initialized)
        == SYNTRA_STATUS_OK
    );

    assert(initialized == 1);

    /* Invalid argument */

    assert(
        hal_is_initialized(NULL)
        == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    /* Reset */

    assert(hal_reset() == SYNTRA_STATUS_OK);

    assert(
        hal_is_initialized(&initialized)
        == SYNTRA_STATUS_OK
    );

    assert(initialized == 0);

    /* Reinitialize after reset */

    assert(hal_init() == SYNTRA_STATUS_OK);

    assert(
        hal_is_initialized(&initialized)
        == SYNTRA_STATUS_OK
    );

    assert(initialized == 1);

    printf("HAL Core V1: all tests passed.\n");

    return 0;
}
