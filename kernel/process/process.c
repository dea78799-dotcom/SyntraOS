#include "process.h"

#include "../scheduler/scheduler.h"
#include "../arch/arm64/address_space.h"

static syntra_process_t processes[SYNTRA_MAX_PROCESSES];
static unsigned int process_count;
static int process_initialized;

static int process_find_slot(unsigned int process_id)
{
    for (unsigned int i = 0U; i < process_count; ++i)
        if (processes[i].process_id == process_id)
            return (int)i;
    return -1;
}

static syntra_status_t process_derive_state(
    const syntra_process_t *process,
    syntra_process_state_t *state)
{
    unsigned int running = 0U, ready = 0U, blocked = 0U, terminated = 0U;

    if (process == NULL || state == NULL)
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    if (process->thread_count == 0U)
    {
        *state = process->state;
        return SYNTRA_STATUS_OK;
    }

    for (unsigned int i = 0U; i < process->thread_count; ++i)
    {
        syntra_thread_t *thread =
            scheduler_get_thread(process->thread_ids[i]);
        if (thread == NULL)
            return SYNTRA_STATUS_ERROR;

        switch (thread->state)
        {
            case THREAD_RUNNING: ++running; break;
            case THREAD_READY: ++ready; break;
            case THREAD_BLOCKED: ++blocked; break;
            case THREAD_TERMINATED: ++terminated; break;
            default: return SYNTRA_STATUS_ERROR;
        }
    }

    if (running != 0U) *state = PROCESS_RUNNING;
    else if (ready != 0U) *state = PROCESS_READY;
    else if (blocked != 0U) *state = PROCESS_BLOCKED;
    else if (terminated == process->thread_count)
        *state = PROCESS_TERMINATED;
    else
        return SYNTRA_STATUS_ERROR;

    return SYNTRA_STATUS_OK;
}

syntra_status_t process_init(void)
{
    if (process_initialized)
        return SYNTRA_STATUS_ERROR;

    process_count = 0U;
    for (unsigned int i = 0U; i < SYNTRA_MAX_PROCESSES; ++i)
    {
        processes[i].process_id = 0U;
        processes[i].priority = 0U;
        processes[i].state = PROCESS_TERMINATED;
        processes[i].thread_count = 0U;
        processes[i].address_space = NULL;
        for (unsigned int j = 0U; j < SYNTRA_MAX_PROCESS_THREADS; ++j)
            processes[i].thread_ids[j] = 0U;
    }

    process_initialized = 1;
    return SYNTRA_STATUS_OK;
}

syntra_status_t process_create(unsigned int process_id, unsigned int priority)
{
    if (!process_initialized)
        return SYNTRA_STATUS_ERROR;
    if (process_id == 0U)
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    if (process_count >= SYNTRA_MAX_PROCESSES)
        return SYNTRA_STATUS_OUT_OF_MEMORY;
    if (process_find_slot(process_id) >= 0)
        return SYNTRA_STATUS_INVALID_ARGUMENT;

    syntra_process_t *process = &processes[process_count++];
    process->process_id = process_id;
    process->priority = priority;
    process->state = PROCESS_READY;
    process->thread_count = 0U;
    process->address_space = NULL;
    for (unsigned int i = 0U; i < SYNTRA_MAX_PROCESS_THREADS; ++i)
        process->thread_ids[i] = 0U;

    return SYNTRA_STATUS_OK;
}

syntra_process_t *process_get(unsigned int process_id)
{
    int slot;
    if (!process_initialized || process_id == 0U)
        return NULL;
    slot = process_find_slot(process_id);
    return slot < 0 ? NULL : &processes[(unsigned int)slot];
}

syntra_status_t process_attach_address_space(
    unsigned int process_id,
    syntra_arm64_address_space_t *address_space)
{
    syntra_process_t *process = process_get(process_id);
    syntra_status_t status;

    if (process == NULL || address_space == NULL)
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    if (process->state == PROCESS_TERMINATED ||
        process->address_space != NULL)
        return SYNTRA_STATUS_PERMISSION_DENIED;

    status = syntra_arm64_address_space_validate(address_space);
    if (status != SYNTRA_STATUS_OK)
        return status;

    for (unsigned int i = 0U; i < process_count; ++i)
        if (processes[i].address_space == address_space)
            return SYNTRA_STATUS_PERMISSION_DENIED;

    process->address_space = address_space;
    return SYNTRA_STATUS_OK;
}

syntra_status_t process_attach_thread(
    unsigned int process_id,
    unsigned int thread_id)
{
    syntra_process_t *process = process_get(process_id);
    syntra_thread_t *thread;
    if (process == NULL || thread_id == 0U)
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    if (process->state == PROCESS_TERMINATED)
        return SYNTRA_STATUS_PERMISSION_DENIED;
    if (process->thread_count >= SYNTRA_MAX_PROCESS_THREADS)
        return SYNTRA_STATUS_OUT_OF_MEMORY;

    thread = scheduler_get_thread(thread_id);
    if (thread == NULL)
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    /* EL0 threads must never be attached to a process without an address space. */
    if (thread->is_user_thread != 0U && process->address_space == NULL)
        return SYNTRA_STATUS_PERMISSION_DENIED;

    for (unsigned int p = 0U; p < process_count; ++p)
    {
        for (unsigned int i = 0U; i < processes[p].thread_count; ++i)
        {
            if (processes[p].thread_ids[i] == thread_id)
                return SYNTRA_STATUS_INVALID_ARGUMENT;
        }
    }

    process->thread_ids[process->thread_count++] = thread_id;
    return process_refresh_state(process_id);
}

