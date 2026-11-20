#ifndef SYNTRA_ARM64_TIMER_H
#define SYNTRA_ARM64_TIMER_H

#include <stdint.h>

#include "../../core/status.h"

/*
 * SyntraOS ARM64 Generic Timer
 * Version: 1.0.0
 *
 * Hardware timer interface for ARM64.
 */

#define SYNTRA_ARM64_TIMER_DEFAULT_HZ 100U

typedef struct
{
    uint64_t counter_frequency;
    uint64_t counter_value;
    uint64_t interval_ticks;

    uint32_t tick_frequency;

    int initialized;
    int enabled;

} syntra_arm64_timer_info_t;

/* Hardware Counter API */

uint64_t syntra_arm64_timer_read_counter(void);

uint64_t syntra_arm64_timer_read_frequency(void);

/* Physical Timer API */

syntra_status_t syntra_arm64_timer_init(
    uint32_t tick_frequency
);

syntra_status_t syntra_arm64_timer_set_interval(
    uint32_t tick_frequency
);

syntra_status_t syntra_arm64_timer_enable(void);

syntra_status_t syntra_arm64_timer_disable(void);

/* Interrupt API */

void syntra_arm64_timer_handle_irq(void);

/* Information API */

syntra_status_t syntra_arm64_timer_get_info(
    syntra_arm64_timer_info_t *info
);

#endif
