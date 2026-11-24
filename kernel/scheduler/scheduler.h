#ifndef SYNTRA_SCHEDULER_H
#define SYNTRA_SCHEDULER_H

#include <stddef.h>
#include <stdint.h>

#include "../core/status.h"
#include "../process/process_handle.h"

#define SYNTRA_MAX_THREADS 32U
#define SYNTRA_THREAD_MIN_STACK_SIZE 1024U
#define SYNTRA_THREAD_STACK_ALIGNMENT 16U

typedef enum
{
    THREAD_READY = 0,
    THREAD_RUNNING,
    THREAD_BLOCKED,
    THREAD_TERMINATED
} thread_state_t;

typedef void (*syntra_thread_entry_t)(void *argument);

typedef struct
{
    uint32_t slot;
    uint32_t generation;
} syntra_thread_handle_t;

typedef struct
{
    /* Stable scheduler-slot identity; generation is nonzero when occupied. */
    syntra_thread_handle_t handle;
    unsigned int thread_id;
    unsigned int priority;
    thread_state_t state;

    /* Caller-owned stack and entry metadata; allocation belongs to Memory V1. */
    syntra_thread_entry_t entry;
    void *argument;
    uintptr_t stack_base;
    size_t stack_size;

    /* User threads keep an independent EL0 stack; stack_* remains EL1. */
    uintptr_t user_stack_base;
    size_t user_stack_size;
    uint8_t is_user_thread;

    /* Architecture layer stores the saved ARM64 exception frame here. */
    void *cpu_context;

    /* Exact process identity; zeroed when has_process_owner is false. */
    syntra_process_handle_t owner_process;
    uint8_t has_process_owner;
} syntra_thread_t;

syntra_status_t scheduler_init(void);

/* Metadata-only compatibility API, useful for scheduler policy tests. */
syntra_status_t scheduler_add_thread(
    unsigned int thread_id,
    unsigned int priority
);

/*
 * Handle-first create API. On success, returns the committed slot identity.
 * D1 is append-only: slots are not reused and generation is currently 1.
 */
syntra_status_t scheduler_create_thread_with_handle(
    unsigned int thread_id,
    unsigned int priority,
    syntra_thread_entry_t entry,
    void *argument,
    void *stack,
    size_t stack_size,
    syntra_thread_handle_t *handle_out
);

/* Create a runnable thread using a caller-provided, 16-byte aligned stack. */
syntra_status_t scheduler_create_thread(
    unsigned int thread_id,
    unsigned int priority,
    syntra_thread_entry_t entry,
    void *argument,
    void *stack,
    size_t stack_size
);

/*
 * Register a prepared User Mode thread. cpu_context must be a validated
 * architecture frame built on kernel_stack; this API never enters EL0.
 */
syntra_status_t scheduler_create_user_thread_with_handle(
    unsigned int thread_id, unsigned int priority,
    syntra_thread_entry_t entry, void *argument,
    void *kernel_stack, size_t kernel_stack_size,
    void *user_stack, size_t user_stack_size, void *cpu_context,
    syntra_thread_handle_t *handle_out
);

syntra_status_t scheduler_create_user_thread(
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

/* Select and mark the next READY thread using round-robin order. */
syntra_thread_t *scheduler_schedule_next(void);

/* Context-switch variant skips READY threads without saved CPU context. */
syntra_thread_t *scheduler_schedule_next_with_context(void);

/* Read-only peek at the next READY thread in round-robin order. */
syntra_thread_t *scheduler_get_next_ready(void);

syntra_thread_t *scheduler_get_current(void);

syntra_status_t scheduler_set_thread_state(
    unsigned int thread_id,
    thread_state_t new_state
);

syntra_status_t scheduler_wake_thread(unsigned int thread_id);
syntra_status_t scheduler_block_current(void);
syntra_status_t scheduler_terminate_current(void);

/*
 * Copy-out inspection APIs. The caller owns the destination snapshot; changes
 * to it never mutate scheduler state.
 */
syntra_status_t scheduler_get_thread_snapshot(
    unsigned int thread_id,
    syntra_thread_t *snapshot
);
syntra_status_t scheduler_get_current_snapshot(syntra_thread_t *snapshot);
syntra_status_t scheduler_get_current_handle(syntra_thread_handle_t *handle_out);

/* Exact-handle copy-out and state update APIs; stale handles are rejected. */
syntra_status_t scheduler_get_thread_snapshot_by_handle(
    syntra_thread_handle_t handle, syntra_thread_t *snapshot);
syntra_status_t scheduler_set_thread_state_by_handle(
    syntra_thread_handle_t handle, thread_state_t new_state);
int scheduler_thread_handle_equal(
    syntra_thread_handle_t left, syntra_thread_handle_t right);

/* Controlled context mutation; avoids exposing a mutable TCB to callers. */
syntra_status_t scheduler_set_thread_context(
    unsigned int thread_id,
    void *cpu_context
);

/* Select the next context-aware thread and return only its saved context. */
syntra_status_t scheduler_schedule_next_context(void **cpu_context_out);

/*
 * Legacy mutable-pointer accessors retained for compatibility only.
 * New kernel code must use snapshot and controlled mutation APIs above.
 */
syntra_thread_t *scheduler_get_thread(unsigned int thread_id);
unsigned int scheduler_get_thread_count(void);
syntra_status_t scheduler_validate(void);

#endif
