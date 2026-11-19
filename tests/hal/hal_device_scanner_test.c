
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
    syntra_hal_scan_event_t event;

    unsigned int count = 0U;
    unsigned int i;

    assert(hal_device_init(&device1, 1U, "Timer",
        SYNTRA_HAL_DEVICE_TIMER) == SYNTRA_STATUS_OK);

    assert(hal_device_init(&device2, 2U, "Generic",
        SYNTRA_HAL_DEVICE_TIMER) == SYNTRA_STATUS_OK);

    assert(hal_device_discovery_init(&discovery) == SYNTRA_STATUS_OK);

    assert(hal_device_discovery_add(&discovery, &device1) == SYNTRA_STATUS_OK);
    assert(hal_device_discovery_add(&discovery, &device2) == SYNTRA_STATUS_OK);

    assert(hal_device_scanner_init(&scanner, &discovery) == SYNTRA_STATUS_OK);

    assert(hal_device_scanner_get_status(&scanner, &status) == SYNTRA_STATUS_OK);
    assert(status == SYNTRA_HAL_SCAN_STATUS_IDLE);

    /* First scan */

    assert(hal_device_scanner_scan(&scanner) == SYNTRA_STATUS_OK);
    assert(scanner.status == SYNTRA_HAL_SCAN_STATUS_COMPLETED);
    assert(scanner.result_count == 2U);
    assert(scanner.scanned_count == 2U);

    assert(hal_device_scanner_get_count(&scanner, &count) == SYNTRA_STATUS_OK);
    assert(count == 2U);

    /* Check results */

    assert(hal_device_scanner_get_result(&scanner, 0U, &result) == SYNTRA_STATUS_OK);
    assert(result.device_id == 1U);
    assert(result.type == SYNTRA_HAL_DEVICE_TIMER);
    assert(result.found == 1);

    assert(hal_device_scanner_get_result(&scanner, 1U, &result) == SYNTRA_STATUS_OK);
    assert(result.device_id == 2U);
    assert(result.found == 1);

    /* Check scan events */

    assert(hal_device_scanner_get_event_count(&scanner, &count) == SYNTRA_STATUS_OK);
    assert(count == 2U);

    assert(hal_device_scanner_get_event(&scanner, 0U, &event) == SYNTRA_STATUS_OK);
    assert(event.sequence == 1U);
    assert(event.type == SYNTRA_HAL_SCAN_EVENT_STARTED);

    assert(hal_device_scanner_get_event(&scanner, 1U, &event) == SYNTRA_STATUS_OK);
    assert(event.sequence == 2U);
    assert(event.type == SYNTRA_HAL_SCAN_EVENT_COMPLETED);
    assert(event.device_count == 2U);

    /* Reset */

    assert(hal_device_scanner_reset(&scanner) == SYNTRA_STATUS_OK);
    assert(scanner.result_count == 0U);
    assert(scanner.status == SYNTRA_HAL_SCAN_STATUS_IDLE);

    assert(hal_device_scanner_get_event_count(&scanner, &count) == SYNTRA_STATUS_OK);
    assert(count == 3U);

    assert(hal_device_scanner_get_event(&scanner, 2U, &event) == SYNTRA_STATUS_OK);
    assert(event.type == SYNTRA_HAL_SCAN_EVENT_RESET);

    /* Scan again */

    assert(hal_device_scanner_scan(&scanner) == SYNTRA_STATUS_OK);
    assert(scanner.result_count == 2U);

    /* Clear events */

    assert(hal_device_scanner_clear_events(&scanner) == SYNTRA_STATUS_OK);

    assert(hal_device_scanner_get_event_count(&scanner, &count) == SYNTRA_STATUS_OK);
    assert(count == 0U);

    /* Fill event history and test capacity */

    for (i = 0U; i < 70U; i++)
    {
        assert(hal_device_scanner_reset(&scanner) == SYNTRA_STATUS_OK);
    }

    assert(scanner.event_count == SYNTRA_HAL_SCANNER_MAX_EVENTS);

    assert(hal_device_scanner_get_event(&scanner, 0U, &event) == SYNTRA_STATUS_OK);

    /* Six old events are discarded; sequence starts at 7. */
    assert(event.sequence == 7U);

    /* Invalid arguments */

    assert(hal_device_scanner_init(NULL, &discovery) == SYNTRA_STATUS_ERROR);
    assert(hal_device_scanner_reset(NULL) == SYNTRA_STATUS_ERROR);
    assert(hal_device_scanner_scan(NULL) == SYNTRA_STATUS_ERROR);

    assert(hal_device_scanner_get_result(&scanner, 0U, NULL) == SYNTRA_STATUS_ERROR);
    assert(hal_device_scanner_get_count(&scanner, NULL) == SYNTRA_STATUS_ERROR);
    assert(hal_device_scanner_get_status(&scanner, NULL) == SYNTRA_STATUS_ERROR);
    assert(hal_device_scanner_get_event_count(&scanner, NULL) == SYNTRA_STATUS_ERROR);
    assert(hal_device_scanner_get_event(&scanner, 0U, NULL) == SYNTRA_STATUS_ERROR);

    assert(hal_device_scanner_get_event(
        &scanner, scanner.event_count, &event
    ) == SYNTRA_STATUS_ERROR);

    assert(hal_device_scanner_clear_events(NULL) == SYNTRA_STATUS_ERROR);

    return 0;
}
