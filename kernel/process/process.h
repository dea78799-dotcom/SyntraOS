
#ifndef SYNTRA_PROCESS_H
#define SYNTRA_PROCESS_H

#include "../core/status.h"

#define SYNTRA_MAX_PROCESSES 64

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
} syntra_process_t;

/* Initialize process manager */
syntra_status_t process_init(void);

/* Create a process */
syntra_status_t process_create(
    unsigned int process_id,
    unsigned int priority
);

/* Find a process */
syntra_process_t *process_get(
    unsigned int process_id
);

/* Change process state */
syntra_status_t process_set_state(
    unsigned int process_id,
    syntra_process_state_t state
);

/* Terminate a process */
syntra_status_t process_terminate(
    unsigned int process_id
);

/* Get process count */
unsigned int process_get_count(void);

#endif
