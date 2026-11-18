
#ifndef SYNTRA_HAL_LIFECYCLE_H
#define SYNTRA_HAL_LIFECYCLE_H

#include "hal_device.h"

/*
 * SyntraOS HAL Lifecycle
 * Version: 1.0.0
 */

/* Device lifecycle */

syntra_status_t hal_device_start(
    syntra_hal_device_t *device
);

syntra_status_t hal_device_stop(
    syntra_hal_device_t *device
);

syntra_status_t hal_device_restart(
    syntra_hal_device_t *device
);

/* Device state checks */

syntra_status_t hal_device_is_online(
    const syntra_hal_device_t *device,
    int *online
);

syntra_status_t hal_device_is_offline(
    const syntra_hal_device_t *device,
    int *offline
);

#endif
