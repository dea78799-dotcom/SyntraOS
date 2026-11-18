
#include "../../kernel/core/log.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    syntra_log_info_t info;
    const syntra_log_entry_t *entry;

    syntra_log_reset();

    /* Test timestamp conversion: 01:01:01 */
    syntra_log_write_at(
        SYNTRA_LOG_INFO,
        "System started",
        3661
    );

    entry = syntra_log_get(0);

    assert(entry != NULL);
    assert(entry->timestamp == 3661);
    assert(entry->hours == 1);
    assert(entry->minutes == 1);
    assert(entry->seconds == 1);
    assert(strcmp(entry->message, "System started") == 0);

    /* Test midnight boundary */
    syntra_log_write_at(
        SYNTRA_LOG_WARNING,
        "Midnight test",
        86399
    );

    entry = syntra_log_get(1);

    assert(entry != NULL);
    assert(entry->hours == 23);
    assert(entry->minutes == 59);
    assert(entry->seconds == 59);

    /* Test counters */
    syntra_log_write_at(
        SYNTRA_LOG_ERROR,
        "Kernel error",
        300
    );

    syntra_log_write_at(
        SYNTRA_LOG_DEBUG,
        "Debug message",
        400
    );

    syntra_log_get_info(&info);

    assert(info.info_count == 1);
    assert(info.warning_count == 1);
    assert(info.error_count == 1);
    assert(info.debug_count == 1);
    assert(info.total_count == 4);

    /* Test filtering */
    entry = syntra_log_find_level(SYNTRA_LOG_ERROR, 0);

    assert(entry != NULL);
    assert(entry->timestamp == 300);

    assert(
        syntra_log_find_level(SYNTRA_LOG_ERROR, 1) == NULL
    );

    /* Test invalid index */
    assert(syntra_log_get(100) == NULL);

    /* Test normal write */
    syntra_log_write(
        SYNTRA_LOG_INFO,
        "Normal log"
    );

    entry = syntra_log_get(4);

    assert(entry != NULL);
    assert(entry->timestamp == 0);
    assert(entry->hours == 0);
    assert(entry->minutes == 0);
    assert(entry->seconds == 0);

    /* Test history limit */
    syntra_log_reset();

    for (unsigned int i = 0; i < 70; i++)
    {
        syntra_log_write_at(
            SYNTRA_LOG_INFO,
            "History test",
            i
        );
    }

    syntra_log_get_info(&info);

    assert(info.total_count == 70);
    assert(syntra_log_get(63) != NULL);
    assert(syntra_log_get(64) == NULL);

    entry = syntra_log_get(0);

    assert(entry != NULL);
    assert(entry->timestamp == 6);

    /* Test reset */
    syntra_log_reset();

    syntra_log_get_info(&info);

    assert(info.total_count == 0);
    assert(syntra_log_get(0) == NULL);

    printf("Logging V5 Test: PASSED\n");

    return 0;
}
