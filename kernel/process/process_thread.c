#include "process_thread.h"
#include "../scheduler/scheduler_process_internal.h"
#include "process.h"
#include "../arch/irq.h"
#include "../memory/memory.h"

#ifdef SYNTRA_HOST_TESTING
/* Host-only fault injection; never compiled into the ARM64 kernel image. */
static unsigned int process_thread_test_failure_stage;

void process_thread_test_fail_at(unsigned int stage)
{
    process_thread_test_failure_stage = stage;
}

static int process_thread_test_should_fail(unsigned int stage)
{
    if (process_thread_test_failure_stage != stage)
        return 0;
    process_thread_test_failure_stage = 0U;
    return 1;
}
#endif

static int process_thread_handle_is_structurally_valid(
    syntra_process_handle_t handle)
{
    return handle.slot < SYNTRA_MAX_PROCESSES && handle.generation != 0U;
}

/*
 * This coordinator is the only supported path for creating process-owned
 * threads. The outer IRQ guard keeps process validation and scheduler commit
 * within one single-core critical section. Nested manager/scheduler guards
 * restore the already-masked DAIF state. This is not SMP synchronization.
 */
syntra_status_t process_thread_create_kernel(
    syntra_process_handle_t owner, unsigned int thread_id,
    unsigned int priority, syntra_thread_entry_t entry, void *argument,
    void *kernel_stack, size_t kernel_stack_size)
{
    syntra_thread_handle_t ignored_handle;
    return process_thread_create_kernel_with_handle(owner, thread_id,
        priority, entry, argument, kernel_stack, kernel_stack_size,
        &ignored_handle);
}

syntra_status_t process_thread_create_kernel_with_handle(
    syntra_process_handle_t owner,
    unsigned int thread_id,
    unsigned int priority,
    syntra_thread_entry_t entry,
    void *argument,
    void *kernel_stack,
    size_t kernel_stack_size,
    syntra_thread_handle_t *handle_out)
{
    syntra_irq_state_t irq;
    syntra_process_t process;
    syntra_status_t status;

    if (!process_thread_handle_is_structurally_valid(owner) ||
        thread_id == 0U || entry == NULL || kernel_stack == NULL ||
        handle_out == NULL)
        return SYNTRA_STATUS_INVALID_ARGUMENT;

    irq = syntra_irq_save();
    status = process_get_snapshot_by_handle(owner, &process);
    if (status == SYNTRA_STATUS_OK && process.state == PROCESS_TERMINATED)
        status = SYNTRA_STATUS_PERMISSION_DENIED;
    if (status == SYNTRA_STATUS_OK)
        status = scheduler_create_owned_thread_with_handle(
            owner, thread_id, priority, entry, argument,
            kernel_stack, kernel_stack_size, handle_out);
    syntra_irq_restore(irq);
    return status;
}

syntra_status_t process_thread_create_user(
    syntra_process_handle_t owner, unsigned int thread_id,
    unsigned int priority, syntra_thread_entry_t entry, void *argument,
    void *kernel_stack, size_t kernel_stack_size,
    void *user_stack, size_t user_stack_size, void *cpu_context)
{
    syntra_thread_handle_t ignored_handle;
    return process_thread_create_user_with_handle(owner, thread_id,
        priority, entry, argument, kernel_stack, kernel_stack_size,
        user_stack, user_stack_size, cpu_context, &ignored_handle);
}

syntra_status_t process_thread_create_user_with_handle(
    syntra_process_handle_t owner,
    unsigned int thread_id,
    unsigned int priority,
    syntra_thread_entry_t entry,
    void *argument,
    void *kernel_stack,
    size_t kernel_stack_size,
    void *user_stack,
    size_t user_stack_size,
    void *cpu_context,
    syntra_thread_handle_t *handle_out)
{
    syntra_irq_state_t irq;
    syntra_process_t process;
    syntra_status_t status;

    if (!process_thread_handle_is_structurally_valid(owner) ||
        thread_id == 0U || entry == NULL || kernel_stack == NULL ||
        user_stack == NULL || cpu_context == NULL || handle_out == NULL)
        return SYNTRA_STATUS_INVALID_ARGUMENT;

    irq = syntra_irq_save();
    status = process_get_snapshot_by_handle(owner, &process);
    if (status == SYNTRA_STATUS_OK && process.state == PROCESS_TERMINATED)
        status = SYNTRA_STATUS_PERMISSION_DENIED;
    if (status == SYNTRA_STATUS_OK)
        status = scheduler_create_owned_user_thread_with_handle(
            owner, thread_id, priority, entry, argument,
            kernel_stack, kernel_stack_size, user_stack,
            user_stack_size, cpu_context, handle_out);
    syntra_irq_restore(irq);
    return status;
}


