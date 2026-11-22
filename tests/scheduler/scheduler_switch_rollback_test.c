#ifdef NDEBUG
#undef NDEBUG
#endif

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "../../kernel/scheduler/scheduler.h"

static _Alignas(16) uint8_t boot_stack[2048];
static _Alignas(16) uint8_t thread_a_stack[2048];
static _Alignas(16) uint8_t thread_b_stack[2048];
static _Alignas(16) uint8_t thread_a_context[320];
static _Alignas(16) uint8_t thread_b_context[320];

static void thread_entry(void *argument)
{
    (void)argument;
}

int main(void)
{
    syntra_thread_t *selected;
    syntra_thread_t *current;

    assert(scheduler_init() == SYNTRA_STATUS_OK);
    assert(scheduler_create_thread(
        1U, 0U, thread_entry, NULL,
        boot_stack, sizeof(boot_stack)) == SYNTRA_STATUS_OK);
    assert(scheduler_create_user_thread(
        2U, 1U, thread_entry, NULL,
        thread_a_stack, sizeof(thread_a_stack),
        thread_a_stack, sizeof(thread_a_stack), thread_a_context) ==
        SYNTRA_STATUS_INVALID_ARGUMENT);

    /* Register two context-bearing kernel threads with disjoint stacks. */
    assert(scheduler_create_thread(
        2U, 1U, thread_entry, NULL,
        thread_a_stack, sizeof(thread_a_stack)) == SYNTRA_STATUS_OK);
    assert(scheduler_create_thread(
        3U, 1U, thread_entry, NULL,
        thread_b_stack, sizeof(thread_b_stack)) == SYNTRA_STATUS_OK);
    scheduler_get_thread(2U)->cpu_context = thread_a_context;
    scheduler_get_thread(3U)->cpu_context = thread_b_context;

    /* Initial scheduler start: a failed activation must restore boot thread. */
    assert(scheduler_set_thread_state(1U, THREAD_BLOCKED) == SYNTRA_STATUS_OK);
    selected = scheduler_schedule_next_with_context();
    assert(selected != NULL && selected->thread_id == 2U);
    assert(scheduler_get_current() == selected);
    assert(scheduler_rollback_context_switch(1U, 2U) == SYNTRA_STATUS_OK);

    current = scheduler_get_current();
    assert(current != NULL && current->thread_id == 1U);
    assert(current->state == THREAD_RUNNING);
    assert(scheduler_get_thread(2U)->state == THREAD_READY);
    assert(scheduler_get_thread(3U)->state == THREAD_READY);
    assert(scheduler_validate() == SYNTRA_STATUS_OK);

    /* Retry is deterministic: the failed candidate is tried first again. */
    assert(scheduler_set_thread_state(1U, THREAD_BLOCKED) == SYNTRA_STATUS_OK);
    selected = scheduler_schedule_next_with_context();
    assert(selected != NULL && selected->thread_id == 2U);
    assert(scheduler_set_thread_state(1U, THREAD_READY) == SYNTRA_STATUS_OK);

    /* Establish thread 2 as current, then simulate IRQ selecting thread 3. */
    assert(scheduler_set_thread_state(2U, THREAD_RUNNING) == SYNTRA_STATUS_OK);
    selected = scheduler_schedule_next_with_context();
    assert(selected != NULL && selected->thread_id == 3U);
    assert(scheduler_rollback_context_switch(2U, 3U) == SYNTRA_STATUS_OK);

    current = scheduler_get_current();
    assert(current != NULL && current->thread_id == 2U);
    assert(current->state == THREAD_RUNNING);
    assert(scheduler_get_thread(3U)->state == THREAD_READY);
    assert(scheduler_validate() == SYNTRA_STATUS_OK);

    /* A retry after rollback selects the same pending target. */
    selected = scheduler_schedule_next_with_context();
    assert(selected != NULL && selected->thread_id == 3U);
    assert(scheduler_validate() == SYNTRA_STATUS_OK);

    /*
     * A thread may terminate before the IRQ selects its successor. Roll back
     * a failed activation to an idle/no-current state, not to the dead thread.
     */
    assert(scheduler_set_thread_state(3U, THREAD_TERMINATED) ==
        SYNTRA_STATUS_OK);
    selected = scheduler_schedule_next_with_context();
    assert(selected != NULL && selected->thread_id == 2U);
    assert(scheduler_rollback_context_switch(0U, 2U) == SYNTRA_STATUS_OK);
    assert(scheduler_get_current() == NULL);
    assert(scheduler_get_thread(2U)->state == THREAD_READY);
    assert(scheduler_validate() == SYNTRA_STATUS_OK);

    selected = scheduler_schedule_next_with_context();
    assert(selected != NULL && selected->thread_id == 2U);
    assert(scheduler_validate() == SYNTRA_STATUS_OK);

    /* Reject stale or forged rollback requests without mutating state. */
    assert(scheduler_rollback_context_switch(999U, 2U) ==
        SYNTRA_STATUS_INVALID_ARGUMENT);
    assert(scheduler_get_current() == selected);
    assert(scheduler_validate() == SYNTRA_STATUS_OK);

    puts("Scheduler context-switch rollback test passed");
    return 0;
}
