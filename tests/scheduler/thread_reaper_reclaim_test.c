#include "../../kernel/memory/memory.h"
#include "../../kernel/process/process.h"
#include "../../kernel/process/process_thread.h"
#include "../../kernel/scheduler/scheduler.h"
#include "../../kernel/scheduler/thread_reaper.h"

#include <stdint.h>
#include <stdio.h>

#define PROCESS_THREAD_TEST_FAIL_AFTER_STAGED_ALLOCATION 1U
#define PROCESS_THREAD_TEST_FAIL_AFTER_TCB_RESERVATION 2U
#define PROCESS_THREAD_TEST_FAIL_AFTER_OWNER_BIND 3U
#define PROCESS_THREAD_TEST_FAIL_AFTER_STACK_ATTACH 4U
#define PROCESS_THREAD_TEST_FAIL_AFTER_ALLOCATION_COMMIT 5U

/* Defined only by the host-test build of syntra_kernel. */
extern void process_thread_test_fail_at(unsigned int stage);
extern syntra_status_t scheduler_test_seed_free_slot_generation(
    unsigned int slot, uint32_t generation);

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

static int find_reaper_candidate_index(
    syntra_thread_handle_t handle)
{
    unsigned int count = 0U;
    syntra_reaper_candidate_t candidate;

    if (thread_reaper_get_candidate_count(&count) != SYNTRA_STATUS_OK)
        return -1;

    for (unsigned int i = 0U; i < count; ++i)
    {
        if (thread_reaper_get_candidate(i, &candidate) == SYNTRA_STATUS_OK &&
            scheduler_thread_handle_equal(candidate.proof.handle, handle))
            return (int)i;
    }

    return -1;
}

