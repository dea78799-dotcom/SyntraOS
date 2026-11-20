#include "timer.h"

#include <stddef.h>
#include <stdint.h>

#include "../../timer/timer.h"

/*
 * SyntraOS ARM64 Generic Timer
 * Version: 1.0.0
 *
 * ARM64 Physical Timer implementation.
 *
 * IRQ routing and exception handling must be initialized
 * separately before enabling the hardware timer.
 */

static uint64_t timer_counter_frequency;
static uint64_t timer_interval_ticks;

static uint32_t timer_tick_frequency;

static int timer_initialized;
static int timer_enabled;

#if defined(__aarch64__)

/*
 * Read the physical counter frequency.
 */
uint64_t syntra_arm64_timer_read_frequency(void)
{
    uint64_t frequency;

    __asm__ volatile(
        "mrs %0, cntfrq_el0"
        : "=r"(frequency)
    );

    return frequency;
}

/*
 * Read the physical counter.
 */
uint64_t syntra_arm64_timer_read_counter(void)
{
    uint64_t counter;

    __asm__ volatile(
        "mrs %0, cntpct_el0"
        : "=r"(counter)
    );

    return counter;
}

/*
 * Write the Physical Timer Value register.
 */
static void timer_write_value(uint64_t ticks)
{
    __asm__ volatile(
        "msr cntp_tval_el0, %0"
        :
        : "r"(ticks)
        : "memory"
    );

    __asm__ volatile("isb");
}

/*
 * Write the Physical Timer Control register.
 */
static void timer_write_control(uint64_t control)
{
    __asm__ volatile(
        "msr cntp_ctl_el0, %0"
        :
        : "r"(control)
        : "memory"
    );

    __asm__ volatile("isb");
}

#else

/*
 * Hardware registers are unavailable on host systems.
 */

uint64_t syntra_arm64_timer_read_frequency(void)
{
    return 0ULL;
}

uint64_t syntra_arm64_timer_read_counter(void)
{
    return 0ULL;
}

#endif

/*
 * Initialize the ARM64 Generic Timer.
 */
syntra_status_t syntra_arm64_timer_init(
    uint32_t tick_frequency
)
{
#if defined(__aarch64__)

    if (tick_frequency == 0U)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (timer_initialized)
    {
        if (timer_tick_frequency != tick_frequency)
        {
            return SYNTRA_STATUS_INVALID_ARGUMENT;
        }

        return SYNTRA_STATUS_OK;
    }

    uint64_t frequency =
        syntra_arm64_timer_read_frequency();

    if (frequency == 0ULL)
    {
        return SYNTRA_STATUS_ERROR;
    }

    uint64_t interval =
        frequency / tick_frequency;

    if (interval == 0ULL || interval > INT32_MAX)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    syntra_status_t status =
        timer_init(tick_frequency);

    if (status != SYNTRA_STATUS_OK)
    {
        return status;
    }

    timer_counter_frequency = frequency;
    timer_interval_ticks = interval;
    timer_tick_frequency = tick_frequency;

    timer_initialized = 1;
    timer_enabled = 0;

    return SYNTRA_STATUS_OK;

#else

    (void)tick_frequency;

    return SYNTRA_STATUS_NOT_IMPLEMENTED;

#endif
}

/*
 * Configure the next timer interval.
 *
 * The software timer frequency cannot be changed after
 * its initialization, so a different frequency is rejected.
 */
syntra_status_t syntra_arm64_timer_set_interval(
    uint32_t tick_frequency
)
{
#if defined(__aarch64__)

    if (!timer_initialized)
    {
        return SYNTRA_STATUS_ERROR;
    }

    if (tick_frequency == 0U ||
        tick_frequency != timer_tick_frequency)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    timer_write_value(timer_interval_ticks);

    return SYNTRA_STATUS_OK;

#else

    (void)tick_frequency;

    return SYNTRA_STATUS_NOT_IMPLEMENTED;

#endif
}

/*
 * Enable the ARM64 Physical Timer.
 *
 * Call only after the IRQ vector and interrupt controller
 * have been configured.
 */
syntra_status_t syntra_arm64_timer_enable(void)
{
#if defined(__aarch64__)

    if (!timer_initialized)
    {
        return SYNTRA_STATUS_ERROR;
    }

    if (timer_enabled)
    {
        return SYNTRA_STATUS_OK;
    }

    timer_write_value(timer_interval_ticks);

    /* ENABLE = 1, IMASK = 0 */
    timer_write_control(1ULL);

    timer_enabled = 1;

    return SYNTRA_STATUS_OK;

#else

    return SYNTRA_STATUS_NOT_IMPLEMENTED;

#endif
}

/*
 * Disable the ARM64 Physical Timer.
 */
syntra_status_t syntra_arm64_timer_disable(void)
{
#if defined(__aarch64__)

    if (!timer_initialized)
    {
        return SYNTRA_STATUS_ERROR;
    }

    /* ENABLE = 0, IMASK = 1 */
    timer_write_control(2ULL);

    timer_enabled = 0;

    return SYNTRA_STATUS_OK;

#else

    return SYNTRA_STATUS_NOT_IMPLEMENTED;

#endif
}

/*
 * Handle a Physical Timer interrupt.
 *
 * The exception vector must route the IRQ here.
 * GIC acknowledgement and end-of-interrupt handling
 * belong to the interrupt controller layer.
 */
void syntra_arm64_timer_handle_irq(void)
{
#if defined(__aarch64__)

    if (!timer_initialized || !timer_enabled)
    {
        return;
    }

    (void)timer_tick();

    timer_write_value(timer_interval_ticks);

#endif
}

/*
 * Retrieve timer information.
 */
syntra_status_t syntra_arm64_timer_get_info(
    syntra_arm64_timer_info_t *info
)
{
    if (info == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

#if defined(__aarch64__)

    if (!timer_initialized)
    {
        return SYNTRA_STATUS_ERROR;
    }

    info->counter_frequency =
        timer_counter_frequency;

    info->counter_value =
        syntra_arm64_timer_read_counter();

    info->interval_ticks =
        timer_interval_ticks;

    info->tick_frequency =
        timer_tick_frequency;

    info->initialized =
        timer_initialized;

    info->enabled =
        timer_enabled;

    return SYNTRA_STATUS_OK;

#else

    return SYNTRA_STATUS_NOT_IMPLEMENTED;

#endif
}
