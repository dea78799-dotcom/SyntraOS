
#include "process.h"

static syntra_process_t processes[SYNTRA_MAX_PROCESSES];

static unsigned int process_count = 0;
static int process_initialized = 0;


syntra_status_t process_init(void)
{
    if (process_initialized)
    {
        return SYNTRA_STATUS_ERROR;
    }

    process_count = 0;
    process_initialized = 1;

    return SYNTRA_STATUS_OK;
}


syntra_status_t process_create(
    unsigned int process_id,
    unsigned int priority
)
{
    if (!process_initialized)
    {
        return SYNTRA_STATUS_ERROR;
    }

    if (process_id == 0)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (process_count >= SYNTRA_MAX_PROCESSES)
    {
        return SYNTRA_STATUS_OUT_OF_MEMORY;
    }

    for (unsigned int i = 0; i < process_count; i++)
    {
        if (processes[i].process_id == process_id)
        {
            return SYNTRA_STATUS_ERROR;
        }
    }

    processes[process_count].process_id = process_id;
    processes[process_count].priority = priority;
    processes[process_count].state = PROCESS_READY;

    process_count++;

    return SYNTRA_STATUS_OK;
}



syntra_status_t process_get_snapshot(
    unsigned int process_id,
    syntra_process_t *snapshot)
{
    if (snapshot == 0 || process_id == 0U)
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    if (!process_initialized)
        return SYNTRA_STATUS_ERROR;
    for (unsigned int i = 0U; i < process_count; ++i)
    {
        if (processes[i].process_id == process_id)
        {
            *snapshot = processes[i];
            return SYNTRA_STATUS_OK;
        }
    }
    return SYNTRA_STATUS_INVALID_ARGUMENT;
}

syntra_process_t *process_get(unsigned int process_id)
{
    if (!process_initialized)
    {
        return 0;
    }

    for (unsigned int i = 0; i < process_count; i++)
    {
        if (processes[i].process_id == process_id)
        {
            return &processes[i];
        }
    }

    return 0;
}


syntra_status_t process_set_state(
    unsigned int process_id,
    syntra_process_state_t state
)
{
    if (!process_initialized)
    {
        return SYNTRA_STATUS_ERROR;
    }

    if (state < PROCESS_READY || state > PROCESS_TERMINATED)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    syntra_process_t *process = process_get(process_id);

    if (process == 0)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    process->state = state;

    return SYNTRA_STATUS_OK;
}


syntra_status_t process_terminate(unsigned int process_id)
{
    return process_set_state(
        process_id,
        PROCESS_TERMINATED
    );
}


unsigned int process_get_count(void)
{
    return process_count;
}
