#include "../../kernel/interrupt/interrupt.h"

#include <assert.h>
#include <stdio.h>

static unsigned int handler_calls;
static unsigned int received_interrupt_id;
static void *received_context;

static unsigned int mock_initialize_calls;
static unsigned int mock_enable_calls;
static unsigned int mock_disable_calls;
static unsigned int mock_priority_calls;
static unsigned int mock_acknowledge_calls;
static unsigned int mock_eoi_calls;
static unsigned int mock_last_priority;
static syntra_interrupt_id_t mock_pending_id = 30U;

static syntra_status_t mock_controller_initialize(void *context)
{
    (void)context;
    mock_initialize_calls++;
    return SYNTRA_STATUS_OK;
}

static syntra_status_t mock_controller_enable(void *context, syntra_interrupt_id_t id)
{
    (void)context;
    assert(id == 30U);
    mock_enable_calls++;
    return SYNTRA_STATUS_OK;
}

static syntra_status_t mock_controller_disable(void *context, syntra_interrupt_id_t id)
{
    (void)context;
    assert(id == 30U);
    mock_disable_calls++;
    return SYNTRA_STATUS_OK;
}

static syntra_status_t mock_controller_priority(
    void *context, syntra_interrupt_id_t id, unsigned int priority)
{
    (void)context;
    assert(id == 30U);
    mock_last_priority = priority;
    mock_priority_calls++;
    return SYNTRA_STATUS_OK;
}

static syntra_status_t mock_controller_acknowledge(
    void *context, syntra_interrupt_id_t *id)
{
    (void)context;
    mock_acknowledge_calls++;
    *id = mock_pending_id;
    return SYNTRA_STATUS_OK;
}

static syntra_status_t mock_controller_eoi(
    void *context, syntra_interrupt_id_t id)
{
    (void)context;
    assert(id == mock_pending_id);
    mock_eoi_calls++;
    return SYNTRA_STATUS_OK;
}

static const syntra_interrupt_controller_ops_t mock_controller_ops = {
    mock_controller_initialize,
    mock_controller_enable,
    mock_controller_disable,
    mock_controller_priority,
    mock_controller_acknowledge,
    mock_controller_eoi
};

static void test_handler(
    syntra_interrupt_id_t interrupt_id,
    void *context
)
{
    handler_calls++;
    received_interrupt_id = interrupt_id;
    received_context = context;
}

int main(void)
{
    int test_context = 123;

    assert(interrupt_init() == SYNTRA_STATUS_OK);

    assert(interrupt_validate() == SYNTRA_STATUS_OK);

    assert(
        interrupt_register(
            10U,
            test_handler,
            &test_context
        ) == SYNTRA_STATUS_OK
    );

    assert(
        interrupt_register(
            10U,
            test_handler,
            &test_context
        ) == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    assert(
        interrupt_dispatch(10U) == SYNTRA_STATUS_OK
    );

    assert(handler_calls == 1U);
    assert(received_interrupt_id == 10U);
    assert(received_context == &test_context);

    const syntra_interrupt_entry_t *entry =
        interrupt_get_entry(10U);

    assert(entry != NULL);
    assert(entry->registered == 1);
    assert(entry->call_count == 1U);

    syntra_interrupt_info_t info;

    assert(
        interrupt_get_info(&info) == SYNTRA_STATUS_OK
    );

    assert(info.total_handlers == SYNTRA_MAX_INTERRUPT_HANDLERS);
    assert(info.registered_handlers == 1U);
    assert(info.dispatched_count == 1U);
    assert(info.failed_dispatches == 0U);
    assert(info.initialized == 1);
    assert(info.valid == 1);

    assert(
        interrupt_dispatch(11U) ==
        SYNTRA_STATUS_INVALID_ARGUMENT
    );

    assert(
        interrupt_unregister(10U) == SYNTRA_STATUS_OK
    );

    assert(interrupt_get_entry(10U) == NULL);

    assert(
        interrupt_unregister(10U) ==
        SYNTRA_STATUS_INVALID_ARGUMENT
    );

    assert(interrupt_validate() == SYNTRA_STATUS_OK);

    assert(
        interrupt_get_info(&info) == SYNTRA_STATUS_OK
    );

    assert(info.registered_handlers == 0U);
    assert(info.dispatched_count == 1U);
    assert(info.failed_dispatches == 1U);
    assert(info.valid == 1);

    assert(interrupt_controller_attach(
        &mock_controller_ops, NULL) == SYNTRA_STATUS_OK);
    assert(interrupt_controller_init() == SYNTRA_STATUS_OK);
    assert(mock_initialize_calls == 1U);

    assert(interrupt_register(30U, test_handler, &test_context) ==
        SYNTRA_STATUS_OK);
    assert(interrupt_set_priority(30U, 0x80U) == SYNTRA_STATUS_OK);
    assert(mock_priority_calls == 1U && mock_last_priority == 0x80U);
    assert(interrupt_enable(30U) == SYNTRA_STATUS_OK);
    assert(mock_enable_calls == 1U);

    mock_pending_id = 30U;
    assert(interrupt_handle_pending() == SYNTRA_STATUS_OK);
    assert(mock_acknowledge_calls == 1U);
    assert(mock_eoi_calls == 1U);
    assert(handler_calls == 2U);
    assert(received_interrupt_id == 30U);

    mock_pending_id = SYNTRA_INTERRUPT_SPURIOUS_ID;
    assert(interrupt_handle_pending() == SYNTRA_STATUS_OK);
    assert(mock_eoi_calls == 1U);

    assert(interrupt_disable(30U) == SYNTRA_STATUS_OK);
    assert(mock_disable_calls == 1U);

    assert(interrupt_unregister(30U) == SYNTRA_STATUS_OK);

    printf("Interrupt Manager V2: registry/controller/dispatch tests passed.\n");

    return 0;
}
