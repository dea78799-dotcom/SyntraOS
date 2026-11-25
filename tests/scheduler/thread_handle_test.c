#include <stdio.h>
#include <stdint.h>

#include "../../kernel/scheduler/scheduler.h"

static int failures;

static void check(const char *name, int ok)
{
    printf("[%s] %s\n", ok ? "PASS" : "FAIL", name);
    if (!ok) ++failures;
}

static void entry(void *argument) { (void)argument; }

static _Alignas(16) unsigned char stack_a[2048];
static _Alignas(16) unsigned char stack_b[2048];

int main(void)
{
    syntra_thread_handle_t first = {0U, 0U};
    syntra_thread_handle_t second = {0U, 0U};
    syntra_thread_handle_t invalid;
    syntra_thread_t snapshot;
    syntra_thread_t before;
    syntra_thread_slot_state_t slot_state;
    uint32_t slot_generation = 0U;

    check("scheduler initializes", scheduler_init() == SYNTRA_STATUS_OK);
    check("fresh scheduler has zero active threads",
        scheduler_get_active_thread_count() == 0U);
    check("first slot starts FREE with generation one",
        scheduler_get_slot_metadata(0U, &slot_state, &slot_generation) ==
            SYNTRA_STATUS_OK && slot_state == SYNTRA_THREAD_SLOT_FREE &&
        slot_generation == 1U);
    check("handle create commits first thread",
        scheduler_create_thread_with_handle(101U, 1U, entry, NULL,
            stack_a, sizeof(stack_a), &first) == SYNTRA_STATUS_OK);
    check("first handle has slot zero and nonzero generation",
        first.slot == 0U && first.generation == 1U);
    check("first publication activates slot zero",
        scheduler_get_slot_metadata(0U, &slot_state, &slot_generation) ==
            SYNTRA_STATUS_OK && slot_state == SYNTRA_THREAD_SLOT_ACTIVE &&
        slot_generation == first.generation);
    check("handle create commits second thread",
        scheduler_create_thread_with_handle(102U, 1U, entry, NULL,
            stack_b, sizeof(stack_b), &second) == SYNTRA_STATUS_OK);
    check("second handle identifies the second slot",
        second.slot == 1U && second.generation == 1U);
    check("high-water and active counts agree before retirement exists",
        scheduler_get_thread_count() == 2U &&
        scheduler_get_active_thread_count() == 2U);
    check("equal handles compare both fields",
        scheduler_thread_handle_equal(first, first) &&
        !scheduler_thread_handle_equal(first, second));

    check("exact handle resolves expected thread",
        scheduler_get_thread_snapshot_by_handle(first, &snapshot) ==
            SYNTRA_STATUS_OK && snapshot.thread_id == 101U &&
            snapshot.handle.slot == first.slot &&
            snapshot.handle.generation == first.generation);

    invalid = first;
    invalid.generation = 0U;
    snapshot.thread_id = 900U;
    check("zero generation is rejected without changing output",
        scheduler_get_thread_snapshot_by_handle(invalid, &snapshot) ==
            SYNTRA_STATUS_INVALID_ARGUMENT && snapshot.thread_id == 900U);

    invalid = first;
    invalid.generation += 1U;
    check("stale generation is rejected",
        scheduler_get_thread_snapshot_by_handle(invalid, &snapshot) ==
            SYNTRA_STATUS_INVALID_ARGUMENT);

    invalid.slot = SYNTRA_MAX_THREADS;
    invalid.generation = 1U;
    check("out-of-range slot is rejected",
        scheduler_get_thread_snapshot_by_handle(invalid, &snapshot) ==
            SYNTRA_STATUS_INVALID_ARGUMENT);

    invalid.slot = 31U;
    invalid.generation = 1U;
    check("unoccupied slot is rejected",
        scheduler_get_thread_snapshot_by_handle(invalid, &snapshot) ==
            SYNTRA_STATUS_INVALID_ARGUMENT);

    check("capture state before invalid update",
        scheduler_get_thread_snapshot_by_handle(first, &before) ==
            SYNTRA_STATUS_OK && before.state == THREAD_READY);
    check("invalid handle cannot update state",
        scheduler_set_thread_state_by_handle(invalid, THREAD_TERMINATED) ==
            SYNTRA_STATUS_INVALID_ARGUMENT);
    check("invalid update leaves target state unchanged",
        scheduler_get_thread_snapshot_by_handle(first, &snapshot) ==
            SYNTRA_STATUS_OK && snapshot.state == before.state);

    check("valid handle terminates exact thread",
        scheduler_set_thread_state_by_handle(first, THREAD_TERMINATED) ==
            SYNTRA_STATUS_OK);
    check("termination preserves thread identity",
        scheduler_get_thread_snapshot_by_handle(first, &snapshot) ==
            SYNTRA_STATUS_OK && snapshot.state == THREAD_TERMINATED &&
            scheduler_thread_handle_equal(snapshot.handle, first));
    check("terminated thread cannot be made READY",
        scheduler_set_thread_state_by_handle(first, THREAD_READY) ==
            SYNTRA_STATUS_INVALID_ARGUMENT);
    check("scheduler invariants include handle metadata",
        scheduler_validate() == SYNTRA_STATUS_OK);

    printf("Thread Handle V1 failures: %d\n", failures);
    return failures == 0 ? 0 : 1;
}
