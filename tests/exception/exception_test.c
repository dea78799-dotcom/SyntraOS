
#include "../../kernel/exception/exception.h"

#include <assert.h>
#include <stdio.h>

static unsigned int handler_calls;
static unsigned int received_exception_id;
static void *received_context;

static void test_handler(
    syntra_exception_id_t exception_id,
    void *context
)
{
    handler_calls++;
    received_exception_id = exception_id;
    received_context = context;
}

int main(void)
{
    int test_context = 456;

    assert(exception_init() == SYNTRA_STATUS_OK);

    assert(exception_validate() == SYNTRA_STATUS_OK);

    assert(
        exception_register(
            5U,
            test_handler,
            &test_context
        ) == SYNTRA_STATUS_OK
    );

    assert(
        exception_register(
            5U,
            test_handler,
            &test_context
        ) == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    assert(
        exception_dispatch(5U) == SYNTRA_STATUS_OK
    );

    assert(handler_calls == 1U);
    assert(received_exception_id == 5U);
    assert(received_context == &test_context);

    const syntra_exception_entry_t *entry =
        exception_get_entry(5U);

    assert(entry != NULL);
    assert(entry->registered == 1);
    assert(entry->call_count == 1U);

    syntra_exception_info_t info;

    assert(
        exception_get_info(&info) == SYNTRA_STATUS_OK
    );

    assert(info.total_handlers == SYNTRA_MAX_EXCEPTION_HANDLERS);
    assert(info.registered_handlers == 1U);
    assert(info.handled_count == 1U);
    assert(info.failed_dispatches == 0U);
    assert(info.initialized == 1);
    assert(info.valid == 1);

    assert(
        exception_dispatch(6U) ==
        SYNTRA_STATUS_INVALID_ARGUMENT
    );

    assert(
        exception_unregister(5U) == SYNTRA_STATUS_OK
    );

    assert(exception_get_entry(5U) == NULL);

    assert(
        exception_unregister(5U) ==
        SYNTRA_STATUS_INVALID_ARGUMENT
    );

    assert(exception_validate() == SYNTRA_STATUS_OK);

    assert(
        exception_get_info(&info) == SYNTRA_STATUS_OK
    );

    assert(info.registered_handlers == 0U);
    assert(info.handled_count == 1U);
    assert(info.failed_dispatches == 1U);
    assert(info.valid == 1);

    printf("Exception Manager V1: all tests passed.\n");

    return 0;
}
