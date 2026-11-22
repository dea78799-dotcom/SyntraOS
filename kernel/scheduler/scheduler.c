#include "scheduler.h"

static syntra_thread_t threads[SYNTRA_MAX_THREADS];
static unsigned int thread_count;
static unsigned int thread_slot_count;
static unsigned int next_slot;
static int current_slot;
static int scheduler_initialized;

static int scheduler_find_slot(unsigned int thread_id)
{
    for (unsigned int i = 0U; i < thread_slot_count; ++i)
    {
        if (threads[i].thread_id == thread_id)
        {
            return (int)i;
        }
    }

    return -1;
}

static int scheduler_ranges_overlap(
    uintptr_t first_base, size_t first_size,
    uintptr_t second_base, size_t second_size)
{
    if (first_size == 0U || second_size == 0U) return 0;
    return first_base < second_base + second_size &&
        second_base < first_base + first_size;
}

static int scheduler_stack_range_conflicts(
    uintptr_t base, size_t size)
{
    for (unsigned int i = 0U; i < thread_slot_count; ++i)
    {
        const syntra_thread_t *thread = &threads[i];
        if (scheduler_ranges_overlap(base, size,
                thread->stack_base, thread->stack_size) ||
            scheduler_ranges_overlap(base, size,
                thread->user_stack_base, thread->user_stack_size))
            return 1;
    }
    return 0;
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

    unsigned int slot;
    for (slot = 0U; slot < thread_slot_count; ++slot)
        if (threads[slot].thread_id == 0U)
            break;

    if (slot == thread_slot_count)
    {
        if (thread_slot_count >= SYNTRA_MAX_THREADS)
            return SYNTRA_STATUS_OUT_OF_MEMORY;
        ++thread_slot_count;
    }

    syntra_thread_t *thread = &threads[slot];
    thread->thread_id = thread_id;
    thread->priority = priority;
    thread->state = THREAD_READY;
    thread->entry = entry;
    thread->argument = argument;
    thread->stack_base = (uintptr_t)stack;
    thread->stack_size = stack_size;
    thread->user_stack_base = 0U;
    thread->user_stack_size = 0U;
    thread->is_user_thread = 0U;
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
    thread_slot_count = 0U;
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
        threads[i].user_stack_base = 0U;
        threads[i].user_stack_size = 0U;
        threads[i].is_user_thread = 0U;
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


syntra_status_t scheduler_create_user_thread(
    unsigned int thread_id,
    unsigned int priority,
    syntra_thread_entry_t entry,
    void *argument,
    void *kernel_stack,
    size_t kernel_stack_size,
    void *user_stack,
    size_t user_stack_size,
    void *cpu_context)
{
    uintptr_t kernel_base = (uintptr_t)kernel_stack;
    uintptr_t user_base = (uintptr_t)user_stack;
    syntra_status_t status;

    if (entry == NULL || kernel_stack == NULL || user_stack == NULL ||
        cpu_context == NULL ||
        kernel_stack_size < SYNTRA_THREAD_MIN_STACK_SIZE ||
        user_stack_size < SYNTRA_THREAD_MIN_STACK_SIZE ||
        (kernel_base & (SYNTRA_THREAD_STACK_ALIGNMENT - 1U)) != 0U ||
        (user_base & (SYNTRA_THREAD_STACK_ALIGNMENT - 1U)) != 0U ||
        kernel_base > UINTPTR_MAX - kernel_stack_size ||
        user_base > UINTPTR_MAX - user_stack_size ||
        scheduler_ranges_overlap(kernel_base, kernel_stack_size,
            user_base, user_stack_size))
        return SYNTRA_STATUS_INVALID_ARGUMENT;

    if (!scheduler_initialized)
        return SYNTRA_STATUS_ERROR;
    if (scheduler_stack_range_conflicts(kernel_base, kernel_stack_size) ||
        scheduler_stack_range_conflicts(user_base, user_stack_size))
        return SYNTRA_STATUS_INVALID_ARGUMENT;

    status = scheduler_insert_thread(
        thread_id, priority, entry, argument,
        kernel_stack, kernel_stack_size);
    if (status != SYNTRA_STATUS_OK) return status;

    {
        syntra_thread_t *thread = scheduler_get_thread(thread_id);
        if (thread == NULL) return SYNTRA_STATUS_ERROR;
        thread->user_stack_base = user_base;
        thread->user_stack_size = user_stack_size;
        thread->is_user_thread = 1U;
        thread->cpu_context = cpu_context;
    }
    return SYNTRA_STATUS_OK;
}

syntra_thread_t *scheduler_get_next_ready(void)
{
    if (!scheduler_initialized || thread_count == 0U)
    {
        return NULL;
    }

    for (unsigned int offset = 0U; offset < thread_slot_count; ++offset)
    {
        unsigned int slot = (next_slot + offset) % thread_slot_count;
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
    next_slot = ((unsigned int)current_slot + 1U) % thread_slot_count;
    return selected;
}


/*
 * Context-switch path: skip READY threads without saved CPU context.
 * Metadata-only threads remain eligible to scheduler_schedule_next().
 */
syntra_thread_t *scheduler_schedule_next_with_context(void)
{
    if (!scheduler_initialized || thread_count == 0U)
        return NULL;

    if (current_slot >= 0 &&
        threads[(unsigned int)current_slot].state == THREAD_RUNNING)
        threads[(unsigned int)current_slot].state = THREAD_READY;

    for (unsigned int offset = 0U; offset < thread_slot_count; ++offset)
    {
        unsigned int slot = (next_slot + offset) % thread_slot_count;
        syntra_thread_t *candidate = &threads[slot];

        if (candidate->state != THREAD_READY ||
            candidate->cpu_context == NULL)
            continue;

        current_slot = (int)slot;
        candidate->state = THREAD_RUNNING;
        next_slot = (slot + 1U) % thread_slot_count;
        return candidate;
    }

    /* If no eligible task exists, resume the interrupted task if possible. */
    if (current_slot >= 0)
    {
        syntra_thread_t *current = &threads[(unsigned int)current_slot];
        if (current->state == THREAD_READY && current->cpu_context != NULL)
        {
            current->state = THREAD_RUNNING;
            return current;
        }
    }

    current_slot = -1;
    return NULL;
}

syntra_status_t scheduler_rollback_context_switch(
    unsigned int previous_thread_id,
    unsigned int selected_thread_id)
{
    int previous_slot = -1;
    int selected_slot = -1;

    if (!scheduler_initialized)
        return SYNTRA_STATUS_ERROR;
    /*
     * A zero previous ID means the interrupted thread had already exited
     * and the scheduler has no current slot (the exception frame is an idle
     * continuation, not a live scheduler thread).
     */
    if (previous_thread_id != 0U)
    {
        previous_slot = scheduler_find_slot(previous_thread_id);
        if (previous_slot < 0)
            return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (selected_thread_id != 0U)
    {
        selected_slot = scheduler_find_slot(selected_thread_id);
        if (selected_slot < 0 || current_slot != selected_slot ||
            threads[(unsigned int)selected_slot].state != THREAD_RUNNING)
            return SYNTRA_STATUS_PERMISSION_DENIED;
    }
    else if (current_slot >= 0)
    {
        return SYNTRA_STATUS_PERMISSION_DENIED;
    }

    if (previous_slot >= 0 &&
        threads[(unsigned int)previous_slot].state == THREAD_TERMINATED)
        return SYNTRA_STATUS_PERMISSION_DENIED;

    if (selected_slot >= 0)
        threads[(unsigned int)selected_slot].state = THREAD_READY;

    if (previous_slot >= 0)
    {
        threads[(unsigned int)previous_slot].state = THREAD_RUNNING;
        current_slot = previous_slot;
    }
    else
    {
        current_slot = -1;
    }

    /*
     * Retry the failed selection first on the next scheduling attempt.
     * When no candidate existed, resume round-robin after the restored task.
     */
    if (selected_slot >= 0)
        next_slot = (unsigned int)selected_slot;
    else if (previous_slot >= 0)
        next_slot = ((unsigned int)previous_slot + 1U) % thread_slot_count;

    return scheduler_validate();
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

syntra_status_t scheduler_reap_terminated(unsigned int thread_id)
{
    int slot;

    if (!scheduler_initialized)
        return SYNTRA_STATUS_ERROR;
    slot = scheduler_find_slot(thread_id);
    if (slot < 0)
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    if (current_slot == slot ||
        threads[(unsigned int)slot].state != THREAD_TERMINATED)
        return SYNTRA_STATUS_PERMISSION_DENIED;

    /*
     * Stack storage is caller-owned. Reaping only releases Scheduler metadata;
     * the owner may free/reuse stacks after this call succeeds.
     */
    syntra_thread_t *thread = &threads[(unsigned int)slot];
    thread->thread_id = 0U;
    thread->priority = 0U;
    thread->state = THREAD_TERMINATED;
    thread->entry = NULL;
    thread->argument = NULL;
    thread->stack_base = 0U;
    thread->stack_size = 0U;
    thread->user_stack_base = 0U;
    thread->user_stack_size = 0U;
    thread->is_user_thread = 0U;
    thread->cpu_context = NULL;
    --thread_count;

    if (thread_slot_count == 0U)
        next_slot = 0U;
    else if (next_slot >= thread_slot_count)
        next_slot = 0U;

    return scheduler_validate();
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

    if (!scheduler_initialized || thread_count > SYNTRA_MAX_THREADS ||
        thread_slot_count > SYNTRA_MAX_THREADS)
    {
        return SYNTRA_STATUS_ERROR;
    }

    unsigned int observed_count = 0U;
    for (unsigned int i = 0U; i < thread_slot_count; ++i)
    {
        const syntra_thread_t *thread = &threads[i];

        if (thread->thread_id == 0U)
        {
            if (thread->priority != 0U || thread->entry != NULL ||
                thread->argument != NULL || thread->stack_base != 0U ||
                thread->stack_size != 0U || thread->user_stack_base != 0U ||
                thread->user_stack_size != 0U ||
                thread->is_user_thread != 0U || thread->cpu_context != NULL ||
                thread->state != THREAD_TERMINATED)
                return SYNTRA_STATUS_ERROR;
            continue;
        }
        ++observed_count;

        if (thread->state < THREAD_READY ||
            thread->state < THREAD_READY ||
            thread->state > THREAD_TERMINATED)
        {
            return SYNTRA_STATUS_ERROR;
        }

        for (unsigned int j = i + 1U; j < thread_slot_count; ++j)
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

        if (thread->is_user_thread > 1U)
            return SYNTRA_STATUS_ERROR;

        if (thread->is_user_thread != 0U)
        {
            if (thread->entry == NULL || thread->cpu_context == NULL ||
                thread->stack_base == 0U ||
                thread->stack_size < SYNTRA_THREAD_MIN_STACK_SIZE ||
                thread->user_stack_base == 0U ||
                thread->user_stack_size < SYNTRA_THREAD_MIN_STACK_SIZE ||
                (thread->user_stack_base &
                    (SYNTRA_THREAD_STACK_ALIGNMENT - 1U)) != 0U ||
                thread->stack_base > UINTPTR_MAX - thread->stack_size ||
                thread->user_stack_base >
                    UINTPTR_MAX - thread->user_stack_size ||
                scheduler_ranges_overlap(
                    thread->stack_base, thread->stack_size,
                    thread->user_stack_base, thread->user_stack_size))
                return SYNTRA_STATUS_ERROR;
        }
        else if (thread->user_stack_base != 0U ||
                 thread->user_stack_size != 0U)
            return SYNTRA_STATUS_ERROR;

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

    if (observed_count != thread_count || running_count > 1U ||
        (current_slot < 0 && running_count != 0U) ||
        (current_slot >= 0 &&
            (unsigned int)current_slot >= thread_slot_count ||
             threads[(unsigned int)current_slot].thread_id == 0U))
    {
        return SYNTRA_STATUS_ERROR;
    }

    return SYNTRA_STATUS_OK;
}
