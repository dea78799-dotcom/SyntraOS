
#ifndef SYNTRA_HAL_DEVICE_H
#define SYNTRA_HAL_DEVICE_H

#include "hal.h"

#define SYNTRA_HAL_DEVICE_NAME_MAX 64U

typedef struct
{
    unsigned int device_id;

    char name[SYNTRA_HAL_DEVICE_NAME_MAX];

    syntra_hal_device_type_t type;

    syntra_hal_device_state_t state;

    int initialized;

} syntra_hal_device_t;

syntra_status_t hal_device_init(
    syntra_hal_device_t *device,
    unsigned int device_id,
    const char *name,
    syntra_hal_device_type_t type
);

syntra_status_t hal_device_set_state(
    syntra_hal_device_t *device,
    syntra_hal_device_state_t state
);

syntra_status_t hal_device_get_state(
    const syntra_hal_device_t *device,
    syntra_hal_device_state_t *state
);

#endif
