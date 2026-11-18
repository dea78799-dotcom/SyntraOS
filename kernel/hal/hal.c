
#include "hal.h"

static int hal_initialized = 0;

syntra_status_t hal_init(void)
{
    if (hal_initialized)
    {
        return SYNTRA_STATUS_OK;
    }

    hal_initialized = 1;

    return SYNTRA_STATUS_OK;
}

syntra_status_t hal_reset(void)
{
    hal_initialized = 0;

    return SYNTRA_STATUS_OK;
}

syntra_status_t hal_is_initialized(int *initialized)
{
    if (initialized == 0)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    *initialized = hal_initialized;

    return SYNTRA_STATUS_OK;
}
