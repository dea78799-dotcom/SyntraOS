
#include "hal_platform.h"

#include <assert.h>
#include <stdio.h>

int main(void)
{
    syntra_hal_platform_t platform;

    syntra_hal_architecture_t architecture =
        SYNTRA_HAL_ARCH_UNKNOWN;

    syntra_hal_platform_type_t type =
        SYNTRA_HAL_PLATFORM_UNKNOWN;

    /* Initialization */

    assert(
        hal_platform_init(
            &platform,
            "SyntraOS ARM64",
            SYNTRA_HAL_ARCH_ARM64,
            SYNTRA_HAL_PLATFORM_GENERIC,
            8U,
            4096U
        ) == SYNTRA_STATUS_OK
    );

    /* Validation */

    assert(
        hal_platform_validate(&platform)
        == SYNTRA_STATUS_OK
    );

    assert(platform.initialized == 1);
    assert(platform.cpu_count == 8U);
    assert(platform.memory_size_mb == 4096U);

    /* Get architecture */

    assert(
        hal_platform_get_architecture(
            &platform,
            &architecture
        ) == SYNTRA_STATUS_OK
    );

    assert(architecture == SYNTRA_HAL_ARCH_ARM64);

    /* Get platform type */

    assert(
        hal_platform_get_type(
            &platform,
            &type
        ) == SYNTRA_STATUS_OK
    );

    assert(type == SYNTRA_HAL_PLATFORM_GENERIC);

    /* Invalid initialization */

    assert(
        hal_platform_init(
            &platform,
            "Invalid",
            SYNTRA_HAL_ARCH_UNKNOWN,
            SYNTRA_HAL_PLATFORM_GENERIC,
            4U,
            2048U
        ) == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    assert(
        hal_platform_init(
            &platform,
            "Invalid",
            SYNTRA_HAL_ARCH_ARM64,
            SYNTRA_HAL_PLATFORM_UNKNOWN,
            4U,
            2048U
        ) == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    assert(
        hal_platform_init(
            &platform,
            "Invalid",
            SYNTRA_HAL_ARCH_ARM64,
            SYNTRA_HAL_PLATFORM_GENERIC,
            0U,
            2048U
        ) == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    assert(
        hal_platform_init(
            &platform,
            "Invalid",
            SYNTRA_HAL_ARCH_ARM64,
            SYNTRA_HAL_PLATFORM_GENERIC,
            4U,
            0U
        ) == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    assert(
        hal_platform_init(
            &platform,
            NULL,
            SYNTRA_HAL_ARCH_ARM64,
            SYNTRA_HAL_PLATFORM_GENERIC,
            4U,
            2048U
        ) == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    /* Invalid arguments */

    assert(
        hal_platform_get_architecture(
            &platform,
            NULL
        ) == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    assert(
        hal_platform_get_type(
            &platform,
            NULL
        ) == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    /* Reset */

    assert(
        hal_platform_reset(&platform)
        == SYNTRA_STATUS_OK
    );

    assert(platform.initialized == 0);

    assert(
        hal_platform_validate(&platform)
        == SYNTRA_STATUS_ERROR
    );

    /* Null pointer checks */

    assert(
        hal_platform_reset(NULL)
        == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    assert(
        hal_platform_validate(NULL)
        == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    printf("HAL Platform Interface V1 tests passed.\n");

    return 0;
}
