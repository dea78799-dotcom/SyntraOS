#include "../../kernel/memory/memory.h"
#include "../../kernel/process/process.h"
#include "../../kernel/process/process_thread.h"
#include "../../kernel/scheduler/scheduler.h"
#include "../../kernel/scheduler/thread_reaper.h"

#include <stdio.h>

static unsigned int failures;

static _Alignas(16) unsigned char successor_stack[4096];
static _Alignas(16) unsigned char later_stack[4096];
static int owned_context_marker;
static int successor_context_marker;
static int later_context_marker;

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
    syntra_process_handle_t owner = {0U, 0U};
    syntra_thread_handle_t owned_handle = {0U, 0U};
    syntra_thread_handle_t successor_handle = {0U, 0U};
    syntra_thread_handle_t later_handle = {0U, 0U};
    syntra_thread_t snapshot;
    syntra_reaper_candidate_t candidate;
    syntra_reaper_stats_t stats;
    syntra_memory_allocation_t allocation;
    syntra_memory_info_t before;
    syntra_memory_info_t after;
    void *selected_context = NULL;
    unsigned int processed = 0U;
    unsigned int candidate_count = 0U;

    check("Initialize memory", memory_init() == SYNTRA_STATUS_OK);
    check("Initialize process", process_init() == SYNTRA_STATUS_OK);
    check("Initialize scheduler", scheduler_init() == SYNTRA_STATUS_OK);
    check("Initialize candidate dispatcher",
        thread_reaper_init() == SYNTRA_STATUS_OK);
    check("Create process owner",
        process_create_with_handle(700U, 2U, &owner) == SYNTRA_STATUS_OK);
    check("Create allocator-owned thread",
        process_thread_create_kernel_owned_with_handle(
            owner, 701U, 1U, test_entry, NULL, 2048U,
            &owned_handle) == SYNTRA_STATUS_OK);
    check("Create first successor",
        scheduler_create_thread_with_handle(
            702U, 1U, test_entry, NULL, successor_stack,
            sizeof(successor_stack), &successor_handle) == SYNTRA_STATUS_OK);
    check("Create second successor",
        scheduler_create_thread_with_handle(
            703U, 1U, test_entry, NULL, later_stack,
            sizeof(later_stack), &later_handle) == SYNTRA_STATUS_OK);

    check("Prepare owned thread context",
        scheduler_set_thread_context(701U, &owned_context_marker) ==
            SYNTRA_STATUS_OK);
    check("Prepare successor context",
        scheduler_set_thread_context(702U, &successor_context_marker) ==
            SYNTRA_STATUS_OK);
    check("Prepare later context",
        scheduler_set_thread_context(703U, &later_context_marker) ==
            SYNTRA_STATUS_OK);

    check("Select owned thread as current",
        scheduler_set_thread_state_by_handle(
            owned_handle, THREAD_RUNNING) == SYNTRA_STATUS_OK);
    check("Terminate current owned thread",
        scheduler_terminate_current() == SYNTRA_STATUS_OK);
    check("Select successor context",
        scheduler_schedule_next_context(&selected_context) == SYNTRA_STATUS_OK &&
        selected_context == &successor_context_marker);
    check("Publish post-stack-switch proof",
        scheduler_complete_switch_away() == SYNTRA_STATUS_OK);

    memory_get_info(&before);
    check("Dispatch terminal proof into candidate queue",
        thread_reaper_dispatch(1U, &processed) == SYNTRA_STATUS_OK &&
        processed == 1U);
    check("Candidate queue contains one exact record",
        thread_reaper_get_candidate_count(&candidate_count) ==
            SYNTRA_STATUS_OK && candidate_count == 1U);
    check("Candidate preserves exact handle and proof sequence",
        thread_reaper_get_candidate(0U, &candidate) == SYNTRA_STATUS_OK &&
        scheduler_thread_handle_equal(
            candidate.proof.handle, owned_handle) &&
        candidate.proof.sequence != 0U &&
        candidate.state == SYNTRA_REAPER_CANDIDATE_QUEUED &&
        candidate.reason == SYNTRA_REAPER_REASON_NONE);
    check("Candidate still resolves to terminated TCB",
        scheduler_get_thread_snapshot_by_handle(
            owned_handle, &snapshot) == SYNTRA_STATUS_OK &&
        snapshot.state == THREAD_TERMINATED &&
        snapshot.stack_ownership == SYNTRA_THREAD_STACK_OWNED);
    check("Candidate allocator token remains THREAD_OWNED",
        memory_get_allocation_by_handle(
            snapshot.stack_allocation, &allocation) == SYNTRA_STATUS_OK &&
        allocation.ownership_state ==
            SYNTRA_MEMORY_ALLOCATION_THREAD_OWNED &&
        allocation.owner.slot == owned_handle.slot &&
        allocation.owner.generation == owned_handle.generation);

    check("Block successor to produce a nonterminal proof",
        scheduler_block_current() == SYNTRA_STATUS_OK);
    check("Select later thread",
        scheduler_schedule_next_context(&selected_context) == SYNTRA_STATUS_OK &&
        selected_context == &later_context_marker);
    check("Complete nonterminal switch-away",
        scheduler_complete_switch_away() == SYNTRA_STATUS_OK);
    processed = 0U;
    check("Dispatcher consumes but does not queue nonterminal proof",
        thread_reaper_dispatch(1U, &processed) == SYNTRA_STATUS_OK &&
        processed == 1U);
    check("Nonterminal proof did not add a reap candidate",
        thread_reaper_get_candidate_count(&candidate_count) ==
            SYNTRA_STATUS_OK && candidate_count == 1U);
    check("Dispatcher statistics distinguish terminal and nonterminal events",
        thread_reaper_get_stats(&stats) == SYNTRA_STATUS_OK &&
        stats.queued == 1U && stats.discarded_nonterminal == 1U &&
        stats.acknowledged_events == 2U);

    memory_get_info(&after);
    check("Dispatcher never releases candidate stack memory",
        before.active_allocations == after.active_allocations &&
        memory_validate() == SYNTRA_STATUS_OK);
    check("Scheduler invariants remain valid",
        scheduler_validate() == SYNTRA_STATUS_OK);

    if (failures != 0U)
    {
        printf("%u thread reaper handoff test(s) failed\n", failures);
        return 1;
    }

    printf("All thread reaper event handoff tests passed\n");
    return 0;
}
