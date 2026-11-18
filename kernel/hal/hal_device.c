
#include "hal_device.h"

#include <string.h>

syntra_status_t hal_device_init(
    syntra_hal_device_t *device,
    unsigned int device_id,
    const char *name,
    syntra_hal_device_type_t type
)
{
    if (device == NULL ||
        name == NULL ||
        name[0] == '\0' ||
        type <= SYNTRA_HAL_DEVICE_UNKNOWN ||
        type > SYNTRA_HAL_DEVICE_INPUT)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    size_t length = strlen(name);

    if (length >= SYNTRA_HAL_DEVICE_NAME_MAX)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    memset(device, 0, sizeof(*device));

    memcpy(device->name, name, length + 1U);

    device->device_id = device_id;
    device->type = type;
    device->state = SYNTRA_HAL_DEVICE_OFFLINE;
    device->initialized = 1;

    return SYNTRA_STATUS_OK;
}

syntra_status_t hal_device_set_state(
    syntra_hal_device_t *device,
    syntra_hal_device_state_t state
)
{
    if (device == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (!device->initialized)
    {
        return SYNTRA_STATUS_ERROR;
    }

    if (state < SYNTRA_HAL_DEVICE_OFFLINE ||
        state > SYNTRA_HAL_DEVICE_ERROR)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    device->state = state;

    return SYNTRA_STATUS_OK;
}

syntra_status_t hal_device_get_state(
    const syntra_hal_device_t *device,
    syntra_hal_device_state_t *state
)
{
    if (device == NULL || state == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (!device->initialized)
    {
        return SYNTRA_STATUS_ERROR;
    }

    *state = device->state;

    return SYNTRA_STATUS_OK;
}
