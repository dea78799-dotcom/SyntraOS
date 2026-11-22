#ifndef SYNTRA_PROCESS_H
#define SYNTRA_PROCESS_H

#include "../core/status.h"
typedef struct syntra_arm64_address_space syntra_arm64_address_space_t;

#define SYNTRA_MAX_PROCESSES 64U
#define SYNTRA_MAX_PROCESS_THREADS 32U

typedef enum
{
    PROCESS_READY,
    PROCESS_RUNNING,
    PROCESS_BLOCKED,
    PROCESS_TERMINATED
} syntra_process_state_t;

typedef struct
{
    unsigned int process_id;
    unsigned int priority;
    syntra_process_state_t state;
    unsigned int thread_ids[SYNTRA_MAX_PROCESS_THREADS];
    unsigned int thread_count;
    syntra_arm64_address_space_t *address_space;
} syntra_process_t;

/* Initialize process manager. */
syntra_status_t process_init(void);

/* Create a metadata process. */
syntra_status_t process_create(
    unsigned int process_id,
    unsigned int priority
);

/* Attach and transfer ownership of a validated, kernel-mapped address space. */
syntra_status_t process_attach_address_space(
    unsigned int process_id,
    syntra_arm64_address_space_t *address_space);

/* Resolve the address space owned by the process containing this thread. */
syntra_arm64_address_space_t *process_get_address_space_for_thread(
    unsigned int thread_id);

/* Refresh the owning process state after this thread changes scheduler state. */
syntra_status_t process_refresh_state_for_thread(unsigned int thread_id);

/* Find a process. */
syntra_process_t *process_get(unsigned int process_id);

/*
 * Associate an existing scheduler thread with exactly one process.
 * This records ownership only; it does not create or schedule the thread.
 */
syntra_status_t process_attach_thread(
    unsigned int process_id,
    unsigned int thread_id
);

/* Return the number of associated threads, or zero for an unknown process. */
unsigned int process_get_thread_count(unsigned int process_id);

/* Return a thread ID by index, or zero for an invalid process/index. */
unsigned int process_get_thread_id(
    unsigned int process_id,
    unsigned int index
);

/*
 * Recompute process state from associated scheduler threads.
 * State precedence: RUNNING, READY, BLOCKED, then TERMINATED.
 */
syntra_status_t process_refresh_state(unsigned int process_id);

/*
 * With attached threads, state changes are accepted only when they match
 * scheduler reality. This function never changes scheduler thread state.
 */
syntra_status_t process_set_state(
    unsigned int process_id,
    syntra_process_state_t state
);

/* A process with attached threads can terminate only after all are terminated.
 * Termination destroys an attached address space owned by the process manager. */
syntra_status_t process_terminate(unsigned int process_id);

/* Validate process table and process-to-thread ownership invariants. */
syntra_status_t process_validate(void);

/* Get process count. */
unsigned int process_get_count(void);

#endif
