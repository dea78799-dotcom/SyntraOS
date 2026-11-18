
#include "panic.h"
#include "log.h"

#include <stddef.h>

/*
 * SyntraOS Kernel Panic
 * Version: 0.2.0
 */

static syntra_panic_info_t panic_info = {
    SYNTRA_PANIC_NONE,
    0,
    ""
};

/* =================================
   COPY REASON
   ================================= */

static void copy_reason(
    char *destination,
    const char *source
)
{
    unsigned int i = 0;

    while (
        i < SYNTRA_PANIC_REASON_SIZE - 1 &&
        source[i] != '\0'
    )
    {
        destination[i] = source[i];
        i++;
    }

    destination[i] = '\0';
}

/* =================================
   PANIC WITH TIMESTAMP
   ================================= */

void syntra_kernel_panic_at(
    const char *reason,
    unsigned int timestamp
)
{
    if (reason == NULL)
    {
        return;
    }

    /* Preserve the first panic reason */
    if (panic_info.state == SYNTRA_PANIC_ACTIVE)
    {
        return;
    }

    panic_info.state = SYNTRA_PANIC_ACTIVE;
    panic_info.timestamp = timestamp;

    copy_reason(
        panic_info.reason,
        reason
    );

    syntra_log_write_at(
        SYNTRA_LOG_ERROR,
        panic_info.reason,
        timestamp
    );
}

/* =================================
   PANIC
   ================================= */

void syntra_kernel_panic(const char *reason)
{
    syntra_kernel_panic_at(reason, 0);
}

/* =================================
   GET PANIC INFORMATION
   ================================= */

void syntra_kernel_panic_get_info(
    syntra_panic_info_t *info
)
{
    if (info == NULL)
    {
        return;
    }

    *info = panic_info;
}

/* =================================
   RESET PANIC
   ================================= */

void syntra_kernel_panic_reset(void)
{
    panic_info.state = SYNTRA_PANIC_NONE;
    panic_info.timestamp = 0;
    panic_info.reason[0] = '\0';
}
