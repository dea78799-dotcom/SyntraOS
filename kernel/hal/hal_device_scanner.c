
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

    device_count = scanner->discovery->device_count;

    if (device_count >
        SYNTRA_HAL_DEVICE_DISCOVERY_MAX_DEVICES ||
        device_count > SYNTRA_HAL_SCANNER_MAX_RESULTS)
    {
        scanner->status = SYNTRA_HAL_SCAN_STATUS_FAILED;
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
