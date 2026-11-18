
#include "kernel.h"
#include "kernel_config.h"

#include "../scheduler/scheduler.h"
#include "../memory/memory.h"
#include "../process/process.h"

syntra_status_t syntra_kernel_init(void)
{
#if SYNTRA_DEBUG
    /* Debug mode is enabled */
#endif

    syntra_status_t status;

    /* Initialize Memory Manager */
    status = memory_init();

    if (status != SYNTRA_STATUS_OK)
    {
        return status;
    }

    /* Initialize Scheduler */
    status = scheduler_init();

    if (status != SYNTRA_STATUS_OK)
    {
        return status;
    }

    /* Initialize Process Manager */
    status = process_init();

    if (status != SYNTRA_STATUS_OK)
    {
        return status;
    }

    return SYNTRA_STATUS_OK;
}
