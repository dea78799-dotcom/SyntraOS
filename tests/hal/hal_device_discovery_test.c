
#include "hal_device_discovery.h"

#include <assert.h>
#include <stddef.h>

int main(void)
{
    syntra_hal_device_t device1;
    syntra_hal_device_t device2;

    syntra_hal_device_discovery_t discovery;

    syntra_hal_device_t *found = NULL;

    syntra_hal_discovery_event_t event;
    syntra_hal_discovery_stats_t stats;
    syntra_hal_discovery_validation_report_t report;

    unsigned int count = 0U;

    assert(hal_device_init(
        &device1, 1U, "Timer",
        SYNTRA_HAL_DEVICE_TIMER
    ) == SYNTRA_STATUS_OK);

    assert(hal_device_init(
        &device2, 2U, "Storage",
        SYNTRA_HAL_DEVICE_TIMER
    ) == SYNTRA_STATUS_OK);

    assert(hal_device_discovery_init(
        &discovery
    ) == SYNTRA_STATUS_OK);

    assert(hal_device_discovery_add(
        &discovery, &device1
    ) == SYNTRA_STATUS_OK);

    assert(hal_device_discovery_add(
        &discovery, &device2
    ) == SYNTRA_STATUS_OK);

    assert(hal_device_discovery_remove(
        &discovery, 1U
    ) == SYNTRA_STATUS_OK);

    assert(discovery.device_count == 1U);
    assert(discovery.event_count == 3U);

    assert(hal_device_discovery_find(
        &discovery, 2U, &found
    ) == SYNTRA_STATUS_OK);

    assert(found == &device2);

    assert(hal_device_discovery_find_event(
        &discovery, 1U, 0U, &event
    ) == SYNTRA_STATUS_OK);

    assert(event.type == SYNTRA_HAL_DISCOVERY_EVENT_ADDED);

    assert(hal_device_discovery_get_event_by_type(
        &discovery,
        SYNTRA_HAL_DISCOVERY_EVENT_REMOVED,
        0U,
        &event
    ) == SYNTRA_STATUS_OK);

    assert(event.device_id == 1U);

    assert(hal_device_discovery_get_stats(
        &discovery, &stats
    ) == SYNTRA_STATUS_OK);

    assert(stats.total_events == 3U);
    assert(stats.added_events == 2U);
    assert(stats.removed_events == 1U);
    assert(stats.reset_events == 0U);

    assert(hal_device_discovery_validate_history(
        &discovery
    ) == SYNTRA_STATUS_OK);

    assert(hal_device_discovery_get_validation_report(
        &discovery, &report
    ) == SYNTRA_STATUS_OK);

    assert(report.invalid_events == 0U);
    assert(report.duplicate_sequences == 0U);
    assert(report.out_of_order_events == 0U);
    assert(report.sequence_gaps == 0U);

    assert(hal_device_discovery_validate(
        &discovery
    ) == SYNTRA_STATUS_OK);

    assert(hal_device_discovery_get_count(
        &discovery, &count
    ) == SYNTRA_STATUS_OK);

    assert(count == 1U);

    assert(hal_device_discovery_update_stats(
        &discovery
    ) == SYNTRA_STATUS_OK);

    assert(discovery.dirty == 0);

    assert(hal_device_discovery_clear_events(
        &discovery
    ) == SYNTRA_STATUS_OK);

    assert(discovery.event_count == 0U);

    assert(hal_device_discovery_get_stats(
        &discovery, &stats
    ) == SYNTRA_STATUS_OK);

    assert(stats.total_events == 0U);
    assert(stats.added_events == 0U);
    assert(stats.removed_events == 0U);

    assert(hal_device_discovery_validate_history(
        &discovery
    ) == SYNTRA_STATUS_OK);

    assert(hal_device_discovery_get_validation_report(
        &discovery, &report
    ) == SYNTRA_STATUS_OK);

    assert(report.invalid_events == 0U);
    assert(report.duplicate_sequences == 0U);
    assert(report.out_of_order_events == 0U);
    assert(report.sequence_gaps == 0U);

    assert(hal_device_discovery_find_event(
        &discovery, 1U, 0U, &event
    ) == SYNTRA_STATUS_ERROR);

    assert(hal_device_discovery_get_event(
        &discovery, 0U, &event
    ) == SYNTRA_STATUS_ERROR);

    assert(hal_device_discovery_get_validation_report(
        NULL, &report
    ) == SYNTRA_STATUS_ERROR);

    assert(hal_device_discovery_validate_history(
        NULL
    ) == SYNTRA_STATUS_ERROR);

    assert(hal_device_discovery_update_stats(
        NULL
    ) == SYNTRA_STATUS_ERROR);

    assert(hal_device_discovery_validate(
        NULL
    ) == SYNTRA_STATUS_ERROR);

    assert(hal_device_discovery_reset(
        &discovery
    ) == SYNTRA_STATUS_OK);

    assert(discovery.device_count == 0U);
    assert(discovery.event_count == 0U);
    assert(discovery.discovery_count == 0U);

    assert(hal_device_discovery_validate(
        &discovery
    ) == SYNTRA_STATUS_OK);

    return 0;
}
