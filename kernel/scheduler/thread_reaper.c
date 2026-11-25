#include "thread_reaper.h"
#include "scheduler_reaper_internal.h"

#include "../arch/irq.h"

static syntra_reaper_candidate_t candidates[
    SYNTRA_REAPER_CANDIDATE_CAPACITY
];
static unsigned int candidate_count;
static syntra_reaper_stats_t reaper_stats;
static int reaper_initialized;
static int execution_context_bound;
static syntra_thread_handle_t execution_context_handle;
static uintptr_t execution_stack_base;
static size_t execution_stack_size;

static uintptr_t thread_reaper_read_stack_pointer(void)
{
#if defined(__aarch64__)
    uintptr_t stack_pointer;
    __asm__ volatile("mov %0, sp" : "=r"(stack_pointer));
    return stack_pointer;
#else
    volatile uintptr_t marker = 0U;
    return (uintptr_t)&marker;
#endif
}

static int thread_reaper_is_on_bound_context_unlocked(void)
{
    syntra_thread_handle_t current_handle;
    uintptr_t stack_pointer;
    uintptr_t stack_top;

    if (!execution_context_bound)
        return 1;

    if (execution_stack_base > UINTPTR_MAX - execution_stack_size)
        return 0;
    stack_top = execution_stack_base + execution_stack_size;

    if (scheduler_get_current_handle(&current_handle) != SYNTRA_STATUS_OK ||
        !scheduler_thread_handle_equal(
            current_handle, execution_context_handle))
        return 0;

    stack_pointer = thread_reaper_read_stack_pointer();
    if (stack_pointer < execution_stack_base || stack_pointer >= stack_top)
        return 0;
#if defined(__aarch64__)
    if ((stack_pointer & 15U) != 0U)
        return 0;
#endif
    return 1;
}


static int reaper_find_available_slot(
    unsigned int *slot_out, int *append_out)
{
    if (slot_out == NULL || append_out == NULL)
        return 0;

    for (unsigned int i = 0U; i < candidate_count; ++i)
    {
        if (candidates[i].state == SYNTRA_REAPER_CANDIDATE_FREE ||
            candidates[i].state == SYNTRA_REAPER_CANDIDATE_RETIRED)
        {
            *slot_out = i;
            *append_out = 0;
            return 1;
        }
    }

    if (candidate_count < SYNTRA_REAPER_CANDIDATE_CAPACITY)
    {
        *slot_out = candidate_count;
        *append_out = 1;
        return 1;
    }

    return 0;
}

static void reaper_clear_candidate(unsigned int index)
{
    candidates[index].proof.handle.slot = 0U;
    candidates[index].proof.handle.generation = 0U;
    candidates[index].proof.sequence = 0U;
    candidates[index].proof.terminal = 0U;
    candidates[index].state = SYNTRA_REAPER_CANDIDATE_FREE;
    candidates[index].reason = SYNTRA_REAPER_REASON_NONE;
}

