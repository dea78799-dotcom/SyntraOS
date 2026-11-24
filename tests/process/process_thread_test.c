#include <stdio.h>
#include <stdint.h>
#include "../../kernel/process/process.h"
#include "../../kernel/process/process_thread.h"
#include "../../kernel/scheduler/scheduler.h"
#include "../../kernel/memory/memory.h"

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
    syntra_process_handle_t owned_process = {0U, 0U};
    syntra_process_handle_t invalid;
    syntra_thread_t snapshot;
    syntra_thread_handle_t kernel_handle = {0U, 0U};
    syntra_thread_handle_t user_handle = {0U, 0U};
    syntra_thread_handle_t owned_handle = {77U, 88U};
    syntra_thread_handle_t current_handle = {77U, 88U};
    unsigned int count = 99U;

    check("Initialize memory manager", memory_init() == SYNTRA_STATUS_OK);
    check("Initialize process manager", process_init() == SYNTRA_STATUS_OK);
    check("Initialize scheduler", scheduler_init() == SYNTRA_STATUS_OK);
    check("Create owner process",
        process_create_with_handle(10U, 4U, &owner) == SYNTRA_STATUS_OK);
    check("Create second process",
        process_create_with_handle(11U, 4U, &other) == SYNTRA_STATUS_OK);
    check("Create process for allocator-owned thread tests",
        process_create_with_handle(12U, 4U, &owned_process) == SYNTRA_STATUS_OK);

    check("Create first process-owned kernel thread and return handle",
        process_thread_create_kernel_with_handle(
            owner, 100U, 3U, thread_entry, NULL,
            stack_a, sizeof(stack_a), &kernel_handle) == SYNTRA_STATUS_OK);
    check("Kernel handle resolves exact process-owned thread",
        kernel_handle.generation != 0U &&
        scheduler_get_thread_snapshot_by_handle(kernel_handle, &snapshot) ==
            SYNTRA_STATUS_OK && snapshot.thread_id == 100U &&
        snapshot.has_process_owner == 1U &&
        snapshot.owner_process.slot == owner.slot &&
        snapshot.owner_process.generation == owner.generation);
    check("Create second thread for same process",
        process_thread_create_kernel(owner, 101U, 3U, thread_entry, NULL,
            stack_b, sizeof(stack_b)) == SYNTRA_STATUS_OK);
    check("Create process-owned user thread and return handle",
        process_thread_create_user_with_handle(
            owner, 102U, 2U, thread_entry, NULL,
            user_kernel_stack, sizeof(user_kernel_stack),
            user_stack, sizeof(user_stack), user_cpu_context,
            &user_handle) == SYNTRA_STATUS_OK);
    check("User handle resolves exact process-owned thread",
        user_handle.generation != 0U &&
        scheduler_get_thread_snapshot_by_handle(user_handle, &snapshot) ==
            SYNTRA_STATUS_OK && snapshot.thread_id == 102U &&
        snapshot.is_user_thread == 1U &&
        snapshot.owner_process.slot == owner.slot &&
        snapshot.owner_process.generation == owner.generation);
    check("Create thread owned by second process",
        process_thread_create_kernel(other, 201U, 2U, thread_entry, NULL,
            stack_d, sizeof(stack_d)) == SYNTRA_STATUS_OK);
    check("Create allocator-owned kernel thread transactionally",
        process_thread_create_kernel_owned_with_handle(
            owned_process, 301U, 2U, thread_entry, NULL, 3000U,
            &owned_handle) == SYNTRA_STATUS_OK);
    check("Owned thread handle resolves exact published TCB",
        owned_handle.generation != 0U &&
        scheduler_get_thread_snapshot_by_handle(owned_handle, &snapshot) ==
            SYNTRA_STATUS_OK && snapshot.thread_id == 301U &&
        snapshot.stack_ownership == SYNTRA_THREAD_STACK_OWNED &&
        snapshot.stack_allocation.generation != 0U &&
        snapshot.has_process_owner == 1U &&
        snapshot.owner_process.slot == owned_process.slot &&
        snapshot.owner_process.generation == owned_process.generation);
    {
        syntra_memory_allocation_t allocation_snapshot;
        syntra_memory_allocation_handle_t token = snapshot.stack_allocation;
        check("Owned TCB token resolves exact THREAD_OWNED allocation",
            memory_get_allocation_by_handle(token, &allocation_snapshot) ==
                SYNTRA_STATUS_OK &&
            allocation_snapshot.ownership_state ==
                SYNTRA_MEMORY_ALLOCATION_THREAD_OWNED &&
            allocation_snapshot.owner.slot == owned_handle.slot &&
            allocation_snapshot.owner.generation == owned_handle.generation &&
            allocation_snapshot.address == (void *)snapshot.stack_base &&
            allocation_snapshot.block_count * SYNTRA_MEMORY_BLOCK_SIZE >=
                snapshot.stack_size);
    }
    {
        syntra_memory_info_t before;
        syntra_memory_info_t after;
        memory_get_info(&before);
        check("Duplicate owned-thread ID fails without publishing",
            process_thread_create_kernel_owned_with_handle(
                owner, 100U, 1U, thread_entry, NULL, 2048U,
                &owned_handle) == SYNTRA_STATUS_INVALID_ARGUMENT);
        memory_get_info(&after);
        check("Failed owned-thread transaction releases staged allocation",
            before.active_allocations == after.active_allocations &&
            scheduler_validate() == SYNTRA_STATUS_OK);
    }

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
    kernel_handle.slot = 77U;
    kernel_handle.generation = 88U;
    check("Reject zero-generation owner without publishing handle",
        process_thread_create_kernel_with_handle(
            invalid, 102U, 1U, thread_entry, NULL,
            stack_c, sizeof(stack_c), &kernel_handle) ==
                SYNTRA_STATUS_INVALID_ARGUMENT &&
        kernel_handle.slot == 77U && kernel_handle.generation == 88U);
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
    {
        syntra_memory_info_t before;
        syntra_memory_info_t after;
        owned_handle.slot = 77U;
        owned_handle.generation = 88U;
        memory_get_info(&before);
        check("Reject owned-thread creation for terminated process",
            process_thread_create_kernel_owned_with_handle(
                other, 202U, 1U, thread_entry, NULL, 2048U,
                &owned_handle) == SYNTRA_STATUS_PERMISSION_DENIED &&
            owned_handle.slot == 77U && owned_handle.generation == 88U);
        memory_get_info(&after);
        check("Terminated-process rejection allocates no stack",
            before.active_allocations == after.active_allocations);
    }
    check("Reject undersized owned kernel stack before allocation",
        process_thread_create_kernel_owned_with_handle(
            owner, 203U, 1U, thread_entry, NULL, 512U,
            &owned_handle) == SYNTRA_STATUS_INVALID_ARGUMENT);


    check("Current handle is unavailable before scheduling and output is unchanged",
        scheduler_get_current_handle(&current_handle) == SYNTRA_STATUS_ERROR &&
        current_handle.slot == 77U && current_handle.generation == 88U);
    check("Schedule an owned thread before cascade",
        scheduler_schedule_next() != NULL);
    check("Current handle matches current thread snapshot",
        scheduler_get_current_handle(&current_handle) == SYNTRA_STATUS_OK &&
        scheduler_get_current_snapshot(&snapshot) == SYNTRA_STATUS_OK &&
        current_handle.slot == snapshot.handle.slot &&
        current_handle.generation == snapshot.handle.generation);
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
    check("Current handle unavailable after owner termination",
        scheduler_get_current_handle(&current_handle) == SYNTRA_STATUS_ERROR);
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

    check("Terminate process containing allocator-owned thread",
        process_thread_terminate_process(owned_process) == SYNTRA_STATUS_OK &&
        scheduler_get_thread_snapshot_by_handle(owned_handle, &snapshot) ==
            SYNTRA_STATUS_OK && snapshot.state == THREAD_TERMINATED);
    check("Owned stack remains accounted until deferred reaper exists",
        memory_get_allocation_by_handle(snapshot.stack_allocation,
            &(syntra_memory_allocation_t){0}) == SYNTRA_STATUS_OK &&
        memory_validate() == SYNTRA_STATUS_OK);
    check("Scheduler invariants remain valid",
        scheduler_validate() == SYNTRA_STATUS_OK);

    printf("Total failures: %d\n", failures);
    return failures == 0 ? 0 : 1;
}
