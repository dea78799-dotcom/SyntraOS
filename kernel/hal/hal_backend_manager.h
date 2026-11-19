
#ifndef SYNTRA_HAL_BACKEND_MANAGER_H
#define SYNTRA_HAL_BACKEND_MANAGER_H

#include "hal_backend.h"

#define SYNTRA_HAL_BACKEND_MANAGER_MAX_BACKENDS 32U

typedef struct
{
    syntra_hal_backend_t *backends[
        SYNTRA_HAL_BACKEND_MANAGER_MAX_BACKENDS
    ];

    unsigned int backend_count;

    unsigned int update_count;

    int initialized;

} syntra_hal_backend_manager_t;

syntra_status_t hal_backend_manager_init(
    syntra_hal_backend_manager_t *manager
);

syntra_status_t hal_backend_manager_reset(
    syntra_hal_backend_manager_t *manager
);

syntra_status_t hal_backend_manager_register(
    syntra_hal_backend_manager_t *manager,
    syntra_hal_backend_t *backend
);

syntra_status_t hal_backend_manager_unregister(
    syntra_hal_backend_manager_t *manager,
    unsigned int backend_id
);

syntra_status_t hal_backend_manager_find(
    const syntra_hal_backend_manager_t *manager,
    unsigned int backend_id,
    syntra_hal_backend_t **result
);

syntra_status_t hal_backend_manager_get_count(
    const syntra_hal_backend_manager_t *manager,
    unsigned int *result
);

syntra_status_t hal_backend_manager_validate(
    const syntra_hal_backend_manager_t *manager
);

#endif
