
#include "timer.h"

#include <limits.h>
#include <stddef.h>

static volatile unsigned long long timer_ticks;
static unsigned int timer_frequency;
static int timer_initialized;

static unsigned long long calculate_milliseconds(void)
{
    unsigned long long whole_seconds =
        timer_ticks / timer_frequency;

    unsigned long long remaining_ticks =
        timer_ticks % timer_frequency;

    if (whole_seconds > ULLONG_MAX / 1000ULL)
    {
        return ULLONG_MAX;
    }

    unsigned long long milliseconds =
        whole_seconds * 1000ULL;

    unsigned long long remaining_milliseconds =
        (remaining_ticks * 1000ULL) / timer_frequency;

    if (milliseconds >
        ULLONG_MAX - remaining_milliseconds)
    {
        return ULLONG_MAX;
    }

    return milliseconds + remaining_milliseconds;
}

syntra_status_t timer_init(
    unsigned int tick_frequency
)
{
    if (tick_frequency == 0U)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (timer_initialized)
    {
        if (timer_frequency != tick_frequency)
        {
            return SYNTRA_STATUS_INVALID_ARGUMENT;
        }

        return SYNTRA_STATUS_OK;
    }

    timer_ticks = 0ULL;
    timer_frequency = tick_frequency;
    timer_initialized = 1;

    return SYNTRA_STATUS_OK;
}

syntra_status_t timer_tick(void)
{
    if (!timer_initialized)
    {
        return SYNTRA_STATUS_ERROR;
    }

    if (timer_ticks == ULLONG_MAX)
    {
        return SYNTRA_STATUS_ERROR;
    }

    timer_ticks++;

    return SYNTRA_STATUS_OK;
}

syntra_status_t timer_reset(void)
{
    if (!timer_initialized)
    {
        return SYNTRA_STATUS_ERROR;
    }

    timer_ticks = 0ULL;

    return SYNTRA_STATUS_OK;
}

unsigned long long timer_get_ticks(void)
{
    if (!timer_initialized)
    {
        return 0ULL;
    }

    return timer_ticks;
}

unsigned long long timer_get_milliseconds(void)
{
    if (!timer_initialized)
    {
        return 0ULL;
    }

    return calculate_milliseconds();
}

syntra_status_t timer_get_info(
    syntra_timer_info_t *info
)
{
    if (info == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (!timer_initialized)
    {
        return SYNTRA_STATUS_ERROR;
    }

    info->ticks = timer_ticks;

    info->elapsed_milliseconds =
        calculate_milliseconds();

    info->tick_frequency = timer_frequency;

    info->initialized = timer_initialized;

    info->valid =
        (timer_validate() == SYNTRA_STATUS_OK);

    return SYNTRA_STATUS_OK;
}

syntra_status_t timer_validate(void)
{
    if (!timer_initialized)
    {
        return SYNTRA_STATUS_ERROR;
    }

    if (timer_frequency == 0U)
    {
        return SYNTRA_STATUS_ERROR;
    }

    return SYNTRA_STATUS_OK;
}
