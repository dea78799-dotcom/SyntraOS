
#include "clock.h"

#include <limits.h>
#include <stddef.h>

static unsigned long long clock_total_milliseconds;
static int clock_initialized;

syntra_status_t clock_init(void)
{
    if (clock_initialized)
    {
        return SYNTRA_STATUS_OK;
    }

    clock_total_milliseconds = 0ULL;
    clock_initialized = 1;

    return SYNTRA_STATUS_OK;
}

syntra_status_t clock_update(
    unsigned long long elapsed_milliseconds
)
{
    if (!clock_initialized)
    {
        return SYNTRA_STATUS_ERROR;
    }

    if (elapsed_milliseconds >
        ULLONG_MAX - clock_total_milliseconds)
    {
        return SYNTRA_STATUS_ERROR;
    }

    clock_total_milliseconds += elapsed_milliseconds;

    return SYNTRA_STATUS_OK;
}

syntra_status_t clock_reset(void)
{
    if (!clock_initialized)
    {
        return SYNTRA_STATUS_ERROR;
    }

    clock_total_milliseconds = 0ULL;

    return SYNTRA_STATUS_OK;
}

unsigned long long clock_get_milliseconds(void)
{
    if (!clock_initialized)
    {
        return 0ULL;
    }

    return clock_total_milliseconds;
}

unsigned int clock_get_seconds(void)
{
    if (!clock_initialized)
    {
        return 0U;
    }

    return (unsigned int)(
        (clock_total_milliseconds / 1000ULL) % 60ULL
    );
}

unsigned int clock_get_minutes(void)
{
    if (!clock_initialized)
    {
        return 0U;
    }

    return (unsigned int)(
        (clock_total_milliseconds / 60000ULL) % 60ULL
    );
}

unsigned int clock_get_hours(void)
{
    if (!clock_initialized)
    {
        return 0U;
    }

    return (unsigned int)(
        (clock_total_milliseconds / 3600000ULL) % 24ULL
    );
}

syntra_status_t clock_get_info(
    syntra_clock_info_t *info
)
{
    if (info == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (!clock_initialized)
    {
        return SYNTRA_STATUS_ERROR;
    }

    unsigned long long total_seconds =
        clock_total_milliseconds / 1000ULL;

    info->total_milliseconds =
        clock_total_milliseconds;

    info->hours = (unsigned int)(
        (total_seconds / 3600ULL) % 24ULL
    );

    info->minutes = (unsigned int)(
        (total_seconds / 60ULL) % 60ULL
    );

    info->seconds = (unsigned int)(
        total_seconds % 60ULL
    );

    info->milliseconds = (unsigned int)(
        clock_total_milliseconds % 1000ULL
    );

    info->initialized = clock_initialized;
    info->valid =
        (clock_validate() == SYNTRA_STATUS_OK);

    return SYNTRA_STATUS_OK;
}

syntra_status_t clock_validate(void)
{
    if (!clock_initialized)
    {
        return SYNTRA_STATUS_ERROR;
    }

    return SYNTRA_STATUS_OK;
}
