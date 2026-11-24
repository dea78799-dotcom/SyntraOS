#include "thread_reaper.h"

#include "../arch/irq.h"

static syntra_reaper_candidate_t candidates[
    SYNTRA_REAPER_CANDIDATE_CAPACITY
];
static unsigned int candidate_count;
static syntra_reaper_stats_t reaper_stats;
static int reaper_initialized;

static int reaper_find_handle(syntra_thread_handle_t handle)
{
    for (unsigned int i = 0U; i < candidate_count; ++i)
    {
        if (scheduler_thread_handle_equal(
                candidates[i].proof.handle, handle))
            return (int)i;
    }
    return -1;
}

syntra_status_t thread_reaper_init(void)
{
    syntra_irq_state_t irq = syntra_irq_save();

    candidate_count = 0U;
    reaper_stats.queued = 0U;
    reaper_stats.quarantined = 0U;
    reaper_stats.discarded_nonterminal = 0U;
    reaper_stats.duplicate_events = 0U;
    reaper_stats.acknowledged_events = 0U;

    for (unsigned int i = 0U; i < SYNTRA_REAPER_CANDIDATE_CAPACITY; ++i)
    {
        candidates[i].proof.handle.slot = 0U;
        candidates[i].proof.handle.generation = 0U;
        candidates[i].proof.sequence = 0U;
        candidates[i].state = SYNTRA_REAPER_CANDIDATE_FREE;
        candidates[i].reason = SYNTRA_REAPER_REASON_NONE;
    }

    reaper_initialized = 1;
    syntra_irq_restore(irq);
    return SYNTRA_STATUS_OK;
}

