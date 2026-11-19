
#ifndef SYNTRA_HAL_DEVICE_MANAGER_H
#define SYNTRA_HAL_DEVICE_MANAGER_H

#include "hal_registry.h"
#include "hal_lifecycle.h"

/*
 * SyntraOS HAL Device Manager
 * Version: 2.0.0
 */

typedef struct
{
    syntra_hal_registry_t *registry;

    unsigned int total_devices;
    unsigned int online_devices;
    unsigned int offline_devices;

    unsigned int error_devices;
    unsigned int update_count;

    int initialized;
    int dirty;

} syntra_hal_device_manager_t;

/* Manager lifecycle */

syntra_status_t hal_device_manager_init(
    syntra_hal_device_manager_t *manager,
    syntra_hal_registry_t *registry
);

syntra_status_t hal_device_manager_reset(
    syntra_hal_device_manager_t *manager
);

/* Device operations */

syntra_status_t hal_device_manager_start(
    syntra_hal_device_manager_t *manager,
    unsigned int device_id
);

syntra_status_t hal_device_manager_stop(
    syntra_hal_device_manager_t *manager,
    unsigned int device_id
);

syntra_status_t hal_device_manager_restart(
    syntra_hal_device_manager_t *manager,
    unsigned int device_id
);

/* Device information */

syntra_status_t hal_device_manager_get_device(
    const syntra_hal_device_manager_t *manager,
    unsigned int device_id,
    syntra_hal_device_t **device
);

syntra_status_t hal_device_manager_get_info(
    const syntra_hal_device_manager_t *manager,
    syntra_hal_device_manager_t *info
);

/* Validation and synchronization */

syntra_status_t hal_device_manager_validate(
    const syntra_hal_device_manager_t *manager
);

syntra_status_t hal_device_manager_refresh(
    syntra_hal_device_manager_t *manager
);

#endif
