#include "interrupt.h"

#include <stddef.h>
#include <string.h>

static syntra_interrupt_entry_t interrupt_entries[
    SYNTRA_MAX_INTERRUPT_HANDLERS
];

static unsigned int registered_handlers;
static unsigned int dispatched_count;
static unsigned int failed_dispatches;

static int interrupt_initialized;

syntra_status_t interrupt_init(void)
{
    if (interrupt_initialized)
    {
        return SYNTRA_STATUS_OK;
    }

    memset(
        interrupt_entries,
        0,
        sizeof(interrupt_entries)
    );

    registered_handlers = 0U;
    dispatched_count = 0U;
    failed_dispatches = 0U;

    interrupt_initialized = 1;

    return SYNTRA_STATUS_OK;
}

syntra_status_t interrupt_register(
    syntra_interrupt_id_t interrupt_id,
    syntra_interrupt_handler_t handler,
    void *context
)
{
    if (!interrupt_initialized)
    {
        return SYNTRA_STATUS_ERROR;
    }

    if (interrupt_id >= SYNTRA_MAX_INTERRUPT_HANDLERS)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (handler == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    syntra_interrupt_entry_t *entry =
        &interrupt_entries[interrupt_id];

    if (entry->registered)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    entry->handler = handler;
    entry->context = context;
    entry->call_count = 0U;
    entry->registered = 1;

    registered_handlers++;

    return SYNTRA_STATUS_OK;
}

syntra_status_t interrupt_unregister(
    syntra_interrupt_id_t interrupt_id
)
{
    if (!interrupt_initialized)
    {
        return SYNTRA_STATUS_ERROR;
    }

    if (interrupt_id >= SYNTRA_MAX_INTERRUPT_HANDLERS)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    syntra_interrupt_entry_t *entry =
        &interrupt_entries[interrupt_id];

    if (!entry->registered)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    entry->handler = NULL;
    entry->context = NULL;
    entry->call_count = 0U;
    entry->registered = 0;

    registered_handlers--;

    return SYNTRA_STATUS_OK;
}

syntra_status_t interrupt_dispatch(
    syntra_interrupt_id_t interrupt_id
)
{
    if (!interrupt_initialized)
    {
        failed_dispatches++;
        return SYNTRA_STATUS_ERROR;
    }

    if (interrupt_id >= SYNTRA_MAX_INTERRUPT_HANDLERS)
    {
        failed_dispatches++;
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    syntra_interrupt_entry_t *entry =
        &interrupt_entries[interrupt_id];

    if (!entry->registered || entry->handler == NULL)
    {
        failed_dispatches++;
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    entry->call_count++;
    dispatched_count++;

    entry->handler(
        interrupt_id,
        entry->context
    );

    return SYNTRA_STATUS_OK;
}

syntra_status_t interrupt_get_info(
    syntra_interrupt_info_t *info
)
{
    if (info == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (!interrupt_initialized)
    {
        return SYNTRA_STATUS_ERROR;
    }

    info->total_handlers =
        SYNTRA_MAX_INTERRUPT_HANDLERS;

    info->registered_handlers =
        registered_handlers;

    info->dispatched_count =
        dispatched_count;

    info->failed_dispatches =
        failed_dispatches;

    info->initialized =
        interrupt_initialized;

    info->valid =
        (interrupt_validate() == SYNTRA_STATUS_OK);

    return SYNTRA_STATUS_OK;
}

const syntra_interrupt_entry_t *interrupt_get_entry(
    syntra_interrupt_id_t interrupt_id
)
{
    if (!interrupt_initialized)
    {
        return NULL;
    }

    if (interrupt_id >= SYNTRA_MAX_INTERRUPT_HANDLERS)
    {
        return NULL;
    }

    if (!interrupt_entries[interrupt_id].registered)
    {
        return NULL;
    }

    return &interrupt_entries[interrupt_id];
}

syntra_status_t interrupt_validate(void)
{
    if (!interrupt_initialized)
    {
        return SYNTRA_STATUS_ERROR;
    }

    unsigned int counted_handlers = 0U;

    for (
        unsigned int i = 0U;
        i < SYNTRA_MAX_INTERRUPT_HANDLERS;
        i++
    )
    {
        const syntra_interrupt_entry_t *entry =
            &interrupt_entries[i];

        if (entry->registered)
        {
            if (entry->handler == NULL)
            {
                return SYNTRA_STATUS_ERROR;
            }

            counted_handlers++;
        }
        else
        {
            if (
                entry->handler != NULL ||
                entry->context != NULL ||
                entry->call_count != 0U
            )
            {
                return SYNTRA_STATUS_ERROR;
            }
        }
    }

    if (counted_handlers != registered_handlers)
    {
        return SYNTRA_STATUS_ERROR;
    }

    return SYNTRA_STATUS_OK;
}
