
#ifndef SYNTRA_HAL_DEVICE_SCANNER_H
#define SYNTRA_HAL_DEVICE_SCANNER_H

#include "hal_device_discovery.h"

#define SYNTRA_HAL_SCANNER_MAX_RESULTS 128U
#define SYNTRA_HAL_SCANNER_MAX_EVENTS 64U

typedef enum
{
    SYNTRA_HAL_SCAN_STATUS_IDLE = 0,
    SYNTRA_HAL_SCAN_STATUS_RUNNING,
    SYNTRA_HAL_SCAN_STATUS_COMPLETED,
    SYNTRA_HAL_SCAN_STATUS_FAILED
} syntra_hal_scan_status_t;

typedef enum
{
    SYNTRA_HAL_SCAN_EVENT_STARTED = 0,
    SYNTRA_HAL_SCAN_EVENT_COMPLETED,
    SYNTRA_HAL_SCAN_EVENT_FAILED,
    SYNTRA_HAL_SCAN_EVENT_RESET,
    SYNTRA_HAL_SCAN_EVENT_TYPE_COUNT
} syntra_hal_scan_event_type_t;

typedef struct
{
    unsigned int device_id;
    syntra_hal_device_type_t type;
    syntra_hal_device_state_t state;
    int found;
} syntra_hal_scan_result_t;

typedef struct
{
    unsigned int sequence;
    syntra_hal_scan_event_type_t type;
    unsigned int device_count;
} syntra_hal_scan_event_t;

/* V3: Event statistics */

typedef struct
{
    unsigned int total;
    unsigned int started;
    unsigned int completed;
    unsigned int failed;
    unsigned int resets;
} syntra_hal_scan_event_statistics_t;

/* V3: Validation report */

typedef struct
{
    unsigned int invalid_events;
    unsigned int duplicate_sequences;
    unsigned int out_of_order_sequences;
    unsigned int sequence_gaps;
} syntra_hal_scan_validation_report_t;

/* V4: Query filter */

typedef struct
{
    unsigned int device_id;
    syntra_hal_device_type_t type;
    syntra_hal_device_state_t state;

    int match_device_id;
    int match_type;
    int match_state;
    int match_found;
} syntra_hal_scan_query_t;

/* V4: Query result */

typedef struct
{
    unsigned int matched_count;
    unsigned int total_count;
} syntra_hal_scan_query_report_t;

typedef struct
{
    syntra_hal_device_discovery_t *discovery;

    syntra_hal_scan_result_t results[
        SYNTRA_HAL_SCANNER_MAX_RESULTS
    ];

    syntra_hal_scan_event_t events[
        SYNTRA_HAL_SCANNER_MAX_EVENTS
    ];

    unsigned int result_count;
    unsigned int scanned_count;

    unsigned int event_count;
    unsigned int event_sequence;

    /* V3 statistics */
    syntra_hal_scan_event_statistics_t statistics;
    unsigned int update_count;
    int dirty;

    /* V4 query tracking */
    unsigned int query_count;
    unsigned int last_query_count;

    syntra_hal_scan_status_t status;
    int initialized;

} syntra_hal_device_scanner_t;

/* Lifecycle */

syntra_status_t hal_device_scanner_init(
    syntra_hal_device_scanner_t *scanner,
    syntra_hal_device_discovery_t *discovery
);

syntra_status_t hal_device_scanner_reset(
    syntra_hal_device_scanner_t *scanner
);

/* Scanning */

syntra_status_t hal_device_scanner_scan(
    syntra_hal_device_scanner_t *scanner
);

/* Results */

syntra_status_t hal_device_scanner_get_result(
    const syntra_hal_device_scanner_t *scanner,
    unsigned int index,
    syntra_hal_scan_result_t *result
);

syntra_status_t hal_device_scanner_get_count(
    const syntra_hal_device_scanner_t *scanner,
    unsigned int *count
);

syntra_status_t hal_device_scanner_get_status(
    const syntra_hal_device_scanner_t *scanner,
    syntra_hal_scan_status_t *status
);

/* Event history */

syntra_status_t hal_device_scanner_get_event_count(
    const syntra_hal_device_scanner_t *scanner,
    unsigned int *count
);

syntra_status_t hal_device_scanner_get_event(
    const syntra_hal_device_scanner_t *scanner,
    unsigned int index,
    syntra_hal_scan_event_t *event
);

syntra_status_t hal_device_scanner_clear_events(
    syntra_hal_device_scanner_t *scanner
);

/* V3: Event filtering */

syntra_status_t hal_device_scanner_find_events_by_type(
    const syntra_hal_device_scanner_t *scanner,
    syntra_hal_scan_event_type_t type,
    syntra_hal_scan_event_t *events,
    unsigned int capacity,
    unsigned int *found
);

/* V3: Statistics */

syntra_status_t hal_device_scanner_update_statistics(
    syntra_hal_device_scanner_t *scanner
);

syntra_status_t hal_device_scanner_get_statistics(
    const syntra_hal_device_scanner_t *scanner,
    syntra_hal_scan_event_statistics_t *statistics
);

/* V3: Validation */

syntra_status_t hal_device_scanner_validate(
    const syntra_hal_device_scanner_t *scanner
);

syntra_status_t hal_device_scanner_get_validation_report(
    const syntra_hal_device_scanner_t *scanner,
    syntra_hal_scan_validation_report_t *report
);

/* V4: Result queries */

syntra_status_t hal_device_scanner_find_by_device_id(
    const syntra_hal_device_scanner_t *scanner,
    unsigned int device_id,
    syntra_hal_scan_result_t *result
);

syntra_status_t hal_device_scanner_query(
    syntra_hal_device_scanner_t *scanner,
    const syntra_hal_scan_query_t *query,
    syntra_hal_scan_result_t *results,
    unsigned int capacity,
    syntra_hal_scan_query_report_t *report
);

syntra_status_t hal_device_scanner_count_matches(
    const syntra_hal_device_scanner_t *scanner,
    const syntra_hal_scan_query_t *query,
    unsigned int *count
);

#endif
