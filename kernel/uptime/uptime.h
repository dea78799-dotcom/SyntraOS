
#ifndef SYNTRA_UPTIME_H
#define SYNTRA_UPTIME_H

#include "../core/status.h"

typedef struct
{
    unsigned long long total_milliseconds;
    unsigned long long total_seconds;
    unsigned long long boot_count;

    int initialized;
    int valid;

} syntra_uptime_info_t;

/* Core API */

syntra_status_t uptime_init(void);

syntra_status_t uptime_update(
    unsigned long long elapsed_milliseconds
);

syntra_status_t uptime_reset(void);

/* Time API */

unsigned long long uptime_get_milliseconds(void);

unsigned long long uptime_get_seconds(void);

/* Information API */

syntra_status_t uptime_get_info(
    syntra_uptime_info_t *info
);

/* Validation API */

syntra_status_t uptime_validate(void);

#endif
