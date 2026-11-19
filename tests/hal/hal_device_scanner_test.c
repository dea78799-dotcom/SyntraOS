
#include "hal_device_scanner.h"

#include <assert.h>
#include <stddef.h>

int main(void)
{
    syntra_hal_device_discovery_t discovery;
    syntra_hal_device_scanner_t scanner;

    syntra_hal_device_t device1;
    syntra_hal_device_t device2;

    syntra_hal_scan_result_t result;
    syntra_hal_scan_status_t status;

    unsigned int count = 0U;

    /* Initialize devices */

    assert(
        hal_device_init(
            &device1,
            1U,
            "Timer",
            SYNTRA_HAL_DEVICE_TIMER
        ) == SYNTRA_STATUS_OK
    );

    assert(
        hal_device_init(
            &device2,
            2U,
            "Generic",
            SYNTRA_HAL_DEVICE_TIMER
        ) == SYNTRA_STATUS_OK
    );

    /* Initialize discovery */

    assert(
        hal_device_discovery_init(&discovery)
        == SYNTRA_STATUS_OK
    );

    assert(
        hal_device_discovery_add(
            &discovery,
            &device1
        ) == SYNTRA_STATUS_OK
    );

    assert(
        hal_device_discovery_add(
            &discovery,
            &device2
        ) == SYNTRA_STATUS_OK
    );

    /* Initialize scanner */

    assert(
        hal_device_scanner_init(
            &scanner,
            &discovery
        ) == SYNTRA_STATUS_OK
    );

    assert(
        hal_device_scanner_get_status(
            &scanner,
            &status
        ) == SYNTRA_STATUS_OK
    );

    assert(status == SYNTRA_HAL_SCAN_STATUS_IDLE);

    /* Run scan */

    assert(
        hal_device_scanner_scan(&scanner)
        == SYNTRA_STATUS_OK
    );

    assert(
        hal_device_scanner_get_status(
            &scanner,
            &status
        ) == SYNTRA_STATUS_OK
    );

    assert(status == SYNTRA_HAL_SCAN_STATUS_COMPLETED);

    /* Check result count */

    assert(
        hal_device_scanner_get_count(
            &scanner,
            &count
        ) == SYNTRA_STATUS_OK
    );

    assert(count == 2U);
    assert(scanner.scanned_count == 2U);

    /* Check first result */

    assert(
        hal_device_scanner_get_result(
            &scanner,
            0U,
            &result
        ) == SYNTRA_STATUS_OK
    );

    assert(result.device_id == 1U);
    assert(result.type == SYNTRA_HAL_DEVICE_TIMER);
    assert(result.found == 1);

    /* Check second result */

    assert(
        hal_device_scanner_get_result(
            &scanner,
            1U,
            &result
        ) == SYNTRA_STATUS_OK
    );

    assert(result.device_id == 2U);
    assert(result.found == 1);

    /* Invalid result index */

    assert(
        hal_device_scanner_get_result(
            &scanner,
            2U,
            &result
        ) == SYNTRA_STATUS_ERROR
    );

    /* Reset scanner */

    assert(
        hal_device_scanner_reset(&scanner)
        == SYNTRA_STATUS_OK
    );

    assert(
        hal_device_scanner_get_count(
            &scanner,
            &count
        ) == SYNTRA_STATUS_OK
    );

    assert(count == 0U);

    assert(
        hal_device_scanner_get_status(
            &scanner,
            &status
        ) == SYNTRA_STATUS_OK
    );

    assert(status == SYNTRA_HAL_SCAN_STATUS_IDLE);

    /* Scan again */

    assert(
        hal_device_scanner_scan(&scanner)
        == SYNTRA_STATUS_OK
    );

    assert(scanner.result_count == 2U);

    /* Invalid arguments */

    assert(
        hal_device_scanner_init(
            NULL,
            &discovery
        ) == SYNTRA_STATUS_ERROR
    );

    assert(
        hal_device_scanner_reset(NULL)
        == SYNTRA_STATUS_ERROR
    );

    assert(
        hal_device_scanner_scan(NULL)
        == SYNTRA_STATUS_ERROR
    );

    assert(
        hal_device_scanner_get_count(
            &scanner,
            NULL
        ) == SYNTRA_STATUS_ERROR
    );

    assert(
        hal_device_scanner_get_status(
            &scanner,
            NULL
        ) == SYNTRA_STATUS_ERROR
    );

    assert(
        hal_device_scanner_get_result(
            &scanner,
            0U,
            NULL
        ) == SYNTRA_STATUS_ERROR
    );

    return 0;
}
