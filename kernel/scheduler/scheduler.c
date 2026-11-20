#include "scheduler.h"

static syntra_thread_t threads[SYNTRA_MAX_THREADS];
static unsigned int thread_count;
static unsigned int next_slot;
static int current_slot;
static int scheduler_initialized;

static int scheduler_find_slot(unsigned int thread_id)
{
    for (unsigned int i = 0U; i < thread_count; ++i)
    {
        if (threads[i].thread_id == thread_id)
        {
            return (int)i;
        }
    }

    return -1;
}

static syntra_status_t scheduler_insert_thread(
    unsigned int thread_id,
    unsigned int priority,
    syntra_thread_entry_t entry,
    void *argument,
    void *stack,
    size_t stack_size
)
{
    if (!scheduler_initialized)
    {
        return SYNTRA_STATUS_ERROR;
    }

    if (thread_id == 0U || scheduler_find_slot(thread_id) >= 0)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (thread_count >= SYNTRA_MAX_THREADS)
    {
        return SYNTRA_STATUS_OUT_OF_MEMORY;
    }

    syntra_thread_t *thread = &threads[thread_count];
    thread->thread_id = thread_id;
    thread->priority = priority;
    thread->state = THREAD_READY;
    thread->entry = entry;
    thread->argument = argument;
    thread->stack_base = (uintptr_t)stack;
    thread->stack_size = stack_size;
    thread->cpu_context = NULL;
    ++thread_count;

    return SYNTRA_STATUS_OK;
}

syntra_status_t scheduler_init(void)
{
    if (scheduler_initialized)
    {
        return SYNTRA_STATUS_ERROR;
    }

    thread_count = 0U;
    next_slot = 0U;
    current_slot = -1;

    for (unsigned int i = 0U; i < SYNTRA_MAX_THREADS; ++i)
    {
        threads[i].thread_id = 0U;
        threads[i].priority = 0U;
        threads[i].state = THREAD_TERMINATED;
        threads[i].entry = NULL;
        threads[i].argument = NULL;
        threads[i].stack_base = 0U;
        threads[i].stack_size = 0U;
        threads[i].cpu_context = NULL;
    }

    scheduler_initialized = 1;
    return SYNTRA_STATUS_OK;
}

syntra_status_t scheduler_add_thread(
    unsigned int thread_id,
    unsigned int priority
)
{
    return scheduler_insert_thread(
        thread_id, priority, NULL, NULL, NULL, 0U);
}

syntra_status_t scheduler_create_thread(
    unsigned int thread_id,
    unsigned int priority,
    syntra_thread_entry_t entry,
    void *argument,
    void *stack,
    size_t stack_size
)
{
    uintptr_t stack_address = (uintptr_t)stack;

    if (entry == NULL || stack == NULL ||
        stack_size < SYNTRA_THREAD_MIN_STACK_SIZE ||
        (stack_address & (SYNTRA_THREAD_STACK_ALIGNMENT - 1U)) != 0U)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (stack_address > UINTPTR_MAX - stack_size)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    return scheduler_insert_thread(
        thread_id, priority, entry, argument, stack, stack_size);
}

syntra_thread_t *scheduler_get_next_ready(void)
{
    if (!scheduler_initialized || thread_count == 0U)
    {
        return NULL;
    }

    for (unsigned int offset = 0U; offset < thread_count; ++offset)
    {
        unsigned int slot = (next_slot + offset) % thread_count;
        if (threads[slot].state == THREAD_READY)
        {
            return &threads[slot];
        }
    }

    return NULL;
}

syntra_thread_t *scheduler_schedule_next(void)
{
    if (!scheduler_initialized || thread_count == 0U)
    {
        return NULL;
    }

    if (current_slot >= 0 &&
        threads[(unsigned int)current_slot].state == THREAD_RUNNING)
    {
        threads[(unsigned int)current_slot].state = THREAD_READY;
    }

    syntra_thread_t *selected = scheduler_get_next_ready();
    if (selected == NULL)
    {
        /* No READY task: keep the current task if it is still runnable. */
        if (current_slot >= 0 &&
            threads[(unsigned int)current_slot].state == THREAD_READY)
        {
            threads[(unsigned int)current_slot].state = THREAD_RUNNING;
            return &threads[(unsigned int)current_slot];
        }
        current_slot = -1;
        return NULL;
    }

    current_slot = (int)(selected - threads);
    selected->state = THREAD_RUNNING;
    next_slot = ((unsigned int)current_slot + 1U) % thread_count;
    return selected;
}

