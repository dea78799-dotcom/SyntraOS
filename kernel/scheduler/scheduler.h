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

syntra_status_t scheduler_init(void);

syntra_status_t scheduler_add_thread(
    unsigned int thread_id,
    unsigned int priority
);

syntra_thread_t *scheduler_get_next_ready(void);

#endif
