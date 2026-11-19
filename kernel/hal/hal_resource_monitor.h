
#ifndef SYNTRA_HAL_RESOURCE_MONITOR_H
#define SYNTRA_HAL_RESOURCE_MONITOR_H

#include "hal_resource_manager.h"

typedef struct
{
    unsigned int total_resources;
    unsigned int allocated_resources;
    unsigned int available_resources;
    unsigned int invalid_resources;

    unsigned int update_count;

    int initialized;

} syntra_hal_resource_monitor_t;

syntra_status_t hal_resource_monitor_init(
    syntra_hal_resource_monitor_t *monitor
);

syntra_status_t hal_resource_monitor_reset(
    syntra_hal_resource_monitor_t *monitor
);

syntra_status_t hal_resource_monitor_update(
    syntra_hal_resource_monitor_t *monitor,
    const syntra_hal_resource_manager_t *manager
);

syntra_status_t hal_resource_monitor_validate(
    const syntra_hal_resource_monitor_t *monitor
);

#endif
