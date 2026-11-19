
#include "hal_resource_monitor.h"

#include <assert.h>
#include <stdio.h>

int main(void)
{
    syntra_hal_resource_manager_t manager;
    syntra_hal_resource_monitor_t monitor;
    syntra_hal_resource_t *resource = NULL;

    /* Initialize */

    assert(hal_resource_manager_init(&manager) == SYNTRA_STATUS_SUCCESS);
    assert(hal_resource_monitor_init(&monitor) == SYNTRA_STATUS_SUCCESS);

    assert(hal_resource_monitor_validate(&monitor) == SYNTRA_STATUS_SUCCESS);

    /* Register resources */

    assert(
        hal_resource_manager_register(
            &manager,
            1U,
            "Memory",
            SYNTRA_HAL_RESOURCE_MEMORY
        ) == SYNTRA_STATUS_SUCCESS
    );

    assert(
        hal_resource_manager_register(
            &manager,
            2U,
            "Interrupt",
            SYNTRA_HAL_RESOURCE_INTERRUPT
        ) == SYNTRA_STATUS_SUCCESS
    );

    assert(
        hal_resource_manager_register(
            &manager,
            3U,
            "DMA",
            SYNTRA_HAL_RESOURCE_DMA
        ) == SYNTRA_STATUS_SUCCESS
    );

    /* Initial monitoring */

    assert(
        hal_resource_monitor_update(&monitor, &manager)
        == SYNTRA_STATUS_SUCCESS
    );

    assert(monitor.total_resources == 3U);
    assert(monitor.allocated_resources == 0U);
    assert(monitor.available_resources == 3U);
    assert(monitor.invalid_resources == 0U);

    /* Allocate resource */

    assert(
        hal_resource_manager_allocate(
            &manager,
            1U,
            100U
        ) == SYNTRA_STATUS_SUCCESS
    );

    assert(
        hal_resource_monitor_update(&monitor, &manager)
        == SYNTRA_STATUS_SUCCESS
    );

    assert(monitor.total_resources == 3U);
    assert(monitor.allocated_resources == 1U);
    assert(monitor.available_resources == 2U);
    assert(monitor.invalid_resources == 0U);

    /* Release resource */

    assert(
        hal_resource_manager_release(
            &manager,
            1U,
            100U
        ) == SYNTRA_STATUS_SUCCESS
    );

    assert(
        hal_resource_monitor_update(&monitor, &manager)
        == SYNTRA_STATUS_SUCCESS
    );

    assert(monitor.allocated_resources == 0U);
    assert(monitor.available_resources == 3U);

    /* Find resource */

    assert(
        hal_resource_manager_find(
            &manager,
            2U,
            &resource
        ) == SYNTRA_STATUS_SUCCESS
    );

    assert(resource != NULL);

    /* Update counter */

    assert(monitor.update_count == 3U);

    assert(
        hal_resource_monitor_validate(&monitor)
        == SYNTRA_STATUS_SUCCESS
    );

    /* Invalid arguments */

    assert(
        hal_resource_monitor_update(&monitor, NULL)
        == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    assert(
        hal_resource_monitor_update(NULL, &manager)
        == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    assert(
        hal_resource_monitor_validate(NULL)
        == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    /* Reset */

    assert(
        hal_resource_monitor_reset(&monitor)
        == SYNTRA_STATUS_SUCCESS
    );

    assert(monitor.initialized == 0);

    assert(
        hal_resource_monitor_validate(&monitor)
        == SYNTRA_STATUS_ERROR
    );

    printf("HAL Resource Monitor V1 tests passed.\n");

    return 0;
}