static int reaper_find_handle(syntra_thread_handle_t handle)
{
    for (unsigned int i = 0U; i < candidate_count; ++i)
    {
        if (candidates[i].state != SYNTRA_REAPER_CANDIDATE_FREE &&
            scheduler_thread_handle_equal(
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
    reaper_stats.execution_context_bound = 0U;
    reaper_stats.execution_context_dispatches = 0U;
    reaper_stats.execution_context_rejections = 0U;
    reaper_stats.reclaimed = 0U;
    reaper_stats.reclaim_failures = 0U;
    reaper_stats.retired = 0U;
    reaper_stats.retirement_failures = 0U;
    execution_context_bound = 0;
    execution_context_handle.slot = 0U;
    execution_context_handle.generation = 0U;
    execution_stack_base = 0U;
    execution_stack_size = 0U;

    for (unsigned int i = 0U; i < SYNTRA_REAPER_CANDIDATE_CAPACITY; ++i)
    {
        candidates[i].proof.handle.slot = 0U;
        candidates[i].proof.handle.generation = 0U;
        candidates[i].proof.sequence = 0U;
        candidates[i].proof.terminal = 0U;
        candidates[i].state = SYNTRA_REAPER_CANDIDATE_FREE;
        candidates[i].reason = SYNTRA_REAPER_REASON_NONE;
    }

    reaper_initialized = 1;
    syntra_irq_restore(irq);
    return SYNTRA_STATUS_OK;
}

syntra_status_t thread_reaper_bind_execution_context(
    syntra_thread_handle_t handle,
    uintptr_t stack_base,
    size_t stack_size)
{
    syntra_irq_state_t irq;
    syntra_thread_t snapshot;
    uintptr_t stack_top;
    uintptr_t context_address;
    syntra_status_t status;

    if (handle.generation == 0U || stack_base == 0U ||
        stack_size < SYNTRA_THREAD_MIN_STACK_SIZE ||
        (stack_base & (SYNTRA_THREAD_STACK_ALIGNMENT - 1U)) != 0U ||
        stack_base > UINTPTR_MAX - stack_size)
        return SYNTRA_STATUS_INVALID_ARGUMENT;

    stack_top = stack_base + stack_size;
    irq = syntra_irq_save();
    if (!reaper_initialized || execution_context_bound)
    {
        syntra_irq_restore(irq);
        return SYNTRA_STATUS_ERROR;
    }

    status = scheduler_get_thread_snapshot_by_handle(handle, &snapshot);
    if (status != SYNTRA_STATUS_OK ||
        snapshot.state != THREAD_READY ||
        snapshot.entry == NULL ||
        snapshot.cpu_context == NULL ||
        snapshot.stack_base != stack_base ||
        snapshot.stack_size != stack_size ||
        snapshot.stack_ownership != SYNTRA_THREAD_STACK_BORROWED ||
        snapshot.is_user_thread != 0U ||
        snapshot.user_stack_size != 0U)
    {
        syntra_irq_restore(irq);
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    context_address = (uintptr_t)snapshot.cpu_context;
    if (context_address < stack_base || context_address >= stack_top)
    {
        syntra_irq_restore(irq);
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    execution_context_handle = handle;
    execution_stack_base = stack_base;
    execution_stack_size = stack_size;
    execution_context_bound = 1;
    reaper_stats.execution_context_bound = 1U;

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
    if (!thread_reaper_is_on_bound_context_unlocked())
    {
        ++reaper_stats.execution_context_rejections;
        *processed_out = 0U;
        syntra_irq_restore(irq);
        return SYNTRA_STATUS_ERROR;
    }
    if (execution_context_bound)
        ++reaper_stats.execution_context_dispatches;

    while (processed < max_events)
    {
        syntra_thread_switch_event_t event;
        syntra_thread_t snapshot;
        syntra_reaper_candidate_t previous_candidate;
        syntra_status_t status;
        int existing_index;
        int candidate_inserted = 0;
        int candidate_modified = 0;
        int candidate_appended = 0;
        unsigned int candidate_index = 0U;

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
        else if (event.terminal == 0U)
        {
            /*
             * Nonterminal switch events do not consume candidate capacity.
             * The event's captured intent is authoritative even if the TCB
             * has since transitioned to TERMINATED.
             */
            status = scheduler_ack_switch_away_event(event);
            if (status != SYNTRA_STATUS_OK)
            {
                *processed_out = processed;
                syntra_irq_restore(irq);
                return status;
            }
            ++reaper_stats.discarded_nonterminal;
            ++reaper_stats.acknowledged_events;
            ++processed;
            continue;
        }
        else
        {
            if (!reaper_find_available_slot(
                    &candidate_index, &candidate_appended))
                break; /* Preserve the FIFO head when no safe record exists. */

            previous_candidate = candidates[candidate_index];
            if (candidate_appended)
                ++candidate_count;

            candidates[candidate_index].proof = event;
            candidates[candidate_index].state =
                SYNTRA_REAPER_CANDIDATE_QUEUED;
            candidates[candidate_index].reason =
                SYNTRA_REAPER_REASON_NONE;
            candidate_inserted = 1;

            status = scheduler_get_thread_snapshot_by_handle(
                event.handle, &snapshot);
            if (status != SYNTRA_STATUS_OK)
            {
                candidates[candidate_index].state =
                    SYNTRA_REAPER_CANDIDATE_QUARANTINED;
                candidates[candidate_index].reason =
                    SYNTRA_REAPER_REASON_STALE_HANDLE;
            }
            else if (snapshot.state != THREAD_TERMINATED)
            {
                candidates[candidate_index].state =
                    SYNTRA_REAPER_CANDIDATE_QUARANTINED;
                candidates[candidate_index].reason =
                    SYNTRA_REAPER_REASON_TERMINAL_STATE_MISMATCH;
            }
        }

        status = scheduler_ack_switch_away_event(event);
        if (status != SYNTRA_STATUS_OK)
        {
            if (candidate_inserted)
            {
                if (candidate_appended)
                {
                    --candidate_count;
                    reaper_clear_candidate(candidate_index);
                }
                else
                    candidates[candidate_index] = previous_candidate;
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
            if (candidates[candidate_index].state ==
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


syntra_status_t thread_reaper_reclaim_candidate(unsigned int index)
{
    syntra_irq_state_t irq;
    syntra_reaper_candidate_t *candidate;
    syntra_status_t status;

    irq = syntra_irq_save();
    if (!reaper_initialized || !execution_context_bound ||
        !thread_reaper_is_on_bound_context_unlocked())
    {
        ++reaper_stats.execution_context_rejections;
        syntra_irq_restore(irq);
        return SYNTRA_STATUS_PERMISSION_DENIED;
    }
    if (index >= candidate_count)
    {
        syntra_irq_restore(irq);
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    candidate = &candidates[index];
    if (candidate->state != SYNTRA_REAPER_CANDIDATE_QUEUED ||
        candidate->proof.terminal == 0U ||
        candidate->proof.sequence == 0U ||
        candidate->proof.handle.generation == 0U)
    {
        syntra_irq_restore(irq);
        return SYNTRA_STATUS_PERMISSION_DENIED;
    }

    status = scheduler_reclaim_owned_thread_stack(
        candidate->proof.handle, candidate->proof.sequence);
    if (status == SYNTRA_STATUS_OK)
    {
        candidate->state = SYNTRA_REAPER_CANDIDATE_RECLAIMED;
        candidate->reason = SYNTRA_REAPER_REASON_NONE;
        if (reaper_stats.queued > 0U)
            --reaper_stats.queued;
        ++reaper_stats.reclaimed;
    }
    else
    {
        candidate->state = SYNTRA_REAPER_CANDIDATE_QUARANTINED;
        candidate->reason = SYNTRA_REAPER_REASON_RESOURCE_RECLAIM_FAILED;
        if (reaper_stats.queued > 0U)
            --reaper_stats.queued;
        ++reaper_stats.quarantined;
        ++reaper_stats.reclaim_failures;
    }

    syntra_irq_restore(irq);
    return status;
}


/*
 * Retire only after stack reclamation has cleared every TCB resource
 * reference. A failed scheduler gate leaves the RECLAIMED candidate intact
 * so diagnostics are preserved and a transient FIFO condition can be retried.
 */
syntra_status_t thread_reaper_retire_candidate(unsigned int index)
{
    syntra_irq_state_t irq;
    syntra_reaper_candidate_t *candidate;
    syntra_status_t status;

    irq = syntra_irq_save();
    if (!reaper_initialized || !execution_context_bound ||
        !thread_reaper_is_on_bound_context_unlocked())
    {
        ++reaper_stats.execution_context_rejections;
        syntra_irq_restore(irq);
        return SYNTRA_STATUS_PERMISSION_DENIED;
    }
    if (index >= candidate_count)
    {
        syntra_irq_restore(irq);
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    candidate = &candidates[index];
    if (candidate->state != SYNTRA_REAPER_CANDIDATE_RECLAIMED ||
        candidate->proof.terminal == 0U ||
        candidate->proof.sequence == 0U ||
        candidate->proof.handle.generation == 0U)
    {
        syntra_irq_restore(irq);
        return SYNTRA_STATUS_PERMISSION_DENIED;
    }

    status = scheduler_retire_thread_after_reap(
        candidate->proof.handle, candidate->proof.sequence);
    if (status == SYNTRA_STATUS_OK)
    {
        candidate->state = SYNTRA_REAPER_CANDIDATE_RETIRED;
        candidate->reason = SYNTRA_REAPER_REASON_NONE;
        ++reaper_stats.retired;
    }
    else
        ++reaper_stats.retirement_failures;

    syntra_irq_restore(irq);
    return status;
}


#ifdef SYNTRA_HOST_TESTING
/* Seed a fully quarantined table to exercise the no-safe-slot path. */
syntra_status_t thread_reaper_test_fill_quarantined(void)
{
    syntra_irq_state_t irq = syntra_irq_save();

    if (!reaper_initialized)
    {
        syntra_irq_restore(irq);
        return SYNTRA_STATUS_ERROR;
    }

    candidate_count = SYNTRA_REAPER_CANDIDATE_CAPACITY;
    for (unsigned int i = 0U; i < SYNTRA_REAPER_CANDIDATE_CAPACITY; ++i)
    {
        candidates[i].proof.handle.slot = SYNTRA_MAX_THREADS + i;
        candidates[i].proof.handle.generation = 1U;
        candidates[i].proof.sequence = (uint64_t)i + 1U;
        candidates[i].proof.terminal = 1U;
        candidates[i].state = SYNTRA_REAPER_CANDIDATE_QUARANTINED;
        candidates[i].reason = SYNTRA_REAPER_REASON_STALE_HANDLE;
    }
    reaper_stats.queued = 0U;
    reaper_stats.quarantined = SYNTRA_REAPER_CANDIDATE_CAPACITY;

    syntra_irq_restore(irq);
    return SYNTRA_STATUS_OK;
}
#endif

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
