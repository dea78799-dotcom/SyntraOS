
#ifndef SYNTRA_HAL_H
#define SYNTRA_HAL_H

#include "../core/status.h"

/*
 * SyntraOS Hardware Abstraction Layer
 * Version: 1.0.0
 */

typedef enum
{
    SYNTRA_HAL_DEVICE_UNKNOWN = 0,
    SYNTRA_HAL_DEVICE_TIMER,
    SYNTRA_HAL_DEVICE_INTERRUPT,
    SYNTRA_HAL_DEVICE_MEMORY,
    SYNTRA_HAL_DEVICE_STORAGE,
    SYNTRA_HAL_DEVICE_DISPLAY,
    SYNTRA_HAL_DEVICE_INPUT
} syntra_hal_device_type_t;

typedef enum
{
    SYNTRA_HAL_DEVICE_OFFLINE = 0,
    SYNTRA_HAL_DEVICE_ONLINE,
    SYNTRA_HAL_DEVICE_ERROR
} syntra_hal_device_state_t;

syntra_status_t hal_init(void);

syntra_status_t hal_reset(void);

syntra_status_t hal_is_initialized(int *initialized);

#endif
