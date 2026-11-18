
#include "../../kernel/timer/timer.h"

#include <assert.h>
#include <stdio.h>

int main(void)
{
    syntra_timer_info_t info;

    /* Initialization tests */

    assert(
        timer_init(0U) ==
        SYNTRA_STATUS_INVALID_ARGUMENT
    );

    assert(
        timer_init(1000U) ==
        SYNTRA_STATUS_OK
    );

    assert(
        timer_init(1000U) ==
        SYNTRA_STATUS_OK
    );

    assert(
        timer_init(500U) ==
        SYNTRA_STATUS_INVALID_ARGUMENT
    );

    /* Initial state tests */

    assert(timer_validate() == SYNTRA_STATUS_OK);
    assert(timer_get_ticks() == 0ULL);
    assert(timer_get_milliseconds() == 0ULL);

    /* Tick tests */

    assert(timer_tick() == SYNTRA_STATUS_OK);
    assert(timer_tick() == SYNTRA_STATUS_OK);
    assert(timer_tick() == SYNTRA_STATUS_OK);

    assert(timer_get_ticks() == 3ULL);
    assert(timer_get_milliseconds() == 3ULL);

    /* Information tests */

    assert(
        timer_get_info(&info) ==
        SYNTRA_STATUS_OK
    );

    assert(info.ticks == 3ULL);
    assert(info.elapsed_milliseconds == 3ULL);
    assert(info.tick_frequency == 1000U);
    assert(info.initialized == 1);
    assert(info.valid == 1);

    assert(
        timer_get_info(NULL) ==
        SYNTRA_STATUS_INVALID_ARGUMENT
    );

    /* Reset tests */

    assert(timer_reset() == SYNTRA_STATUS_OK);

    assert(timer_get_ticks() == 0ULL);
    assert(timer_get_milliseconds() == 0ULL);

    assert(timer_validate() == SYNTRA_STATUS_OK);

    assert(
        timer_get_info(&info) ==
        SYNTRA_STATUS_OK
    );

    assert(info.ticks == 0ULL);
    assert(info.elapsed_milliseconds == 0ULL);
    assert(info.valid == 1);

    printf("System Timer V1: all tests passed.\n");

    return 0;
}
