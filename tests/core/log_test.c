
#include "../../kernel/core/log.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    syntra_log_info_t info;

    /* Test reset */
    syntra_log_reset();

    /* Test writing logs */
    syntra_log_write(SYNTRA_LOG_INFO, "System started");
    syntra_log_write(SYNTRA_LOG_WARNING, "Low memory");
    syntra_log_write(SYNTRA_LOG_ERROR, "Kernel error");
    syntra_log_write(SYNTRA_LOG_DEBUG, "Debug message");

    /* Test counters */
    syntra_log_get_info(&info);

    assert(info.info_count == 1);
    assert(info.warning_count == 1);
    assert(info.error_count == 1);
    assert(info.debug_count == 1);
    assert(info.total_count == 4);

    /* Test log history */
    const syntra_log_entry_t *entry;

    entry = syntra_log_get(0);

    assert(entry != NULL);
    assert(entry->level == SYNTRA_LOG_INFO);
    assert(strcmp(entry->message, "System started") == 0);

    entry = syntra_log_get(1);

    assert(entry != NULL);
    assert(entry->level == SYNTRA_LOG_WARNING);
    assert(strcmp(entry->message, "Low memory") == 0);

    /* Test invalid index */
    assert(syntra_log_get(100) == NULL);

    /* Test NULL message */
    syntra_log_write(SYNTRA_LOG_INFO, NULL);

    syntra_log_get_info(&info);
    assert(info.total_count == 4);

    /* Test history limit */
    syntra_log_reset();

    for (unsigned int i = 0; i < 70; i++)
    {
        syntra_log_write(
            SYNTRA_LOG_INFO,
            "History test"
        );
    }

    syntra_log_get_info(&info);

    assert(info.total_count == 70);
    assert(info.info_count == 70);

    assert(syntra_log_get(63) != NULL);
    assert(syntra_log_get(64) == NULL);

    /* Test reset */
    syntra_log_reset();

    syntra_log_get_info(&info);

    assert(info.total_count == 0);
    assert(syntra_log_get(0) == NULL);

    printf("Logging V3 Test: PASSED\n");

    return 0;
}
