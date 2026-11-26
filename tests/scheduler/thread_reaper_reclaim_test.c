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
extern syntra_status_t scheduler_test_fail_next_switch_event_ack(void);
extern syntra_status_t scheduler_test_enqueue_switch_away_event(
    syntra_thread_switch_event_t event);
extern syntra_status_t thread_reaper_test_fill_quarantined(void);

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
    int exhaustion_reusable_candidate_index = -1;

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

    {
        syntra_thread_handle_t blocked_handle = {0U, 0U};
        syntra_thread_switch_event_t nonterminal_event;
        syntra_reaper_stats_t stats_before;
        syntra_reaper_stats_t stats_after;
        unsigned int candidate_count_before = 0U;
        unsigned int candidate_count_after = 0U;
        unsigned int nonterminal_processed = 0U;

        check("Create thread for nonterminal event accounting",
            process_thread_create_kernel_owned_with_handle(
                process, 953U, 2U, test_entry, NULL, 4096U,
                &blocked_handle) == SYNTRA_STATUS_OK);
        check("Make nonterminal test thread current",
            scheduler_set_thread_state_by_handle(
                blocked_handle, THREAD_RUNNING) == SYNTRA_STATUS_OK);
        check("Block current thread to stage nonterminal switch",
            scheduler_block_current() == SYNTRA_STATUS_OK);
        check("Switch to reaper for nonterminal event",
            scheduler_set_thread_state_by_handle(
                reaper_handle, THREAD_RUNNING) == SYNTRA_STATUS_OK);
        check("Publish nonterminal switch-away event",
            scheduler_complete_switch_away() == SYNTRA_STATUS_OK);
        check("Nonterminal FIFO head has terminal intent clear",
            scheduler_peek_switch_away_event(&nonterminal_event) ==
                SYNTRA_STATUS_OK &&
            scheduler_thread_handle_equal(
                nonterminal_event.handle, blocked_handle) &&
            nonterminal_event.terminal == 0U);
        thread_reaper_get_candidate_count(&candidate_count_before);
        thread_reaper_get_stats(&stats_before);
        check("Dispatch acknowledges nonterminal event",
            thread_reaper_dispatch(1U, &nonterminal_processed) ==
                SYNTRA_STATUS_OK && nonterminal_processed == 1U);
        thread_reaper_get_candidate_count(&candidate_count_after);
        thread_reaper_get_stats(&stats_after);
        check("Nonterminal event does not consume candidate capacity",
            candidate_count_after == candidate_count_before &&
            find_reaper_candidate_index(blocked_handle) < 0);
        check("Nonterminal event increments discard and ACK counters once",
            stats_after.discarded_nonterminal ==
                stats_before.discarded_nonterminal + 1U &&
            stats_after.acknowledged_events ==
                stats_before.acknowledged_events + 1U);
        check("Nonterminal event leaves terminal queue count unchanged",
            stats_after.queued == stats_before.queued);
        check("Scheduler remains valid after nonterminal event dispatch",
            scheduler_validate() == SYNTRA_STATUS_OK);

        check("Resume nonterminal thread for owned retirement",
            scheduler_set_thread_state_by_handle(
                blocked_handle, THREAD_RUNNING) == SYNTRA_STATUS_OK);
        check("Terminate nonterminal test thread after accounting",
            scheduler_terminate_current() == SYNTRA_STATUS_OK);
        check("Return to reaper for nonterminal test cleanup",
            scheduler_set_thread_state_by_handle(
                reaper_handle, THREAD_RUNNING) == SYNTRA_STATUS_OK);
        check("Publish terminal proof for nonterminal test cleanup",
            scheduler_complete_switch_away() == SYNTRA_STATUS_OK);
        check("Dispatch terminal proof for nonterminal test cleanup",
            thread_reaper_dispatch(1U, &nonterminal_processed) ==
                SYNTRA_STATUS_OK && nonterminal_processed == 1U);
        {
            int cleanup_index = find_reaper_candidate_index(blocked_handle);
            check("Locate cleanup candidate by exact handle",
                cleanup_index >= 0);
            if (cleanup_index >= 0)
            {
                check("Reclaim owned stack after nonterminal test",
                    thread_reaper_reclaim_candidate(
                        (unsigned int)cleanup_index) == SYNTRA_STATUS_OK);
                check("Retire owned TCB after nonterminal test",
                    thread_reaper_retire_candidate(
                        (unsigned int)cleanup_index) == SYNTRA_STATUS_OK);
            }
        }
    }


    check("Create allocator-owned kernel thread",
        process_thread_create_kernel_owned_with_handle(
            process, 951U, 1U, test_entry, NULL, 4096U,
            &owned_handle) == SYNTRA_STATUS_OK);
    check("Capture owned thread snapshot",
        scheduler_get_thread_snapshot_by_handle(
            owned_handle, &owned_before) == SYNTRA_STATUS_OK &&
        owned_before.stack_ownership == SYNTRA_THREAD_STACK_OWNED &&
        owned_before.stack_allocation.generation != 0U);

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
        stats.reclaimed == 2U && stats.reclaim_failures == 0U);
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
            stats.retired == 2U && stats.retirement_failures == 0U);
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
            stats.reclaimed == 2U && stats.retired == 2U &&
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
        exhaustion_reusable_candidate_index =
            find_reaper_candidate_index(exhaustion_handle);
        check("Retired terminal-generation candidate remains identifiable",
            exhaustion_reusable_candidate_index >= 0);
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
        syntra_thread_handle_t rollback_handle = {0U, 0U};
        syntra_thread_switch_event_t expected_event;
        syntra_thread_switch_event_t pending_event;
        syntra_reaper_candidate_t before_candidate;
        unsigned int rollback_processed = 99U;
        unsigned int candidate_count_before = 0U;
        unsigned int candidate_count_after = 0U;
        int rollback_candidate_index =
            exhaustion_reusable_candidate_index;

        check("Locate reusable RETIRED candidate before ack rollback",
            rollback_candidate_index >= 0);
        if (rollback_candidate_index < 0)
            rollback_candidate_index = 0;

        check("Snapshot RETIRED candidate before reuse rollback",
            thread_reaper_get_candidate(
                (unsigned int)rollback_candidate_index,
                &before_candidate) == SYNTRA_STATUS_OK &&
            before_candidate.state == SYNTRA_REAPER_CANDIDATE_RETIRED);
        check("Capture candidate high-water before ack rollback",
            thread_reaper_get_candidate_count(&candidate_count_before) ==
                SYNTRA_STATUS_OK && candidate_count_before == 2U);

        check("Create owned thread for acknowledgement rollback",
            process_thread_create_kernel_owned_with_handle(
                process, 958U, 2U, test_entry, NULL, 4096U,
                &rollback_handle) == SYNTRA_STATUS_OK);
        check("Run thread for acknowledgement rollback",
            scheduler_set_thread_state_by_handle(
                rollback_handle, THREAD_RUNNING) == SYNTRA_STATUS_OK);
        check("Terminate thread for acknowledgement rollback",
            scheduler_terminate_current() == SYNTRA_STATUS_OK);
        check("Switch to reaper before acknowledgement rollback",
            scheduler_set_thread_state_by_handle(
                reaper_handle, THREAD_RUNNING) == SYNTRA_STATUS_OK);
        check("Publish proof for acknowledgement rollback",
            scheduler_complete_switch_away() == SYNTRA_STATUS_OK);
        check("Capture exact FIFO event before injected failure",
            scheduler_peek_switch_away_event(&expected_event) ==
                SYNTRA_STATUS_OK &&
            scheduler_thread_handle_equal(
                expected_event.handle, rollback_handle) &&
            expected_event.terminal != 0U);

        check("Arm one-shot host acknowledgement failure",
            scheduler_test_fail_next_switch_event_ack() ==
                SYNTRA_STATUS_OK);
        check("Failed FIFO acknowledgement returns error without processing",
            thread_reaper_dispatch(1U, &rollback_processed) ==
                SYNTRA_STATUS_ERROR && rollback_processed == 0U);
        check("Reused candidate is restored exactly after ack failure",
            thread_reaper_get_candidate(
                (unsigned int)rollback_candidate_index, &candidate) ==
                    SYNTRA_STATUS_OK &&
            candidate.state == before_candidate.state &&
            candidate.reason == before_candidate.reason &&
            scheduler_thread_handle_equal(
                candidate.proof.handle, before_candidate.proof.handle) &&
            candidate.proof.sequence == before_candidate.proof.sequence &&
            candidate.proof.terminal == before_candidate.proof.terminal);
        check("Candidate high-water is unchanged after ack rollback",
            thread_reaper_get_candidate_count(&candidate_count_after) ==
                SYNTRA_STATUS_OK &&
            candidate_count_after == candidate_count_before);
        check("FIFO head remains the exact unacknowledged event",
            scheduler_peek_switch_away_event(&pending_event) ==
                SYNTRA_STATUS_OK &&
            scheduler_thread_handle_equal(
                pending_event.handle, expected_event.handle) &&
            pending_event.sequence == expected_event.sequence &&
            pending_event.terminal == expected_event.terminal);

        rollback_processed = 0U;
        check("Retry dispatch after one-shot acknowledgement failure",
            thread_reaper_dispatch(1U, &rollback_processed) ==
                SYNTRA_STATUS_OK && rollback_processed == 1U);
        check("Retry reuses the same candidate for the pending event",
            thread_reaper_get_candidate(
                (unsigned int)rollback_candidate_index, &candidate) ==
                    SYNTRA_STATUS_OK &&
            candidate.state == SYNTRA_REAPER_CANDIDATE_QUEUED &&
            scheduler_thread_handle_equal(
                candidate.proof.handle, rollback_handle) &&
            candidate.proof.sequence == expected_event.sequence);
        check("Reclaim candidate after successful retry",
            thread_reaper_reclaim_candidate(
                (unsigned int)rollback_candidate_index) == SYNTRA_STATUS_OK);
        check("Retire candidate after successful retry",
            thread_reaper_retire_candidate(
                (unsigned int)rollback_candidate_index) == SYNTRA_STATUS_OK);
        exhaustion_reusable_candidate_index =
            rollback_candidate_index;
        check("Scheduler remains valid after acknowledgement rollback test",
            scheduler_validate() == SYNTRA_STATUS_OK);
    }

    {
        const unsigned int recycle_cycles = 40U;
        unsigned int candidate_count_before = 0U;
        unsigned int reusable_scheduler_slot = UINT32_MAX;
        syntra_reaper_candidate_t quarantined_record_before;
        syntra_reaper_candidate_t quarantined_record_after;
        int reusable_candidate_index =
            exhaustion_reusable_candidate_index;

        check("Locate retired candidate record for reuse",
            reusable_candidate_index >= 0);
        if (reusable_candidate_index < 0)
            reusable_candidate_index = 0;
        check("Capture bounded reaper table count before recycle stress",
            thread_reaper_get_candidate_count(&candidate_count_before) ==
                SYNTRA_STATUS_OK && candidate_count_before == 2U);
        check("Retired reuse candidate is distinct from quarantined diagnostics",
            reusable_candidate_index != 0);
        check("Capture quarantined diagnostic proof before recycle stress",
            thread_reaper_get_candidate(0U, &quarantined_record_before) ==
                SYNTRA_STATUS_OK &&
            quarantined_record_before.state ==
                SYNTRA_REAPER_CANDIDATE_QUARANTINED &&
            quarantined_record_before.reason ==
                SYNTRA_REAPER_REASON_RESOURCE_RECLAIM_FAILED &&
            quarantined_record_before.proof.handle.generation != 0U &&
            quarantined_record_before.proof.sequence != 0U &&
            quarantined_record_before.proof.terminal != 0U);

        for (unsigned int cycle = 0U; cycle < recycle_cycles; ++cycle)
        {
            syntra_thread_handle_t cycle_handle = {0U, 0U};
            unsigned int cycle_processed = 0U;
            syntra_status_t cycle_status;
            syntra_reaper_stats_t cycle_stats_before;
            syntra_reaper_stats_t cycle_stats_after_dispatch;
            syntra_reaper_stats_t cycle_stats_after_reclaim;
            syntra_reaper_stats_t cycle_stats_after_retire;

            check("Capture reaper accounting before recycle cycle",
                thread_reaper_get_stats(&cycle_stats_before) ==
                    SYNTRA_STATUS_OK &&
                cycle_stats_before.queued == 0U &&
                cycle_stats_before.quarantined == 1U);

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

            check("Dispatch advances queue, ACK, and context counters exactly once",
                thread_reaper_get_stats(&cycle_stats_after_dispatch) ==
                    SYNTRA_STATUS_OK &&
                cycle_stats_after_dispatch.queued ==
                    cycle_stats_before.queued + 1U &&
                cycle_stats_after_dispatch.acknowledged_events ==
                    cycle_stats_before.acknowledged_events + 1U &&
                cycle_stats_after_dispatch.execution_context_dispatches ==
                    cycle_stats_before.execution_context_dispatches + 1U &&
                cycle_stats_after_dispatch.quarantined ==
                    cycle_stats_before.quarantined &&
                cycle_stats_after_dispatch.reclaimed ==
                    cycle_stats_before.reclaimed &&
                cycle_stats_after_dispatch.retired ==
                    cycle_stats_before.retired &&
                cycle_stats_after_dispatch.reclaim_failures ==
                    cycle_stats_before.reclaim_failures &&
                cycle_stats_after_dispatch.retirement_failures ==
                    cycle_stats_before.retirement_failures &&
                cycle_stats_after_dispatch.discarded_nonterminal ==
                    cycle_stats_before.discarded_nonterminal &&
                cycle_stats_after_dispatch.duplicate_events ==
                    cycle_stats_before.duplicate_events);

            check("Dispatch never overwrites quarantined diagnostic proof",
                thread_reaper_get_candidate(
                    0U, &quarantined_record_after) == SYNTRA_STATUS_OK &&
                quarantined_record_after.state ==
                    quarantined_record_before.state &&
                quarantined_record_after.reason ==
                    quarantined_record_before.reason &&
                scheduler_thread_handle_equal(
                    quarantined_record_after.proof.handle,
                    quarantined_record_before.proof.handle) &&
                quarantined_record_after.proof.sequence ==
                    quarantined_record_before.proof.sequence &&
                quarantined_record_after.proof.terminal ==
                    quarantined_record_before.proof.terminal);

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

            check("Reclaim moves queued accounting to reclaimed exactly once",
                thread_reaper_get_stats(&cycle_stats_after_reclaim) ==
                    SYNTRA_STATUS_OK &&
                cycle_stats_after_reclaim.queued ==
                    cycle_stats_before.queued &&
                cycle_stats_after_reclaim.reclaimed ==
                    cycle_stats_before.reclaimed + 1U &&
                cycle_stats_after_reclaim.acknowledged_events ==
                    cycle_stats_after_dispatch.acknowledged_events &&
                cycle_stats_after_reclaim.quarantined ==
                    cycle_stats_before.quarantined &&
                cycle_stats_after_reclaim.retired ==
                    cycle_stats_before.retired);

            cycle_status = thread_reaper_retire_candidate(
                (unsigned int)reusable_candidate_index);
            check("Retire recycle-stress TCB",
                cycle_status == SYNTRA_STATUS_OK);
            if (cycle_status != SYNTRA_STATUS_OK)
                break;

            check("Retirement increments cumulative count exactly once",
                thread_reaper_get_stats(&cycle_stats_after_retire) ==
                    SYNTRA_STATUS_OK &&
                cycle_stats_after_retire.queued ==
                    cycle_stats_before.queued &&
                cycle_stats_after_retire.reclaimed ==
                    cycle_stats_before.reclaimed + 1U &&
                cycle_stats_after_retire.retired ==
                    cycle_stats_before.retired + 1U &&
                cycle_stats_after_retire.acknowledged_events ==
                    cycle_stats_after_dispatch.acknowledged_events &&
                cycle_stats_after_retire.quarantined ==
                    cycle_stats_before.quarantined &&
                cycle_stats_after_retire.reclaim_failures ==
                    cycle_stats_before.reclaim_failures &&
                cycle_stats_after_retire.retirement_failures ==
                    cycle_stats_before.retirement_failures);

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
        check("Quarantined diagnostic record retains exact proof after all recycle cycles",
            thread_reaper_get_candidate(0U, &quarantined_record_after) ==
                SYNTRA_STATUS_OK &&
            quarantined_record_after.state ==
                quarantined_record_before.state &&
            quarantined_record_after.reason ==
                quarantined_record_before.reason &&
            scheduler_thread_handle_equal(
                quarantined_record_after.proof.handle,
                quarantined_record_before.proof.handle) &&
            quarantined_record_after.proof.sequence ==
                quarantined_record_before.proof.sequence &&
            quarantined_record_after.proof.terminal ==
                quarantined_record_before.proof.terminal);
        check("Reaper retirement statistics include all recycle cycles",
            thread_reaper_get_stats(&stats) == SYNTRA_STATUS_OK &&
            stats.retired == 44U);
        check("Scheduler validates after repeated reaper candidate reuse",
            scheduler_validate() == SYNTRA_STATUS_OK);
    }


    {
        syntra_thread_handle_t first_handle = {0U, 0U};
        syntra_thread_handle_t second_handle = {0U, 0U};
        syntra_thread_switch_event_t first_event;
        syntra_thread_switch_event_t pending_event;
        syntra_reaper_candidate_t reusable_before_failure;
        syntra_reaper_candidate_t reusable_after_failure;
        syntra_reaper_candidate_t candidate_after_retry;
        syntra_reaper_stats_t sequence_before;
        syntra_reaper_stats_t after_first_dispatch;
        syntra_reaper_stats_t after_first_retire;
        syntra_reaper_stats_t after_second_ack_failure;
        syntra_reaper_stats_t after_second_retry;
        unsigned int sequence_processed = 0U;
        unsigned int candidate_count_before = 0U;
        unsigned int candidate_count_after = 0U;
        int reusable_index = exhaustion_reusable_candidate_index;

        if (reusable_index < 0)
            reusable_index = 1;

        check("Capture clean baseline before ordered multi-event test",
            thread_reaper_get_stats(&sequence_before) == SYNTRA_STATUS_OK &&
            sequence_before.queued == 0U &&
            sequence_before.quarantined == 1U);
        check("Capture candidate high-water before ordered multi-event test",
            thread_reaper_get_candidate_count(&candidate_count_before) ==
                SYNTRA_STATUS_OK && candidate_count_before == 2U);

        check("Create first thread in ordered FIFO sequence",
            process_thread_create_kernel_owned_with_handle(
                process, 3101U, 2U, test_entry, NULL, 4096U,
                &first_handle) == SYNTRA_STATUS_OK);
        check("Run first ordered FIFO thread",
            scheduler_set_thread_state_by_handle(
                first_handle, THREAD_RUNNING) == SYNTRA_STATUS_OK);
        check("Terminate first ordered FIFO thread",
            scheduler_terminate_current() == SYNTRA_STATUS_OK);
        check("Switch back to reaper after first ordered thread",
            scheduler_set_thread_state_by_handle(
                reaper_handle, THREAD_RUNNING) == SYNTRA_STATUS_OK);
        check("Publish first terminal FIFO event",
            scheduler_complete_switch_away() == SYNTRA_STATUS_OK);

        check("Create second thread behind first FIFO event",
            process_thread_create_kernel_owned_with_handle(
                process, 3102U, 2U, test_entry, NULL, 4096U,
                &second_handle) == SYNTRA_STATUS_OK);
        check("Run second ordered FIFO thread",
            scheduler_set_thread_state_by_handle(
                second_handle, THREAD_RUNNING) == SYNTRA_STATUS_OK);
        check("Terminate second ordered FIFO thread",
            scheduler_terminate_current() == SYNTRA_STATUS_OK);
        check("Switch back to reaper after second ordered thread",
            scheduler_set_thread_state_by_handle(
                reaper_handle, THREAD_RUNNING) == SYNTRA_STATUS_OK);
        check("Publish second terminal FIFO event",
            scheduler_complete_switch_away() == SYNTRA_STATUS_OK);

        check("FIFO preserves first event ordering",
            scheduler_peek_switch_away_event(&first_event) ==
                SYNTRA_STATUS_OK &&
            scheduler_thread_handle_equal(first_event.handle, first_handle) &&
            first_event.terminal != 0U);
        check("Dispatch exactly the first event from a two-event FIFO",
            thread_reaper_dispatch(1U, &sequence_processed) ==
                SYNTRA_STATUS_OK && sequence_processed == 1U);
        check("First event is acknowledged and queued once",
            thread_reaper_get_stats(&after_first_dispatch) ==
                SYNTRA_STATUS_OK &&
            after_first_dispatch.queued == sequence_before.queued + 1U &&
            after_first_dispatch.acknowledged_events ==
                sequence_before.acknowledged_events + 1U &&
            after_first_dispatch.execution_context_dispatches ==
                sequence_before.execution_context_dispatches + 1U);
        check("Second event remains FIFO head after first dispatch",
            scheduler_peek_switch_away_event(&pending_event) ==
                SYNTRA_STATUS_OK &&
            scheduler_thread_handle_equal(
                pending_event.handle, second_handle) &&
            pending_event.terminal != 0U);

        {
            int first_index = find_reaper_candidate_index(first_handle);
            check("Locate first queued event candidate", first_index >= 0);
            if (first_index >= 0)
            {
                check("Reclaim first event before processing next FIFO head",
                    thread_reaper_reclaim_candidate(
                        (unsigned int)first_index) == SYNTRA_STATUS_OK);
                check("Retire first event before processing next FIFO head",
                    thread_reaper_retire_candidate(
                        (unsigned int)first_index) == SYNTRA_STATUS_OK);
            }
        }
        check("Capture counters after first event retirement",
            thread_reaper_get_stats(&after_first_retire) ==
                SYNTRA_STATUS_OK &&
            after_first_retire.queued == sequence_before.queued &&
            after_first_retire.reclaimed == sequence_before.reclaimed + 1U &&
            after_first_retire.retired == sequence_before.retired + 1U &&
            after_first_retire.acknowledged_events ==
                after_first_dispatch.acknowledged_events);

        check("Snapshot reusable candidate before second-event ACK failure",
            thread_reaper_get_candidate(
                (unsigned int)reusable_index, &reusable_before_failure) ==
                SYNTRA_STATUS_OK &&
            reusable_before_failure.state == SYNTRA_REAPER_CANDIDATE_RETIRED);
        check("Arm one-shot ACK failure for second FIFO event",
            scheduler_test_fail_next_switch_event_ack() == SYNTRA_STATUS_OK);
        sequence_processed = 99U;
        check("Second event ACK failure reports zero newly processed events",
            thread_reaper_dispatch(1U, &sequence_processed) ==
                SYNTRA_STATUS_ERROR && sequence_processed == 0U);
        check("Failed second ACK restores retired candidate exactly",
            thread_reaper_get_candidate(
                (unsigned int)reusable_index, &reusable_after_failure) ==
                SYNTRA_STATUS_OK &&
            reusable_after_failure.state == reusable_before_failure.state &&
            reusable_after_failure.reason == reusable_before_failure.reason &&
            scheduler_thread_handle_equal(
                reusable_after_failure.proof.handle,
                reusable_before_failure.proof.handle) &&
            reusable_after_failure.proof.sequence ==
                reusable_before_failure.proof.sequence &&
            reusable_after_failure.proof.terminal ==
                reusable_before_failure.proof.terminal);
        check("Failed second ACK preserves second FIFO head",
            scheduler_peek_switch_away_event(&pending_event) ==
                SYNTRA_STATUS_OK &&
            scheduler_thread_handle_equal(
                pending_event.handle, second_handle) &&
            pending_event.sequence != 0U &&
            pending_event.terminal != 0U);
        check("Failed second ACK changes dispatch count but not queue or ACK",
            thread_reaper_get_stats(&after_second_ack_failure) ==
                SYNTRA_STATUS_OK &&
            after_second_ack_failure.queued == after_first_retire.queued &&
            after_second_ack_failure.acknowledged_events ==
                after_first_retire.acknowledged_events &&
            after_second_ack_failure.execution_context_dispatches ==
                after_first_retire.execution_context_dispatches + 1U &&
            after_second_ack_failure.reclaimed ==
                after_first_retire.reclaimed &&
            after_second_ack_failure.retired == after_first_retire.retired &&
            after_second_ack_failure.quarantined ==
                after_first_retire.quarantined);
        check("Failed second ACK does not grow candidate table",
            thread_reaper_get_candidate_count(&candidate_count_after) ==
                SYNTRA_STATUS_OK &&
            candidate_count_after == candidate_count_before);

        sequence_processed = 0U;
        check("Retry second FIFO event after one-shot ACK failure",
            thread_reaper_dispatch(1U, &sequence_processed) ==
                SYNTRA_STATUS_OK && sequence_processed == 1U);
        check("Retry acknowledges and queues second event exactly once",
            thread_reaper_get_stats(&after_second_retry) ==
                SYNTRA_STATUS_OK &&
            after_second_retry.queued == after_second_ack_failure.queued + 1U &&
            after_second_retry.acknowledged_events ==
                after_second_ack_failure.acknowledged_events + 1U &&
            after_second_retry.execution_context_dispatches ==
                after_second_ack_failure.execution_context_dispatches + 1U);
        check("Retry reuses candidate for exact second FIFO event",
            thread_reaper_get_candidate(
                (unsigned int)reusable_index, &candidate_after_retry) ==
                SYNTRA_STATUS_OK &&
            candidate_after_retry.state == SYNTRA_REAPER_CANDIDATE_QUEUED &&
            scheduler_thread_handle_equal(
                candidate_after_retry.proof.handle, second_handle) &&
            candidate_after_retry.proof.sequence == pending_event.sequence);
        check("Reclaim second FIFO event after successful retry",
            thread_reaper_reclaim_candidate(
                (unsigned int)reusable_index) == SYNTRA_STATUS_OK);
        check("Retire second FIFO event after successful retry",
            thread_reaper_retire_candidate(
                (unsigned int)reusable_index) == SYNTRA_STATUS_OK);
        check("Ordered multi-event sequence ends with balanced lifecycle counters",
            thread_reaper_get_stats(&stats) == SYNTRA_STATUS_OK &&
            stats.queued == sequence_before.queued &&
            stats.quarantined == sequence_before.quarantined &&
            stats.acknowledged_events ==
                sequence_before.acknowledged_events + 2U &&
            stats.reclaimed == sequence_before.reclaimed + 2U &&
            stats.retired == sequence_before.retired + 2U &&
            stats.reclaim_failures == sequence_before.reclaim_failures &&
            stats.retirement_failures == sequence_before.retirement_failures);
        check("Candidate table high-water remains stable after ordered FIFO sequence",
            thread_reaper_get_candidate_count(&candidate_count_after) ==
                SYNTRA_STATUS_OK &&
            candidate_count_after == candidate_count_before);
        check("Scheduler remains valid after ordered multi-event ACK boundary",
            scheduler_validate() == SYNTRA_STATUS_OK);
    }


    {
        syntra_thread_handle_t first_handle = {0U, 0U};
        syntra_thread_handle_t final_handle = {0U, 0U};
        syntra_thread_switch_event_t first_proof = {0};
        syntra_thread_switch_event_t duplicate_event;
        syntra_thread_switch_event_t conflicting_event;
        syntra_thread_switch_event_t nonterminal_event;
        syntra_thread_switch_event_t final_event;
        syntra_reaper_candidate_t first_candidate = {0};
        syntra_reaper_candidate_t first_after_conflict = {0};
        syntra_reaper_stats_t mixed_before;
        syntra_reaper_stats_t after_first;
        syntra_reaper_stats_t after_duplicate;
        syntra_reaper_stats_t after_conflict;
        syntra_reaper_stats_t after_nonterminal;
        syntra_reaper_stats_t after_final;
        unsigned int mixed_processed = 0U;
        unsigned int candidate_count_before = 0U;
        unsigned int candidate_count_after = 0U;
        int first_index = -1;
        int final_index = -1;

        check("Capture baseline before mixed FIFO event sequence",
            thread_reaper_get_stats(&mixed_before) == SYNTRA_STATUS_OK &&
            mixed_before.queued == 0U);
        check("Capture candidate high-water before mixed FIFO sequence",
            thread_reaper_get_candidate_count(&candidate_count_before) ==
                SYNTRA_STATUS_OK);

        check("Create first owned thread for mixed FIFO sequence",
            process_thread_create_kernel_owned_with_handle(
                process, 3201U, 2U, test_entry, NULL, 4096U,
                &first_handle) == SYNTRA_STATUS_OK);
        check("Run first owned thread for mixed FIFO sequence",
            scheduler_set_thread_state_by_handle(
                first_handle, THREAD_RUNNING) == SYNTRA_STATUS_OK);
        check("Terminate first owned thread for mixed FIFO sequence",
            scheduler_terminate_current() == SYNTRA_STATUS_OK);
        check("Return to reaper before first mixed-sequence proof",
            scheduler_set_thread_state_by_handle(
                reaper_handle, THREAD_RUNNING) == SYNTRA_STATUS_OK);
        check("Publish first terminal proof for mixed FIFO sequence",
            scheduler_complete_switch_away() == SYNTRA_STATUS_OK);
        check("Dispatch first terminal proof before injecting replay events",
            thread_reaper_dispatch(1U, &mixed_processed) ==
                SYNTRA_STATUS_OK && mixed_processed == 1U);
        first_index = find_reaper_candidate_index(first_handle);
        check("Locate queued candidate for mixed FIFO replay sequence",
            first_index >= 0);
        check("Capture original proof for mixed FIFO replay sequence",
            first_index >= 0 &&
            thread_reaper_get_candidate(
                (unsigned int)first_index, &first_candidate) ==
                SYNTRA_STATUS_OK &&
            first_candidate.state == SYNTRA_REAPER_CANDIDATE_QUEUED);
        first_proof = first_candidate.proof;
        check("First proof dispatch increments queue and ACK exactly once",
            thread_reaper_get_stats(&after_first) == SYNTRA_STATUS_OK &&
            after_first.queued == mixed_before.queued + 1U &&
            after_first.acknowledged_events ==
                mixed_before.acknowledged_events + 1U);

        duplicate_event = first_proof;
        conflicting_event = first_proof;
        conflicting_event.sequence = first_proof.sequence + 1U;
        if (conflicting_event.sequence == 0U)
            conflicting_event.sequence = 1U;
        nonterminal_event.handle = reaper_handle;
        nonterminal_event.sequence = first_proof.sequence + 2U;
        if (nonterminal_event.sequence == 0U)
            nonterminal_event.sequence = 1U;
        nonterminal_event.terminal = 0U;

        check("Append duplicate proof after original candidate is queued",
            scheduler_test_enqueue_switch_away_event(duplicate_event) ==
                SYNTRA_STATUS_OK);
        check("Append conflicting proof after duplicate FIFO event",
            scheduler_test_enqueue_switch_away_event(conflicting_event) ==
                SYNTRA_STATUS_OK);
        check("Append nonterminal event after conflicting FIFO event",
            scheduler_test_enqueue_switch_away_event(nonterminal_event) ==
                SYNTRA_STATUS_OK);

        check("Create final owned thread behind injected FIFO events",
            process_thread_create_kernel_owned_with_handle(
                process, 3202U, 2U, test_entry, NULL, 4096U,
                &final_handle) == SYNTRA_STATUS_OK);
        check("Run final owned thread behind injected FIFO events",
            scheduler_set_thread_state_by_handle(
                final_handle, THREAD_RUNNING) == SYNTRA_STATUS_OK);
        check("Terminate final owned thread behind injected FIFO events",
            scheduler_terminate_current() == SYNTRA_STATUS_OK);
        check("Return to reaper before final mixed-sequence proof",
            scheduler_set_thread_state_by_handle(
                reaper_handle, THREAD_RUNNING) == SYNTRA_STATUS_OK);
        check("Publish final terminal proof behind mixed FIFO events",
            scheduler_complete_switch_away() == SYNTRA_STATUS_OK);

        check("Mixed FIFO begins with exact duplicate proof",
            scheduler_peek_switch_away_event(&final_event) ==
                SYNTRA_STATUS_OK &&
            scheduler_thread_handle_equal(final_event.handle, first_proof.handle) &&
            final_event.sequence == first_proof.sequence &&
            final_event.terminal == first_proof.terminal);
        mixed_processed = 0U;
        check("Dispatch duplicate proof without consuming later FIFO events",
            thread_reaper_dispatch(1U, &mixed_processed) ==
                SYNTRA_STATUS_OK && mixed_processed == 1U);
        check("Duplicate proof preserves queued candidate and increments duplicate accounting",
            thread_reaper_get_stats(&after_duplicate) == SYNTRA_STATUS_OK &&
            after_duplicate.queued == after_first.queued &&
            after_duplicate.quarantined == after_first.quarantined &&
            after_duplicate.duplicate_events ==
                after_first.duplicate_events + 1U &&
            after_duplicate.acknowledged_events ==
                after_first.acknowledged_events + 1U);
        check("Conflicting proof remains the next FIFO head",
            scheduler_peek_switch_away_event(&final_event) ==
                SYNTRA_STATUS_OK &&
            scheduler_thread_handle_equal(final_event.handle, first_proof.handle) &&
            final_event.sequence == conflicting_event.sequence &&
            final_event.terminal == 1U);

        mixed_processed = 0U;
        check("Dispatch conflicting proof after duplicate proof",
            thread_reaper_dispatch(1U, &mixed_processed) ==
                SYNTRA_STATUS_OK && mixed_processed == 1U);
        check("Conflicting proof quarantines original candidate without replacing diagnostics",
            first_index >= 0 &&
            thread_reaper_get_candidate(
                (unsigned int)first_index, &first_after_conflict) ==
                SYNTRA_STATUS_OK &&
            first_after_conflict.state ==
                SYNTRA_REAPER_CANDIDATE_QUARANTINED &&
            first_after_conflict.reason ==
                SYNTRA_REAPER_REASON_SEQUENCE_CONFLICT &&
            scheduler_thread_handle_equal(
                first_after_conflict.proof.handle, first_proof.handle) &&
            first_after_conflict.proof.sequence == first_proof.sequence &&
            first_after_conflict.proof.terminal == first_proof.terminal);
        check("Conflict transition updates queue quarantine and ACK counters once",
            thread_reaper_get_stats(&after_conflict) == SYNTRA_STATUS_OK &&
            after_conflict.queued + 1U == after_duplicate.queued &&
            after_conflict.quarantined ==
                after_duplicate.quarantined + 1U &&
            after_conflict.acknowledged_events ==
                after_duplicate.acknowledged_events + 1U);
        check("Nonterminal event remains FIFO head after conflict handling",
            scheduler_peek_switch_away_event(&final_event) ==
                SYNTRA_STATUS_OK &&
            scheduler_thread_handle_equal(
                final_event.handle, nonterminal_event.handle) &&
            final_event.sequence == nonterminal_event.sequence &&
            final_event.terminal == 0U);

        mixed_processed = 0U;
        check("Dispatch nonterminal event after conflict without candidate insertion",
            thread_reaper_dispatch(1U, &mixed_processed) ==
                SYNTRA_STATUS_OK && mixed_processed == 1U);
        check("Nonterminal event updates only discard and ACK accounting",
            thread_reaper_get_stats(&after_nonterminal) == SYNTRA_STATUS_OK &&
            after_nonterminal.queued == after_conflict.queued &&
            after_nonterminal.quarantined == after_conflict.quarantined &&
            after_nonterminal.discarded_nonterminal ==
                after_conflict.discarded_nonterminal + 1U &&
            after_nonterminal.acknowledged_events ==
                after_conflict.acknowledged_events + 1U);
        check("Final terminal proof remains FIFO head after nonterminal event",
            scheduler_peek_switch_away_event(&final_event) ==
                SYNTRA_STATUS_OK &&
            scheduler_thread_handle_equal(final_event.handle, final_handle) &&
            final_event.terminal == 1U);

        mixed_processed = 0U;
        check("Dispatch final terminal proof after mixed replay and discard events",
            thread_reaper_dispatch(1U, &mixed_processed) ==
                SYNTRA_STATUS_OK && mixed_processed == 1U);
        final_index = find_reaper_candidate_index(final_handle);
        check("Final terminal proof becomes a separate queued candidate",
            final_index >= 0 &&
            thread_reaper_get_candidate(
                (unsigned int)final_index, &first_candidate) ==
                SYNTRA_STATUS_OK &&
            first_candidate.state == SYNTRA_REAPER_CANDIDATE_QUEUED &&
            scheduler_thread_handle_equal(
                first_candidate.proof.handle, final_handle));
        check("Candidate high-water grows once after first candidate is quarantined",
            thread_reaper_get_candidate_count(&candidate_count_after) ==
                SYNTRA_STATUS_OK &&
            candidate_count_after == candidate_count_before + 1U);
        check("Final terminal proof ACK is counted exactly once",
            thread_reaper_get_stats(&after_final) == SYNTRA_STATUS_OK &&
            after_final.queued == mixed_before.queued + 1U &&
            after_final.quarantined == mixed_before.quarantined + 1U &&
            after_final.acknowledged_events ==
                mixed_before.acknowledged_events + 5U &&
            after_final.duplicate_events ==
                mixed_before.duplicate_events + 1U &&
            after_final.discarded_nonterminal ==
                mixed_before.discarded_nonterminal + 1U &&
            after_final.execution_context_dispatches ==
                mixed_before.execution_context_dispatches + 5U);

        check("Reclaim final candidate after mixed FIFO sequence",
            final_index >= 0 &&
            thread_reaper_reclaim_candidate(
                (unsigned int)final_index) == SYNTRA_STATUS_OK);
        check("Retire final candidate after mixed FIFO sequence",
            final_index >= 0 &&
            thread_reaper_retire_candidate(
                (unsigned int)final_index) == SYNTRA_STATUS_OK);
        check("Mixed FIFO lifecycle counters balance while quarantine remains retained",
            thread_reaper_get_stats(&stats) == SYNTRA_STATUS_OK &&
            stats.queued == mixed_before.queued &&
            stats.quarantined == mixed_before.quarantined + 1U &&
            stats.reclaimed == mixed_before.reclaimed + 1U &&
            stats.retired == mixed_before.retired + 1U &&
            stats.reclaim_failures == mixed_before.reclaim_failures &&
            stats.retirement_failures == mixed_before.retirement_failures);
        check("Mixed FIFO event sequence leaves scheduler valid",
            scheduler_validate() == SYNTRA_STATUS_OK);
    }

    {
        syntra_thread_handle_t quarantined_handle = {0U, 0U};
        syntra_thread_switch_event_t original_proof;
        syntra_thread_switch_event_t conflicting_proof;
        syntra_reaper_candidate_t quarantined_before;
        syntra_reaper_candidate_t quarantined_after;
        syntra_thread_t quarantined_snapshot;
        syntra_memory_allocation_t quarantine_allocation;
        syntra_memory_info_t quarantine_memory_before;
        syntra_memory_info_t quarantine_memory_after;
        syntra_reaper_stats_t quarantine_stats_before;
        syntra_reaper_stats_t quarantine_stats_after;
        unsigned int quarantine_processed = 0U;
        int quarantined_index = -1;
        syntra_status_t operation_status;

        check("Create owned thread for quarantined-operation guard test",
            process_thread_create_kernel_owned_with_handle(
                process, 3000U, 2U, test_entry, NULL, 4096U,
                &quarantined_handle) == SYNTRA_STATUS_OK);
        check("Run thread for quarantined-operation guard test",
            scheduler_set_thread_state_by_handle(
                quarantined_handle, THREAD_RUNNING) == SYNTRA_STATUS_OK);
        check("Terminate thread for quarantined-operation guard test",
            scheduler_terminate_current() == SYNTRA_STATUS_OK);
        check("Switch to bound reaper before quarantine guard test",
            scheduler_set_thread_state_by_handle(
                reaper_handle, THREAD_RUNNING) == SYNTRA_STATUS_OK);
        check("Publish terminal proof for quarantine guard test",
            scheduler_complete_switch_away() == SYNTRA_STATUS_OK);
        check("Dispatch terminal proof into reusable candidate",
            thread_reaper_dispatch(1U, &quarantine_processed) ==
                SYNTRA_STATUS_OK && quarantine_processed == 1U);

        quarantined_index = find_reaper_candidate_index(quarantined_handle);
        check("Locate candidate by exact quarantined-test handle",
            quarantined_index >= 0);
        if (quarantined_index < 0)
            quarantined_index = 1;

        check("Capture original QUEUED proof before sequence conflict",
            thread_reaper_get_candidate(
                (unsigned int)quarantined_index, &quarantined_before) ==
                SYNTRA_STATUS_OK &&
            quarantined_before.state == SYNTRA_REAPER_CANDIDATE_QUEUED);
        original_proof = quarantined_before.proof;
        conflicting_proof = original_proof;
        ++conflicting_proof.sequence;
        check("Queue conflicting terminal proof for same handle",
            scheduler_test_enqueue_switch_away_event(conflicting_proof) ==
                SYNTRA_STATUS_OK);
        quarantine_processed = 0U;
        check("Dispatch conflict and quarantine candidate",
            thread_reaper_dispatch(1U, &quarantine_processed) ==
                SYNTRA_STATUS_OK && quarantine_processed == 1U);
        check("Candidate is quarantined with original proof retained",
            thread_reaper_get_candidate(
                (unsigned int)quarantined_index, &quarantined_before) ==
                SYNTRA_STATUS_OK &&
            quarantined_before.state == SYNTRA_REAPER_CANDIDATE_QUARANTINED &&
            quarantined_before.reason ==
                SYNTRA_REAPER_REASON_SEQUENCE_CONFLICT &&
            scheduler_thread_handle_equal(
                quarantined_before.proof.handle, original_proof.handle) &&
            quarantined_before.proof.sequence == original_proof.sequence &&
            quarantined_before.proof.terminal == original_proof.terminal);

        memory_get_info(&quarantine_memory_before);
        check("Capture TCB and allocator state before guarded operations",
            scheduler_get_thread_snapshot_by_handle(
                quarantined_handle, &quarantined_snapshot) ==
                SYNTRA_STATUS_OK &&
            quarantined_snapshot.state == THREAD_TERMINATED &&
            quarantined_snapshot.stack_ownership ==
                SYNTRA_THREAD_STACK_OWNED &&
            memory_get_allocation_by_handle(
                quarantined_snapshot.stack_allocation, &quarantine_allocation) ==
                SYNTRA_STATUS_OK &&
            quarantine_allocation.ownership_state ==
                SYNTRA_MEMORY_ALLOCATION_THREAD_OWNED);
        check("Capture reaper counters before guarded operations",
            thread_reaper_get_stats(&quarantine_stats_before) ==
                SYNTRA_STATUS_OK);

        operation_status = thread_reaper_reclaim_candidate(
            (unsigned int)quarantined_index);
        check("QUARANTINED candidate cannot reclaim its owned stack",
            operation_status == SYNTRA_STATUS_PERMISSION_DENIED);
        if (operation_status == SYNTRA_STATUS_PERMISSION_DENIED)
            operation_status = thread_reaper_retire_candidate(
                (unsigned int)quarantined_index);
        else
            operation_status = SYNTRA_STATUS_ERROR;
        check("QUARANTINED candidate cannot retire its TCB",
            operation_status == SYNTRA_STATUS_PERMISSION_DENIED);

        check("Rejected operations preserve candidate proof and quarantine reason",
            thread_reaper_get_candidate(
                (unsigned int)quarantined_index, &quarantined_after) ==
                SYNTRA_STATUS_OK &&
            quarantined_after.state == SYNTRA_REAPER_CANDIDATE_QUARANTINED &&
            quarantined_after.reason ==
                SYNTRA_REAPER_REASON_SEQUENCE_CONFLICT &&
            scheduler_thread_handle_equal(
                quarantined_after.proof.handle, original_proof.handle) &&
            quarantined_after.proof.sequence == original_proof.sequence &&
            quarantined_after.proof.terminal == original_proof.terminal);
        check("Rejected operations preserve terminated TCB and owned allocation",
            scheduler_get_thread_snapshot_by_handle(
                quarantined_handle, &quarantined_snapshot) ==
                SYNTRA_STATUS_OK &&
            quarantined_snapshot.state == THREAD_TERMINATED &&
            quarantined_snapshot.stack_ownership ==
                SYNTRA_THREAD_STACK_OWNED &&
            memory_get_allocation_by_handle(
                quarantined_snapshot.stack_allocation, &quarantine_allocation) ==
                SYNTRA_STATUS_OK &&
            quarantine_allocation.ownership_state ==
                SYNTRA_MEMORY_ALLOCATION_THREAD_OWNED);
        memory_get_info(&quarantine_memory_after);
        check("Rejected operations do not change active allocations",
            quarantine_memory_after.active_allocations ==
                quarantine_memory_before.active_allocations);
        check("Rejected operations do not change reclaim or retirement counters",
            thread_reaper_get_stats(&quarantine_stats_after) ==
                SYNTRA_STATUS_OK &&
            quarantine_stats_after.reclaimed ==
                quarantine_stats_before.reclaimed &&
            quarantine_stats_after.retired ==
                quarantine_stats_before.retired &&
            quarantine_stats_after.reclaim_failures ==
                quarantine_stats_before.reclaim_failures &&
            quarantine_stats_after.retirement_failures ==
                quarantine_stats_before.retirement_failures);
        check("Scheduler and allocator remain valid after guarded operations",
            scheduler_validate() == SYNTRA_STATUS_OK &&
            memory_validate() == SYNTRA_STATUS_OK);
    }


    {
        syntra_thread_handle_t saturation_handle = {0U, 0U};
        syntra_thread_switch_event_t expected_event;
        syntra_thread_switch_event_t pending_event;
        unsigned int saturation_processed = 77U;
        unsigned int saturated_count = 0U;

        check("Create owned thread for full-table FIFO preservation",
            process_thread_create_kernel_owned_with_handle(
                process, 2000U, 2U, test_entry, NULL, 4096U,
                &saturation_handle) == SYNTRA_STATUS_OK);
        check("Run thread for full-table FIFO preservation",
            scheduler_set_thread_state_by_handle(
                saturation_handle, THREAD_RUNNING) == SYNTRA_STATUS_OK);
        check("Terminate thread for full-table FIFO preservation",
            scheduler_terminate_current() == SYNTRA_STATUS_OK);
        check("Switch to reaper before full-table dispatch",
            scheduler_set_thread_state_by_handle(
                reaper_handle, THREAD_RUNNING) == SYNTRA_STATUS_OK);
        check("Publish terminal event before saturating candidate table",
            scheduler_complete_switch_away() == SYNTRA_STATUS_OK);
        check("Capture FIFO head before candidate-table saturation",
            scheduler_peek_switch_away_event(&expected_event) ==
                SYNTRA_STATUS_OK &&
            scheduler_thread_handle_equal(
                expected_event.handle, saturation_handle) &&
            expected_event.terminal != 0U);
        check("Host test fills every candidate slot with quarantine records",
            thread_reaper_test_fill_quarantined() == SYNTRA_STATUS_OK);
        check("Candidate table reaches exact fixed capacity",
            thread_reaper_get_candidate_count(&saturated_count) ==
                SYNTRA_STATUS_OK &&
            saturated_count == SYNTRA_REAPER_CANDIDATE_CAPACITY);
        for (unsigned int i = 0U; i < SYNTRA_REAPER_CANDIDATE_CAPACITY; ++i)
        {
            check("Quarantined candidate remains reserved under saturation",
                thread_reaper_get_candidate(i, &candidate) ==
                    SYNTRA_STATUS_OK &&
                candidate.state == SYNTRA_REAPER_CANDIDATE_QUARANTINED &&
                candidate.reason == SYNTRA_REAPER_REASON_STALE_HANDLE);
        }
        check("Dispatch with no reusable slot leaves event unprocessed",
            thread_reaper_dispatch(1U, &saturation_processed) ==
                SYNTRA_STATUS_OK && saturation_processed == 0U);
        check("Candidate high-water remains at fixed capacity",
            thread_reaper_get_candidate_count(&saturated_count) ==
                SYNTRA_STATUS_OK &&
            saturated_count == SYNTRA_REAPER_CANDIDATE_CAPACITY);
        check("Exact FIFO head remains pending when table is full",
            scheduler_peek_switch_away_event(&pending_event) ==
                SYNTRA_STATUS_OK &&
            scheduler_thread_handle_equal(
                pending_event.handle, expected_event.handle) &&
            pending_event.sequence == expected_event.sequence &&
            pending_event.terminal == expected_event.terminal);
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
