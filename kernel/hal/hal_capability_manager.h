
#ifndef SYNTRA_HAL_CAPABILITY_MANAGER_H
#define SYNTRA_HAL_CAPABILITY_MANAGER_H

#include "hal.h"

#define SYNTRA_HAL_MAX_CAPABILITIES 128U
#define SYNTRA_HAL_CAPABILITY_NAME_MAX 64U

typedef struct
{
    unsigned int capability_id;
    unsigned int device_id;

    char name[SYNTRA_HAL_CAPABILITY_NAME_MAX];

    int initialized;

} syntra_hal_capability_t;

typedef struct
{
    syntra_hal_capability_t capabilities[
        SYNTRA_HAL_MAX_CAPABILITIES
    ];

    unsigned int capability_count;

    unsigned int update_count;

    int initialized;

} syntra_hal_capability_manager_t;

syntra_status_t hal_capability_manager_init(
    syntra_hal_capability_manager_t *manager
);

syntra_status_t hal_capability_manager_reset(
    syntra_hal_capability_manager_t *manager
);

syntra_status_t hal_capability_manager_register(
    syntra_hal_capability_manager_t *manager,
    unsigned int capability_id,
    unsigned int device_id,
    const char *name
);

syntra_status_t hal_capability_manager_unregister(
    syntra_hal_capability_manager_t *manager,
    unsigned int capability_id
);

syntra_status_t hal_capability_manager_has(
    const syntra_hal_capability_manager_t *manager,
    unsigned int device_id,
    unsigned int capability_id,
    int *result
);

syntra_status_t hal_capability_manager_get_count(
    const syntra_hal_capability_manager_t *manager,
    unsigned int *count
);

syntra_status_t hal_capability_manager_validate(
    const syntra_hal_capability_manager_t *manager
);

#endif
