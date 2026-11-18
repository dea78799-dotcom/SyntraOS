#include "scheduler.h"

static syntra_thread_t threads[SYNTRA_MAX_THREADS];

static unsigned int thread_count = 0;
static int scheduler_initialized = 0;


syntra_status_t scheduler_init(void)
{
    if (scheduler_initialized)
    {
        return SYNTRA_STATUS_ERROR;
    }

    thread_count = 0;
    scheduler_initialized = 1;

    return SYNTRA_STATUS_OK;
}


syntra_status_t scheduler_add_thread(
    unsigned int thread_id,
    unsigned int priority
)
{
    if (!scheduler_initialized)
    {
        return SYNTRA_STATUS_ERROR;
    }

    if (thread_id == 0)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (thread_count >= SYNTRA_MAX_THREADS)
    {
        return SYNTRA_STATUS_OUT_OF_MEMORY;
    }

    for (unsigned int i = 0; i < thread_count; i++)
    {
        if (threads[i].thread_id == thread_id)
        {
            return SYNTRA_STATUS_ERROR;
        }
    }

    threads[thread_count].thread_id = thread_id;
    threads[thread_count].priority = priority;
    threads[thread_count].state = THREAD_READY;

    thread_count++;

    return SYNTRA_STATUS_OK;
}


syntra_thread_t *scheduler_get_next_ready(void)
{
    if (!scheduler_initialized)
    {
        return 0;
    }

    syntra_thread_t *selected = 0;

    for (unsigned int i = 0; i < thread_count; i++)
    {
        if (threads[i].state != THREAD_READY)
        {
            continue;
        }

        if (selected == 0 ||
            threads[i].priority > selected->priority)
        {
            selected = &threads[i];
        }
    }

    return selected;
}


syntra_status_t scheduler_set_thread_state(
    unsigned int thread_id,
    thread_state_t new_state
)
{
    if (!scheduler_initialized)
    {
        return SYNTRA_STATUS_ERROR;
    }

    if (new_state < THREAD_READY ||
        new_state > THREAD_TERMINATED)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    for (unsigned int i = 0; i < thread_count; i++)
    {
        if (threads[i].thread_id == thread_id)
        {
            threads[i].state = new_state;
            return SYNTRA_STATUS_OK;
        }
    }

    return SYNTRA_STATUS_INVALID_ARGUMENT;
}


syntra_thread_t *scheduler_get_thread(
    unsigned int thread_id
)
{
    if (!scheduler_initialized)
    {
        return 0;
    }

    for (unsigned int i = 0; i < thread_count; i++)
    {
        if (threads[i].thread_id == thread_id)
        {
            return &threads[i];
        }
    }

    return 0;
}


unsigned int scheduler_get_thread_count(void)
{
    return thread_count;
}
