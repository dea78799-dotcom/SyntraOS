#include <stdio.h>
#include <stdint.h>
#include "../../kernel/process/process.h"
#include "../../kernel/process/process_thread.h"
#include "../../kernel/scheduler/scheduler.h"

static int failures;

static void check(const char *name, int ok)
{
    printf("[%s] %s\n", ok ? "PASS" : "FAIL", name);
    if (!ok) ++failures;
}

static void thread_entry(void *argument)
{
    (void)argument;
}

int main(void)
{
    static _Alignas(16) unsigned char stack_a[2048];
    static _Alignas(16) unsigned char stack_b[2048];
    static _Alignas(16) unsigned char stack_c[2048];
    static _Alignas(16) unsigned char stack_d[2048];
    static _Alignas(16) unsigned char user_kernel_stack[2048];
    static _Alignas(16) unsigned char user_stack[2048];
    static unsigned char user_cpu_context[256];
    syntra_process_handle_t owner = {0U, 0U};
    syntra_process_handle_t other = {0U, 0U};
    syntra_process_handle_t invalid;
    syntra_thread_t snapshot;
    unsigned int count = 99U;

    check("Initialize process manager", process_init() == SYNTRA_STATUS_OK);
    check("Initialize scheduler", scheduler_init() == SYNTRA_STATUS_OK);
    check("Create owner process",
        process_create_with_handle(10U, 4U, &owner) == SYNTRA_STATUS_OK);
    check("Create second process",
        process_create_with_handle(11U, 4U, &other) == SYNTRA_STATUS_OK);

    check("Create first process-owned kernel thread",
        process_thread_create_kernel(owner, 100U, 3U, thread_entry, NULL,
            stack_a, sizeof(stack_a)) == SYNTRA_STATUS_OK);
    check("Create second thread for same process",
        process_thread_create_kernel(owner, 101U, 3U, thread_entry, NULL,
            stack_b, sizeof(stack_b)) == SYNTRA_STATUS_OK);
    check("Create process-owned user thread",
        process_thread_create_user(owner, 102U, 2U, thread_entry, NULL,
            user_kernel_stack, sizeof(user_kernel_stack),
            user_stack, sizeof(user_stack), user_cpu_context) == SYNTRA_STATUS_OK);
    check("Create thread owned by second process",
        process_thread_create_kernel(other, 201U, 2U, thread_entry, NULL,
            stack_d, sizeof(stack_d)) == SYNTRA_STATUS_OK);
    check("Count exact owner threads",
        process_thread_get_count(owner, &count) == SYNTRA_STATUS_OK &&
        count == 3U);
    count = 99U;
    check("Other process has exactly its own thread",
        process_thread_get_count(other, &count) == SYNTRA_STATUS_OK &&
        count == 1U);
    check("Read owner thread snapshot",
        process_thread_get_snapshot(owner, 1U, &snapshot) == SYNTRA_STATUS_OK &&
        snapshot.thread_id == 101U && snapshot.has_process_owner == 1U &&
        snapshot.owner_process.slot == owner.slot &&
        snapshot.owner_process.generation == owner.generation);
    check("User thread retains context and owner identity",
        process_thread_get_snapshot(owner, 2U, &snapshot) == SYNTRA_STATUS_OK &&
        snapshot.thread_id == 102U && snapshot.is_user_thread == 1U &&
        snapshot.cpu_context == user_cpu_context &&
        snapshot.has_process_owner == 1U &&
        snapshot.owner_process.slot == owner.slot &&
        snapshot.owner_process.generation == owner.generation);

    invalid = owner;
    invalid.generation = 0U;
    check("Reject zero-generation owner",
        process_thread_create_kernel(invalid, 102U, 1U, thread_entry, NULL,
            stack_c, sizeof(stack_c)) == SYNTRA_STATUS_INVALID_ARGUMENT);
    invalid = owner;
    ++invalid.generation;
    check("Reject stale or mismatched owner",
        process_thread_create_kernel(invalid, 102U, 1U, thread_entry, NULL,
            stack_c, sizeof(stack_c)) == SYNTRA_STATUS_INVALID_ARGUMENT);

    check("Reject duplicate thread ID without partial insertion",
        process_thread_create_kernel(owner, 100U, 1U, thread_entry, NULL,
            stack_c, sizeof(stack_c)) == SYNTRA_STATUS_INVALID_ARGUMENT);
    count = 0U;
    check("Failed creation leaves owner count unchanged",
        process_thread_get_count(owner, &count) == SYNTRA_STATUS_OK &&
        count == 3U);

    check("Mark second process terminated through metadata API",
        process_set_state_by_handle(other, PROCESS_TERMINATED) == SYNTRA_STATUS_OK);
    check("Reject thread creation for terminated process",
        process_thread_create_kernel(other, 200U, 1U, thread_entry, NULL,
            stack_c, sizeof(stack_c)) == SYNTRA_STATUS_PERMISSION_DENIED);

    check("Schedule an owned thread before cascade",
        scheduler_schedule_next() != NULL);
    check("Terminate owner process and its complete thread set",
        process_thread_terminate_process(owner) == SYNTRA_STATUS_OK);
    {
        syntra_process_t process_snapshot;
        check("Owner process state becomes terminated",
            process_get_snapshot_by_handle(owner, &process_snapshot) == SYNTRA_STATUS_OK &&
            process_snapshot.state == PROCESS_TERMINATED);
    }
    check("All owner threads are terminated",
        process_thread_get_snapshot(owner, 0U, &snapshot) == SYNTRA_STATUS_OK &&
        snapshot.state == THREAD_TERMINATED &&
        process_thread_get_snapshot(owner, 1U, &snapshot) == SYNTRA_STATUS_OK &&
        snapshot.state == THREAD_TERMINATED &&
        process_thread_get_snapshot(owner, 2U, &snapshot) == SYNTRA_STATUS_OK &&
        snapshot.state == THREAD_TERMINATED);
    check("Current thread pointer is cleared after current-owner termination",
        scheduler_get_current() == NULL);
    check("Terminated thread cannot be made runnable again",
        scheduler_set_thread_state(100U, THREAD_READY) ==
            SYNTRA_STATUS_INVALID_ARGUMENT);
    check("Second process thread remains unaffected by owner cascade",
        scheduler_get_thread_snapshot(201U, &snapshot) == SYNTRA_STATUS_OK &&
        snapshot.state == THREAD_READY);

    check("Cascade repairs active threads on already-terminated process",
        process_thread_terminate_process(other) == SYNTRA_STATUS_OK &&
        scheduler_get_thread_snapshot(201U, &snapshot) == SYNTRA_STATUS_OK &&
        snapshot.state == THREAD_TERMINATED);
    check("Repeated process cascade is idempotent",
        process_thread_terminate_process(owner) == SYNTRA_STATUS_OK);
    check("Invalid handle does not mutate scheduler",
        process_thread_terminate_process(invalid) == SYNTRA_STATUS_INVALID_ARGUMENT &&
        scheduler_get_thread_snapshot(201U, &snapshot) == SYNTRA_STATUS_OK &&
        snapshot.state == THREAD_TERMINATED);

    check("Legacy scheduler thread remains unowned",
        scheduler_add_thread(300U, 1U) == SYNTRA_STATUS_OK &&
        scheduler_get_thread_snapshot(300U, &snapshot) == SYNTRA_STATUS_OK &&
        snapshot.has_process_owner == 0U &&
        snapshot.owner_process.slot == 0U &&
        snapshot.owner_process.generation == 0U);
    check("Scheduler invariants remain valid",
        scheduler_validate() == SYNTRA_STATUS_OK);

    printf("Total failures: %d\n", failures);
    return failures == 0 ? 0 : 1;
}
