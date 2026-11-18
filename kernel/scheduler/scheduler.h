#ifndef SYNTRA_SCHEDULER_H
#define SYNTRA_SCHEDULER_H

#include "../core/status.h"

#define SYNTRA_MAX_THREADS 32

typedef enum
{
    THREAD_READY,
    THREAD_RUNNING,
    THREAD_BLOCKED,
    THREAD_TERMINATED
} thread_state_t;

typedef struct
{
    unsigned int thread_id;
    unsigned int priority;
    thread_state_t state;
} syntra_thread_t;

/* Initialize scheduler */
syntra_status_t scheduler_init(void);

/* Register a new thread */
syntra_status_t scheduler_add_thread(
    unsigned int thread_id,
    unsigned int priority
);

/* Find highest-priority ready thread */
syntra_thread_t *scheduler_get_next_ready(void);

/* Update thread state */
syntra_status_t scheduler_set_thread_state(
    unsigned int thread_id,
    thread_state_t new_state
);

/* Get thread by ID */
syntra_thread_t *scheduler_get_thread(
    unsigned int thread_id
);

/* Get total thread count */
unsigned int scheduler_get_thread_count(void);

#endif
