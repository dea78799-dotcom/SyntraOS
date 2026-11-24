#include "process.h"
#include "../arch/irq.h"

typedef struct
{
    syntra_process_t process;
    uint32_t generation;
    int occupied;
} syntra_process_slot_t;

static syntra_process_slot_t process_slots[SYNTRA_MAX_PROCESSES];
static unsigned int process_count = 0U;
static int process_initialized = 0;

static int process_find_slot_by_id_unlocked(
    unsigned int process_id,
    unsigned int *slot_out)
{
    if (process_id == 0U || slot_out == 0)
        return 0;

    for (unsigned int i = 0U; i < SYNTRA_MAX_PROCESSES; ++i)
    {
        if (process_slots[i].occupied &&
            process_slots[i].process.process_id == process_id)
        {
            *slot_out = i;
            return 1;
        }
    }

    return 0;
}

static syntra_status_t process_init_unlocked(void)
{
    if (process_initialized)
        return SYNTRA_STATUS_ERROR;

    process_count = 0U;
    for (unsigned int i = 0U; i < SYNTRA_MAX_PROCESSES; ++i)
    {
        process_slots[i].generation = 1U;
        process_slots[i].occupied = 0;
        process_slots[i].process.process_id = 0U;
        process_slots[i].process.priority = 0U;
        process_slots[i].process.state = PROCESS_READY;
    }

    process_initialized = 1;
    return SYNTRA_STATUS_OK;
}

static syntra_status_t process_create_unlocked(
    unsigned int process_id,
    unsigned int priority)
{
    if (!process_initialized)
        return SYNTRA_STATUS_ERROR;

    if (process_id == 0U)
        return SYNTRA_STATUS_INVALID_ARGUMENT;

    if (process_count >= SYNTRA_MAX_PROCESSES)
        return SYNTRA_STATUS_OUT_OF_MEMORY;

    unsigned int existing_slot = 0U;
    if (process_find_slot_by_id_unlocked(process_id, &existing_slot))
        return SYNTRA_STATUS_ERROR;

    for (unsigned int i = 0U; i < SYNTRA_MAX_PROCESSES; ++i)
    {
        syntra_process_slot_t *slot = &process_slots[i];
        if (slot->occupied)
            continue;

        /* Generation starts at one. Reuse is not enabled in this phase. */
        if (slot->generation == 0U)
            return SYNTRA_STATUS_ERROR;

        slot->process.process_id = process_id;
        slot->process.priority = priority;
        slot->process.state = PROCESS_READY;
        slot->occupied = 1;
        process_count++;
        return SYNTRA_STATUS_OK;
    }

    return SYNTRA_STATUS_OUT_OF_MEMORY;
}

static syntra_status_t process_get_handle_unlocked(
    unsigned int process_id,
    syntra_process_handle_t *handle)
{
    if (handle == 0 || process_id == 0U)
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    if (!process_initialized)
        return SYNTRA_STATUS_ERROR;

    unsigned int slot_index = 0U;
    if (!process_find_slot_by_id_unlocked(process_id, &slot_index))
        return SYNTRA_STATUS_INVALID_ARGUMENT;

    handle->slot = (uint32_t)slot_index;
    handle->generation = process_slots[slot_index].generation;
    return SYNTRA_STATUS_OK;
}

static syntra_status_t process_create_with_handle_unlocked(
    unsigned int process_id,
    unsigned int priority,
    syntra_process_handle_t *handle)
{
    if (handle == 0)
        return SYNTRA_STATUS_INVALID_ARGUMENT;

    syntra_status_t status = process_create_unlocked(process_id, priority);
    if (status != SYNTRA_STATUS_OK)
        return status;

    return process_get_handle_unlocked(process_id, handle);
}

static syntra_process_slot_t *process_resolve_handle_unlocked(
    syntra_process_handle_t handle)
{
    if (!process_initialized ||
        handle.slot >= SYNTRA_MAX_PROCESSES ||
        handle.generation == 0U)
        return 0;

    syntra_process_slot_t *slot = &process_slots[handle.slot];
    if (!slot->occupied || slot->generation != handle.generation)
        return 0;

    return slot;
}

static syntra_status_t process_get_snapshot_by_handle_unlocked(
    syntra_process_handle_t handle,
    syntra_process_t *snapshot)
{
    if (snapshot == 0)
        return SYNTRA_STATUS_INVALID_ARGUMENT;

    syntra_process_slot_t *slot = process_resolve_handle_unlocked(handle);
    if (slot == 0)
        return SYNTRA_STATUS_INVALID_ARGUMENT;

    *snapshot = slot->process;
    return SYNTRA_STATUS_OK;
}

static syntra_status_t process_get_snapshot_unlocked(
    unsigned int process_id,
    syntra_process_t *snapshot)
{
    if (snapshot == 0 || process_id == 0U)
        return SYNTRA_STATUS_INVALID_ARGUMENT;

    syntra_process_handle_t handle;
    syntra_status_t status = process_get_handle_unlocked(process_id, &handle);
    if (status != SYNTRA_STATUS_OK)
        return status;

    return process_get_snapshot_by_handle_unlocked(handle, snapshot);
}

static syntra_process_t *process_get_unlocked(unsigned int process_id)
{
    if (!process_initialized)
        return 0;

    unsigned int slot_index = 0U;
    if (!process_find_slot_by_id_unlocked(process_id, &slot_index))
        return 0;

    return &process_slots[slot_index].process;
}

