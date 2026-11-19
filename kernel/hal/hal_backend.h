
#ifndef SYNTRA_HAL_BACKEND_H
#define SYNTRA_HAL_BACKEND_H

#include "hal.h"

#define SYNTRA_HAL_BACKEND_NAME_MAX 64U

typedef enum
{
    SYNTRA_HAL_BACKEND_UNKNOWN = 0,
    SYNTRA_HAL_BACKEND_SOFTWARE,
    SYNTRA_HAL_BACKEND_ARM64,
    SYNTRA_HAL_BACKEND_X86_64,
    SYNTRA_HAL_BACKEND_RISCV64

} syntra_hal_backend_type_t;

typedef struct
{
    unsigned int backend_id;

    char name[SYNTRA_HAL_BACKEND_NAME_MAX];

    syntra_hal_backend_type_t type;

    int initialized;

    int available;

} syntra_hal_backend_t;

syntra_status_t hal_backend_init(
    syntra_hal_backend_t *backend,
    unsigned int backend_id,
    const char *name,
    syntra_hal_backend_type_t type
);

syntra_status_t hal_backend_reset(
    syntra_hal_backend_t *backend
);

syntra_status_t hal_backend_validate(
    const syntra_hal_backend_t *backend
);

syntra_status_t hal_backend_is_available(
    const syntra_hal_backend_t *backend,
    int *result
);

#endif