/*
 * Canonical owned-kernel-thread transaction:
 * STAGED allocation -> unpublished TCB -> exact owner bind -> descriptor
 * attach -> THREAD_OWNED commit -> READY publication.
 *
 * The outer IRQ guard makes process validation and all transaction stages
 * atomic with respect to local single-core interrupt-driven scheduling.
 */
syntra_status_t process_thread_create_kernel_owned_with_handle(
    syntra_process_handle_t owner,
    unsigned int thread_id,
    unsigned int priority,
    syntra_thread_entry_t entry,
    void *argument,
    size_t stack_size,
    syntra_thread_handle_t *handle_out)
{
    syntra_irq_state_t irq;
    syntra_process_t process;
    syntra_thread_handle_t reserved_handle;
    syntra_memory_allocation_handle_t allocation;
    syntra_memory_owner_handle_t allocation_owner;
    void *kernel_stack = NULL;
    unsigned int blocks;
    syntra_status_t status;
    syntra_status_t cleanup_status = SYNTRA_STATUS_OK;
    int reservation_created = 0;
    int allocation_created = 0;
    int allocation_reserved = 0;
    int allocation_committed = 0;

    if (!process_thread_handle_is_structurally_valid(owner) ||
        thread_id == 0U || entry == NULL || handle_out == NULL ||
        stack_size < SYNTRA_THREAD_MIN_STACK_SIZE ||
        stack_size >
            (size_t)SYNTRA_MAX_MEMORY_BLOCKS * SYNTRA_MEMORY_BLOCK_SIZE)
        return SYNTRA_STATUS_INVALID_ARGUMENT;

    blocks = (unsigned int)(
        (stack_size + (SYNTRA_MEMORY_BLOCK_SIZE - 1U)) /
        SYNTRA_MEMORY_BLOCK_SIZE);
    if (blocks == 0U || blocks > SYNTRA_MAX_MEMORY_BLOCKS)
        return SYNTRA_STATUS_INVALID_ARGUMENT;

    irq = syntra_irq_save();

    status = process_get_snapshot_by_handle(owner, &process);
    if (status != SYNTRA_STATUS_OK)
        goto rollback;
    if (process.state == PROCESS_TERMINATED)
    {
        status = SYNTRA_STATUS_PERMISSION_DENIED;
        goto rollback;
    }

    status = memory_allocate_staged(blocks, &kernel_stack, &allocation);
    if (status != SYNTRA_STATUS_OK)
        goto rollback;
    allocation_created = 1;
#ifdef SYNTRA_HOST_TESTING
    if (process_thread_test_should_fail(1U))
    {
        status = SYNTRA_STATUS_ERROR;
        goto rollback;
    }
#endif

    status = scheduler_reserve_owned_thread(
        owner, thread_id, priority, entry, argument,
        kernel_stack, stack_size, &reserved_handle);
    if (status != SYNTRA_STATUS_OK)
        goto rollback;
    reservation_created = 1;
#ifdef SYNTRA_HOST_TESTING
    if (process_thread_test_should_fail(2U))
    {
        status = SYNTRA_STATUS_ERROR;
        goto rollback;
    }
#endif

    allocation_owner.slot = reserved_handle.slot;
    allocation_owner.generation = reserved_handle.generation;

    status = memory_bind_staged_allocation(allocation, allocation_owner);
    if (status != SYNTRA_STATUS_OK)
        goto rollback;
    allocation_reserved = 1;
#ifdef SYNTRA_HOST_TESTING
    if (process_thread_test_should_fail(3U))
    {
        status = SYNTRA_STATUS_ERROR;
        goto rollback;
    }
#endif

    status = scheduler_attach_owned_stack(reserved_handle, allocation);
    if (status != SYNTRA_STATUS_OK)
        goto rollback;
#ifdef SYNTRA_HOST_TESTING
    if (process_thread_test_should_fail(4U))
    {
        status = SYNTRA_STATUS_ERROR;
        goto rollback;
    }
#endif

    status = memory_commit_thread_allocation(allocation, allocation_owner);
    if (status != SYNTRA_STATUS_OK)
        goto rollback;
    allocation_committed = 1;
#ifdef SYNTRA_HOST_TESTING
    if (process_thread_test_should_fail(5U))
    {
        status = SYNTRA_STATUS_ERROR;
        goto rollback;
    }
#endif

    status = scheduler_publish_owned_thread(reserved_handle);
    if (status != SYNTRA_STATUS_OK)
        goto rollback;

    *handle_out = reserved_handle;
    syntra_irq_restore(irq);
    return SYNTRA_STATUS_OK;

rollback:
    /*
     * Never release an allocation while its reservation may still exist.
     * If exact TCB abort fails, leave the allocation quarantined.
     */
    if (reservation_created)
        cleanup_status = scheduler_abort_reserved_thread(reserved_handle);

    if (cleanup_status == SYNTRA_STATUS_OK && allocation_created)
    {
        if (allocation_committed)
            cleanup_status = memory_release_owned_allocation(
                allocation, allocation_owner);
        else if (allocation_reserved)
            cleanup_status = memory_abort_reserved_allocation(
                allocation, allocation_owner);
        else
            cleanup_status = memory_abort_staged_allocation(allocation);
    }

    syntra_irq_restore(irq);
    if (cleanup_status != SYNTRA_STATUS_OK)
        return SYNTRA_STATUS_ERROR;
    return status;
}

