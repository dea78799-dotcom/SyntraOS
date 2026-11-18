
#ifndef SYNTRA_CLOCK_H
#define SYNTRA_CLOCK_H

#include "../core/status.h"

typedef struct
{
    unsigned long long total_milliseconds;

    unsigned int hours;
    unsigned int minutes;
    unsigned int seconds;
    unsigned int milliseconds;

    int initialized;
    int valid;

} syntra_clock_info_t;

/* Core API */

syntra_status_t clock_init(void);

syntra_status_t clock_update(
    unsigned long long elapsed_milliseconds
);

syntra_status_t clock_reset(void);

/* Time API */

unsigned long long clock_get_milliseconds(void);

unsigned int clock_get_seconds(void);

unsigned int clock_get_minutes(void);

unsigned int clock_get_hours(void);

/* Information API */

syntra_status_t clock_get_info(
    syntra_clock_info_t *info
);

/* Validation API */

syntra_status_t clock_validate(void);

#endif
