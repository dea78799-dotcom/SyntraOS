#include "../../kernel/scheduler/scheduler.h"
#include "../../kernel/scheduler/thread_reaper.h"

#include <stdint.h>
#include <stdio.h>

static unsigned int failures;

static void test_entry(void *argument)
{
    (void)argument;
}

static void check(const char *name, int condition)
{
    if (!condition)
    {
        ++failures;
        printf("FAIL: %s\n", name);
    }
    else
        printf("PASS: %s\n", name);
}

int main(void)
{
    volatile uintptr_t stack_anchor = 0U;
    uintptr_t anchor = (uintptr_t)&stack_anchor;
    uintptr_t stack_base;
    const size_t stack_size = 1024U * 1024U;
    void *initial_context;
    syntra_thread_handle_t reaper_handle = {0U, 0U};
    syntra_thread_handle_t other_handle = {0U, 0U};
    syntra_reaper_stats_t stats;
    unsigned int processed = 99U;

    check("Initialize scheduler", scheduler_init() == SYNTRA_STATUS_OK);
    check("Initialize reaper", thread_reaper_init() == SYNTRA_STATUS_OK);

    if (anchor < stack_size)
    {
        printf("FAIL: host stack address range underflow\n");
        return 1;
    }
    stack_base = (anchor - (stack_size / 2U)) & ~(uintptr_t)15U;
    initial_context = (void *)(stack_base + stack_size - 320U);

    check("Create permanent borrowed reaper thread",
        scheduler_create_thread_with_handle(
            901U, 0U, test_entry, NULL, (void *)stack_base, stack_size,
            &reaper_handle) == SYNTRA_STATUS_OK);
    check("Install initial context inside reaper stack",
        scheduler_set_thread_context(901U, initial_context) ==
            SYNTRA_STATUS_OK);
    check("Reject mismatched stack descriptor",
        thread_reaper_bind_execution_context(
            reaper_handle, stack_base + 16U, stack_size) ==
                SYNTRA_STATUS_INVALID_ARGUMENT);
    check("Bind exact reaper handle and stack range",
        thread_reaper_bind_execution_context(
            reaper_handle, stack_base, stack_size) == SYNTRA_STATUS_OK);
    check("Reject a second execution-context binding",
        thread_reaper_bind_execution_context(
            reaper_handle, stack_base, stack_size) == SYNTRA_STATUS_ERROR);

    check("Select bound reaper as current thread",
        scheduler_set_thread_state_by_handle(
            reaper_handle, THREAD_RUNNING) == SYNTRA_STATUS_OK);
    check("Dispatch accepted from bound current stack",
        thread_reaper_dispatch(1U, &processed) == SYNTRA_STATUS_OK &&
        processed == 0U);

    check("Add unrelated scheduler thread",
        scheduler_add_thread(902U, 0U) == SYNTRA_STATUS_OK);
    check("Switch scheduler current identity away from reaper",
        scheduler_set_thread_state(902U, THREAD_RUNNING) == SYNTRA_STATUS_OK);
    processed = 99U;
    check("Reject dispatch from a different current thread",
        thread_reaper_dispatch(1U, &processed) == SYNTRA_STATUS_ERROR &&
        processed == 0U);

    check("Execution-context diagnostics are exact",
        thread_reaper_get_stats(&stats) == SYNTRA_STATUS_OK &&
        stats.execution_context_bound == 1U &&
        stats.execution_context_dispatches == 1U &&
        stats.execution_context_rejections == 1U);
    check("Reaper identity remains stable",
        scheduler_get_thread_snapshot_by_handle(
            reaper_handle, &(syntra_thread_t){0}) == SYNTRA_STATUS_OK);
    check("Scheduler invariants remain valid",
        scheduler_validate() == SYNTRA_STATUS_OK);

    if (failures != 0U)
    {
        printf("%u reaper execution-context test(s) failed\n", failures);
        return 1;
    }

    printf("All reaper execution-context tests passed\n");
    return 0;
}
