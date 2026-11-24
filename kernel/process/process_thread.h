#ifndef SYNTRA_PROCESS_THREAD_H
#define SYNTRA_PROCESS_THREAD_H

#include <stddef.h>
#include "../core/status.h"
#include "process_handle.h"
#include "../scheduler/scheduler.h"

/*
 * Coordinated process-owned thread creation.
 * Stacks and CPU contexts remain caller-owned in this phase.
 */
syntra_status_t process_thread_create_kernel(
    syntra_process_handle_t owner,
    unsigned int thread_id,
    unsigned int priority,
    syntra_thread_entry_t entry,
    void *argument,
    void *kernel_stack,
    size_t kernel_stack_size
);

syntra_status_t process_thread_create_user(
    syntra_process_handle_t owner,
    unsigned int thread_id,
    unsigned int priority,
    syntra_thread_entry_t entry,
    void *argument,
    void *kernel_stack,
    size_t kernel_stack_size,
    void *user_stack,
    size_t user_stack_size,
    void *cpu_context
);

/*
 * Terminate a process and all scheduler threads carrying its exact handle.
 * This is the canonical process-termination path. It does not reclaim stacks,
 * remove TCBs, or make process slots reusable.
 */
syntra_status_t process_thread_terminate_process(
    syntra_process_handle_t owner
);

syntra_status_t process_thread_get_count(
    syntra_process_handle_t owner,
    unsigned int *count_out
);

syntra_status_t process_thread_get_snapshot(
    syntra_process_handle_t owner,
    unsigned int index,
    syntra_thread_t *snapshot
);

#endif
