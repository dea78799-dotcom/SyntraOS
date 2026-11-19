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

    syntra_hal_scan_event_t filtered_events[
        SYNTRA_HAL_SCANNER_MAX_EVENTS
    ];

    syntra_hal_scan_event_statistics_t statistics;
    syntra_hal_scan_validation_report_t validation_report;

    unsigned int count = 0U;
    unsigned int filtered_count = 0U;
    unsigned int previous_update_count;
    unsigned int i;

    /* Initialize devices */

    assert(hal_device_init(
        &device1, 1U, "Timer",
        SYNTRA_HAL_DEVICE_TIMER
    ) == SYNTRA_STATUS_OK);

    assert(hal_device_init(
        &device2, 2U, "Generic",
        SYNTRA_HAL_DEVICE_TIMER
    ) == SYNTRA_STATUS_OK);

    /* Initialize discovery */

    assert(hal_device_discovery_init(
        &discovery
    ) == SYNTRA_STATUS_OK);

    assert(hal_device_discovery_add(
        &discovery, &device1
    ) == SYNTRA_STATUS_OK);

    assert(hal_device_discovery_add(
        &discovery, &device2
    ) == SYNTRA_STATUS_OK);

    /* Initialize scanner */

    assert(hal_device_scanner_init(
        &scanner, &discovery
    ) == SYNTRA_STATUS_OK);

    assert(scanner.initialized == 1);
    assert(scanner.dirty == 0);

    assert(hal_device_scanner_get_status(
        &scanner, &status
    ) == SYNTRA_STATUS_OK);

    assert(status == SYNTRA_HAL_SCAN_STATUS_IDLE);

    /* Initial statistics */

    assert(hal_device_scanner_get_statistics(
        &scanner, &statistics
    ) == SYNTRA_STATUS_OK);

    assert(statistics.total == 0U);
    assert(statistics.started == 0U);
    assert(statistics.completed == 0U);
    assert(statistics.failed == 0U);
    assert(statistics.resets == 0U);

    /* First scan */

    assert(hal_device_scanner_scan(
        &scanner
    ) == SYNTRA_STATUS_OK);

    assert(scanner.status == SYNTRA_HAL_SCAN_STATUS_COMPLETED);
    assert(scanner.result_count == 2U);
    assert(scanner.scanned_count == 2U);

    assert(hal_device_scanner_get_count(
        &scanner, &count
    ) == SYNTRA_STATUS_OK);

    assert(count == 2U);

    /* First result */

    assert(hal_device_scanner_get_result(
        &scanner, 0U, &result
    ) == SYNTRA_STATUS_OK);

    assert(result.device_id == 1U);
    assert(result.type == SYNTRA_HAL_DEVICE_TIMER);
    assert(result.found == 1);

    /* Second result */

    assert(hal_device_scanner_get_result(
        &scanner, 1U, &result
    ) == SYNTRA_STATUS_OK);

    assert(result.device_id == 2U);
    assert(result.type == SYNTRA_HAL_DEVICE_TIMER);
    assert(result.found == 1);

    /* Check events */

    assert(hal_device_scanner_get_event_count(
        &scanner, &count
    ) == SYNTRA_STATUS_OK);

    assert(count == 2U);

    assert(hal_device_scanner_get_event(
        &scanner, 0U, &event
    ) == SYNTRA_STATUS_OK);

    assert(event.sequence == 1U);
    assert(event.type == SYNTRA_HAL_SCAN_EVENT_STARTED);

    assert(hal_device_scanner_get_event(
        &scanner, 1U, &event
    ) == SYNTRA_STATUS_OK);

    assert(event.sequence == 2U);
    assert(event.type == SYNTRA_HAL_SCAN_EVENT_COMPLETED);
    assert(event.device_count == 2U);

    /* Statistics after first scan */

    assert(hal_device_scanner_get_statistics(
        &scanner, &statistics
    ) == SYNTRA_STATUS_OK);

    assert(statistics.total == 2U);
    assert(statistics.started == 1U);
    assert(statistics.completed == 1U);
    assert(statistics.failed == 0U);
    assert(statistics.resets == 0U);

    /* Filter STARTED events */

    assert(hal_device_scanner_find_events_by_type(
        &scanner,
        SYNTRA_HAL_SCAN_EVENT_STARTED,
        filtered_events,
        SYNTRA_HAL_SCANNER_MAX_EVENTS,
        &filtered_count
    ) == SYNTRA_STATUS_OK);

    assert(filtered_count == 1U);

    /* Filter COMPLETED events */

    assert(hal_device_scanner_find_events_by_type(
        &scanner,
        SYNTRA_HAL_SCAN_EVENT_COMPLETED,
        filtered_events,
        SYNTRA_HAL_SCANNER_MAX_EVENTS,
        &filtered_count
    ) == SYNTRA_STATUS_OK);

    assert(filtered_count == 1U);

    /* Filter RESET events */

    assert(hal_device_scanner_find_events_by_type(
        &scanner,
        SYNTRA_HAL_SCAN_EVENT_RESET,
        filtered_events,
        SYNTRA_HAL_SCANNER_MAX_EVENTS,
        &filtered_count
    ) == SYNTRA_STATUS_OK);

    assert(filtered_count == 0U);

    /* Reset scanner */

    assert(hal_device_scanner_reset(
        &scanner
    ) == SYNTRA_STATUS_OK);

    assert(scanner.result_count == 0U);
    assert(scanner.scanned_count == 0U);
    assert(scanner.status == SYNTRA_HAL_SCAN_STATUS_IDLE);

    assert(hal_device_scanner_get_event_count(
        &scanner, &count
    ) == SYNTRA_STATUS_OK);

    assert(count == 3U);

    assert(hal_device_scanner_get_event(
        &scanner, 2U, &event
    ) == SYNTRA_STATUS_OK);

    assert(event.type == SYNTRA_HAL_SCAN_EVENT_RESET);

    /* Statistics after reset */

    assert(hal_device_scanner_get_statistics(
        &scanner, &statistics
    ) == SYNTRA_STATUS_OK);

    assert(statistics.total == 3U);
    assert(statistics.started == 1U);
    assert(statistics.completed == 1U);
    assert(statistics.failed == 0U);
    assert(statistics.resets == 1U);

    /* Scan again */

    assert(hal_device_scanner_scan(
        &scanner
    ) == SYNTRA_STATUS_OK);

    assert(scanner.result_count == 2U);

    /* Updated statistics */

    assert(hal_device_scanner_get_statistics(
        &scanner, &statistics
    ) == SYNTRA_STATUS_OK);

    assert(statistics.total == 5U);
    assert(statistics.started == 2U);
    assert(statistics.completed == 2U);
    assert(statistics.failed == 0U);
    assert(statistics.resets == 1U);

    /* Clear events */

    assert(hal_device_scanner_clear_events(
        &scanner
    ) == SYNTRA_STATUS_OK);

    assert(hal_device_scanner_get_event_count(
        &scanner, &count
    ) == SYNTRA_STATUS_OK);

    assert(count == 0U);
    assert(scanner.event_sequence == 0U);

    /* Statistics after clear */

    assert(hal_device_scanner_get_statistics(
        &scanner, &statistics
    ) == SYNTRA_STATUS_OK);

    assert(statistics.total == 0U);
    assert(statistics.started == 0U);
    assert(statistics.completed == 0U);
    assert(statistics.failed == 0U);
    assert(statistics.resets == 0U);

    /* Fill event history */

    for (i = 0U; i < 70U; i++)
    {
        assert(hal_device_scanner_reset(
            &scanner
        ) == SYNTRA_STATUS_OK);
    }

    assert(scanner.event_count == SYNTRA_HAL_SCANNER_MAX_EVENTS);

    /* Oldest retained event */

    assert(hal_device_scanner_get_event(
        &scanner, 0U, &event
    ) == SYNTRA_STATUS_OK);

    assert(event.sequence == 7U);
    assert(event.type == SYNTRA_HAL_SCAN_EVENT_RESET);

    /* Latest retained event */

    assert(hal_device_scanner_get_event(
        &scanner, 63U, &event
    ) == SYNTRA_STATUS_OK);

    assert(event.sequence == 70U);
    assert(event.type == SYNTRA_HAL_SCAN_EVENT_RESET);

    /* Filter after history overflow */

    assert(hal_device_scanner_find_events_by_type(
        &scanner,
        SYNTRA_HAL_SCAN_EVENT_RESET,
        filtered_events,
        SYNTRA_HAL_SCANNER_MAX_EVENTS,
        &filtered_count
    ) == SYNTRA_STATUS_OK);

    assert(filtered_count == SYNTRA_HAL_SCANNER_MAX_EVENTS);

    /* Statistics after overflow */

    assert(hal_device_scanner_get_statistics(
        &scanner, &statistics
    ) == SYNTRA_STATUS_OK);

    assert(statistics.total == SYNTRA_HAL_SCANNER_MAX_EVENTS);
    assert(statistics.started == 0U);
    assert(statistics.completed == 0U);
    assert(statistics.failed == 0U);
    assert(statistics.resets == SYNTRA_HAL_SCANNER_MAX_EVENTS);

    /* Validate history */

    assert(hal_device_scanner_validate(
        &scanner
    ) == SYNTRA_STATUS_OK);

    assert(hal_device_scanner_get_validation_report(
        &scanner, &validation_report
    ) == SYNTRA_STATUS_OK);

    assert(validation_report.invalid_events == 0U);
    assert(validation_report.duplicate_sequences == 0U);
    assert(validation_report.out_of_order_sequences == 0U);
    assert(validation_report.sequence_gaps == 0U);

    /* Update statistics */

    previous_update_count = scanner.update_count;

    assert(hal_device_scanner_update_statistics(
        &scanner
    ) == SYNTRA_STATUS_OK);

    assert(scanner.update_count > previous_update_count);
    assert(scanner.dirty == 0);

    /* Invalid arguments */

    assert(hal_device_scanner_init(
        NULL, &discovery
    ) == SYNTRA_STATUS_INVALID_ARGUMENT);

    assert(hal_device_scanner_reset(
        NULL
    ) == SYNTRA_STATUS_INVALID_ARGUMENT);

    assert(hal_device_scanner_scan(
        NULL
    ) == SYNTRA_STATUS_INVALID_ARGUMENT);

    assert(hal_device_scanner_get_result(
        &scanner, 0U, NULL
    ) == SYNTRA_STATUS_INVALID_ARGUMENT);

    assert(hal_device_scanner_get_count(
        &scanner, NULL
    ) == SYNTRA_STATUS_INVALID_ARGUMENT);

    assert(hal_device_scanner_get_status(
        &scanner, NULL
    ) == SYNTRA_STATUS_INVALID_ARGUMENT);

    assert(hal_device_scanner_get_event_count(
        &scanner, NULL
    ) == SYNTRA_STATUS_INVALID_ARGUMENT);

    assert(hal_device_scanner_get_event(
        &scanner, 0U, NULL
    ) == SYNTRA_STATUS_INVALID_ARGUMENT);

    /* Invalid event index */

    assert(hal_device_scanner_get_event(
        &scanner, scanner.event_count, &event
    ) == SYNTRA_STATUS_ERROR);

    assert(hal_device_scanner_clear_events(
        NULL
    ) == SYNTRA_STATUS_INVALID_ARGUMENT);

    assert(hal_device_scanner_get_statistics(
        &scanner, NULL
    ) == SYNTRA_STATUS_INVALID_ARGUMENT);

    assert(hal_device_scanner_get_validation_report(
        &scanner, NULL
    ) == SYNTRA_STATUS_INVALID_ARGUMENT);

    /* NULL output array with zero capacity is valid */

    filtered_count = 99U;

    assert(hal_device_scanner_find_events_by_type(
        &scanner,
        SYNTRA_HAL_SCAN_EVENT_STARTED,
        NULL,
        0U,
        &filtered_count
    ) == SYNTRA_STATUS_OK);

    assert(filtered_count == 0U);

    /* Invalid event type */

    assert(hal_device_scanner_find_events_by_type(
        &scanner,
        SYNTRA_HAL_SCAN_EVENT_TYPE_COUNT,
        filtered_events,
        SYNTRA_HAL_SCANNER_MAX_EVENTS,
        &filtered_count
    ) == SYNTRA_STATUS_INVALID_ARGUMENT);

    assert(hal_device_scanner_update_statistics(
        NULL
    ) == SYNTRA_STATUS_INVALID_ARGUMENT);

    assert(hal_device_scanner_validate(
        NULL
    ) == SYNTRA_STATUS_INVALID_ARGUMENT);

    return 0;
}
