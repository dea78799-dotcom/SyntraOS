
#include "../../kernel/uptime/uptime.h"

#include <assert.h>
#include <stdio.h>

int main(void)
{
    syntra_uptime_info_t info;

    /* Initialization tests */

    assert(
        uptime_validate() ==
        SYNTRA_STATUS_ERROR
    );

    assert(
        uptime_init() ==
        SYNTRA_STATUS_OK
    );

    assert(
        uptime_init() ==
        SYNTRA_STATUS_OK
    );

    /* Initial state tests */

    assert(uptime_get_milliseconds() == 0ULL);
    assert(uptime_get_seconds() == 0ULL);

    assert(
        uptime_validate() ==
        SYNTRA_STATUS_OK
    );

    /* Update tests */

    assert(
        uptime_update(1500ULL) ==
        SYNTRA_STATUS_OK
    );

    assert(uptime_get_milliseconds() == 1500ULL);
    assert(uptime_get_seconds() == 1ULL);

    assert(
        uptime_update(58500ULL) ==
        SYNTRA_STATUS_OK
    );

    assert(uptime_get_milliseconds() == 60000ULL);
    assert(uptime_get_seconds() == 60ULL);

    /* Information tests */

    assert(
        uptime_get_info(&info) ==
        SYNTRA_STATUS_OK
    );

    assert(info.total_milliseconds == 60000ULL);
    assert(info.total_seconds == 60ULL);
    assert(info.boot_count == 1ULL);
    assert(info.initialized == 1);
    assert(info.valid == 1);

    assert(
        uptime_get_info(NULL) ==
        SYNTRA_STATUS_INVALID_ARGUMENT
    );

    /* Reset tests */

    assert(
        uptime_reset() ==
        SYNTRA_STATUS_OK
    );

    assert(uptime_get_milliseconds() == 0ULL);
    assert(uptime_get_seconds() == 0ULL);

    assert(
        uptime_get_info(&info) ==
        SYNTRA_STATUS_OK
    );

    assert(info.total_milliseconds == 0ULL);
    assert(info.total_seconds == 0ULL);
    assert(info.boot_count == 1ULL);
    assert(info.valid == 1);

    printf("System Uptime V1: all tests passed.\n");

    return 0;
}
