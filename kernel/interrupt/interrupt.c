#include "interrupt.h"

#include <stddef.h>
#define SYNTRA_INTERRUPT_PRIORITY_MAX 255U

static syntra_interrupt_entry_t interrupt_entries[
    SYNTRA_MAX_INTERRUPT_HANDLERS
];

static unsigned int registered_handlers;
static unsigned int dispatched_count;
static unsigned int failed_dispatches;

static int interrupt_initialized;
static syntra_interrupt_controller_ops_t controller_ops;
static void *controller_context;
static int controller_attached;
static int controller_initialized;

syntra_status_t interrupt_init(void)
{
    if (interrupt_initialized)
    {
        return SYNTRA_STATUS_OK;
    }

    for (unsigned int i = 0U;
         i < SYNTRA_MAX_INTERRUPT_HANDLERS;
         ++i)
    {
        interrupt_entries[i].handler = NULL;
        interrupt_entries[i].context = NULL;
        interrupt_entries[i].call_count = 0U;
        interrupt_entries[i].registered = 0;
    }

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

syntra_status_t interrupt_controller_attach(
    const syntra_interrupt_controller_ops_t *ops,
    void *context
)
{
    if (!interrupt_initialized)
    {
        return SYNTRA_STATUS_ERROR;
    }

    if (ops == NULL ||
        ops->initialize == NULL ||
        ops->enable == NULL ||
        ops->disable == NULL ||
        ops->set_priority == NULL ||
        ops->acknowledge == NULL ||
        ops->end_of_interrupt == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (controller_initialized)
    {
        return SYNTRA_STATUS_PERMISSION_DENIED;
    }

    controller_ops = *ops;
    controller_context = context;
    controller_attached = 1;
    return SYNTRA_STATUS_OK;
}

syntra_status_t interrupt_controller_init(void)
{
    syntra_status_t status;

    if (!interrupt_initialized || !controller_attached)
    {
        return SYNTRA_STATUS_ERROR;
    }

    if (controller_initialized)
    {
        return SYNTRA_STATUS_OK;
    }

    status = controller_ops.initialize(controller_context);
    if (status == SYNTRA_STATUS_OK)
    {
        controller_initialized = 1;
    }
    return status;
}

syntra_status_t interrupt_enable(syntra_interrupt_id_t interrupt_id)
{
    if (!controller_initialized)
        return SYNTRA_STATUS_ERROR;
    if (interrupt_id >= SYNTRA_MAX_INTERRUPT_HANDLERS)
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    return controller_ops.enable(controller_context, interrupt_id);
}

syntra_status_t interrupt_disable(syntra_interrupt_id_t interrupt_id)
{
    if (!controller_initialized)
        return SYNTRA_STATUS_ERROR;
    if (interrupt_id >= SYNTRA_MAX_INTERRUPT_HANDLERS)
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    return controller_ops.disable(controller_context, interrupt_id);
}

syntra_status_t interrupt_set_priority(
    syntra_interrupt_id_t interrupt_id,
    unsigned int priority
)
{
    if (!controller_initialized)
        return SYNTRA_STATUS_ERROR;
    if (interrupt_id >= SYNTRA_MAX_INTERRUPT_HANDLERS ||
        priority > SYNTRA_INTERRUPT_PRIORITY_MAX)
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    return controller_ops.set_priority(
        controller_context, interrupt_id, priority);
}

syntra_status_t interrupt_handle_pending(void)
{
    syntra_interrupt_id_t interrupt_id;
    syntra_status_t dispatch_status;
    syntra_status_t eoi_status;
    syntra_status_t status;

    if (!controller_initialized)
        return SYNTRA_STATUS_ERROR;

    status = controller_ops.acknowledge(
        controller_context, &interrupt_id);
    if (status != SYNTRA_STATUS_OK)
        return status;

    /* GIC INTIDs 1020-1023 are special/spurious and must not be EOI'd. */
    if (interrupt_id >= SYNTRA_MAX_INTERRUPT_HANDLERS)
        return SYNTRA_STATUS_OK;

    dispatch_status = interrupt_dispatch(interrupt_id);
    eoi_status = controller_ops.end_of_interrupt(
        controller_context, interrupt_id);

    if (eoi_status != SYNTRA_STATUS_OK)
        return eoi_status;

    return dispatch_status;
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
