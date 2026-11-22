#ifdef NDEBUG
#undef NDEBUG
#endif

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "../../kernel/scheduler/scheduler.h"

static void thread_entry(void *argument)
{
    (void)argument;
}

static _Alignas(16) uint8_t reusable_stack[2048];

int main(void)
{
    assert(scheduler_init() == SYNTRA_STATUS_OK);

    /* A live, blocked, or selected thread must never be reaped. */
    assert(scheduler_add_thread(1U, 1U) == SYNTRA_STATUS_OK);
    assert(scheduler_reap_terminated(1U) ==
        SYNTRA_STATUS_PERMISSION_DENIED);
    assert(scheduler_set_thread_state(1U, THREAD_BLOCKED) ==
        SYNTRA_STATUS_OK);
    assert(scheduler_reap_terminated(1U) ==
        SYNTRA_STATUS_PERMISSION_DENIED);
    assert(scheduler_wake_thread(1U) == SYNTRA_STATUS_OK);
    assert(scheduler_set_thread_state(1U, THREAD_RUNNING) ==
        SYNTRA_STATUS_OK);
    assert(scheduler_reap_terminated(1U) ==
        SYNTRA_STATUS_PERMISSION_DENIED);
    assert(scheduler_terminate_current() == SYNTRA_STATUS_OK);
    assert(scheduler_reap_terminated(1U) == SYNTRA_STATUS_OK);
    assert(scheduler_get_thread(1U) == NULL);
    assert(scheduler_get_thread_count() == 0U);
    assert(scheduler_validate() == SYNTRA_STATUS_OK);

    /* Caller-owned stack metadata is forgotten, not freed, by the reaper. */
    assert(scheduler_create_thread(2U, 3U, thread_entry, NULL,
        reusable_stack, sizeof(reusable_stack)) == SYNTRA_STATUS_OK);
    syntra_thread_t *thread = scheduler_get_thread(2U);
    assert(thread != NULL && thread->stack_base == (uintptr_t)reusable_stack);
    assert(scheduler_set_thread_state(2U, THREAD_TERMINATED) ==
        SYNTRA_STATUS_OK);
    assert(scheduler_reap_terminated(2U) == SYNTRA_STATUS_OK);
    assert(scheduler_get_thread(2U) == NULL);
    assert(scheduler_create_thread(3U, 3U, thread_entry, NULL,
        reusable_stack, sizeof(reusable_stack)) == SYNTRA_STATUS_OK);
    assert(scheduler_set_thread_state(3U, THREAD_TERMINATED) ==
        SYNTRA_STATUS_OK);
    assert(scheduler_reap_terminated(3U) == SYNTRA_STATUS_OK);

    /* A terminated ID remains reserved until its record is explicitly reaped. */
    assert(scheduler_add_thread(10U, 1U) == SYNTRA_STATUS_OK);
    assert(scheduler_set_thread_state(10U, THREAD_TERMINATED) ==
        SYNTRA_STATUS_OK);
    assert(scheduler_add_thread(10U, 1U) == SYNTRA_STATUS_INVALID_ARGUMENT);
    assert(scheduler_reap_terminated(10U) == SYNTRA_STATUS_OK);
    assert(scheduler_add_thread(10U, 1U) == SYNTRA_STATUS_OK);
    assert(scheduler_set_thread_state(10U, THREAD_TERMINATED) ==
        SYNTRA_STATUS_OK);
    assert(scheduler_reap_terminated(10U) == SYNTRA_STATUS_OK);

    /* Hundreds of lifecycles reuse bounded slots without growing the table. */
    for (unsigned int i = 0U; i < 512U; ++i)
    {
        unsigned int id = 1000U + i;
        assert(scheduler_add_thread(id, 5U) == SYNTRA_STATUS_OK);
        assert(scheduler_get_thread_count() == 1U);
        assert(scheduler_set_thread_state(id, THREAD_TERMINATED) ==
            SYNTRA_STATUS_OK);
        assert(scheduler_reap_terminated(id) == SYNTRA_STATUS_OK);
        assert(scheduler_get_thread(id) == NULL);
        assert(scheduler_get_thread_count() == 0U);
        assert(scheduler_validate() == SYNTRA_STATUS_OK);
    }

    /* Fill every slot, reject overflow, then reclaim and reuse all slots. */
    for (unsigned int i = 0U; i < SYNTRA_MAX_THREADS; ++i)
        assert(scheduler_add_thread(2000U + i, 1U) == SYNTRA_STATUS_OK);
    assert(scheduler_get_thread_count() == SYNTRA_MAX_THREADS);
    assert(scheduler_add_thread(3000U, 1U) == SYNTRA_STATUS_OUT_OF_MEMORY);
    for (unsigned int i = 0U; i < SYNTRA_MAX_THREADS; ++i)
    {
        unsigned int id = 2000U + i;
        assert(scheduler_set_thread_state(id, THREAD_TERMINATED) ==
            SYNTRA_STATUS_OK);
        assert(scheduler_reap_terminated(id) == SYNTRA_STATUS_OK);
    }
    assert(scheduler_get_thread_count() == 0U);
    assert(scheduler_add_thread(3000U, 1U) == SYNTRA_STATUS_OK);
    assert(scheduler_set_thread_state(3000U, THREAD_TERMINATED) ==
        SYNTRA_STATUS_OK);
    assert(scheduler_reap_terminated(3000U) == SYNTRA_STATUS_OK);

    /* Reclaimed holes must not break round-robin or context-aware selection. */
    assert(scheduler_add_thread(4000U, 1U) == SYNTRA_STATUS_OK);
    assert(scheduler_add_thread(4001U, 1U) == SYNTRA_STATUS_OK);
    assert(scheduler_set_thread_state(4000U, THREAD_TERMINATED) ==
        SYNTRA_STATUS_OK);
    assert(scheduler_reap_terminated(4000U) == SYNTRA_STATUS_OK);
    syntra_thread_t *selected = scheduler_schedule_next();
    assert(selected != NULL && selected->thread_id == 4001U);
    assert(scheduler_terminate_current() == SYNTRA_STATUS_OK);
    assert(scheduler_reap_terminated(4001U) == SYNTRA_STATUS_OK);
    assert(scheduler_validate() == SYNTRA_STATUS_OK);

    puts("Scheduler safe thread reclamation and slot reuse passed");
    return 0;
}
