
#include "process.h"
#include "../arch/irq.h"

static syntra_process_t processes[SYNTRA_MAX_PROCESSES];

static unsigned int process_count = 0;
static int process_initialized = 0;


static syntra_status_t process_init_unlocked(void)
{
    if (process_initialized)
    {
        return SYNTRA_STATUS_ERROR;
    }

    process_count = 0;
    process_initialized = 1;

    return SYNTRA_STATUS_OK;
}


static syntra_status_t process_create_unlocked(
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



static syntra_status_t process_get_snapshot_unlocked(
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

static syntra_process_t *process_get_unlocked(unsigned int process_id)
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


static syntra_status_t process_set_state_unlocked(
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

    syntra_process_t *process = process_get_unlocked(process_id);

    if (process == 0)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    process->state = state;

    return SYNTRA_STATUS_OK;
}


static syntra_status_t process_terminate_unlocked(unsigned int process_id)
{
    return process_set_state_unlocked(
        process_id,
        PROCESS_TERMINATED
    );
}


static unsigned int process_get_count_unlocked(void)
{
    return process_count;
}


/*
 * Process metadata is shared kernel state. Local IRQ masking serializes each
 * public operation on the current single-core ARM64 target. This is not an
 * SMP lock. process_get() remains a compatibility escape hatch and its
 * returned pointer must not be retained or mutated by new kernel code.
 */
syntra_status_t process_init(void)
{
    syntra_irq_state_t irq = syntra_irq_save();
    syntra_status_t status = process_init_unlocked();
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
