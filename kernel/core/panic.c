
#include "panic.h"
#include "log.h"

#include <stddef.h>

/*
 * SyntraOS Kernel Panic
 * Version: 0.3.0
 */

static syntra_panic_info_t panic_info = {
    .state = SYNTRA_PANIC_NONE,
    .level = SYNTRA_PANIC_LEVEL_LOW,
    .error_code = 0,
    .process_id = 0,
    .timestamp = 0,
    .reason = ""
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
   EXTENDED PANIC
   ================================= */

void syntra_kernel_panic_ex(
    const char *reason,
    unsigned int error_code,
    syntra_panic_level_t level,
    unsigned int process_id,
    unsigned int timestamp
)
{
    if (reason == NULL)
    {
        return;
    }

    if (
        level < SYNTRA_PANIC_LEVEL_LOW ||
        level > SYNTRA_PANIC_LEVEL_CRITICAL
    )
    {
        return;
    }

    /* Preserve the first panic */
    if (panic_info.state == SYNTRA_PANIC_ACTIVE)
    {
        return;
    }

    panic_info.state = SYNTRA_PANIC_ACTIVE;
    panic_info.level = level;
    panic_info.error_code = error_code;
    panic_info.process_id = process_id;
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
   PANIC WITH TIMESTAMP
   ================================= */

void syntra_kernel_panic_at(
    const char *reason,
    unsigned int timestamp
)
{
    syntra_kernel_panic_ex(
        reason,
        0,
        SYNTRA_PANIC_LEVEL_CRITICAL,
        0,
        timestamp
    );
}

/* =================================
   BASIC PANIC
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
    panic_info.level = SYNTRA_PANIC_LEVEL_LOW;
    panic_info.error_code = 0;
    panic_info.process_id = 0;
    panic_info.timestamp = 0;
    panic_info.reason[0] = '\0';
}
