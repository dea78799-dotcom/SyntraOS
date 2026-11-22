#ifdef NDEBUG
#undef NDEBUG
#endif

#include <assert.h>
#include <stdio.h>

#include "../../kernel/process/process.h"
#include "../../kernel/scheduler/scheduler.h"

int main(void)
{
    assert(scheduler_init() == SYNTRA_STATUS_OK);
    assert(process_init() == SYNTRA_STATUS_OK);

    /* Repeatedly consume and release the same bounded process-table capacity. */
    for (unsigned int i = 0U; i < 512U; ++i)
    {
        unsigned int process_id = i + 1U;
        assert(process_create(process_id, 5U) == SYNTRA_STATUS_OK);
        assert(process_get_count() == 1U);
        assert(process_terminate(process_id) == SYNTRA_STATUS_OK);
        assert(process_release_terminated(process_id) == SYNTRA_STATUS_OK);
        assert(process_get(process_id) == NULL);
        assert(process_get_count() == 0U);
        assert(process_refresh_all_states() == SYNTRA_STATUS_OK);
        assert(process_validate() == SYNTRA_STATUS_OK);
    }

    /* A live process and a process with live threads cannot be reclaimed. */
    assert(process_create(10000U, 5U) == SYNTRA_STATUS_OK);
    assert(process_release_terminated(10000U) ==
        SYNTRA_STATUS_PERMISSION_DENIED);
    assert(scheduler_add_thread(700U, 5U) == SYNTRA_STATUS_OK);
    assert(process_attach_thread(10000U, 700U) == SYNTRA_STATUS_OK);
    assert(process_terminate(10000U) == SYNTRA_STATUS_PERMISSION_DENIED);
    assert(process_terminate_threads(10000U) == SYNTRA_STATUS_OK);
    assert(process_refresh_state(10000U) == SYNTRA_STATUS_OK);
    assert(process_terminate(10000U) == SYNTRA_STATUS_OK);
    assert(process_release_terminated(10000U) == SYNTRA_STATUS_OK);
    assert(process_get_id_for_thread(700U) == 0U);
    assert(process_validate() == SYNTRA_STATUS_OK);

    /* Fill every slot, reject overflow, then prove every slot is reusable. */
    for (unsigned int i = 0U; i < SYNTRA_MAX_PROCESSES; ++i)
        assert(process_create(20000U + i, 1U) == SYNTRA_STATUS_OK);
    assert(process_get_count() == SYNTRA_MAX_PROCESSES);
    assert(process_create(30000U, 1U) == SYNTRA_STATUS_OUT_OF_MEMORY);
    for (unsigned int i = 0U; i < SYNTRA_MAX_PROCESSES; ++i)
    {
        unsigned int process_id = 20000U + i;
        assert(process_terminate(process_id) == SYNTRA_STATUS_OK);
        assert(process_release_terminated(process_id) == SYNTRA_STATUS_OK);
    }
    assert(process_get_count() == 0U);
    assert(process_create(30000U, 1U) == SYNTRA_STATUS_OK);
    assert(process_terminate(30000U) == SYNTRA_STATUS_OK);
    assert(process_release_terminated(30000U) == SYNTRA_STATUS_OK);
    assert(process_validate() == SYNTRA_STATUS_OK);

    puts("Process lifecycle safe reclamation and slot reuse passed");
    return 0;
}
