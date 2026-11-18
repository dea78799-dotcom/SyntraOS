
#include "log.h"

#include <stddef.h>

/*
 * SyntraOS Kernel Logging
 * Version: 0.4.0
 */

static syntra_log_entry_t log_entries[SYNTRA_LOG_MAX_ENTRIES];

static syntra_log_info_t log_info = {0};

static unsigned int log_count = 0;

/* =================================
   COPY MESSAGE
   ================================= */

static void copy_message(char *destination, const char *source)
{
    unsigned int i = 0;

    while (
        i < SYNTRA_LOG_MESSAGE_SIZE - 1 &&
        source[i] != '\0'
    )
    {
        destination[i] = source[i];
        i++;
    }

    destination[i] = '\0';
}

/* =================================
   STORE LOG
   ================================= */

static void store_log(
    syntra_log_level_t level,
    const char *message,
    unsigned int timestamp
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

    if (log_count >= SYNTRA_LOG_MAX_ENTRIES)
    {
        for (unsigned int i = 1; i < SYNTRA_LOG_MAX_ENTRIES; i++)
        {
            log_entries[i - 1] = log_entries[i];
        }

        log_count--;
    }

    syntra_log_entry_t *entry = &log_entries[log_count];

    entry->level = (unsigned int)level;
    entry->timestamp = timestamp;

    copy_message(entry->message, message);

    log_count++;
}

/* =================================
   WRITE LOG
   ================================= */

void syntra_log_write(
    syntra_log_level_t level,
    const char *message
)
{
    store_log(level, message, 0);
}

/* =================================
   WRITE LOG WITH TIMESTAMP
   ================================= */

void syntra_log_write_at(
    syntra_log_level_t level,
    const char *message,
    unsigned int timestamp
)
{
    store_log(level, message, timestamp);
}

/* =================================
   GET LOG INFORMATION
   ================================= */

void syntra_log_get_info(syntra_log_info_t *info)
{
    if (info == NULL)
    {
        return;
    }

    *info = log_info;
}

/* =================================
   GET LOG BY INDEX
   ================================= */

const syntra_log_entry_t *syntra_log_get(unsigned int index)
{
    if (index >= log_count)
    {
        return NULL;
    }

    return &log_entries[index];
}

/* =================================
   FIND LOG BY LEVEL
   ================================= */

const syntra_log_entry_t *syntra_log_find_level(
    syntra_log_level_t level,
    unsigned int index
)
{
    unsigned int found = 0;

    for (unsigned int i = 0; i < log_count; i++)
    {
        if (log_entries[i].level == (unsigned int)level)
        {
            if (found == index)
            {
                return &log_entries[i];
            }

            found++;
        }
    }

    return NULL;
}

/* =================================
   RESET LOGGING
   ================================= */

void syntra_log_reset(void)
{
    for (unsigned int i = 0; i < SYNTRA_LOG_MAX_ENTRIES; i++)
    {
        log_entries[i].level = SYNTRA_LOG_INFO;
        log_entries[i].timestamp = 0;
        log_entries[i].message[0] = '\0';
    }

    log_info.info_count = 0;
    log_info.warning_count = 0;
    log_info.error_count = 0;
    log_info.debug_count = 0;
    log_info.total_count = 0;

    log_count = 0;
}
