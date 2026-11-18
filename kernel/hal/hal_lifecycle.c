#include <stddef.h>

#include "hal_lifecycle.h"

syntra_status_t hal_device_start(
syntra_hal_device_t *device
)
{
if (device == NULL)
{
return SYNTRA_STATUS_INVALID_ARGUMENT;
}

if (device->initialized != 1)  
{  
    return SYNTRA_STATUS_ERROR;  
}  

if (device->state == SYNTRA_HAL_DEVICE_ONLINE)  
{  
    return SYNTRA_STATUS_OK;  
}  

if (device->state != SYNTRA_HAL_DEVICE_OFFLINE)  
{  
    return SYNTRA_STATUS_ERROR;  
}  

device->state = SYNTRA_HAL_DEVICE_ONLINE;  

return SYNTRA_STATUS_OK;

}

syntra_status_t hal_device_stop(
syntra_hal_device_t *device
)
{
if (device == NULL)
{
return SYNTRA_STATUS_INVALID_ARGUMENT;
}

if (device->initialized != 1)  
{  
    return SYNTRA_STATUS_ERROR;  
}  

if (device->state == SYNTRA_HAL_DEVICE_OFFLINE)  
{  
    return SYNTRA_STATUS_OK;  
}  

if (device->state != SYNTRA_HAL_DEVICE_ONLINE)  
{  
    return SYNTRA_STATUS_ERROR;  
}  

device->state = SYNTRA_HAL_DEVICE_OFFLINE;  

return SYNTRA_STATUS_OK;

}

syntra_status_t hal_device_restart(
syntra_hal_device_t *device
)
{
if (device == NULL)
{
return SYNTRA_STATUS_INVALID_ARGUMENT;
}

if (device->initialized != 1)  
{  
    return SYNTRA_STATUS_ERROR;  
}  

if (device->state != SYNTRA_HAL_DEVICE_ONLINE &&  
    device->state != SYNTRA_HAL_DEVICE_OFFLINE)  
{  
    return SYNTRA_STATUS_ERROR;  
}  

device->state = SYNTRA_HAL_DEVICE_OFFLINE;  
device->state = SYNTRA_HAL_DEVICE_ONLINE;  

return SYNTRA_STATUS_OK;

}

syntra_status_t hal_device_is_online(
const syntra_hal_device_t *device,
int *online
)
{
if (device == NULL || online == NULL)
{
return SYNTRA_STATUS_INVALID_ARGUMENT;
}

if (device->initialized != 1)  
{  
    return SYNTRA_STATUS_ERROR;  
}  

*online =  
    (device->state == SYNTRA_HAL_DEVICE_ONLINE);  

return SYNTRA_STATUS_OK;

}

syntra_status_t hal_device_is_offline(
const syntra_hal_device_t *device,
int *offline
)
{
if (device == NULL || offline == NULL)
{
return SYNTRA_STATUS_INVALID_ARGUMENT;
}

if (device->initialized != 1)  
{  
    return SYNTRA_STATUS_ERROR;  
}  

*offline =  
    (device->state == SYNTRA_HAL_DEVICE_OFFLINE);  

return SYNTRA_STATUS_OK;

}
