#ifndef SYNTRA_SCHEDULER_H
#define SYNTRA_SCHEDULER_H

#include <stddef.h>
#include <stdint.h>

#include "../core/status.h"

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
} syntra_thread_t;

syntra_status_t scheduler_init(void);

/* Metadata-only compatibility API, useful for scheduler policy tests. */
syntra_status_t scheduler_add_thread(
    unsigned int thread_id,
    unsigned int priority
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

/*
 * Roll back a context selection when architecture activation fails.
 * previous_thread_id may name a blocked bootstrap thread, or be zero when
 * the interrupted thread has already exited. selected_thread_id is zero only
 * when the scheduler found no runnable context.
 */
syntra_status_t scheduler_rollback_context_switch(
    unsigned int previous_thread_id,
    unsigned int selected_thread_id);

syntra_status_t scheduler_set_thread_state(
    unsigned int thread_id,
    thread_state_t new_state
);

syntra_status_t scheduler_wake_thread(unsigned int thread_id);
syntra_status_t scheduler_block_current(void);
syntra_status_t scheduler_terminate_current(void);

/*
 * Release a TERMINATED thread's Scheduler slot after all external owners
 * (for example, the Process Manager) have dropped references to its ID.
 * Caller-owned stack storage is not freed; it may be reused after success.
 * The currently selected slot can never be reaped.
 */
syntra_status_t scheduler_reap_terminated(unsigned int thread_id);

syntra_thread_t *scheduler_get_thread(unsigned int thread_id);
/* Number of registered records, including terminated records not yet reaped. */
unsigned int scheduler_get_thread_count(void);
syntra_status_t scheduler_validate(void);

#endif
