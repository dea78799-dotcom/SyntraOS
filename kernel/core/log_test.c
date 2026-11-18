
#include "../../kernel/core/log.h"

#include <assert.h>
#include <stdio.h>

int main(void)
{
    syntra_log_info_t info;

    /* Reset counters */
    syntra_log_reset();

    /* Test INFO */
    syntra_log_write(SYNTRA_LOG_INFO, "System started");

    /* Test WARNING */
    syntra_log_write(SYNTRA_LOG_WARNING, "Low memory");

    /* Test ERROR */
    syntra_log_write(SYNTRA_LOG_ERROR, "Kernel error");

    /* Test DEBUG */
    syntra_log_write(SYNTRA_LOG_DEBUG, "Debug message");

    /* Read log information */
    syntra_log_get_info(&info);

    assert(info.info_count == 1);
    assert(info.warning_count == 1);
    assert(info.error_count == 1);
    assert(info.debug_count == 1);
    assert(info.total_count == 4);

    /* Test multiple logs */
    syntra_log_write(SYNTRA_LOG_INFO, "Another message");

    syntra_log_get_info(&info);

    assert(info.info_count == 2);
    assert(info.total_count == 5);

    /* Test reset */
    syntra_log_reset();

    syntra_log_get_info(&info);

    assert(info.total_count == 0);

    printf("Logging V2 Test: PASSED\n");

    return 0;
}
