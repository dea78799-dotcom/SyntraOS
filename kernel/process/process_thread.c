#include "process_thread.h"
#include "../scheduler/scheduler_process_internal.h"
#include "process.h"
#include "../arch/irq.h"

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
