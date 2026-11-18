
#include "uptime.h"

#include <limits.h>
#include <stddef.h>

static unsigned long long uptime_total_milliseconds;
static unsigned long long uptime_boot_count;
static int uptime_initialized;

syntra_status_t uptime_init(void)
{
    if (uptime_initialized)
    {
        return SYNTRA_STATUS_OK;
    }

    uptime_total_milliseconds = 0ULL;
    uptime_boot_count++;

    uptime_initialized = 1;

    return SYNTRA_STATUS_OK;
}

syntra_status_t uptime_update(
    unsigned long long elapsed_milliseconds
)
{
    if (!uptime_initialized)
    {
        return SYNTRA_STATUS_ERROR;
    }

    if (elapsed_milliseconds >
        ULLONG_MAX - uptime_total_milliseconds)
    {
        return SYNTRA_STATUS_ERROR;
    }

    uptime_total_milliseconds += elapsed_milliseconds;

    return SYNTRA_STATUS_OK;
}

syntra_status_t uptime_reset(void)
{
    if (!uptime_initialized)
    {
        return SYNTRA_STATUS_ERROR;
    }

    uptime_total_milliseconds = 0ULL;

    return SYNTRA_STATUS_OK;
}

unsigned long long uptime_get_milliseconds(void)
{
    if (!uptime_initialized)
    {
        return 0ULL;
    }

    return uptime_total_milliseconds;
}

unsigned long long uptime_get_seconds(void)
{
    if (!uptime_initialized)
    {
        return 0ULL;
    }

    return uptime_total_milliseconds / 1000ULL;
}

syntra_status_t uptime_get_info(
    syntra_uptime_info_t *info
)
{
    if (info == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (!uptime_initialized)
    {
        return SYNTRA_STATUS_ERROR;
    }

    info->total_milliseconds =
        uptime_total_milliseconds;

    info->total_seconds =
        uptime_get_seconds();

    info->boot_count =
        uptime_boot_count;

    info->initialized =
        uptime_initialized;

    info->valid =
        (uptime_validate() == SYNTRA_STATUS_OK);

    return SYNTRA_STATUS_OK;
}

syntra_status_t uptime_validate(void)
{
    if (!uptime_initialized)
    {
        return SYNTRA_STATUS_ERROR;
    }

    return SYNTRA_STATUS_OK;
}
