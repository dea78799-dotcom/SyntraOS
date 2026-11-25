#include "../../kernel/memory/memory.h"
#include "../../kernel/process/process.h"
#include "../../kernel/process/process_thread.h"
#include "../../kernel/scheduler/scheduler.h"
#include "../../kernel/scheduler/thread_reaper.h"

#include <stdint.h>
#include <stdio.h>

static unsigned int failures;
static _Alignas(16) unsigned char borrowed_stack[4096];
static int borrowed_context_marker;

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
    syntra_thread_handle_t reused_handle = {0U, 0U};
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
    {
        syntra_thread_slot_state_t retired_state;
        uint32_t retired_generation = 0U;
        unsigned int high_water_before = scheduler_get_thread_count();
        unsigned int active_before = scheduler_get_active_thread_count();

        check("Retire exact RECLAIMED candidate from bound reaper context",
            thread_reaper_retire_candidate(0U) == SYNTRA_STATUS_OK);
        check("Candidate history transitions to RETIRED",
            thread_reaper_get_candidate(0U, &candidate) == SYNTRA_STATUS_OK &&
            candidate.state == SYNTRA_REAPER_CANDIDATE_RETIRED &&
            candidate.reason == SYNTRA_REAPER_REASON_NONE);
        check("Retirement invalidates the old exact handle",
            scheduler_get_thread_snapshot_by_handle(
                owned_handle, &owned_after) == SYNTRA_STATUS_INVALID_ARGUMENT &&
            scheduler_set_thread_state_by_handle(
                owned_handle, THREAD_READY) == SYNTRA_STATUS_INVALID_ARGUMENT);
        check("Retired slot advances generation before becoming FREE",
            scheduler_get_slot_metadata(owned_handle.slot, &retired_state,
                &retired_generation) == SYNTRA_STATUS_OK &&
            retired_state == SYNTRA_THREAD_SLOT_FREE &&
            retired_generation == owned_handle.generation + 1U);
        check("Retirement preserves high-water and decrements active count",
            scheduler_get_thread_count() == high_water_before &&
            scheduler_get_active_thread_count() + 1U == active_before);
        check("Duplicate retirement cannot retire the old handle twice",
            thread_reaper_retire_candidate(0U) ==
                SYNTRA_STATUS_PERMISSION_DENIED);
        check("Scheduler validation accepts the retired slot hole",
            scheduler_validate() == SYNTRA_STATUS_OK);
        check("Retirement statistics record exactly one TCB retirement",
            thread_reaper_get_stats(&stats) == SYNTRA_STATUS_OK &&
            stats.retired == 1U && stats.retirement_failures == 0U);
    }
    check("Reject invalid owned-thread stack size before reserving a slot",
        process_thread_create_kernel_owned_with_handle(
            process, 955U, 2U, test_entry, NULL, 1U,
            &reused_handle) == SYNTRA_STATUS_INVALID_ARGUMENT);
    {
        syntra_thread_slot_state_t rejected_state;
        uint32_t rejected_generation = 0U;
        check("Rejected creation preserves retired FREE slot generation",
            scheduler_get_slot_metadata(owned_handle.slot, &rejected_state,
                &rejected_generation) == SYNTRA_STATUS_OK &&
            rejected_state == SYNTRA_THREAD_SLOT_FREE &&
            rejected_generation == owned_handle.generation + 1U);
        check("Rejected creation leaves scheduler invariants intact",
            scheduler_validate() == SYNTRA_STATUS_OK);
    }
    check("Create thread in retired slot using a fresh stack",
        process_thread_create_kernel_owned_with_handle(
            process, 954U, 2U, test_entry, NULL, 4096U,
            &reused_handle) == SYNTRA_STATUS_OK);
    check("Slot reuse preserves index and advances generation",
        reused_handle.slot == owned_handle.slot &&
        reused_handle.generation == owned_handle.generation + 1U);
    check("Stale handle cannot resolve after slot reuse",
        scheduler_get_thread_snapshot_by_handle(
            owned_handle, &owned_after) == SYNTRA_STATUS_INVALID_ARGUMENT &&
        scheduler_set_thread_state_by_handle(
            owned_handle, THREAD_RUNNING) == SYNTRA_STATUS_INVALID_ARGUMENT);
    check("New handle resolves to the reused TCB",
        scheduler_get_thread_snapshot_by_handle(
            reused_handle, &owned_after) == SYNTRA_STATUS_OK &&
        owned_after.thread_id == 954U &&
        owned_after.has_process_owner == 1U &&
        owned_after.owner_process.slot == process.slot &&
        owned_after.owner_process.generation == process.generation &&
        scheduler_thread_handle_equal(owned_after.handle, reused_handle));
    check("High-water count stays stable while active count increases",
        scheduler_get_thread_count() == 2U &&
        scheduler_get_active_thread_count() == 2U);
    check("Scheduler validation accepts a reused slot",
        scheduler_validate() == SYNTRA_STATUS_OK);
    check("Create a BORROWED kernel thread for fail-closed coverage",
        scheduler_create_thread_with_handle(
            953U, 1U, test_entry, NULL, borrowed_stack,
            sizeof(borrowed_stack), &(syntra_thread_handle_t){0U, 0U}) ==
                SYNTRA_STATUS_OK);
    {
        syntra_thread_handle_t borrowed_handle;
        syntra_thread_t borrowed_before;
        syntra_thread_t borrowed_after;
        unsigned int borrowed_processed = 0U;

        check("Resolve BORROWED thread identity",
            scheduler_get_thread_snapshot(
                953U, &borrowed_before) == SYNTRA_STATUS_OK);
        borrowed_handle = borrowed_before.handle;
        check("Install BORROWED thread context",
            scheduler_set_thread_context(
                953U, &borrowed_context_marker) == SYNTRA_STATUS_OK);
        check("Make BORROWED thread current",
            scheduler_set_thread_state_by_handle(
                borrowed_handle, THREAD_RUNNING) == SYNTRA_STATUS_OK);
        check("Terminate BORROWED thread",
            scheduler_terminate_current() == SYNTRA_STATUS_OK);
        check("Return current execution to dedicated reaper",
            scheduler_set_thread_state_by_handle(
                reaper_handle, THREAD_RUNNING) == SYNTRA_STATUS_OK);
        check("Publish BORROWED terminal switch-away proof",
            scheduler_complete_switch_away() == SYNTRA_STATUS_OK);
        check("Dispatch BORROWED terminal candidate",
            thread_reaper_dispatch(1U, &borrowed_processed) ==
                SYNTRA_STATUS_OK && borrowed_processed == 1U);
        memory_get_info(&memory_before);
        check("Reject BORROWED stack reclamation and quarantine candidate",
            thread_reaper_reclaim_candidate(1U) ==
                SYNTRA_STATUS_INVALID_ARGUMENT &&
            thread_reaper_get_candidate(1U, &candidate) == SYNTRA_STATUS_OK &&
            candidate.state == SYNTRA_REAPER_CANDIDATE_QUARANTINED &&
            candidate.reason ==
                SYNTRA_REAPER_REASON_RESOURCE_RECLAIM_FAILED);
        check("BORROWED TCB stack and context remain intact",
            scheduler_get_thread_snapshot_by_handle(
                borrowed_handle, &borrowed_after) == SYNTRA_STATUS_OK &&
            borrowed_after.state == THREAD_TERMINATED &&
            borrowed_after.stack_base == (uintptr_t)borrowed_stack &&
            borrowed_after.stack_size == sizeof(borrowed_stack) &&
            borrowed_after.cpu_context == &borrowed_context_marker &&
            borrowed_after.stack_ownership ==
                SYNTRA_THREAD_STACK_BORROWED);
        memory_get_info(&memory_after);
        check("Rejected BORROWED candidate does not alter allocator",
            memory_after.active_allocations ==
                memory_before.active_allocations);
        check("Quarantined candidate cannot be retried",
            thread_reaper_reclaim_candidate(1U) ==
                SYNTRA_STATUS_PERMISSION_DENIED);
        check("Quarantined BORROWED candidate cannot be retired",
            thread_reaper_retire_candidate(1U) ==
                SYNTRA_STATUS_PERMISSION_DENIED);
        check("Reaper statistics report reclaim, retirement and quarantine",
            thread_reaper_get_stats(&stats) == SYNTRA_STATUS_OK &&
            stats.reclaimed == 1U && stats.retired == 1U &&
            stats.quarantined == 1U && stats.reclaim_failures == 1U);
    }
    check("Memory allocator invariants remain valid after quarantine",
        memory_validate() == SYNTRA_STATUS_OK);
    check("Scheduler invariants remain valid after quarantine",
        scheduler_validate() == SYNTRA_STATUS_OK);

    if (failures != 0U)
    {
        printf("%u safe thread reclamation test(s) failed\n", failures);
        return 1;
    }

    printf("All safe thread resource reclamation tests passed\n");
    return 0;
}
