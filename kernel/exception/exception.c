
#include "exception.h"

#include <stddef.h>
#include <string.h>

static syntra_exception_entry_t exception_entries[
    SYNTRA_MAX_EXCEPTION_HANDLERS
];

static unsigned int registered_handlers;
static unsigned int handled_count;
static unsigned int failed_dispatches;

static int exception_initialized;

syntra_status_t exception_init(void)
{
    if (exception_initialized)
    {
        return SYNTRA_STATUS_OK;
    }

    memset(
        exception_entries,
        0,
        sizeof(exception_entries)
    );

    registered_handlers = 0U;
    handled_count = 0U;
    failed_dispatches = 0U;

    exception_initialized = 1;

    return SYNTRA_STATUS_OK;
}

syntra_status_t exception_register(
    syntra_exception_id_t exception_id,
    syntra_exception_handler_t handler,
    void *context
)
{
    if (!exception_initialized)
    {
        return SYNTRA_STATUS_ERROR;
    }

    if (exception_id >= SYNTRA_MAX_EXCEPTION_HANDLERS)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (handler == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    syntra_exception_entry_t *entry =
        &exception_entries[exception_id];

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

syntra_status_t exception_unregister(
    syntra_exception_id_t exception_id
)
{
    if (!exception_initialized)
    {
        return SYNTRA_STATUS_ERROR;
    }

    if (exception_id >= SYNTRA_MAX_EXCEPTION_HANDLERS)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    syntra_exception_entry_t *entry =
        &exception_entries[exception_id];

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

syntra_status_t exception_dispatch(
    syntra_exception_id_t exception_id
)
{
    if (!exception_initialized)
    {
        failed_dispatches++;
        return SYNTRA_STATUS_ERROR;
    }

    if (exception_id >= SYNTRA_MAX_EXCEPTION_HANDLERS)
    {
        failed_dispatches++;
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    syntra_exception_entry_t *entry =
        &exception_entries[exception_id];

    if (!entry->registered || entry->handler == NULL)
    {
        failed_dispatches++;
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    entry->call_count++;
    handled_count++;

    entry->handler(
        exception_id,
        entry->context
    );

    return SYNTRA_STATUS_OK;
}

syntra_status_t exception_get_info(
    syntra_exception_info_t *info
)
{
    if (info == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (!exception_initialized)
    {
        return SYNTRA_STATUS_ERROR;
    }

    info->total_handlers =
        SYNTRA_MAX_EXCEPTION_HANDLERS;

    info->registered_handlers =
        registered_handlers;

    info->handled_count =
        handled_count;

    info->failed_dispatches =
        failed_dispatches;

    info->initialized =
        exception_initialized;

    info->valid =
        (exception_validate() == SYNTRA_STATUS_OK);

    return SYNTRA_STATUS_OK;
}

const syntra_exception_entry_t *exception_get_entry(
    syntra_exception_id_t exception_id
)
{
    if (!exception_initialized)
    {
        return NULL;
    }

    if (exception_id >= SYNTRA_MAX_EXCEPTION_HANDLERS)
    {
        return NULL;
    }

    if (!exception_entries[exception_id].registered)
    {
        return NULL;
    }

    return &exception_entries[exception_id];
}

syntra_status_t exception_validate(void)
{
    if (!exception_initialized)
    {
        return SYNTRA_STATUS_ERROR;
    }

    unsigned int counted_handlers = 0U;

    for (
        unsigned int i = 0U;
        i < SYNTRA_MAX_EXCEPTION_HANDLERS;
        i++
    )
    {
        const syntra_exception_entry_t *entry =
            &exception_entries[i];

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
