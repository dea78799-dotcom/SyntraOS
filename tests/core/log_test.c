
#include "../../kernel/core/log.h"

#include <assert.h>
#include <stdio.h>

int main(void)
{
    syntra_log_info_t info;

    /* Test reset */
    syntra_log_reset();

    syntra_log_get_info(&info);
    assert(info.total_count == 0);

    /* Test INFO */
    syntra_log_write(
        SYNTRA_LOG_INFO,
        "System started"
    );

    /* Test WARNING */
    syntra_log_write(
        SYNTRA_LOG_WARNING,
        "Low memory"
    );

    /* Test ERROR */
    syntra_log_write(
        SYNTRA_LOG_ERROR,
        "Kernel error"
    );

    /* Test DEBUG */
    syntra_log_write(
        SYNTRA_LOG_DEBUG,
        "Debug message"
    );

    syntra_log_get_info(&info);

    assert(info.info_count == 1);
    assert(info.warning_count == 1);
    assert(info.error_count == 1);
    assert(info.debug_count == 1);
    assert(info.total_count == 4);

    /* Test multiple logs */
    syntra_log_write(
        SYNTRA_LOG_INFO,
        "Another message"
    );

    syntra_log_get_info(&info);

    assert(info.info_count == 2);
    assert(info.total_count == 5);

    /* Test invalid level */
    syntra_log_write(
        (syntra_log_level_t)99,
        "Invalid log"
    );

    syntra_log_get_info(&info);
    assert(info.total_count == 5);

    /* Test NULL message */
    syntra_log_write(
        SYNTRA_LOG_INFO,
        NULL
    );

    syntra_log_get_info(&info);
    assert(info.total_count == 5);

    /* Test reset again */
    syntra_log_reset();

    syntra_log_get_info(&info);

    assert(info.info_count == 0);
    assert(info.warning_count == 0);
    assert(info.error_count == 0);
    assert(info.debug_count == 0);
    assert(info.total_count == 0);

    printf("Logging V2 Test: PASSED\n");

    return 0;
}
