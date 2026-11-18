
#include "../../kernel/core/panic.h"
#include "../../kernel/core/log.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    syntra_panic_info_t info;
    syntra_log_info_t log_info;

    /* Test initial state */
    syntra_kernel_panic_reset();
    syntra_log_reset();

    syntra_kernel_panic_get_info(&info);

    assert(info.state == SYNTRA_PANIC_NONE);
    assert(info.timestamp == 0);
    assert(info.reason[0] == '\0');

    /* Test panic with timestamp */
    syntra_kernel_panic_at(
        "Memory failure",
        3661
    );

    syntra_kernel_panic_get_info(&info);

    assert(info.state == SYNTRA_PANIC_ACTIVE);
    assert(info.timestamp == 3661);
    assert(strcmp(info.reason, "Memory failure") == 0);

    /* Test logging integration */
    syntra_log_get_info(&log_info);

    assert(log_info.error_count == 1);
    assert(log_info.total_count == 1);

    /* Test preserving first panic */
    syntra_kernel_panic_at(
        "Second failure",
        5000
    );

    syntra_kernel_panic_get_info(&info);

    assert(info.timestamp == 3661);
    assert(strcmp(info.reason, "Memory failure") == 0);

    /* Test normal panic */
    syntra_kernel_panic_reset();
    syntra_log_reset();

    syntra_kernel_panic("Kernel failure");

    syntra_kernel_panic_get_info(&info);

    assert(info.state == SYNTRA_PANIC_ACTIVE);
    assert(info.timestamp == 0);
    assert(strcmp(info.reason, "Kernel failure") == 0);

    /* Test null reason */
    syntra_kernel_panic_reset();

    syntra_kernel_panic(NULL);

    syntra_kernel_panic_get_info(&info);

    assert(info.state == SYNTRA_PANIC_NONE);

    /* Test reset */
    syntra_kernel_panic_reset();

    syntra_kernel_panic_get_info(&info);

    assert(info.state == SYNTRA_PANIC_NONE);
    assert(info.timestamp == 0);
    assert(info.reason[0] == '\0');

    printf("Kernel Panic V2 Test: PASSED\n");

    return 0;
}