static void verify_owned_thread_creation_rollback(
    syntra_process_handle_t process,
    unsigned int stage,
    unsigned int thread_id,
    unsigned int expected_slot,
    uint32_t expected_generation,
    const char *stage_name)
{
    syntra_memory_info_t before;
    syntra_memory_info_t after;
    syntra_thread_slot_state_t slot_state = SYNTRA_THREAD_SLOT_EXHAUSTED;
    uint32_t slot_generation = 0U;
    unsigned int process_threads_before = 0U;
    unsigned int process_threads_after = 0U;
    unsigned int active_threads_before = scheduler_get_active_thread_count();
    syntra_thread_handle_t failed_handle = {0U, 0U};
    syntra_status_t status;

    memory_get_info(&before);
    check("Capture process thread count before rollback injection",
        process_thread_get_count(process, &process_threads_before) ==
            SYNTRA_STATUS_OK);

    process_thread_test_fail_at(stage);
    status = process_thread_create_kernel_owned_with_handle(
        process, thread_id, 2U, test_entry, NULL, 4096U, &failed_handle);
    check(stage_name, status == SYNTRA_STATUS_ERROR);

    memory_get_info(&after);
    check("Rollback restores active allocation count",
        after.active_allocations == before.active_allocations);
    check("Rollback restores used memory blocks",
        after.used_blocks == before.used_blocks);
    check("Rollback leaves allocator internally valid",
        memory_validate() == SYNTRA_STATUS_OK);
    check("Rollback preserves retired slot state and generation",
        scheduler_get_slot_metadata(expected_slot, &slot_state,
            &slot_generation) == SYNTRA_STATUS_OK &&
        slot_state == SYNTRA_THREAD_SLOT_FREE &&
        slot_generation == expected_generation);
    check("Rollback restores scheduler active-thread count",
        scheduler_get_active_thread_count() == active_threads_before);
    check("Rollback leaves scheduler internally valid",
        scheduler_validate() == SYNTRA_STATUS_OK);
    check("Rollback preserves process-owned thread count",
        process_thread_get_count(process, &process_threads_after) ==
            SYNTRA_STATUS_OK &&
        process_threads_after == process_threads_before);
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
    verify_owned_thread_creation_rollback(process,
        PROCESS_THREAD_TEST_FAIL_AFTER_STAGED_ALLOCATION, 960U,
        owned_handle.slot, owned_handle.generation + 1U,
        "Inject failure after staged stack allocation");
    verify_owned_thread_creation_rollback(process,
        PROCESS_THREAD_TEST_FAIL_AFTER_TCB_RESERVATION, 961U,
        owned_handle.slot, owned_handle.generation + 1U,
        "Inject failure after TCB reservation");
    verify_owned_thread_creation_rollback(process,
        PROCESS_THREAD_TEST_FAIL_AFTER_OWNER_BIND, 962U,
        owned_handle.slot, owned_handle.generation + 1U,
        "Inject failure after allocation owner binding");
    verify_owned_thread_creation_rollback(process,
        PROCESS_THREAD_TEST_FAIL_AFTER_STACK_ATTACH, 963U,
        owned_handle.slot, owned_handle.generation + 1U,
        "Inject failure after stack descriptor attachment");
    verify_owned_thread_creation_rollback(process,
        PROCESS_THREAD_TEST_FAIL_AFTER_ALLOCATION_COMMIT, 964U,
        owned_handle.slot, owned_handle.generation + 1U,
        "Inject failure after owned-allocation commit");

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
        int borrowed_candidate_index = -1;

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
        borrowed_candidate_index =
            find_reaper_candidate_index(borrowed_handle);
        check("Locate BORROWED candidate by exact handle",
            borrowed_candidate_index >= 0);
        if (borrowed_candidate_index < 0)
            borrowed_candidate_index = 0;
        memory_get_info(&memory_before);
        check("Reject BORROWED stack reclamation and quarantine candidate",
            thread_reaper_reclaim_candidate(
                (unsigned int)borrowed_candidate_index) ==
                    SYNTRA_STATUS_INVALID_ARGUMENT &&
            thread_reaper_get_candidate(
                (unsigned int)borrowed_candidate_index, &candidate) ==
                    SYNTRA_STATUS_OK &&
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
            thread_reaper_reclaim_candidate(
                (unsigned int)borrowed_candidate_index) ==
                    SYNTRA_STATUS_PERMISSION_DENIED);
        check("Quarantined BORROWED candidate cannot be retired",
            thread_reaper_retire_candidate(
                (unsigned int)borrowed_candidate_index) ==
                    SYNTRA_STATUS_PERMISSION_DENIED);
        check("Reaper statistics report reclaim, retirement and quarantine",
            thread_reaper_get_stats(&stats) == SYNTRA_STATUS_OK &&
            stats.reclaimed == 1U && stats.retired == 1U &&
            stats.quarantined == 1U && stats.reclaim_failures == 1U);
    }

    {
        const unsigned int exhaustion_slot = 3U;
        syntra_thread_handle_t exhaustion_handle = {0U, 0U};
        syntra_thread_handle_t after_exhaustion_handle = {0U, 0U};
        syntra_thread_slot_state_t exhaustion_state =
            SYNTRA_THREAD_SLOT_FREE;
        uint32_t exhaustion_generation = 0U;
        unsigned int exhaustion_processed = 0U;
        unsigned int high_water_before = scheduler_get_thread_count();
        int exhaustion_candidate_index = -1;

        check("Seed unused tail slot at UINT32_MAX for host boundary test",
            scheduler_test_seed_free_slot_generation(
                exhaustion_slot, UINT32_MAX) == SYNTRA_STATUS_OK);
        check("Create owned thread with terminal generation",
            process_thread_create_kernel_owned_with_handle(
                process, 956U, 2U, test_entry, NULL, 4096U,
                &exhaustion_handle) == SYNTRA_STATUS_OK &&
            exhaustion_handle.slot == exhaustion_slot &&
            exhaustion_handle.generation == UINT32_MAX);
        check("Make terminal-generation thread current",
            scheduler_set_thread_state_by_handle(
                exhaustion_handle, THREAD_RUNNING) == SYNTRA_STATUS_OK);
        check("Terminate terminal-generation thread",
            scheduler_terminate_current() == SYNTRA_STATUS_OK);
        check("Switch to reaper for terminal-generation thread",
            scheduler_set_thread_state_by_handle(
                reaper_handle, THREAD_RUNNING) == SYNTRA_STATUS_OK);
        check("Publish terminal-generation switch-away proof",
            scheduler_complete_switch_away() == SYNTRA_STATUS_OK);
        check("Dispatch terminal-generation candidate",
            thread_reaper_dispatch(1U, &exhaustion_processed) ==
                SYNTRA_STATUS_OK && exhaustion_processed == 1U);
        exhaustion_candidate_index =
            find_reaper_candidate_index(exhaustion_handle);
        check("Locate terminal-generation candidate by exact handle",
            exhaustion_candidate_index >= 0);
        if (exhaustion_candidate_index < 0)
            exhaustion_candidate_index = 0;
        check("Reclaim terminal-generation thread stack",
            thread_reaper_reclaim_candidate(
                (unsigned int)exhaustion_candidate_index) == SYNTRA_STATUS_OK);
        check("Retire terminal-generation TCB without wrapping",
            thread_reaper_retire_candidate(
                (unsigned int)exhaustion_candidate_index) == SYNTRA_STATUS_OK);
        check("UINT32_MAX generation becomes permanently EXHAUSTED",
            scheduler_get_slot_metadata(exhaustion_slot, &exhaustion_state,
                &exhaustion_generation) == SYNTRA_STATUS_OK &&
            exhaustion_state == SYNTRA_THREAD_SLOT_EXHAUSTED &&
            exhaustion_generation == UINT32_MAX);
        check("Exhaustion invalidates the terminal-generation handle",
            scheduler_get_thread_snapshot_by_handle(
                exhaustion_handle, &owned_after) ==
                    SYNTRA_STATUS_INVALID_ARGUMENT);
        check("Exhaustion preserves high-water count and active accounting",
            scheduler_get_thread_count() == high_water_before + 1U &&
            scheduler_get_active_thread_count() == 3U);
        check("Scheduler remains valid after permanent slot exhaustion",
            scheduler_validate() == SYNTRA_STATUS_OK);
        check("New creation skips EXHAUSTED slot and appends a fresh slot",
            process_thread_create_kernel_owned_with_handle(
                process, 957U, 2U, test_entry, NULL, 4096U,
                &after_exhaustion_handle) == SYNTRA_STATUS_OK &&
            after_exhaustion_handle.slot != exhaustion_slot &&
            after_exhaustion_handle.slot == exhaustion_slot + 1U);
        check("EXHAUSTED slot remains unchanged after later allocation",
            scheduler_get_slot_metadata(exhaustion_slot, &exhaustion_state,
                &exhaustion_generation) == SYNTRA_STATUS_OK &&
            exhaustion_state == SYNTRA_THREAD_SLOT_EXHAUSTED &&
            exhaustion_generation == UINT32_MAX);
        check("Scheduler validation accepts appended slot beside exhaustion",
            scheduler_validate() == SYNTRA_STATUS_OK);
    }


    {
        const unsigned int recycle_cycles = 40U;
        unsigned int candidate_count_before = 0U;
        unsigned int reusable_scheduler_slot = UINT32_MAX;
        int reusable_candidate_index =
            find_reaper_candidate_index(exhaustion_handle);

        check("Locate retired candidate record for reuse",
            reusable_candidate_index >= 0);
        if (reusable_candidate_index < 0)
            reusable_candidate_index = 0;
        check("Capture bounded reaper table count before recycle stress",
            thread_reaper_get_candidate_count(&candidate_count_before) ==
                SYNTRA_STATUS_OK && candidate_count_before == 2U);

        for (unsigned int cycle = 0U; cycle < recycle_cycles; ++cycle)
        {
            syntra_thread_handle_t cycle_handle = {0U, 0U};
            unsigned int cycle_processed = 0U;
            syntra_status_t cycle_status;

            cycle_status = process_thread_create_kernel_owned_with_handle(
                process, 1000U + cycle, 2U, test_entry, NULL, 4096U,
                &cycle_handle);
            check("Create next owned thread in recycle stress",
                cycle_status == SYNTRA_STATUS_OK);
            if (cycle_status != SYNTRA_STATUS_OK)
                break;

            if (reusable_scheduler_slot == UINT32_MAX)
                reusable_scheduler_slot = cycle_handle.slot;
            check("Scheduler reuses the same retired TCB slot during stress",
                cycle_handle.slot == reusable_scheduler_slot);

            cycle_status = scheduler_set_thread_state_by_handle(
                cycle_handle, THREAD_RUNNING);
            check("Run recycle-stress thread", cycle_status == SYNTRA_STATUS_OK);
            if (cycle_status != SYNTRA_STATUS_OK)
                break;

            cycle_status = scheduler_terminate_current();
            check("Terminate recycle-stress thread",
                cycle_status == SYNTRA_STATUS_OK);
            if (cycle_status != SYNTRA_STATUS_OK)
                break;

            cycle_status = scheduler_set_thread_state_by_handle(
                reaper_handle, THREAD_RUNNING);
            check("Return to reaper for recycle-stress thread",
                cycle_status == SYNTRA_STATUS_OK);
            if (cycle_status != SYNTRA_STATUS_OK)
                break;

            cycle_status = scheduler_complete_switch_away();
            check("Publish recycle-stress switch-away proof",
                cycle_status == SYNTRA_STATUS_OK);
            if (cycle_status != SYNTRA_STATUS_OK)
                break;

            cycle_status = thread_reaper_dispatch(1U, &cycle_processed);
            check("Dispatch recycle-stress proof into reused candidate",
                cycle_status == SYNTRA_STATUS_OK && cycle_processed == 1U);
            if (cycle_status != SYNTRA_STATUS_OK || cycle_processed != 1U)
                break;

            check("Recycled candidate records the newest exact handle",
                thread_reaper_get_candidate(
                    (unsigned int)reusable_candidate_index, &candidate) ==
                    SYNTRA_STATUS_OK &&
                scheduler_thread_handle_equal(
                    candidate.proof.handle, cycle_handle) &&
                candidate.state == SYNTRA_REAPER_CANDIDATE_QUEUED);

            cycle_status = thread_reaper_reclaim_candidate(
                (unsigned int)reusable_candidate_index);
            check("Reclaim recycle-stress owned stack",
                cycle_status == SYNTRA_STATUS_OK);
            if (cycle_status != SYNTRA_STATUS_OK)
                break;

            cycle_status = thread_reaper_retire_candidate(
                (unsigned int)reusable_candidate_index);
            check("Retire recycle-stress TCB",
                cycle_status == SYNTRA_STATUS_OK);
            if (cycle_status != SYNTRA_STATUS_OK)
                break;

            check("Recycled candidate returns to RETIRED",
                thread_reaper_get_candidate(
                    (unsigned int)reusable_candidate_index, &candidate) ==
                    SYNTRA_STATUS_OK &&
                candidate.state == SYNTRA_REAPER_CANDIDATE_RETIRED);
            check("Scheduler remains valid after each recycle cycle",
                scheduler_validate() == SYNTRA_STATUS_OK);
        }

        check("More than candidate capacity can be reaped without table growth",
            thread_reaper_get_candidate_count(&candidate_count_before) ==
                SYNTRA_STATUS_OK && candidate_count_before == 2U);
        check("Quarantined diagnostic record is never recycled",
            thread_reaper_get_candidate(0U, &candidate) == SYNTRA_STATUS_OK &&
            candidate.state == SYNTRA_REAPER_CANDIDATE_QUARANTINED &&
            candidate.reason ==
                SYNTRA_REAPER_REASON_RESOURCE_RECLAIM_FAILED);
        check("Reaper retirement statistics include all recycle cycles",
            thread_reaper_get_stats(&stats) == SYNTRA_STATUS_OK &&
            stats.retired == 42U);
        check("Scheduler validates after repeated reaper candidate reuse",
            scheduler_validate() == SYNTRA_STATUS_OK);
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
