#include <stdio.h>
#include <stddef.h>
#include <stdint.h>

#include "../../kernel/scheduler/scheduler.h"
#include "../../kernel/scheduler/scheduler_process_internal.h"

static int failures;

static void check(const char *name, int condition)
{
    if (condition)
    {
        printf("[PASS] %s\n", name);
    }
    else
    {
        printf("[FAIL] %s\n", name);
        ++failures;
    }
}

static void dummy_thread(void *argument)
{
    (void)argument;
}

static _Alignas(16) unsigned char stack_a[2048];
static _Alignas(16) unsigned char stack_b[2048];
static _Alignas(16) unsigned char fake_cpu_context[320];

int main(void)
{
    printf("\n=================================\n");
    printf(" SYNTRAOS SCHEDULER V1 TEST\n");
    printf("=================================\n");

    check("Initialize scheduler",
        scheduler_init() == SYNTRA_STATUS_OK);
    check("Add metadata thread 1",
        scheduler_add_thread(1U, 5U) == SYNTRA_STATUS_OK);
    check("Reject duplicate ID",
        scheduler_add_thread(1U, 3U) == SYNTRA_STATUS_INVALID_ARGUMENT);
    check("Create thread with caller stack",
        scheduler_create_thread(2U, 99U, dummy_thread, NULL,
            stack_a, sizeof(stack_a)) == SYNTRA_STATUS_OK);
    check("Scheduler invariants accept legacy borrowed stack descriptor",
        scheduler_validate() == SYNTRA_STATUS_OK);
    check("Reject unaligned stack",
        scheduler_create_thread(3U, 1U, dummy_thread, NULL,
            &stack_b[1], sizeof(stack_b) - 1U)
            == SYNTRA_STATUS_INVALID_ARGUMENT);
    check("Reject undersized stack",
        scheduler_create_thread(3U, 1U, dummy_thread, NULL,
            stack_b, 512U) == SYNTRA_STATUS_INVALID_ARGUMENT);

    syntra_thread_t *thread = scheduler_get_thread(2U);
    check("Store entry and stack metadata",
        thread != NULL && thread->entry == dummy_thread &&
        thread->stack_base == (uintptr_t)stack_a &&
        thread->stack_size == sizeof(stack_a) &&
        thread->stack_ownership == SYNTRA_THREAD_STACK_BORROWED &&
        thread->stack_allocation.generation == 0U);

    {
        syntra_thread_t snapshot;
        check("Read thread through copy-out snapshot",
            scheduler_get_thread_snapshot(2U, &snapshot) == SYNTRA_STATUS_OK);
        snapshot.state = THREAD_TERMINATED;
        check("Snapshot mutation does not alter scheduler record",
            scheduler_get_thread_snapshot(2U, &snapshot) == SYNTRA_STATUS_OK &&
            snapshot.state == THREAD_READY);
        check("Reject NULL thread snapshot",
            scheduler_get_thread_snapshot(2U, NULL) ==
                SYNTRA_STATUS_INVALID_ARGUMENT);
    }

    thread = scheduler_schedule_next();
    check("Round-robin selects first READY thread",
        thread != NULL && thread->thread_id == 1U &&
        thread->state == THREAD_RUNNING);

    thread = scheduler_schedule_next();
    check("Round-robin selects next thread, not highest priority",
        thread != NULL && thread->thread_id == 2U &&
        thread->state == THREAD_RUNNING);

    thread = scheduler_schedule_next();
    check("Round-robin wraps to first thread",
        thread != NULL && thread->thread_id == 1U);

    {
        syntra_thread_t outgoing_snapshot;
        syntra_thread_switch_event_t event;

        check("Capture blocked thread identity before detaching current slot",
            scheduler_get_thread_snapshot(1U, &outgoing_snapshot) ==
                SYNTRA_STATUS_OK);
        check("Block current thread",
            scheduler_block_current() == SYNTRA_STATUS_OK);
        check("Blocked thread is not selected",
            scheduler_schedule_next() != NULL &&
            scheduler_get_current()->thread_id == 2U);
        check("Publish blocked-thread switch proof",
            scheduler_complete_switch_away() == SYNTRA_STATUS_OK);
        check("Consume exact blocked-thread switch event",
            scheduler_take_switch_away_event(&event) == SYNTRA_STATUS_OK &&
            scheduler_thread_handle_equal(event.handle, outgoing_snapshot.handle) &&
            event.sequence != 0U);
    }
    check("Wake blocked thread",
        scheduler_wake_thread(1U) == SYNTRA_STATUS_OK);

    {
        syntra_thread_t outgoing_snapshot;
        syntra_thread_switch_event_t event;
        check("Capture terminating thread identity",
            scheduler_get_thread_snapshot(2U, &outgoing_snapshot) ==
                SYNTRA_STATUS_OK);
        check("Terminate current thread",
            scheduler_terminate_current() == SYNTRA_STATUS_OK);
        check("Terminated thread remains terminated",
            scheduler_get_thread(2U)->state == THREAD_TERMINATED);
        check("Next READY thread is scheduled",
            scheduler_schedule_next() != NULL &&
            scheduler_get_current()->thread_id == 1U);
        check("Publish terminated-thread switch proof",
            scheduler_complete_switch_away() == SYNTRA_STATUS_OK);
        check("Consume exact terminated-thread switch event",
            scheduler_take_switch_away_event(&event) == SYNTRA_STATUS_OK &&
            scheduler_thread_handle_equal(event.handle, outgoing_snapshot.handle) &&
            event.sequence != 0U);
    }

    check("Thread count remains consistent",
        scheduler_get_thread_count() == 2U);
    check("Scheduler invariants",
        scheduler_validate() == SYNTRA_STATUS_OK);


    {
        static _Alignas(16) unsigned char user_kernel_stack[2048];
        static _Alignas(16) unsigned char user_stack[2048];
        static _Alignas(16) unsigned char user_kernel_stack_2[2048];
        static _Alignas(16) unsigned char user_stack_2[2048];
        static _Alignas(16) unsigned char fake_context[320];
        syntra_thread_t *user_thread;
        syntra_thread_handle_t user_handle = {0U, 0U};

        check("Create user thread and return stable handle",
            scheduler_create_user_thread_with_handle(
                4U, 1U, dummy_thread, NULL,
                user_kernel_stack, sizeof(user_kernel_stack),
                user_stack, sizeof(user_stack),
                fake_context, &user_handle) == SYNTRA_STATUS_OK);
        {
            syntra_thread_t handle_snapshot;
            check("User thread handle resolves exact registered thread",
                user_handle.generation != 0U &&
                scheduler_get_thread_snapshot_by_handle(user_handle,
                    &handle_snapshot) == SYNTRA_STATUS_OK &&
                handle_snapshot.thread_id == 4U &&
                handle_snapshot.is_user_thread == 1U);
        }

        user_thread = scheduler_get_thread(4U);
        check("Store kernel stack and user stack independently",
            user_thread != NULL &&
            user_thread->stack_base == (uintptr_t)user_kernel_stack &&
            user_thread->stack_size == sizeof(user_kernel_stack) &&
            user_thread->user_stack_base == (uintptr_t)user_stack &&
            user_thread->user_stack_size == sizeof(user_stack));

        check("Mark user thread and preserve prepared CPU context",
            user_thread != NULL && user_thread->is_user_thread == 1U &&
            user_thread->cpu_context == fake_context);

        check("Reject overlapping kernel and user stacks",
            scheduler_create_user_thread(
                5U, 1U, dummy_thread, NULL,
                user_kernel_stack, sizeof(user_kernel_stack),
                user_kernel_stack, sizeof(user_kernel_stack),
                fake_context) == SYNTRA_STATUS_INVALID_ARGUMENT);

        check("Reject user stack overlapping an existing thread stack",
            scheduler_create_user_thread(
                5U, 1U, dummy_thread, NULL,
                user_kernel_stack_2, sizeof(user_kernel_stack_2),
                stack_a, sizeof(stack_a),
                fake_context) == SYNTRA_STATUS_INVALID_ARGUMENT);

        check("Reject user thread without a prepared context",
            scheduler_create_user_thread(
                5U, 1U, dummy_thread, NULL,
                user_kernel_stack_2, sizeof(user_kernel_stack_2),
                user_stack_2, sizeof(user_stack_2),
                NULL) == SYNTRA_STATUS_INVALID_ARGUMENT);

        check("Scheduler validates user stack metadata",
            scheduler_validate() == SYNTRA_STATUS_OK);
        check("User thread remains READY until explicitly scheduled",
            user_thread != NULL && user_thread->state == THREAD_READY);
    }

    {
        syntra_thread_t *selected;
        syntra_thread_t *metadata_thread = scheduler_get_thread(2U);
        syntra_thread_t *context_thread = scheduler_get_thread(4U);

        /* Thread 2 is READY but has no context; thread 4 is eligible. */
        if (metadata_thread != NULL)
        {
            metadata_thread->state = THREAD_READY;
            metadata_thread->cpu_context = NULL;
        }
        if (context_thread != NULL)
            context_thread->state = THREAD_READY;

        {
            void *selected_context = NULL;
            syntra_thread_t outgoing_snapshot;
            syntra_thread_switch_event_t event;
            (void)scheduler_get_thread_snapshot(1U, &outgoing_snapshot);
            check("Context-aware selection skips metadata-only READY thread",
                scheduler_schedule_next_context(&selected_context) ==
                    SYNTRA_STATUS_OK &&
                selected_context == context_thread->cpu_context &&
                scheduler_get_current() != NULL &&
                scheduler_get_current()->thread_id == 4U);
            check("Publish outgoing thread proof after simulated stack handoff",
                scheduler_complete_switch_away() == SYNTRA_STATUS_OK);
            check("Context switch event identifies exact outgoing generation",
                scheduler_take_switch_away_event(&event) == SYNTRA_STATUS_OK &&
                scheduler_thread_handle_equal(event.handle, outgoing_snapshot.handle) &&
                event.sequence != 0U);
        }
        selected = scheduler_get_current();
        check("Context-aware selection marks successor RUNNING",
            selected != NULL && selected->thread_id == 4U &&
            selected->cpu_context != NULL &&
            selected->state == THREAD_RUNNING);
    }

    {
        syntra_thread_t *user_thread = scheduler_get_thread(4U);

        check("Reject a second thread using the exact same stack",
            scheduler_create_thread(5U, 1U, dummy_thread, NULL,
                stack_a, sizeof(stack_a)) == SYNTRA_STATUS_INVALID_ARGUMENT);

        check("Reject partially overlapping thread stacks",
            scheduler_create_thread(6U, 1U, dummy_thread, NULL,
                &stack_a[1024], 1024U) == SYNTRA_STATUS_INVALID_ARGUMENT);

        check("Reject kernel thread stack overlapping a user stack",
            user_thread != NULL &&
            scheduler_create_thread(7U, 1U, dummy_thread, NULL,
                (void *)user_thread->user_stack_base,
                user_thread->user_stack_size) == SYNTRA_STATUS_INVALID_ARGUMENT);

        check("Reject stack range overflowing uintptr_t",
            scheduler_create_thread(8U, 1U, dummy_thread, NULL,
                (void *)(UINTPTR_MAX - 15U), 1024U)
                == SYNTRA_STATUS_INVALID_ARGUMENT);
    }

    {
        void *resumed_context = NULL;
        syntra_thread_t *context_thread = scheduler_get_thread(4U);

        check("Block current thread for wake-before-switch case",
            scheduler_block_current() == SYNTRA_STATUS_OK);
        check("Wake thread before a successor stack is installed",
            scheduler_wake_thread(4U) == SYNTRA_STATUS_OK);
        check("Scheduler may resume the same exact thread context",
            scheduler_schedule_next_context(&resumed_context) == SYNTRA_STATUS_OK &&
            context_thread != NULL &&
            resumed_context == context_thread->cpu_context);
        check("Same-thread resume cancels stale switch-away proof",
            scheduler_complete_switch_away() == SYNTRA_STATUS_ERROR);
    }

    {
        syntra_thread_t snapshot_a;
        syntra_thread_t snapshot_b;

        check("Terminate remaining context-aware thread",
            scheduler_set_thread_state(4U, THREAD_TERMINATED) ==
                SYNTRA_STATUS_OK);
        check("Terminate remaining metadata-only thread",
            scheduler_set_thread_state(1U, THREAD_TERMINATED) ==
                SYNTRA_STATUS_OK);
        {
            void *no_context = NULL;
            syntra_thread_switch_event_t no_event;
            check("No-successor context selection returns no context",
                scheduler_schedule_next_context(&no_context) ==
                    SYNTRA_STATUS_ERROR && no_context == NULL);
            check("No-successor does not publish switch-away proof",
                scheduler_take_switch_away_event(&no_event) ==
                    SYNTRA_STATUS_ERROR);
        }
        check("No-successor leaves no current thread",
            scheduler_get_current() == NULL);
        check("Terminated threads remain terminal after empty selection",
            scheduler_get_thread_snapshot(1U, &snapshot_a) == SYNTRA_STATUS_OK &&
            scheduler_get_thread_snapshot(4U, &snapshot_b) == SYNTRA_STATUS_OK &&
            snapshot_a.state == THREAD_TERMINATED &&
            snapshot_b.state == THREAD_TERMINATED);
        check("Scheduler invariants hold with no runnable context",
            scheduler_validate() == SYNTRA_STATUS_OK);
    }

    {
        void *owned_stack = NULL;
        syntra_memory_allocation_handle_t allocation = {UINT32_MAX, 0U};
        syntra_thread_handle_t reserved = {0U, 0U};
        syntra_memory_owner_handle_t owner;

        check("Initialize memory manager for owned-stack transaction",
            memory_init() == SYNTRA_STATUS_OK);
        check("Stage allocation for unpublished thread",
            memory_allocate_staged(1U, &owned_stack, &allocation) ==
                SYNTRA_STATUS_OK && owned_stack != NULL &&
                allocation.generation != 0U);

        owner.slot = 0U;
        owner.generation = 1U;
        check("Reserve owned thread without publishing it",
            scheduler_reserve_owned_thread((syntra_process_handle_t){0U, 1U}, 88U, 3U, dummy_thread,
                NULL, owned_stack, 2048U, &reserved) == SYNTRA_STATUS_OK);
        check("Reservation is invisible to published thread count and selection",
            scheduler_get_thread_count() == 3U &&
            scheduler_get_thread_snapshot_by_handle(reserved,
                &(syntra_thread_t){0}) == SYNTRA_STATUS_INVALID_ARGUMENT &&
            (scheduler_get_next_ready() == NULL ||
                scheduler_get_next_ready()->thread_id != 88U));
        check("Public insertion cannot steal reserved append-only slot",
            scheduler_add_thread(90U, 1U) == SYNTRA_STATUS_ERROR &&
            scheduler_get_thread_count() == 3U);
        check("Scheduler invariants accept unpublished reservation",
            scheduler_validate() == SYNTRA_STATUS_OK);

        owner.slot = reserved.slot;
        owner.generation = reserved.generation;
        check("Bind staged allocation to exact reserved thread",
            memory_bind_staged_allocation(allocation, owner) ==
                SYNTRA_STATUS_OK);
        check("Attach exact reserved allocation descriptor",
            scheduler_attach_owned_stack(reserved, allocation) ==
                SYNTRA_STATUS_OK);
        check("Commit allocator ownership before scheduler publication",
            memory_commit_thread_allocation(allocation, owner) ==
                SYNTRA_STATUS_OK);
        check("Publish complete owned thread as final visibility step",
            scheduler_publish_owned_thread(reserved) == SYNTRA_STATUS_OK);
        {
            syntra_thread_t published;
            check("Published thread exposes exact OWNED token",
                scheduler_get_thread_snapshot_by_handle(reserved,
                    &published) == SYNTRA_STATUS_OK &&
                published.state == THREAD_READY &&
                published.stack_ownership == SYNTRA_THREAD_STACK_OWNED &&
                published.stack_allocation.slot == allocation.slot &&
                published.stack_allocation.generation ==
                    allocation.generation);
        }
        check("Published owned thread preserves scheduler invariants",
            scheduler_get_thread_count() == 4U &&
            scheduler_validate() == SYNTRA_STATUS_OK);
        check("Allocator snapshot resolves exact THREAD_OWNED token",
            memory_get_allocation_by_handle(allocation,
                &(syntra_memory_allocation_t){0}) == SYNTRA_STATUS_OK);
    }

    {
        void *owned_stack = NULL;
        syntra_memory_allocation_handle_t allocation = {UINT32_MAX, 0U};
        syntra_thread_handle_t reserved = {0U, 0U};
        syntra_memory_owner_handle_t wrong_owner;

        check("Stage allocation for owner-mismatch rollback",
            memory_allocate_staged(1U, &owned_stack, &allocation) ==
                SYNTRA_STATUS_OK);
        check("Reserve second unpublished thread",
            scheduler_reserve_owned_thread(
                (syntra_process_handle_t){0U, 1U}, 89U, 1U,
                dummy_thread, NULL, owned_stack, 2048U, &reserved) ==
                SYNTRA_STATUS_OK);
        wrong_owner.slot = reserved.slot + 1U;
        wrong_owner.generation = reserved.generation;
        check("Bind token to deliberately wrong thread identity",
            memory_bind_staged_allocation(allocation, wrong_owner) ==
                SYNTRA_STATUS_OK);
        check("Reject descriptor attachment for owner mismatch",
            scheduler_attach_owned_stack(reserved, allocation) ==
                SYNTRA_STATUS_PERMISSION_DENIED);
        check("Abort unpublished reservation after failed attachment",
            scheduler_abort_reserved_thread(reserved) == SYNTRA_STATUS_OK);
        check("Abort exact reserved allocation during rollback",
            memory_abort_reserved_allocation(allocation, wrong_owner) ==
                SYNTRA_STATUS_OK);
        check("Failed reservation never changes published thread count",
            scheduler_get_thread_count() == 4U);
        check("Scheduler invariants hold after reservation abort",
            scheduler_validate() == SYNTRA_STATUS_OK);
    }

    printf("\n=================================\n");
    printf(" Total failures: %d\n", failures);
    printf("=================================\n");
    return failures == 0 ? 0 : 1;
}
