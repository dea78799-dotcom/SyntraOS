#include "scheduler.h"

static int scheduler_initialized = 0;

syntra_status_t scheduler_init(void)
{
    if (scheduler_initialized)
    {
        return SYNTRA_STATUS_ERROR;
    }

    scheduler_initialized = 1;

    return SYNTRA_STATUS_OK;
}
