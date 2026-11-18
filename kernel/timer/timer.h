
#ifndef SYNTRA_TIMER_H
#define SYNTRA_TIMER_H

#include "../core/status.h"

typedef struct
{
    unsigned long long ticks;
    unsigned long long elapsed_milliseconds;
    unsigned int tick_frequency;
    int initialized;
    int valid;

} syntra_timer_info_t;

/* Core API */

syntra_status_t timer_init(
    unsigned int tick_frequency
);

syntra_status_t timer_tick(void);

syntra_status_t timer_reset(void);

/* Time API */

unsigned long long timer_get_ticks(void);

unsigned long long timer_get_milliseconds(void);

/* Information API */

syntra_status_t timer_get_info(
    syntra_timer_info_t *info
);

/* Validation API */

syntra_status_t timer_validate(void);

#endif
