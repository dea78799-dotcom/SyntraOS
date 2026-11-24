#ifndef SYNTRA_THREAD_REAPER_H
#define SYNTRA_THREAD_REAPER_H

#include "scheduler.h"

#define SYNTRA_REAPER_CANDIDATE_CAPACITY SYNTRA_MAX_THREADS

typedef enum
{
    SYNTRA_REAPER_CANDIDATE_FREE = 0,
    SYNTRA_REAPER_CANDIDATE_QUEUED = 1,
    SYNTRA_REAPER_CANDIDATE_QUARANTINED = 2
} syntra_reaper_candidate_state_t;

typedef enum
{
    SYNTRA_REAPER_REASON_NONE = 0,
    SYNTRA_REAPER_REASON_STALE_HANDLE = 1,
    SYNTRA_REAPER_REASON_SEQUENCE_CONFLICT = 2,
    SYNTRA_REAPER_REASON_TERMINAL_STATE_MISMATCH = 3
} syntra_reaper_candidate_reason_t;

/*
 * Identity/proof only. This record deliberately stores no TCB pointer,
 * stack address, allocation token, or allocator-owned resource.
 */
typedef struct
{
    syntra_thread_switch_event_t proof;
    syntra_reaper_candidate_state_t state;
    syntra_reaper_candidate_reason_t reason;
} syntra_reaper_candidate_t;

typedef struct
{
    unsigned int queued;
    unsigned int quarantined;
    unsigned int discarded_nonterminal;
    unsigned int duplicate_events;
    unsigned int acknowledged_events;
} syntra_reaper_stats_t;

/* Initialize after scheduler_init(); no resource reclamation is enabled. */
syntra_status_t thread_reaper_init(void);

/*
 * Move at most max_events proof records from the scheduler FIFO into the
 * bounded candidate queue. If the queue is full, the FIFO head is preserved.
 * This function never releases memory or mutates a TCB.
 */
syntra_status_t thread_reaper_dispatch(
    unsigned int max_events,
    unsigned int *processed_out
);

syntra_status_t thread_reaper_get_candidate_count(
    unsigned int *count_out
);

syntra_status_t thread_reaper_get_candidate(
    unsigned int index,
    syntra_reaper_candidate_t *candidate_out
);

syntra_status_t thread_reaper_get_stats(
    syntra_reaper_stats_t *stats_out
);

#endif
