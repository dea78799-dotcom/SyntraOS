
#include "hal_device_scanner.h"

#include <stddef.h>
#include <string.h>

static syntra_status_t scanner_validate(
    const syntra_hal_device_scanner_t *scanner
)
{
    if (scanner == NULL)
    {
        return SYNTRA_STATUS_ERROR;
    }

    if (scanner->initialized == 0 ||
        scanner->discovery == NULL ||
        scanner->discovery->initialized == 0)
    {
        return SYNTRA_STATUS_ERROR;
    }

    return SYNTRA_STATUS_OK;
}

static void scanner_record_event(
    syntra_hal_device_scanner_t *scanner,
    syntra_hal_scan_event_type_t type,
    unsigned int device_count
)
{
    unsigned int i;

    if (scanner->event_count >=
        SYNTRA_HAL_SCANNER_MAX_EVENTS)
    {
        for (i = 1U;
             i < SYNTRA_HAL_SCANNER_MAX_EVENTS;
             i++)
        {
            scanner->events[i - 1U] =
                scanner->events[i];
        }

        scanner->event_count--;
    }

    scanner->event_sequence++;

    scanner->events[scanner->event_count].sequence =
        scanner->event_sequence;

    scanner->events[scanner->event_count].type =
        type;

    scanner->events[scanner->event_count].device_count =
        device_count;

    scanner->event_count++;
}

syntra_status_t hal_device_scanner_init(
    syntra_hal_device_scanner_t *scanner,
    syntra_hal_device_discovery_t *discovery
)
{
    if (scanner == NULL || discovery == NULL)
    {
        return SYNTRA_STATUS_ERROR;
    }

    if (discovery->initialized == 0)
    {
        return SYNTRA_STATUS_ERROR;
    }

    memset(scanner, 0, sizeof(*scanner));

    scanner->discovery = discovery;
    scanner->status = SYNTRA_HAL_SCAN_STATUS_IDLE;
    scanner->initialized = 1;

    return SYNTRA_STATUS_OK;
}

syntra_status_t hal_device_scanner_reset(
    syntra_hal_device_scanner_t *scanner
)
{
    if (scanner_validate(scanner) != SYNTRA_STATUS_OK)
    {
        return SYNTRA_STATUS_ERROR;
    }

    memset(
        scanner->results,
        0,
        sizeof(scanner->results)
    );

    scanner->result_count = 0U;
    scanner->scanned_count = 0U;
    scanner->status = SYNTRA_HAL_SCAN_STATUS_IDLE;

    scanner_record_event(
        scanner,
        SYNTRA_HAL_SCAN_EVENT_RESET,
        0U
    );

    return SYNTRA_STATUS_OK;
}

syntra_status_t hal_device_scanner_scan(
    syntra_hal_device_scanner_t *scanner
)
{
    unsigned int device_count;
    unsigned int i;

    if (scanner_validate(scanner) != SYNTRA_STATUS_OK)
    {
        return SYNTRA_STATUS_ERROR;
    }

    scanner->status = SYNTRA_HAL_SCAN_STATUS_RUNNING;
    scanner->result_count = 0U;
    scanner->scanned_count = 0U;

    memset(
        scanner->results,
        0,
        sizeof(scanner->results)
    );

    scanner_record_event(
        scanner,
        SYNTRA_HAL_SCAN_EVENT_STARTED,
        0U
    );

    device_count = scanner->discovery->device_count;

    if (device_count >
        SYNTRA_HAL_DEVICE_DISCOVERY_MAX_DEVICES ||
        device_count > SYNTRA_HAL_SCANNER_MAX_RESULTS)
    {
        scanner->status = SYNTRA_HAL_SCAN_STATUS_FAILED;

        scanner_record_event(
            scanner,
            SYNTRA_HAL_SCAN_EVENT_FAILED,
            scanner->scanned_count
        );

        return SYNTRA_STATUS_ERROR;
    }

    for (i = 0U; i < device_count; i++)
    {
        syntra_hal_device_t *device;
        syntra_hal_scan_result_t *result;

        device = scanner->discovery->devices[i];

        if (device == NULL || device->initialized == 0)
        {
            scanner->status = SYNTRA_HAL_SCAN_STATUS_FAILED;

            scanner_record_event(
                scanner,
                SYNTRA_HAL_SCAN_EVENT_FAILED,
                scanner->scanned_count
            );

            return SYNTRA_STATUS_ERROR;
        }

        result = &scanner->results[scanner->result_count];

        result->device_id = device->device_id;
        result->type = device->type;
        result->state = device->state;
        result->found = 1;

        scanner->result_count++;
        scanner->scanned_count++;
    }

    scanner->status = SYNTRA_HAL_SCAN_STATUS_COMPLETED;

    scanner_record_event(
        scanner,
        SYNTRA_HAL_SCAN_EVENT_COMPLETED,
        scanner->scanned_count
    );

    return SYNTRA_STATUS_OK;
}

syntra_status_t hal_device_scanner_get_result(
    const syntra_hal_device_scanner_t *scanner,
    unsigned int index,
    syntra_hal_scan_result_t *result
)
{
    if (scanner_validate(scanner) != SYNTRA_STATUS_OK ||
        result == NULL ||
        index >= scanner->result_count)
    {
        return SYNTRA_STATUS_ERROR;
    }

    *result = scanner->results[index];

    return SYNTRA_STATUS_OK;
}

syntra_status_t hal_device_scanner_get_count(
    const syntra_hal_device_scanner_t *scanner,
    unsigned int *count
)
{
    if (scanner_validate(scanner) != SYNTRA_STATUS_OK ||
        count == NULL)
    {
        return SYNTRA_STATUS_ERROR;
    }

    *count = scanner->result_count;

    return SYNTRA_STATUS_OK;
}

syntra_status_t hal_device_scanner_get_status(
    const syntra_hal_device_scanner_t *scanner,
    syntra_hal_scan_status_t *status
)
{
    if (scanner_validate(scanner) != SYNTRA_STATUS_OK ||
        status == NULL)
    {
        return SYNTRA_STATUS_ERROR;
    }

    *status = scanner->status;

    return SYNTRA_STATUS_OK;
}

syntra_status_t hal_device_scanner_get_event_count(
    const syntra_hal_device_scanner_t *scanner,
    unsigned int *count
)
{
    if (scanner_validate(scanner) != SYNTRA_STATUS_OK ||
        count == NULL)
    {
        return SYNTRA_STATUS_ERROR;
    }

    *count = scanner->event_count;

    return SYNTRA_STATUS_OK;
}

syntra_status_t hal_device_scanner_get_event(
    const syntra_hal_device_scanner_t *scanner,
    unsigned int index,
    syntra_hal_scan_event_t *event
)
{
    if (scanner_validate(scanner) != SYNTRA_STATUS_OK ||
        event == NULL ||
        index >= scanner->event_count)
    {
        return SYNTRA_STATUS_ERROR;
    }

    *event = scanner->events[index];

    return SYNTRA_STATUS_OK;
}

syntra_status_t hal_device_scanner_clear_events(
    syntra_hal_device_scanner_t *scanner
)
{
    if (scanner_validate(scanner) != SYNTRA_STATUS_OK)
    {
        return SYNTRA_STATUS_ERROR;
    }

    memset(
        scanner->events,
        0,
        sizeof(scanner->events)
    );

    scanner->event_count = 0U;
    scanner->event_sequence = 0U;

    return SYNTRA_STATUS_OK;
}
