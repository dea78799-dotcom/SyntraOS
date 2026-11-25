#include "../../kernel/memory/memory.h"
#include "../../kernel/process/process.h"
#include "../../kernel/process/process_thread.h"
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
    uintptr_t reaper_stack_base;
    const size_t reaper_stack_size = 65536U;
    syntra_process_handle_t process = {0U, 0U};
    syntra_thread_handle_t owned_handle = {0U, 0U};
    syntra_thread_handle_t reaper_handle = {0U, 0U};
    syntra_thread_t owned_before;
    syntra_thread_t owned_after;
    syntra_reaper_candidate_t candidate;
    syntra_reaper_stats_t stats;
    syntra_memory_info_t memory_before;
    syntra_memory_info_t memory_after;
    unsigned int processed = 0U;

    check("Initialize memory", memory_init() == SYNTRA_STATUS_OK);
    check("Initialize process manager", process_init() == SYNTRA_STATUS_OK);
    check("Initialize scheduler", scheduler_init() == SYNTRA_STATUS_OK);
    check("Initialize reaper", thread_reaper_init() == SYNTRA_STATUS_OK);

    if (anchor < reaper_stack_size / 2U)
    {
        printf("FAIL: host stack range underflow\n");
        return 1;
    }
    reaper_stack_base =
        (anchor - reaper_stack_size / 2U) & ~(uintptr_t)15U;

    check("Create exact process owner",
        process_create_with_handle(950U, 1U, &process) == SYNTRA_STATUS_OK);
    check("Create allocator-owned kernel thread",
        process_thread_create_kernel_owned_with_handle(
            process, 951U, 1U, test_entry, NULL, 4096U,
            &owned_handle) == SYNTRA_STATUS_OK);
    check("Capture owned thread snapshot",
        scheduler_get_thread_snapshot_by_handle(
            owned_handle, &owned_before) == SYNTRA_STATUS_OK &&
        owned_before.stack_ownership == SYNTRA_THREAD_STACK_OWNED &&
        owned_before.stack_allocation.generation != 0U);

    check("Create dedicated borrowed reaper thread",
        scheduler_create_thread_with_handle(
            952U, 0U, test_entry, NULL, (void *)reaper_stack_base,
            reaper_stack_size, &reaper_handle) == SYNTRA_STATUS_OK);
    check("Install reaper context inside its stack",
        scheduler_set_thread_context(
            952U, (void *)(reaper_stack_base + reaper_stack_size - 256U)) ==
                SYNTRA_STATUS_OK);
    check("Bind exact reaper handle and stack",
        thread_reaper_bind_execution_context(
            reaper_handle, reaper_stack_base, reaper_stack_size) ==
                SYNTRA_STATUS_OK);

    check("Make owned thread current",
        scheduler_set_thread_state_by_handle(
            owned_handle, THREAD_RUNNING) == SYNTRA_STATUS_OK);
    check("Terminate owned thread",
        scheduler_terminate_current() == SYNTRA_STATUS_OK);
    check("Switch current execution to reaper",
        scheduler_set_thread_state_by_handle(
            reaper_handle, THREAD_RUNNING) == SYNTRA_STATUS_OK);
    check("Publish exact post-switch-away proof",
        scheduler_complete_switch_away() == SYNTRA_STATUS_OK);

    memory_get_info(&memory_before);
    check("Dispatch terminal proof on bound reaper context",
        thread_reaper_dispatch(4U, &processed) == SYNTRA_STATUS_OK &&
        processed == 1U);
    check("Candidate preserves exact terminal proof",
        thread_reaper_get_candidate(0U, &candidate) == SYNTRA_STATUS_OK &&
        scheduler_thread_handle_equal(candidate.proof.handle, owned_handle) &&
        candidate.proof.terminal == 1U &&
        candidate.proof.sequence != 0U &&
        candidate.state == SYNTRA_REAPER_CANDIDATE_QUEUED);

    check("Reject stale switch-away sequence without freeing stack",
        scheduler_reclaim_owned_thread_stack(
            owned_handle, candidate.proof.sequence + 1U) ==
                SYNTRA_STATUS_INVALID_ARGUMENT);
    memory_get_info(&memory_after);
    check("Stale proof leaves allocation active",
        memory_after.active_allocations == memory_before.active_allocations);

    check("Reclaim exact owned stack from reaper context",
        thread_reaper_reclaim_candidate(0U) == SYNTRA_STATUS_OK);
    check("Candidate transitions to RECLAIMED",
        thread_reaper_get_candidate(0U, &candidate) == SYNTRA_STATUS_OK &&
        candidate.state == SYNTRA_REAPER_CANDIDATE_RECLAIMED &&
        candidate.reason == SYNTRA_REAPER_REASON_NONE);
    check("Terminated TCB identity remains allocated",
        scheduler_get_thread_snapshot_by_handle(
            owned_handle, &owned_after) == SYNTRA_STATUS_OK &&
        owned_after.state == THREAD_TERMINATED &&
        scheduler_thread_handle_equal(owned_after.handle, owned_handle));
    check("TCB no longer references released stack or context",
        owned_after.stack_base == 0U &&
        owned_after.stack_size == 0U &&
        owned_after.stack_ownership == SYNTRA_THREAD_STACK_BORROWED &&
        owned_after.stack_allocation.slot == 0U &&
        owned_after.stack_allocation.generation == 0U &&
        owned_after.cpu_context == NULL &&
        owned_after.entry == NULL &&
        owned_after.argument == NULL);
    check("Old allocation token cannot resolve after reclamation",
        memory_get_allocation_by_handle(
            owned_before.stack_allocation,
            &(syntra_memory_allocation_t){0}) ==
                SYNTRA_STATUS_INVALID_ARGUMENT);
    memory_get_info(&memory_after);
    check("Owned stack allocation count decreases exactly once",
        memory_after.active_allocations + 1U ==
            memory_before.active_allocations);
    check("Duplicate candidate reclamation is rejected",
        thread_reaper_reclaim_candidate(0U) ==
            SYNTRA_STATUS_PERMISSION_DENIED);
    check("Reaper statistics record one reclamation",
        thread_reaper_get_stats(&stats) == SYNTRA_STATUS_OK &&
        stats.reclaimed == 1U && stats.reclaim_failures == 0U);
    check("Memory allocator invariants remain valid",
        memory_validate() == SYNTRA_STATUS_OK);
    check("Scheduler invariants remain valid",
        scheduler_validate() == SYNTRA_STATUS_OK);

    if (failures != 0U)
    {
        printf("%u safe thread reclamation test(s) failed\n", failures);
        return 1;
    }

    printf("All safe thread resource reclamation tests passed\n");
    return 0;
}
