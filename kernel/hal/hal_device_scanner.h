
#ifndef SYNTRA_HAL_DEVICE_SCANNER_H
#define SYNTRA_HAL_DEVICE_SCANNER_H

#include "hal_device_discovery.h"

#define SYNTRA_HAL_SCANNER_MAX_RESULTS 128U

typedef enum
{
    SYNTRA_HAL_SCAN_STATUS_IDLE = 0,
    SYNTRA_HAL_SCAN_STATUS_RUNNING,
    SYNTRA_HAL_SCAN_STATUS_COMPLETED,
    SYNTRA_HAL_SCAN_STATUS_FAILED
} syntra_hal_scan_status_t;

typedef struct
{
    unsigned int device_id;
    syntra_hal_device_type_t type;
    syntra_hal_device_state_t state;
    int found;
} syntra_hal_scan_result_t;

typedef struct
{
    syntra_hal_device_discovery_t *discovery;

    syntra_hal_scan_result_t results[
        SYNTRA_HAL_SCANNER_MAX_RESULTS
    ];

    unsigned int result_count;
    unsigned int scanned_count;
    syntra_hal_scan_status_t status;
    int initialized;

} syntra_hal_device_scanner_t;

syntra_status_t hal_device_scanner_init(
    syntra_hal_device_scanner_t *scanner,
    syntra_hal_device_discovery_t *discovery
);

syntra_status_t hal_device_scanner_reset(
    syntra_hal_device_scanner_t *scanner
);

syntra_status_t hal_device_scanner_scan(
    syntra_hal_device_scanner_t *scanner
);

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

#endif
