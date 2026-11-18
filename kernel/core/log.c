
#include "log.h"

#include <stddef.h>

/*
 * SyntraOS Kernel Logging
 * Version: 0.2.0
 */

static syntra_log_info_t log_info = {0};

/* =================================
   WRITE LOG
   ================================= */

void syntra_log_write(
    syntra_log_level_t level,
    const char *message
)
{
    if (message == NULL)
    {
        return;
    }

    switch (level)
    {
        case SYNTRA_LOG_INFO:
            log_info.info_count++;
            break;

        case SYNTRA_LOG_WARNING:
            log_info.warning_count++;
            break;

        case SYNTRA_LOG_ERROR:
            log_info.error_count++;
            break;

        case SYNTRA_LOG_DEBUG:
            log_info.debug_count++;
            break;

        default:
            return;
    }

    log_info.total_count++;
}

/* =================================
   GET LOG INFORMATION
   ================================= */

void syntra_log_get_info(
    syntra_log_info_t *info
)
{
    if (info == NULL)
    {
        return;
    }

    *info = log_info;
}

/* =================================
   RESET LOG COUNTERS
   ================================= */

void syntra_log_reset(void)
{
    log_info.info_count = 0;
    log_info.warning_count = 0;
    log_info.error_count = 0;
    log_info.debug_count = 0;
    log_info.total_count = 0;
}
