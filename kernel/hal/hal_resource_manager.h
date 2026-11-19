
#ifndef SYNTRA_HAL_RESOURCE_MANAGER_H
#define SYNTRA_HAL_RESOURCE_MANAGER_H

#include "hal.h"

#define SYNTRA_HAL_MAX_RESOURCES 128U
#define SYNTRA_HAL_RESOURCE_NAME_MAX 64U

/* Resource types */

typedef enum
{
    SYNTRA_HAL_RESOURCE_GENERIC = 0,
    SYNTRA_HAL_RESOURCE_MEMORY,
    SYNTRA_HAL_RESOURCE_IO_PORT,
    SYNTRA_HAL_RESOURCE_INTERRUPT,
    SYNTRA_HAL_RESOURCE_DMA

} syntra_hal_resource_type_t;

/* Resource states */

typedef enum
{
    SYNTRA_HAL_RESOURCE_AVAILABLE = 0,
    SYNTRA_HAL_RESOURCE_ALLOCATED

} syntra_hal_resource_state_t;

/* Resource descriptor */

typedef struct
{
    unsigned int resource_id;

    char name[SYNTRA_HAL_RESOURCE_NAME_MAX];

    syntra_hal_resource_type_t type;

    syntra_hal_resource_state_t state;

    unsigned int owner_device_id;

    int initialized;

} syntra_hal_resource_t;

/* Resource manager */

typedef struct
{
    syntra_hal_resource_t resources[
        SYNTRA_HAL_MAX_RESOURCES
    ];

    unsigned int resource_count;

    unsigned int allocated_count;

    unsigned int available_count;

    int initialized;

} syntra_hal_resource_manager_t;

/* Initialization */

syntra_status_t hal_resource_manager_init(
    syntra_hal_resource_manager_t *manager
);

/* Reset */

syntra_status_t hal_resource_manager_reset(
    syntra_hal_resource_manager_t *manager
);

/* Resource operations */

syntra_status_t hal_resource_manager_register(
    syntra_hal_resource_manager_t *manager,
    unsigned int resource_id,
    const char *name,
    syntra_hal_resource_type_t type
);

syntra_status_t hal_resource_manager_unregister(
    syntra_hal_resource_manager_t *manager,
    unsigned int resource_id
);

/* Allocation */

syntra_status_t hal_resource_manager_allocate(
    syntra_hal_resource_manager_t *manager,
    unsigned int resource_id,
    unsigned int device_id
);

/* Release */

syntra_status_t hal_resource_manager_release(
    syntra_hal_resource_manager_t *manager,
    unsigned int resource_id,
    unsigned int device_id
);

/* Lookup */

syntra_status_t hal_resource_manager_find(
    syntra_hal_resource_manager_t *manager,
    unsigned int resource_id,
    syntra_hal_resource_t **resource
);

/* Statistics */

syntra_status_t hal_resource_manager_get_count(
    const syntra_hal_resource_manager_t *manager,
    unsigned int *total,
    unsigned int *allocated,
    unsigned int *available
);

/* Validation */

syntra_status_t hal_resource_manager_validate(
    const syntra_hal_resource_manager_t *manager
);

#endif
