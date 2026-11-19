
#include "hal_device_scanner.h"

#include <limits.h>
#include <stddef.h>
#include <string.h>

/*
 * SyntraOS HAL Device Scanner
 * Version: 5.0.0
 *
 * V5:
 * - Stronger input and state validation.
 * - Discovery device validation before scanning.
 * - Safer event sequence and counter handling.
 * - Query capacity boundary handling.
 * - Transactional scan preparation.
 * - Backward-compatible V1-V4 API.
 */

static int scan_event_type_is_valid(
    syntra_hal_scan_event_type_t type
)
{
    return
        (int)type >= (int)SYNTRA_HAL_SCAN_EVENT_STARTED &&
        (int)type < (int)SYNTRA_HAL_SCAN_EVENT_TYPE_COUNT;
}

static int scan_status_is_valid(
    syntra_hal_scan_status_t status
)
{
    return
        status == SYNTRA_HAL_SCAN_STATUS_IDLE ||
        status == SYNTRA_HAL_SCAN_STATUS_RUNNING ||
        status == SYNTRA_HAL_SCAN_STATUS_COMPLETED ||
        status == SYNTRA_HAL_SCAN_STATUS_FAILED;
}

static int device_type_is_valid(
    syntra_hal_device_type_t type
)
{
    return
        (int)type >= (int)SYNTRA_HAL_DEVICE_UNKNOWN &&
        (int)type <= (int)SYNTRA_HAL_DEVICE_INPUT;
}

static int device_state_is_valid(
    syntra_hal_device_state_t state
)
{
    return
        (int)state >= (int)SYNTRA_HAL_DEVICE_OFFLINE &&
        (int)state <= (int)SYNTRA_HAL_DEVICE_ERROR;
}

static int scanner_is_valid(
    const syntra_hal_device_scanner_t *scanner
)
{
    if (scanner == NULL)
    {
        return 0;
    }

    if (scanner->initialized != 1)
    {
        return 0;
    }

    if (scanner->discovery == NULL)
    {
        return 0;
    }

    if (scanner->discovery->initialized != 1)
    {
        return 0;
    }

    if (scanner->result_count >
        SYNTRA_HAL_SCANNER_MAX_RESULTS)
    {
        return 0;
    }

    if (scanner->event_count >
        SYNTRA_HAL_SCANNER_MAX_EVENTS)
    {
        return 0;
    }

    if (!scan_status_is_valid(scanner->status))
    {
        return 0;
    }

    return 1;
}

static int discovery_devices_are_valid(
    const syntra_hal_device_discovery_t *discovery
)
{
    unsigned int i;
    unsigned int j;

    if (discovery == NULL)
    {
        return 0;
    }

    if (discovery->initialized != 1)
    {
        return 0;
    }

    if (discovery->device_count >
        SYNTRA_HAL_DEVICE_DISCOVERY_MAX_DEVICES)
    {
        return 0;
    }

    for (i = 0U; i < discovery->device_count; ++i)
    {
        const syntra_hal_device_t *device;

        device = discovery->devices[i];

        if (device == NULL)
        {
            return 0;
        }

        if (device->initialized != 1)
        {
            return 0;
        }

        if (!device_type_is_valid(device->type))
        {
            return 0;
        }

        if (!device_state_is_valid(device->state))
        {
            return 0;
        }

        for (j = i + 1U;
             j < discovery->device_count;
             ++j)
        {
            const syntra_hal_device_t *other;

            other = discovery->devices[j];

            if (other == NULL)
            {
                return 0;
            }

            if (device->device_id == other->device_id)
            {
                return 0;
            }
        }
    }

    return 1;
}

static void statistics_reset(
    syntra_hal_scan_event_statistics_t *statistics
)
{
    if (statistics == NULL)
    {
        return;
    }

    memset(statistics, 0, sizeof(*statistics));
}

