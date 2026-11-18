
#include "../../kernel/clock/clock.h"

#include <assert.h>
#include <stdio.h>

int main(void)
{
    syntra_clock_info_t info;

    /* Initialization tests */

    assert(
        clock_validate() ==
        SYNTRA_STATUS_ERROR
    );

    assert(
        clock_init() ==
        SYNTRA_STATUS_OK
    );

    assert(
        clock_init() ==
        SYNTRA_STATUS_OK
    );

    /* Initial state tests */

    assert(clock_get_milliseconds() == 0ULL);
    assert(clock_get_seconds() == 0U);
    assert(clock_get_minutes() == 0U);
    assert(clock_get_hours() == 0U);

    assert(
        clock_validate() ==
        SYNTRA_STATUS_OK
    );

    /* Update tests */

    assert(
        clock_update(1500ULL) ==
        SYNTRA_STATUS_OK
    );

    assert(clock_get_milliseconds() == 1500ULL);
    assert(clock_get_seconds() == 1U);

    assert(
        clock_update(59000ULL) ==
        SYNTRA_STATUS_OK
    );

    assert(clock_get_seconds() == 0U);
    assert(clock_get_minutes() == 1U);

    /* Information tests */

    assert(
        clock_get_info(&info) ==
        SYNTRA_STATUS_OK
    );

    assert(info.total_milliseconds == 60500ULL);
    assert(info.hours == 0U);
    assert(info.minutes == 1U);
    assert(info.seconds == 0U);
    assert(info.milliseconds == 500U);
    assert(info.initialized == 1);
    assert(info.valid == 1);

    assert(
        clock_get_info(NULL) ==
        SYNTRA_STATUS_INVALID_ARGUMENT
    );

    /* Reset tests */

    assert(
        clock_reset() ==
        SYNTRA_STATUS_OK
    );

    assert(clock_get_milliseconds() == 0ULL);
    assert(clock_get_seconds() == 0U);
    assert(clock_get_minutes() == 0U);
    assert(clock_get_hours() == 0U);

    assert(
        clock_get_info(&info) ==
        SYNTRA_STATUS_OK
    );

    assert(info.total_milliseconds == 0ULL);
    assert(info.valid == 1);

    printf("Clock Manager V1: all tests passed.\n");

    return 0;
}