static int process_transition_allowed(
    syntra_process_state_t current,
    syntra_process_state_t next)
{
    if (current == next)
        return 1;

    switch (current)
    {
        case PROCESS_READY:
            return next == PROCESS_RUNNING ||
                next == PROCESS_BLOCKED ||
                next == PROCESS_TERMINATED;
        case PROCESS_RUNNING:
            return next == PROCESS_READY ||
                next == PROCESS_BLOCKED ||
                next == PROCESS_TERMINATED;
        case PROCESS_BLOCKED:
            return next == PROCESS_READY ||
                next == PROCESS_TERMINATED;
        case PROCESS_TERMINATED:
        default:
            return 0;
    }
}

static syntra_status_t process_set_state_by_handle_unlocked(
    syntra_process_handle_t handle,
    syntra_process_state_t state)
{
    if (state < PROCESS_READY || state > PROCESS_TERMINATED)
        return SYNTRA_STATUS_INVALID_ARGUMENT;

    syntra_process_slot_t *slot = process_resolve_handle_unlocked(handle);
    if (slot == 0)
        return SYNTRA_STATUS_INVALID_ARGUMENT;

    if (!process_transition_allowed(slot->process.state, state))
        return SYNTRA_STATUS_INVALID_ARGUMENT;

    slot->process.state = state;
    return SYNTRA_STATUS_OK;
}

static syntra_status_t process_set_state_unlocked(
    unsigned int process_id,
    syntra_process_state_t state)
{
    syntra_process_handle_t handle;
    syntra_status_t status = process_get_handle_unlocked(process_id, &handle);
    if (status != SYNTRA_STATUS_OK)
        return status;

    return process_set_state_by_handle_unlocked(handle, state);
}

static syntra_status_t process_terminate_unlocked(unsigned int process_id)
{
    return process_set_state_unlocked(process_id, PROCESS_TERMINATED);
}

static unsigned int process_get_count_unlocked(void)
{
    return process_count;
}

/*
 * Local IRQ masking serializes metadata access on the current single-core
 * ARM64 target only. It is not SMP or FIQ synchronization. The legacy
 * process_get() pointer escapes after IRQ restoration and is compatibility
 * only; new code must use generation-aware handles and snapshots.
 */
syntra_status_t process_init(void)
{
    syntra_irq_state_t irq = syntra_irq_save();
    syntra_status_t status = process_init_unlocked();
    syntra_irq_restore(irq);
    return status;
}

syntra_status_t process_create_with_handle(
    unsigned int id,
    unsigned int priority,
    syntra_process_handle_t *handle)
{
    syntra_irq_state_t irq = syntra_irq_save();
    syntra_status_t status =
        process_create_with_handle_unlocked(id, priority, handle);
    syntra_irq_restore(irq);
    return status;
}

syntra_status_t process_create(unsigned int id, unsigned int priority)
{
    syntra_irq_state_t irq = syntra_irq_save();
    syntra_status_t status = process_create_unlocked(id, priority);
    syntra_irq_restore(irq);
    return status;
}

syntra_status_t process_get_handle(
    unsigned int id,
    syntra_process_handle_t *handle)
{
    syntra_irq_state_t irq = syntra_irq_save();
    syntra_status_t status = process_get_handle_unlocked(id, handle);
    syntra_irq_restore(irq);
    return status;
}

syntra_status_t process_get_snapshot_by_handle(
    syntra_process_handle_t handle,
    syntra_process_t *snapshot)
{
    syntra_irq_state_t irq = syntra_irq_save();
    syntra_status_t status =
        process_get_snapshot_by_handle_unlocked(handle, snapshot);
    syntra_irq_restore(irq);
    return status;
}

syntra_status_t process_set_state_by_handle(
    syntra_process_handle_t handle,
    syntra_process_state_t state)
{
    syntra_irq_state_t irq = syntra_irq_save();
    syntra_status_t status =
        process_set_state_by_handle_unlocked(handle, state);
    syntra_irq_restore(irq);
    return status;
}

syntra_status_t process_get_snapshot(unsigned int id, syntra_process_t *out)
{
    syntra_irq_state_t irq = syntra_irq_save();
    syntra_status_t status = process_get_snapshot_unlocked(id, out);
    syntra_irq_restore(irq);
    return status;
}

syntra_process_t *process_get(unsigned int id)
{
    syntra_irq_state_t irq = syntra_irq_save();
    syntra_process_t *process = process_get_unlocked(id);
    syntra_irq_restore(irq);
    return process;
}

syntra_status_t process_set_state(unsigned int id, syntra_process_state_t state)
{
    syntra_irq_state_t irq = syntra_irq_save();
    syntra_status_t status = process_set_state_unlocked(id, state);
    syntra_irq_restore(irq);
    return status;
}

syntra_status_t process_terminate(unsigned int id)
{
    syntra_irq_state_t irq = syntra_irq_save();
    syntra_status_t status = process_terminate_unlocked(id);
    syntra_irq_restore(irq);
    return status;
}

unsigned int process_get_count(void)
{
    syntra_irq_state_t irq = syntra_irq_save();
    unsigned int count = process_get_count_unlocked();
    syntra_irq_restore(irq);
    return count;
}