syntra_arm64_address_space_t *process_get_address_space_for_thread(
    unsigned int thread_id)
{
    if (!process_initialized || thread_id == 0U)
        return NULL;

    for (unsigned int p = 0U; p < process_count; ++p)
        for (unsigned int i = 0U; i < processes[p].thread_count; ++i)
            if (processes[p].thread_ids[i] == thread_id)
                return processes[p].address_space;

    return NULL;
}

unsigned int process_get_thread_count(unsigned int process_id)
{
    syntra_process_t *process = process_get(process_id);
    return process == NULL ? 0U : process->thread_count;
}

unsigned int process_get_thread_id(unsigned int process_id, unsigned int index)
{
    syntra_process_t *process = process_get(process_id);
    if (process == NULL || index >= process->thread_count)
        return 0U;
    return process->thread_ids[index];
}

syntra_status_t process_refresh_state(unsigned int process_id)
{
    syntra_process_t *process = process_get(process_id);
    syntra_process_state_t derived;
    syntra_status_t status;
    if (process == NULL)
        return SYNTRA_STATUS_INVALID_ARGUMENT;

    status = process_derive_state(process, &derived);
    if (status != SYNTRA_STATUS_OK)
        return status;
    process->state = derived;
    return SYNTRA_STATUS_OK;
}

syntra_status_t process_set_state(
    unsigned int process_id,
    syntra_process_state_t state)
{
    syntra_process_t *process = process_get(process_id);
    syntra_process_state_t derived;
    syntra_status_t status;

    if (process == NULL)
        return process_initialized
            ? SYNTRA_STATUS_INVALID_ARGUMENT : SYNTRA_STATUS_ERROR;
    if (state < PROCESS_READY || state > PROCESS_TERMINATED)
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    if (state == PROCESS_TERMINATED && process->address_space != NULL)
        return SYNTRA_STATUS_PERMISSION_DENIED;

    if (process->thread_count != 0U)
    {
        status = process_derive_state(process, &derived);
        if (status != SYNTRA_STATUS_OK)
            return status;
        if (state != derived)
            return SYNTRA_STATUS_PERMISSION_DENIED;
    }

    process->state = state;
    return SYNTRA_STATUS_OK;
}

syntra_status_t process_terminate(unsigned int process_id)
{
    syntra_process_t *process = process_get(process_id);
    syntra_process_state_t derived;
    syntra_status_t status;

    if (process == NULL)
        return process_initialized
            ? SYNTRA_STATUS_INVALID_ARGUMENT : SYNTRA_STATUS_ERROR;

    if (process->thread_count != 0U)
    {
        status = process_derive_state(process, &derived);
        if (status != SYNTRA_STATUS_OK)
            return status;
        if (derived != PROCESS_TERMINATED)
            return SYNTRA_STATUS_PERMISSION_DENIED;
    }

    if (process->address_space != NULL)
    {
        status = syntra_arm64_address_space_destroy(process->address_space);
        if (status != SYNTRA_STATUS_OK)
            return status;
        process->address_space = NULL;
    }

    process->state = PROCESS_TERMINATED;
    return SYNTRA_STATUS_OK;
}

syntra_status_t process_validate(void)
{
    if (!process_initialized || process_count > SYNTRA_MAX_PROCESSES)
        return SYNTRA_STATUS_ERROR;

    for (unsigned int p = 0U; p < process_count; ++p)
    {
        const syntra_process_t *process = &processes[p];
        syntra_process_state_t derived;

        if (process->process_id == 0U ||
            process->state < PROCESS_READY ||
            process->state > PROCESS_TERMINATED ||
            process->thread_count > SYNTRA_MAX_PROCESS_THREADS)
            return SYNTRA_STATUS_ERROR;

        if (process->address_space != NULL)
        {
            if (process->state == PROCESS_TERMINATED ||
                syntra_arm64_address_space_validate(
                    process->address_space) != SYNTRA_STATUS_OK)
                return SYNTRA_STATUS_ERROR;
        }

        for (unsigned int q = p + 1U; q < process_count; ++q)
        {
            if (process->process_id == processes[q].process_id ||
                (process->address_space != NULL &&
                 process->address_space == processes[q].address_space))
                return SYNTRA_STATUS_ERROR;
        }

        for (unsigned int i = 0U; i < process->thread_count; ++i)
        {
            syntra_thread_t *thread =
                scheduler_get_thread(process->thread_ids[i]);
            if (process->thread_ids[i] == 0U || thread == NULL)
                return SYNTRA_STATUS_ERROR;
            if (thread->is_user_thread != 0U &&
                process->state != PROCESS_TERMINATED &&
                process->address_space == NULL)
                return SYNTRA_STATUS_ERROR;

            for (unsigned int j = i + 1U; j < process->thread_count; ++j)
                if (process->thread_ids[i] == process->thread_ids[j])
                    return SYNTRA_STATUS_ERROR;

            for (unsigned int q = p + 1U; q < process_count; ++q)
                for (unsigned int j = 0U; j < processes[q].thread_count; ++j)
                    if (process->thread_ids[i] == processes[q].thread_ids[j])
                        return SYNTRA_STATUS_ERROR;
        }

        if (process->thread_count != 0U)
        {
            if (process_derive_state(process, &derived) != SYNTRA_STATUS_OK ||
                derived != process->state)
                return SYNTRA_STATUS_ERROR;
        }
    }

    return scheduler_validate();
}

unsigned int process_get_count(void)
{
    return process_initialized ? process_count : 0U;
}
