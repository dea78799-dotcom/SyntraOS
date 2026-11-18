#include "../../kernel/interrupt/interrupt.h"

#include <assert.h>
#include <stdio.h>

static unsigned int handler_calls;
static unsigned int received_interrupt_id;
static void *received_context;

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

    printf("Interrupt Manager V1: all tests passed.\n");

    return 0;
}
