
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

    unsigned int count = 0U;

    assert(hal_device_init(
        &device1,
        1U,
        "Timer",
        SYNTRA_HAL_DEVICE_TIMER
    ) == SYNTRA_STATUS_OK);

    assert(hal_device_init(
        &device2,
        2U,
        "Storage",
        SYNTRA_HAL_DEVICE_TIMER
    ) == SYNTRA_STATUS_OK);

    assert(hal_device_discovery_init(
        &discovery
    ) == SYNTRA_STATUS_OK);

    assert(hal_device_discovery_add(
        &discovery,
        &device1
    ) == SYNTRA_STATUS_OK);

    assert(hal_device_discovery_add(
        &discovery,
        &device2
    ) == SYNTRA_STATUS_OK);

    assert(discovery.device_count == 2U);
    assert(discovery.discovery_count == 2U);
    assert(discovery.event_count == 2U);

    assert(hal_device_discovery_get_count(
        &discovery,
        &count
    ) == SYNTRA_STATUS_OK);

    assert(count == 2U);

    assert(hal_device_discovery_get_event_count(
        &discovery,
        &count
    ) == SYNTRA_STATUS_OK);

    assert(count == 2U);

    assert(hal_device_discovery_get_event(
        &discovery,
        0U,
        &event
    ) == SYNTRA_STATUS_OK);

    assert(event.sequence == 1U);
    assert(event.device_id == 1U);
    assert(event.type == SYNTRA_HAL_DISCOVERY_EVENT_ADDED);

    assert(hal_device_discovery_get_event(
        &discovery,
        1U,
        &event
    ) == SYNTRA_STATUS_OK);

    assert(event.sequence == 2U);
    assert(event.device_id == 2U);
    assert(event.type == SYNTRA_HAL_DISCOVERY_EVENT_ADDED);

    assert(hal_device_discovery_find(
        &discovery,
        1U,
        &found
    ) == SYNTRA_STATUS_OK);

    assert(found == &device1);

    assert(hal_device_discovery_add(
        &discovery,
        &device1
    ) == SYNTRA_STATUS_ERROR);

    assert(hal_device_discovery_remove(
        &discovery,
        1U
    ) == SYNTRA_STATUS_OK);

    assert(discovery.device_count == 1U);
    assert(discovery.event_count == 3U);

    assert(hal_device_discovery_get_event(
        &discovery,
        2U,
        &event
    ) == SYNTRA_STATUS_OK);

    assert(event.sequence == 3U);
    assert(event.device_id == 1U);
    assert(event.type == SYNTRA_HAL_DISCOVERY_EVENT_REMOVED);

    assert(hal_device_discovery_find(
        &discovery,
        1U,
        &found
    ) == SYNTRA_STATUS_ERROR);

    assert(found == NULL);

    assert(hal_device_discovery_clear_events(
        &discovery
    ) == SYNTRA_STATUS_OK);

    assert(discovery.event_count == 0U);

    assert(hal_device_discovery_get_event(
        &discovery,
        0U,
        &event
    ) == SYNTRA_STATUS_ERROR);

    assert(hal_device_discovery_validate(
        &discovery
    ) == SYNTRA_STATUS_OK);

    assert(hal_device_discovery_reset(
        &discovery
    ) == SYNTRA_STATUS_OK);

    assert(discovery.device_count == 0U);
    assert(discovery.event_count == 0U);
    assert(discovery.discovery_count == 0U);

    assert(hal_device_discovery_validate(
        &discovery
    ) == SYNTRA_STATUS_OK);

    assert(hal_device_discovery_get_count(
        NULL,
        &count
    ) == SYNTRA_STATUS_ERROR);

    assert(hal_device_discovery_get_event_count(
        NULL,
        &count
    ) == SYNTRA_STATUS_ERROR);

    assert(hal_device_discovery_get_event(
        NULL,
        0U,
        &event
    ) == SYNTRA_STATUS_ERROR);

    assert(hal_device_discovery_clear_events(
        NULL
    ) == SYNTRA_STATUS_ERROR);

    assert(hal_device_discovery_validate(
        NULL
    ) == SYNTRA_STATUS_ERROR);

    return 0;
}