syntra_status_t thread_reaper_dispatch(
    unsigned int max_events,
    unsigned int *processed_out)
{
    syntra_irq_state_t irq;
    unsigned int processed = 0U;

    if (processed_out == NULL)
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    *processed_out = 0U;

    irq = syntra_irq_save();
    if (!reaper_initialized)
    {
        syntra_irq_restore(irq);
        return SYNTRA_STATUS_ERROR;
    }

    while (processed < max_events)
    {
        syntra_thread_switch_event_t event;
        syntra_thread_t snapshot;
        syntra_reaper_candidate_t previous_candidate;
        syntra_status_t status;
        int existing_index;
        int candidate_inserted = 0;
        int candidate_modified = 0;

        /*
         * Reserve candidate capacity before inspecting/removing the FIFO
         * head. A full candidate queue therefore cannot lose proof events.
         */
        if (candidate_count >= SYNTRA_REAPER_CANDIDATE_CAPACITY)
            break;

        status = scheduler_peek_switch_away_event(&event);
        if (status != SYNTRA_STATUS_OK)
            break; /* Empty FIFO is a normal no-work condition. */

        existing_index = reaper_find_handle(event.handle);
        if (existing_index >= 0)
        {
            syntra_reaper_candidate_t *existing =
                &candidates[(unsigned int)existing_index];

            if (existing->proof.sequence == event.sequence &&
                existing->proof.terminal == event.terminal)
            {
                status = scheduler_ack_switch_away_event(event);
                if (status != SYNTRA_STATUS_OK)
                {
                    *processed_out = processed;
                    syntra_irq_restore(irq);
                    return status;
                }
                ++reaper_stats.duplicate_events;
                ++reaper_stats.acknowledged_events;
                ++processed;
                continue;
            }

            previous_candidate = *existing;
            existing->state = SYNTRA_REAPER_CANDIDATE_QUARANTINED;
            existing->reason =
                SYNTRA_REAPER_REASON_SEQUENCE_CONFLICT;
            candidate_modified = 1;
        }
        else
        {
            syntra_reaper_candidate_t *candidate =
                &candidates[candidate_count];

            candidate->proof = event;
            candidate->state = SYNTRA_REAPER_CANDIDATE_QUEUED;
            candidate->reason = SYNTRA_REAPER_REASON_NONE;
            ++candidate_count;
            candidate_inserted = 1;

            if (event.terminal == 0U)
            {
                /*
                 * The thread may have terminated after this older preemption
                 * event was published. Event-time intent, not current TCB
                 * state, decides whether this proof is a reap candidate.
                 */
                --candidate_count;
                candidate->state = SYNTRA_REAPER_CANDIDATE_FREE;
                candidate->proof.handle.slot = 0U;
                candidate->proof.handle.generation = 0U;
                candidate->proof.sequence = 0U;
                candidate->proof.terminal = 0U;
                candidate->reason = SYNTRA_REAPER_REASON_NONE;
                candidate_inserted = 0;
            }
            else
            {
                status = scheduler_get_thread_snapshot_by_handle(
                    event.handle, &snapshot);
                if (status != SYNTRA_STATUS_OK)
                {
                    candidate->state = SYNTRA_REAPER_CANDIDATE_QUARANTINED;
                    candidate->reason = SYNTRA_REAPER_REASON_STALE_HANDLE;
                }
                else if (snapshot.state != THREAD_TERMINATED)
                {
                    candidate->state = SYNTRA_REAPER_CANDIDATE_QUARANTINED;
                    candidate->reason =
                        SYNTRA_REAPER_REASON_TERMINAL_STATE_MISMATCH;
                }
            }
        }

        status = scheduler_ack_switch_away_event(event);
        if (status != SYNTRA_STATUS_OK)
        {
            if (candidate_inserted)
            {
                --candidate_count;
                candidates[candidate_count].proof.handle.slot = 0U;
                candidates[candidate_count].proof.handle.generation = 0U;
                candidates[candidate_count].proof.sequence = 0U;
                candidates[candidate_count].state =
                    SYNTRA_REAPER_CANDIDATE_FREE;
                candidates[candidate_count].reason =
                    SYNTRA_REAPER_REASON_NONE;
            }
            else if (candidate_modified)
            {
                candidates[(unsigned int)existing_index] =
                    previous_candidate;
            }
            *processed_out = processed;
            syntra_irq_restore(irq);
            return status;
        }

        if (candidate_inserted)
        {
            if (candidates[candidate_count - 1U].state ==
                SYNTRA_REAPER_CANDIDATE_QUEUED)
                ++reaper_stats.queued;
            else
                ++reaper_stats.quarantined;
        }
        else if (candidate_modified)
        {
            if (previous_candidate.state == SYNTRA_REAPER_CANDIDATE_QUEUED)
            {
                if (reaper_stats.queued > 0U)
                    --reaper_stats.queued;
                ++reaper_stats.quarantined;
            }
        }
        else
            ++reaper_stats.discarded_nonterminal;

        ++reaper_stats.acknowledged_events;
        ++processed;
    }

    *processed_out = processed;
    syntra_irq_restore(irq);
    return SYNTRA_STATUS_OK;
}

syntra_status_t thread_reaper_get_candidate_count(
    unsigned int *count_out)
{
    syntra_irq_state_t irq;

    if (count_out == NULL)
        return SYNTRA_STATUS_INVALID_ARGUMENT;

    irq = syntra_irq_save();
    if (!reaper_initialized)
    {
        syntra_irq_restore(irq);
        return SYNTRA_STATUS_ERROR;
    }
    *count_out = candidate_count;
    syntra_irq_restore(irq);
    return SYNTRA_STATUS_OK;
}

syntra_status_t thread_reaper_get_candidate(
    unsigned int index,
    syntra_reaper_candidate_t *candidate_out)
{
    syntra_irq_state_t irq;

    if (candidate_out == NULL)
        return SYNTRA_STATUS_INVALID_ARGUMENT;

    irq = syntra_irq_save();
    if (!reaper_initialized || index >= candidate_count)
    {
        syntra_irq_restore(irq);
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }
    *candidate_out = candidates[index];
    syntra_irq_restore(irq);
    return SYNTRA_STATUS_OK;
}

syntra_status_t thread_reaper_get_stats(
    syntra_reaper_stats_t *stats_out)
{
    syntra_irq_state_t irq;

    if (stats_out == NULL)
        return SYNTRA_STATUS_INVALID_ARGUMENT;

    irq = syntra_irq_save();
    if (!reaper_initialized)
    {
        syntra_irq_restore(irq);
        return SYNTRA_STATUS_ERROR;
    }
    *stats_out = reaper_stats;
    syntra_irq_restore(irq);
    return SYNTRA_STATUS_OK;
}