static void statistics_add_event(
    syntra_hal_scan_event_statistics_t *statistics,
    syntra_hal_scan_event_type_t type
)
{
    if (statistics == NULL)
    {
        return;
    }

    if (statistics->total < UINT_MAX)
    {
        statistics->total++;
    }

    switch (type)
    {
        case SYNTRA_HAL_SCAN_EVENT_STARTED:
            if (statistics->started < UINT_MAX)
            {
                statistics->started++;
            }
            break;

        case SYNTRA_HAL_SCAN_EVENT_COMPLETED:
            if (statistics->completed < UINT_MAX)
            {
                statistics->completed++;
            }
            break;

        case SYNTRA_HAL_SCAN_EVENT_FAILED:
            if (statistics->failed < UINT_MAX)
            {
                statistics->failed++;
            }
            break;

        case SYNTRA_HAL_SCAN_EVENT_RESET:
            if (statistics->resets < UINT_MAX)
            {
                statistics->resets++;
            }
            break;

        default:
            break;
    }
}

static syntra_status_t scanner_record_event(
    syntra_hal_device_scanner_t *scanner,
    syntra_hal_scan_event_type_t type
)
{
    syntra_hal_scan_event_t *event;

    if (!scanner_is_valid(scanner))
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (!scan_event_type_is_valid(type))
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (scanner->event_sequence == UINT_MAX)
    {
        return SYNTRA_STATUS_ERROR;
    }

    if (scanner->event_count >=
        SYNTRA_HAL_SCANNER_MAX_EVENTS)
    {
        memmove(
            &scanner->events[0],
            &scanner->events[1],
            sizeof(scanner->events[0]) *
                (SYNTRA_HAL_SCANNER_MAX_EVENTS - 1U)
        );

        scanner->event_count =
            SYNTRA_HAL_SCANNER_MAX_EVENTS - 1U;
    }

    event = &scanner->events[scanner->event_count];

    scanner->event_sequence++;

    event->sequence = scanner->event_sequence;
    event->type = type;
    event->device_count = scanner->result_count;

    scanner->event_count++;
    scanner->dirty = 1;

    return SYNTRA_STATUS_OK;
}

static int scan_query_is_valid(
    const syntra_hal_scan_query_t *query
)
{
    if (query == NULL)
    {
        return 0;
    }

    if ((query->match_device_id != 0) &&
        (query->match_device_id != 1))
    {
        return 0;
    }

    if ((query->match_type != 0) &&
        (query->match_type != 1))
    {
        return 0;
    }

    if ((query->match_state != 0) &&
        (query->match_state != 1))
    {
        return 0;
    }

    if ((query->match_found != 0) &&
        (query->match_found != 1))
    {
        return 0;
    }

    if (query->match_type &&
        !device_type_is_valid(query->type))
    {
        return 0;
    }

    if (query->match_state &&
        !device_state_is_valid(query->state))
    {
        return 0;
    }

    return 1;
}

static int scan_result_matches(
    const syntra_hal_scan_result_t *result,
    const syntra_hal_scan_query_t *query
)
{
    if (result == NULL || query == NULL)
    {
        return 0;
    }

    if (query->match_device_id &&
        result->device_id != query->device_id)
    {
        return 0;
    }

    if (query->match_type &&
        result->type != query->type)
    {
        return 0;
    }

    if (query->match_state &&
        result->state != query->state)
    {
        return 0;
    }

    if (query->match_found && result->found != 1)
    {
        return 0;
    }

    return 1;
}

/* Lifecycle */

