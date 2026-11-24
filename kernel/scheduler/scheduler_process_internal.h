#ifndef SYNTRA_SCHEDULER_PROCESS_INTERNAL_H
#define SYNTRA_SCHEDULER_PROCESS_INTERNAL_H

#include "scheduler.h"

/*
 * Private Process/Thread coordinator boundary.
 * Do not call these functions from drivers or ordinary kernel clients.
 */
syntra_status_t scheduler_create_owned_thread_with_handle(
    syntra_process_handle_t owner, unsigned int thread_id,
    unsigned int priority, syntra_thread_entry_t entry, void *argument,
    void *stack, size_t stack_size, syntra_thread_handle_t *handle_out);

syntra_status_t scheduler_create_owned_user_thread_with_handle(
    syntra_process_handle_t owner, unsigned int thread_id,
    unsigned int priority, syntra_thread_entry_t entry, void *argument,
    void *kernel_stack, size_t kernel_stack_size,
    void *user_stack, size_t user_stack_size, void *cpu_context,
    syntra_thread_handle_t *handle_out);

syntra_status_t scheduler_terminate_process_threads(
    syntra_process_handle_t owner);

syntra_status_t scheduler_get_process_thread_count(
    syntra_process_handle_t owner, unsigned int *count_out);

syntra_status_t scheduler_get_process_thread_snapshot(
    syntra_process_handle_t owner, unsigned int index,
    syntra_thread_t *snapshot);

#endif
