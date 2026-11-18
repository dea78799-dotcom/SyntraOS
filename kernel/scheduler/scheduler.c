#include "scheduler.h"
#include "../core/status.h"
#include "kernel.h"
#include "kernel_config.h"
#include "../scheduler/scheduler.h"

syntra_status_t syntra_kernel_init(void)
{
#if SYNTRA_DEBUG
    /* Debug mode is enabled */
#endif

    syntra_status_t status = scheduler_init();

    if (status != SYNTRA_STATUS_OK)
    {
        return status;
    }

    return SYNTRA_STATUS_OK;
}