syntra_thread_t *scheduler_get_current(void)
{
    if (!scheduler_initialized || current_slot < 0)
    {
        return NULL;
    }

    return &threads[(unsigned int)current_slot];
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

    if (new_state < THREAD_READY || new_state > THREAD_TERMINATED)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    int slot = scheduler_find_slot(thread_id);
    if (slot < 0)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (new_state == THREAD_RUNNING)
    {
        if (current_slot >= 0 && current_slot != slot &&
            threads[(unsigned int)current_slot].state == THREAD_RUNNING)
        {
            threads[(unsigned int)current_slot].state = THREAD_READY;
        }
        current_slot = slot;
    }
    else if (current_slot == slot)
    {
        current_slot = -1;
    }

    threads[(unsigned int)slot].state = new_state;
    return SYNTRA_STATUS_OK;
}

syntra_status_t scheduler_wake_thread(unsigned int thread_id)
{
    syntra_thread_t *thread = scheduler_get_thread(thread_id);
    if (thread == NULL || thread->state != THREAD_BLOCKED)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    thread->state = THREAD_READY;
    return SYNTRA_STATUS_OK;
}

syntra_status_t scheduler_block_current(void)
{
    syntra_thread_t *current = scheduler_get_current();
    if (current == NULL)
    {
        return SYNTRA_STATUS_ERROR;
    }

    current->state = THREAD_BLOCKED;
    current_slot = -1;
    return SYNTRA_STATUS_OK;
}

syntra_status_t scheduler_terminate_current(void)
{
    syntra_thread_t *current = scheduler_get_current();
    if (current == NULL)
    {
        return SYNTRA_STATUS_ERROR;
    }

    current->state = THREAD_TERMINATED;
    current_slot = -1;
    return SYNTRA_STATUS_OK;
}

syntra_thread_t *scheduler_get_thread(unsigned int thread_id)
{
    if (!scheduler_initialized || thread_id == 0U)
    {
        return NULL;
    }

    int slot = scheduler_find_slot(thread_id);
    return slot < 0 ? NULL : &threads[(unsigned int)slot];
}

unsigned int scheduler_get_thread_count(void)
{
    return scheduler_initialized ? thread_count : 0U;
}

syntra_status_t scheduler_validate(void)
{
    unsigned int running_count = 0U;

    if (!scheduler_initialized || thread_count > SYNTRA_MAX_THREADS)
    {
        return SYNTRA_STATUS_ERROR;
    }

    for (unsigned int i = 0U; i < thread_count; ++i)
    {
        const syntra_thread_t *thread = &threads[i];

        if (thread->thread_id == 0U ||
            thread->state < THREAD_READY ||
            thread->state > THREAD_TERMINATED)
        {
            return SYNTRA_STATUS_ERROR;
        }

        for (unsigned int j = i + 1U; j < thread_count; ++j)
        {
            if (thread->thread_id == threads[j].thread_id)
            {
                return SYNTRA_STATUS_ERROR;
            }
        }

        if (thread->state == THREAD_RUNNING)
        {
            ++running_count;
            if (current_slot != (int)i)
            {
                return SYNTRA_STATUS_ERROR;
            }
        }

        if (thread->entry != NULL)
        {
            if (thread->stack_base == 0U ||
                thread->stack_size < SYNTRA_THREAD_MIN_STACK_SIZE ||
                (thread->stack_base &
                    (SYNTRA_THREAD_STACK_ALIGNMENT - 1U)) != 0U)
            {
                return SYNTRA_STATUS_ERROR;
            }
        }
    }

    if (running_count > 1U ||
        (current_slot < 0 && running_count != 0U) ||
        (current_slot >= 0 &&
            (unsigned int)current_slot >= thread_count))
    {
        return SYNTRA_STATUS_ERROR;
    }

    return SYNTRA_STATUS_OK;
}
