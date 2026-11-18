
#ifndef SYNTRA_HAL_REGISTRY_H
#define SYNTRA_HAL_REGISTRY_H

#include "hal_device.h"

#define SYNTRA_HAL_REGISTRY_MAX_DEVICES 64U

typedef struct
{
    syntra_hal_device_t *devices[
        SYNTRA_HAL_REGISTRY_MAX_DEVICES
    ];

    unsigned int device_count;
    int initialized;

} syntra_hal_registry_t;

/* Registry lifecycle */

syntra_status_t hal_registry_init(
    syntra_hal_registry_t *registry
);

syntra_status_t hal_registry_reset(
    syntra_hal_registry_t *registry
);

/* Device registration */

syntra_status_t hal_registry_register(
    syntra_hal_registry_t *registry,
    syntra_hal_device_t *device
);

syntra_status_t hal_registry_unregister(
    syntra_hal_registry_t *registry,
    unsigned int device_id
);

/* Device lookup */

syntra_status_t hal_registry_find_by_id(
    const syntra_hal_registry_t *registry,
    unsigned int device_id,
    syntra_hal_device_t **device
);

syntra_status_t hal_registry_find_by_name(
    const syntra_hal_registry_t *registry,
    const char *name,
    syntra_hal_device_t **device
);

/* Registry information */

syntra_status_t hal_registry_get_count(
    const syntra_hal_registry_t *registry,
    unsigned int *count
);

#endif
