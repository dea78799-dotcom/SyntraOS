#ifndef SYNTRA_PROCESS_H
#define SYNTRA_PROCESS_H

#include <stdint.h>
#include "../core/status.h"
#include "process_handle.h"

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

/* Initialize process manager. */
syntra_status_t process_init(void);

/* Create a process record and return its generation-aware handle. */
syntra_status_t process_create_with_handle(
    unsigned int process_id,
    unsigned int priority,
    syntra_process_handle_t *handle
);

/* Legacy creation API using caller-supplied IDs. */
syntra_status_t process_create(
    unsigned int process_id,
    unsigned int priority
);

/* Resolve a legacy ID to its generation-aware identity. */
syntra_status_t process_get_handle(
    unsigned int process_id,
    syntra_process_handle_t *handle
);

/* Read or update metadata using a generation-aware identity. */
syntra_status_t process_get_snapshot_by_handle(
    syntra_process_handle_t handle,
    syntra_process_t *snapshot
);

syntra_status_t process_set_state_by_handle(
    syntra_process_handle_t handle,
    syntra_process_state_t state
);

/* Legacy ID-based snapshot API. */
syntra_status_t process_get_snapshot(
    unsigned int process_id,
    syntra_process_t *snapshot
);

/* Legacy mutable-pointer accessor; new code must not use it. */
syntra_process_t *process_get(unsigned int process_id);

/* Legacy ID-based state APIs. */
syntra_status_t process_set_state(
    unsigned int process_id,
    syntra_process_state_t state
);

syntra_status_t process_terminate(unsigned int process_id);
unsigned int process_get_count(void);

#endif