syntra_status_t hal_device_scanner_init(
    syntra_hal_device_scanner_t *scanner,
    syntra_hal_device_discovery_t *discovery
)
{
    if (scanner == NULL || discovery == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (discovery->initialized != 1)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (discovery->device_count >
        SYNTRA_HAL_DEVICE_DISCOVERY_MAX_DEVICES)
    {
        return SYNTRA_STATUS_ERROR;
    }

    memset(scanner, 0, sizeof(*scanner));

    scanner->discovery = discovery;
    scanner->status = SYNTRA_HAL_SCAN_STATUS_IDLE;
    scanner->initialized = 1;
    scanner->dirty = 1;

    return hal_device_scanner_update_statistics(scanner);
}

syntra_status_t hal_device_scanner_reset(
    syntra_hal_device_scanner_t *scanner
)
{
    syntra_status_t status;

    if (!scanner_is_valid(scanner))
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (scanner->event_sequence == UINT_MAX)
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

    scanner->query_count = 0U;
    scanner->last_query_count = 0U;

    scanner->status = SYNTRA_HAL_SCAN_STATUS_IDLE;
    scanner->dirty = 1;

    status = scanner_record_event(
        scanner,
        SYNTRA_HAL_SCAN_EVENT_RESET
    );

    if (status != SYNTRA_STATUS_OK)
    {
        return status;
    }

    return hal_device_scanner_update_statistics(scanner);
}

/* Scanning */

syntra_status_t hal_device_scanner_scan(
    syntra_hal_device_scanner_t *scanner
)
{
    unsigned int i;
    syntra_status_t status;

    if (!scanner_is_valid(scanner))
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    /*
     * A complete scan needs two new event sequence values:
     * STARTED and COMPLETED.
     */
    if (scanner->event_sequence > UINT_MAX - 2U)
    {
        return SYNTRA_STATUS_ERROR;
    }

    /*
     * Validate the complete discovery list before modifying
     * the scanner's previous results.
     */
    if (!discovery_devices_are_valid(scanner->discovery))
    {
        scanner->status = SYNTRA_HAL_SCAN_STATUS_FAILED;
        scanner->dirty = 1;

        if (scanner->event_sequence < UINT_MAX)
        {
            (void)scanner_record_event(
                scanner,
                SYNTRA_HAL_SCAN_EVENT_FAILED
            );

            (void)hal_device_scanner_update_statistics(
                scanner
            );
        }

        return SYNTRA_STATUS_ERROR;
    }

    if (scanner->discovery->device_count >
        SYNTRA_HAL_SCANNER_MAX_RESULTS)
    {
        scanner->status = SYNTRA_HAL_SCAN_STATUS_FAILED;
        scanner->dirty = 1;

        (void)scanner_record_event(
            scanner,
            SYNTRA_HAL_SCAN_EVENT_FAILED
        );

        (void)hal_device_scanner_update_statistics(
            scanner
        );

        return SYNTRA_STATUS_ERROR;
    }

    memset(
        scanner->results,
        0,
        sizeof(scanner->results)
    );

    scanner->result_count = 0U;
    scanner->scanned_count = 0U;

    scanner->query_count = 0U;
    scanner->last_query_count = 0U;

    scanner->status = SYNTRA_HAL_SCAN_STATUS_RUNNING;
    scanner->dirty = 1;

    status = scanner_record_event(
        scanner,
        SYNTRA_HAL_SCAN_EVENT_STARTED
    );

    if (status != SYNTRA_STATUS_OK)
    {
        scanner->status = SYNTRA_HAL_SCAN_STATUS_FAILED;
        return status;
    }

    for (i = 0U;
         i < scanner->discovery->device_count;
         ++i)
    {
        const syntra_hal_device_t *device;
        syntra_hal_scan_result_t *result;

        device = scanner->discovery->devices[i];
        result = &scanner->results[scanner->result_count];

        result->device_id = device->device_id;
        result->type = device->type;
        result->state = device->state;
        result->found = 1;

        scanner->result_count++;
        scanner->scanned_count++;
    }

    scanner->status = SYNTRA_HAL_SCAN_STATUS_COMPLETED;
    scanner->dirty = 1;

    status = scanner_record_event(
        scanner,
        SYNTRA_HAL_SCAN_EVENT_COMPLETED
    );

    if (status != SYNTRA_STATUS_OK)
    {
        scanner->status = SYNTRA_HAL_SCAN_STATUS_FAILED;
        return status;
    }

    return hal_device_scanner_update_statistics(scanner);
}

/* Results */

syntra_status_t hal_device_scanner_get_result(
    const syntra_hal_device_scanner_t *scanner,
    unsigned int index,
    syntra_hal_scan_result_t *result
)
{
    if (!scanner_is_valid(scanner) || result == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (index >= scanner->result_count)
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
    if (!scanner_is_valid(scanner) || count == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    *count = scanner->result_count;

    return SYNTRA_STATUS_OK;
}

syntra_status_t hal_device_scanner_get_status(
    const syntra_hal_device_scanner_t *scanner,
    syntra_hal_scan_status_t *status
)
{
    if (!scanner_is_valid(scanner) || status == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    *status = scanner->status;

    return SYNTRA_STATUS_OK;
}

/* Event history */

syntra_status_t hal_device_scanner_get_event_count(
    const syntra_hal_device_scanner_t *scanner,
    unsigned int *count
)
{
    if (!scanner_is_valid(scanner) || count == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
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
    if (!scanner_is_valid(scanner) || event == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (index >= scanner->event_count)
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
    if (!scanner_is_valid(scanner))
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    memset(
        scanner->events,
        0,
        sizeof(scanner->events)
    );

    scanner->event_count = 0U;
    scanner->event_sequence = 0U;
    scanner->dirty = 1;

    return hal_device_scanner_update_statistics(scanner);
}

/* V3: Event filtering */

syntra_status_t hal_device_scanner_find_events_by_type(
    const syntra_hal_device_scanner_t *scanner,
    syntra_hal_scan_event_type_t type,
    syntra_hal_scan_event_t *events,
    unsigned int capacity,
    unsigned int *found
)
{
    unsigned int i;
    unsigned int total;
    unsigned int copied;

    if (!scanner_is_valid(scanner) || found == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (!scan_event_type_is_valid(type))
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (capacity > 0U && events == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    total = 0U;
    copied = 0U;

    for (i = 0U; i < scanner->event_count; ++i)
    {
        if (scanner->events[i].type == type)
        {
            if (copied < capacity)
            {
                events[copied] = scanner->events[i];
                copied++;
            }

            if (total < UINT_MAX)
            {
                total++;
            }
        }
    }

    *found = total;

    if (total > capacity)
    {
        return SYNTRA_STATUS_ERROR;
    }

    return SYNTRA_STATUS_OK;
}

/* V3: Statistics */

syntra_status_t hal_device_scanner_update_statistics(
    syntra_hal_device_scanner_t *scanner
)
{
    unsigned int i;
    syntra_hal_scan_event_statistics_t statistics;

    if (!scanner_is_valid(scanner))
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    statistics_reset(&statistics);

    for (i = 0U; i < scanner->event_count; ++i)
    {
        syntra_hal_scan_event_type_t type;

        type = scanner->events[i].type;

        if (!scan_event_type_is_valid(type))
        {
            return SYNTRA_STATUS_ERROR;
        }

        statistics_add_event(&statistics, type);
    }

    scanner->statistics = statistics;

    if (scanner->update_count < UINT_MAX)
    {
        scanner->update_count++;
    }

    scanner->dirty = 0;

    return SYNTRA_STATUS_OK;
}

syntra_status_t hal_device_scanner_get_statistics(
    const syntra_hal_device_scanner_t *scanner,
    syntra_hal_scan_event_statistics_t *statistics
)
{
    if (!scanner_is_valid(scanner) || statistics == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    *statistics = scanner->statistics;

    return SYNTRA_STATUS_OK;
}

/* V3: Validation */

syntra_status_t hal_device_scanner_validate(
    const syntra_hal_device_scanner_t *scanner
)
{
    unsigned int i;
    unsigned int previous_sequence;

    if (!scanner_is_valid(scanner))
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    previous_sequence = 0U;

    for (i = 0U; i < scanner->event_count; ++i)
    {
        const syntra_hal_scan_event_t *event;

        event = &scanner->events[i];

        if (!scan_event_type_is_valid(event->type))
        {
            return SYNTRA_STATUS_ERROR;
        }

        if (event->sequence == 0U)
        {
            return SYNTRA_STATUS_ERROR;
        }

        if (event->sequence > scanner->event_sequence)
        {
            return SYNTRA_STATUS_ERROR;
        }

        if (i > 0U &&
            event->sequence <= previous_sequence)
        {
            return SYNTRA_STATUS_ERROR;
        }

        previous_sequence = event->sequence;
    }

    return SYNTRA_STATUS_OK;
}

syntra_status_t hal_device_scanner_get_validation_report(
    const syntra_hal_device_scanner_t *scanner,
    syntra_hal_scan_validation_report_t *report
)
{
    unsigned int i;
    unsigned int j;
    unsigned int previous_sequence;

    if (!scanner_is_valid(scanner) || report == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    memset(report, 0, sizeof(*report));

    previous_sequence = 0U;

    for (i = 0U; i < scanner->event_count; ++i)
    {
        const syntra_hal_scan_event_t *event;

        event = &scanner->events[i];

        if (!scan_event_type_is_valid(event->type) ||
            event->sequence == 0U ||
            event->sequence > scanner->event_sequence)
        {
            report->invalid_events++;
        }

        if (i > 0U)
        {
            if (event->sequence < previous_sequence)
            {
                report->out_of_order_sequences++;
            }

            if (event->sequence == previous_sequence)
            {
                report->duplicate_sequences++;
            }

            if (event->sequence > previous_sequence &&
                previous_sequence < UINT_MAX &&
                event->sequence > previous_sequence + 1U)
            {
                report->sequence_gaps +=
                    event->sequence - previous_sequence - 1U;
            }
        }

        for (j = 0U; j < i; ++j)
        {
            if (scanner->events[j].sequence ==
                event->sequence)
            {
                if (report->duplicate_sequences < UINT_MAX)
                {
                    report->duplicate_sequences++;
                }

                break;
            }
        }

        previous_sequence = event->sequence;
    }

    return SYNTRA_STATUS_OK;
}

/* V4: Result queries */

syntra_status_t hal_device_scanner_find_by_device_id(
    const syntra_hal_device_scanner_t *scanner,
    unsigned int device_id,
    syntra_hal_scan_result_t *result
)
{
    unsigned int i;

    if (!scanner_is_valid(scanner) || result == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    for (i = 0U; i < scanner->result_count; ++i)
    {
        if (scanner->results[i].device_id == device_id &&
            scanner->results[i].found == 1)
        {
            *result = scanner->results[i];
            return SYNTRA_STATUS_OK;
        }
    }

    return SYNTRA_STATUS_ERROR;
}

syntra_status_t hal_device_scanner_query(
    syntra_hal_device_scanner_t *scanner,
    const syntra_hal_scan_query_t *query,
    syntra_hal_scan_result_t *results,
    unsigned int capacity,
    syntra_hal_scan_query_report_t *report
)
{
    unsigned int i;
    unsigned int matched;
    unsigned int copied;

    if (!scanner_is_valid(scanner) ||
        !scan_query_is_valid(query) ||
        report == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (capacity > 0U && results == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (capacity > SYNTRA_HAL_SCANNER_MAX_RESULTS)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    report->matched_count = 0U;
    report->total_count = scanner->result_count;

    matched = 0U;
    copied = 0U;

    for (i = 0U; i < scanner->result_count; ++i)
    {
        if (!scan_result_matches(
                &scanner->results[i],
                query))
        {
            continue;
        }

        if (copied < capacity)
        {
            results[copied] = scanner->results[i];
            copied++;
        }

        if (matched < UINT_MAX)
        {
            matched++;
        }
    }

    report->matched_count = matched;

    if (scanner->query_count < UINT_MAX)
    {
        scanner->query_count++;
    }

    scanner->last_query_count = matched;

    if (matched > capacity)
    {
        return SYNTRA_STATUS_ERROR;
    }

    return SYNTRA_STATUS_OK;
}

syntra_status_t hal_device_scanner_count_matches(
    const syntra_hal_device_scanner_t *scanner,
    const syntra_hal_scan_query_t *query,
    unsigned int *count
)
{
    unsigned int i;
    unsigned int matched;

    if (!scanner_is_valid(scanner) ||
        !scan_query_is_valid(query) ||
        count == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    matched = 0U;

    for (i = 0U; i < scanner->result_count; ++i)
    {
        if (scan_result_matches(
                &scanner->results[i],
                query))
        {
            if (matched < UINT_MAX)
            {
                matched++;
            }
        }
    }

    *count = matched;

    return SYNTRA_STATUS_OK;
}
