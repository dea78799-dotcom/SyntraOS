
#include "../../kernel/hal/hal_resource_manager.h"

#include <assert.h>
#include <stdio.h>

int main(void)
{
    syntra_hal_resource_manager_t manager;
    syntra_hal_resource_t *resource = NULL;

    unsigned int total = 0U;
    unsigned int allocated = 0U;
    unsigned int available = 0U;

    /* Initialization */

    assert(
        hal_resource_manager_init(&manager)
        == SYNTRA_STATUS_OK
    );

    assert(manager.initialized == 1);
    assert(manager.resource_count == 0U);
    assert(manager.allocated_count == 0U);
    assert(manager.available_count == 0U);

    assert(
        hal_resource_manager_validate(&manager)
        == SYNTRA_STATUS_OK
    );

    /* Register resources */

    assert(
        hal_resource_manager_register(
            &manager,
            1U,
            "SystemMemory",
            SYNTRA_HAL_RESOURCE_MEMORY
        ) == SYNTRA_STATUS_OK
    );

    assert(
        hal_resource_manager_register(
            &manager,
            2U,
            "TimerInterrupt",
            SYNTRA_HAL_RESOURCE_INTERRUPT
        ) == SYNTRA_STATUS_OK
    );

    assert(
        hal_resource_manager_register(
            &manager,
            3U,
            "DisplayPort",
            SYNTRA_HAL_RESOURCE_IO_PORT
        ) == SYNTRA_STATUS_OK
    );

    assert(manager.resource_count == 3U);
    assert(manager.available_count == 3U);

    /* Find resource */

    assert(
        hal_resource_manager_find(
            &manager,
            1U,
            &resource
        ) == SYNTRA_STATUS_OK
    );

    assert(resource != NULL);
    assert(resource->resource_id == 1U);
    assert(resource->state == SYNTRA_HAL_RESOURCE_AVAILABLE);

    /* Allocate resource */

    assert(
        hal_resource_manager_allocate(
            &manager,
            1U,
            10U
        ) == SYNTRA_STATUS_OK
    );

    assert(manager.allocated_count == 1U);
    assert(manager.available_count == 2U);

    assert(
        hal_resource_manager_find(
            &manager,
            1U,
            &resource
        ) == SYNTRA_STATUS_OK
    );

    assert(resource->state == SYNTRA_HAL_RESOURCE_ALLOCATED);
    assert(resource->owner_device_id == 10U);

    /* Prevent duplicate allocation */

    assert(
        hal_resource_manager_allocate(
            &manager,
            1U,
            20U
        ) == SYNTRA_STATUS_ERROR
    );

    /* Allocate another resource */

    assert(
        hal_resource_manager_allocate(
            &manager,
            2U,
            20U
        ) == SYNTRA_STATUS_OK
    );

    assert(manager.allocated_count == 2U);
    assert(manager.available_count == 1U);

    /* Prevent unregistering allocated resource */

    assert(
        hal_resource_manager_unregister(
            &manager,
            1U
        ) == SYNTRA_STATUS_ERROR
    );

    /* Prevent release by wrong device */

    assert(
        hal_resource_manager_release(
            &manager,
            1U,
            20U
        ) == SYNTRA_STATUS_ERROR
    );

    /* Release resource */

    assert(
        hal_resource_manager_release(
            &manager,
            1U,
            10U
        ) == SYNTRA_STATUS_OK
    );

    assert(manager.allocated_count == 1U);
    assert(manager.available_count == 2U);

    /* Release second resource */

    assert(
        hal_resource_manager_release(
            &manager,
            2U,
            20U
        ) == SYNTRA_STATUS_OK
    );

    assert(manager.allocated_count == 0U);
    assert(manager.available_count == 3U);

    /* Statistics */

    assert(
        hal_resource_manager_get_count(
            &manager,
            &total,
            &allocated,
            &available
        ) == SYNTRA_STATUS_OK
    );

    assert(total == 3U);
    assert(allocated == 0U);
    assert(available == 3U);

    /* Unregister resource */

    assert(
        hal_resource_manager_unregister(
            &manager,
            3U
        ) == SYNTRA_STATUS_OK
    );

    assert(manager.resource_count == 2U);
    assert(manager.available_count == 2U);

    /* Missing resource */

    assert(
        hal_resource_manager_find(
            &manager,
            99U,
            &resource
        ) == SYNTRA_STATUS_ERROR
    );

    assert(resource == NULL);

    assert(
        hal_resource_manager_unregister(
            &manager,
            99U
        ) == SYNTRA_STATUS_ERROR
    );

    /* Duplicate resource ID */

    assert(
        hal_resource_manager_register(
            &manager,
            1U,
            "Duplicate",
            SYNTRA_HAL_RESOURCE_GENERIC
        ) == SYNTRA_STATUS_ERROR
    );

    /* Invalid arguments */

    assert(
        hal_resource_manager_init(NULL)
        == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    assert(
        hal_resource_manager_register(
            NULL,
            4U,
            "Invalid",
            SYNTRA_HAL_RESOURCE_GENERIC
        ) == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    assert(
        hal_resource_manager_register(
            &manager,
            0U,
            "Invalid",
            SYNTRA_HAL_RESOURCE_GENERIC
        ) == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    assert(
        hal_resource_manager_register(
            &manager,
            4U,
            "",
            SYNTRA_HAL_RESOURCE_GENERIC
        ) == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    assert(
        hal_resource_manager_register(
            &manager,
            4U,
            "Invalid",
            (syntra_hal_resource_type_t)99
        ) == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    assert(
        hal_resource_manager_allocate(
            &manager,
            1U,
            0U
        ) == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    assert(
        hal_resource_manager_release(
            &manager,
            1U,
            0U
        ) == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    assert(
        hal_resource_manager_find(
            &manager,
            1U,
            NULL
        ) == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    assert(
        hal_resource_manager_get_count(
            &manager,
            NULL,
            &allocated,
            &available
        ) == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    /* Final validation */

    assert(
        hal_resource_manager_validate(&manager)
        == SYNTRA_STATUS_OK
    );

    /* Reset */

    assert(
        hal_resource_manager_reset(&manager)
        == SYNTRA_STATUS_OK
    );

    assert(manager.initialized == 0);
    assert(manager.resource_count == 0U);
    assert(manager.allocated_count == 0U);
    assert(manager.available_count == 0U);

    printf("HAL Resource Manager V1: all tests passed.\n");

    return 0;
}