/*
 * Canonical lifecycle coordinator. The scheduler cascade runs before the
 * process state transition while local IRQs are masked. The scheduler helper
 * validates readiness before mutating and has no failure path after that
 * validation, so an invalid handle or unavailable scheduler changes nothing.
 * Calling this again for an already-terminated process still repairs any
 * active owned threads left by metadata-only legacy state APIs.
 */
syntra_status_t process_thread_terminate_process(
    syntra_process_handle_t owner)
{
    syntra_irq_state_t irq;
    syntra_process_t process;
    syntra_status_t status;

    if (!process_thread_handle_is_structurally_valid(owner))
        return SYNTRA_STATUS_INVALID_ARGUMENT;

    irq = syntra_irq_save();
    status = process_get_snapshot_by_handle(owner, &process);
    if (status == SYNTRA_STATUS_OK)
        status = scheduler_terminate_process_threads(owner);
    if (status == SYNTRA_STATUS_OK && process.state != PROCESS_TERMINATED)
        status = process_set_state_by_handle(owner, PROCESS_TERMINATED);
    syntra_irq_restore(irq);
    return status;
}

syntra_status_t process_thread_get_count(
    syntra_process_handle_t owner, unsigned int *count_out)
{
    syntra_irq_state_t irq;
    syntra_process_t process;
    syntra_status_t status;

    if (!process_thread_handle_is_structurally_valid(owner) ||
        count_out == NULL)
        return SYNTRA_STATUS_INVALID_ARGUMENT;

    irq = syntra_irq_save();
    status = process_get_snapshot_by_handle(owner, &process);
    if (status == SYNTRA_STATUS_OK)
        status = scheduler_get_process_thread_count(owner, count_out);
    syntra_irq_restore(irq);
    return status;
}

syntra_status_t process_thread_get_snapshot(
    syntra_process_handle_t owner, unsigned int index,
    syntra_thread_t *snapshot)
{
    syntra_irq_state_t irq;
    syntra_process_t process;
    syntra_status_t status;

    if (!process_thread_handle_is_structurally_valid(owner) ||
        snapshot == NULL)
        return SYNTRA_STATUS_INVALID_ARGUMENT;

    irq = syntra_irq_save();
    status = process_get_snapshot_by_handle(owner, &process);
    if (status == SYNTRA_STATUS_OK)
        status = scheduler_get_process_thread_snapshot(owner, index, snapshot);
    syntra_irq_restore(irq);
    return status;
}
