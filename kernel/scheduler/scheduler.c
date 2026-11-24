#include "scheduler.h"
#include "../arch/irq.h"

static syntra_thread_t threads[SYNTRA_MAX_THREADS];
static unsigned int thread_count;
static unsigned int next_slot;
static int current_slot;
static int scheduler_initialized;

/* One unpublished reservation is sufficient for the single-core coordinator. */
static syntra_thread_t reserved_thread;
static int reserved_thread_valid;

static void scheduler_clear_reserved_thread(void)
{
    reserved_thread.handle.slot = 0U;
    reserved_thread.handle.generation = 0U;
    reserved_thread.thread_id = 0U;
    reserved_thread.priority = 0U;
    reserved_thread.state = THREAD_TERMINATED;
    reserved_thread.entry = NULL;
    reserved_thread.argument = NULL;
    reserved_thread.stack_base = 0U;
    reserved_thread.stack_size = 0U;
    reserved_thread.stack_ownership = SYNTRA_THREAD_STACK_BORROWED;
    reserved_thread.stack_allocation.slot = 0U;
    reserved_thread.stack_allocation.generation = 0U;
    reserved_thread.user_stack_base = 0U;
    reserved_thread.user_stack_size = 0U;
    reserved_thread.is_user_thread = 0U;
    reserved_thread.cpu_context = NULL;
    reserved_thread.owner_process.slot = 0U;
    reserved_thread.owner_process.generation = 0U;
    reserved_thread.has_process_owner = 0U;
}

#define SYNTRA_SWITCH_COMPLETION_CAPACITY 128U

typedef struct
{
    syntra_thread_handle_t handle;
    uint64_t sequence;
    int valid;
} scheduler_pending_switch_t;

static scheduler_pending_switch_t pending_switch;
static syntra_thread_switch_event_t switch_events[SYNTRA_SWITCH_COMPLETION_CAPACITY];
static unsigned int switch_event_head;
static unsigned int switch_event_tail;
static unsigned int switch_event_count;
static uint64_t switch_sequence;

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
    for (unsigned int i = 0U; i < thread_count; ++i)
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

static syntra_status_t scheduler_stage_switch_unlocked(
    syntra_thread_handle_t handle)
{
    if (pending_switch.valid ||
        switch_event_count >= SYNTRA_SWITCH_COMPLETION_CAPACITY ||
        switch_sequence == UINT64_MAX ||
        handle.generation == 0U ||
        handle.slot >= thread_count)
        return SYNTRA_STATUS_ERROR;

    ++switch_sequence;
    pending_switch.handle = handle;
    pending_switch.sequence = switch_sequence;
    pending_switch.valid = 1;
    return SYNTRA_STATUS_OK;
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

    /* Keep the reserved append-only slot stable until publish or abort. */
    if (reserved_thread_valid)
        return SYNTRA_STATUS_ERROR;

    if (thread_id == 0U || scheduler_find_slot(thread_id) >= 0)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (thread_count >= SYNTRA_MAX_THREADS)
    {
        return SYNTRA_STATUS_OUT_OF_MEMORY;
    }

    syntra_thread_t *thread = &threads[thread_count];
    thread->handle.slot = (uint32_t)thread_count;
    thread->handle.generation = 1U;
    thread->thread_id = thread_id;
    thread->priority = priority;
    thread->state = THREAD_READY;
    thread->entry = entry;
    thread->argument = argument;
    thread->stack_base = (uintptr_t)stack;
    thread->stack_size = stack_size;
    thread->stack_ownership = SYNTRA_THREAD_STACK_BORROWED;
    thread->stack_allocation.slot = 0U;
    thread->stack_allocation.generation = 0U;
    thread->user_stack_base = 0U;
    thread->user_stack_size = 0U;
    thread->is_user_thread = 0U;
    thread->cpu_context = NULL;
    thread->owner_process.slot = 0U;
    thread->owner_process.generation = 0U;
    thread->has_process_owner = 0U;
    ++thread_count;

    return SYNTRA_STATUS_OK;
}

static syntra_status_t scheduler_init_unlocked(void)
{
    if (scheduler_initialized)
    {
        return SYNTRA_STATUS_ERROR;
    }

    thread_count = 0U;
    scheduler_clear_reserved_thread();
    reserved_thread_valid = 0;
    next_slot = 0U;
    current_slot = -1;
    pending_switch.handle.slot = 0U;
    pending_switch.handle.generation = 0U;
    pending_switch.sequence = 0U;
    pending_switch.valid = 0;
    switch_event_head = 0U;
    switch_event_tail = 0U;
    switch_event_count = 0U;
    switch_sequence = 0U;

    for (unsigned int i = 0U; i < SYNTRA_MAX_THREADS; ++i)
    {
        threads[i].handle.slot = 0U;
        threads[i].handle.generation = 0U;
        threads[i].thread_id = 0U;
        threads[i].priority = 0U;
        threads[i].state = THREAD_TERMINATED;
        threads[i].entry = NULL;
        threads[i].argument = NULL;
        threads[i].stack_base = 0U;
        threads[i].stack_size = 0U;
        threads[i].stack_ownership = SYNTRA_THREAD_STACK_BORROWED;
        threads[i].stack_allocation.slot = 0U;
        threads[i].stack_allocation.generation = 0U;
        threads[i].user_stack_base = 0U;
        threads[i].user_stack_size = 0U;
        threads[i].is_user_thread = 0U;
        threads[i].cpu_context = NULL;
        threads[i].owner_process.slot = 0U;
        threads[i].owner_process.generation = 0U;
        threads[i].has_process_owner = 0U;
    }

    scheduler_initialized = 1;
    return SYNTRA_STATUS_OK;
}

static syntra_status_t scheduler_add_thread_unlocked(
    unsigned int thread_id,
    unsigned int priority
)
{
    return scheduler_insert_thread(
        thread_id, priority, NULL, NULL, NULL, 0U);
}

static syntra_status_t scheduler_create_thread_unlocked(
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

    /*
     * Every registered thread retains exclusive stack ranges until the
     * scheduler is reset. Check both EL1 and EL0 ranges.
     */
    if (scheduler_stack_range_conflicts(stack_address, stack_size))
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    return scheduler_insert_thread(
        thread_id, priority, entry, argument, stack, stack_size);
}


static syntra_status_t scheduler_create_user_thread_unlocked(
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
        int slot = scheduler_find_slot(thread_id);
        if (slot < 0) return SYNTRA_STATUS_ERROR;
        threads[(unsigned int)slot].user_stack_base = user_base;
        threads[(unsigned int)slot].user_stack_size = user_stack_size;
        threads[(unsigned int)slot].is_user_thread = 1U;
        threads[(unsigned int)slot].cpu_context = cpu_context;
    }
    return SYNTRA_STATUS_OK;
}

static syntra_thread_t *scheduler_get_next_ready_unlocked(void)
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

static syntra_thread_t *scheduler_schedule_next_unlocked(void)
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

    syntra_thread_t *selected = scheduler_get_next_ready_unlocked();
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


/*
 * Context-switch path: skip READY threads without saved CPU context.
 * Metadata-only threads remain eligible to scheduler_schedule_next().
 */
static syntra_thread_t *scheduler_schedule_next_with_context_unlocked(void)
{
    if (!scheduler_initialized || thread_count == 0U)
        return NULL;

    if (current_slot >= 0 &&
        threads[(unsigned int)current_slot].state == THREAD_RUNNING)
        threads[(unsigned int)current_slot].state = THREAD_READY;

    for (unsigned int offset = 0U; offset < thread_count; ++offset)
    {
        unsigned int slot = (next_slot + offset) % thread_count;
        syntra_thread_t *candidate = &threads[slot];

        if (candidate->state != THREAD_READY ||
            candidate->cpu_context == NULL)
            continue;

        current_slot = (int)slot;
        candidate->state = THREAD_RUNNING;
        next_slot = (slot + 1U) % thread_count;
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

static syntra_thread_t *scheduler_get_current_unlocked(void)
{
    if (!scheduler_initialized || current_slot < 0)
    {
        return NULL;
    }

    return &threads[(unsigned int)current_slot];
}

static syntra_status_t scheduler_set_thread_state_unlocked(
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

    /* Terminated TCBs are terminal in this lifecycle phase. */
    if (threads[(unsigned int)slot].state == THREAD_TERMINATED &&
        new_state != THREAD_TERMINATED)
        return SYNTRA_STATUS_INVALID_ARGUMENT;

    if (current_slot == slot && new_state != THREAD_RUNNING)
    {
        syntra_status_t stage_status =
            scheduler_stage_switch_unlocked(threads[(unsigned int)slot].handle);
        if (stage_status != SYNTRA_STATUS_OK)
            return stage_status;
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

static syntra_status_t scheduler_wake_thread_unlocked(unsigned int thread_id)
{
    int slot = scheduler_find_slot(thread_id);
    if (slot < 0 || threads[(unsigned int)slot].state != THREAD_BLOCKED)
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    threads[(unsigned int)slot].state = THREAD_READY;
    return SYNTRA_STATUS_OK;
}

static syntra_status_t scheduler_block_current_unlocked(void)
{
    if (!scheduler_initialized || current_slot < 0)
        return SYNTRA_STATUS_ERROR;
    if (scheduler_stage_switch_unlocked(
            threads[(unsigned int)current_slot].handle) != SYNTRA_STATUS_OK)
        return SYNTRA_STATUS_ERROR;
    threads[(unsigned int)current_slot].state = THREAD_BLOCKED;
    current_slot = -1;
    return SYNTRA_STATUS_OK;
}

static syntra_status_t scheduler_terminate_current_unlocked(void)
{
    if (!scheduler_initialized || current_slot < 0)
        return SYNTRA_STATUS_ERROR;
    if (scheduler_stage_switch_unlocked(
            threads[(unsigned int)current_slot].handle) != SYNTRA_STATUS_OK)
        return SYNTRA_STATUS_ERROR;
    threads[(unsigned int)current_slot].state = THREAD_TERMINATED;
    current_slot = -1;
    return SYNTRA_STATUS_OK;
}


static int scheduler_resolve_handle_unlocked(syntra_thread_handle_t handle)
{
    if (!scheduler_initialized || handle.generation == 0U ||
        handle.slot >= SYNTRA_MAX_THREADS || handle.slot >= thread_count)
        return -1;

    const syntra_thread_t *thread = &threads[handle.slot];
    if (thread->handle.slot != handle.slot ||
        thread->handle.generation != handle.generation)
        return -1;

    return (int)handle.slot;
}

static syntra_status_t scheduler_get_thread_snapshot_by_handle_unlocked(
    syntra_thread_handle_t handle, syntra_thread_t *snapshot)
{
    if (snapshot == NULL)
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    if (!scheduler_initialized)
        return SYNTRA_STATUS_ERROR;

    int slot = scheduler_resolve_handle_unlocked(handle);
    if (slot < 0)
        return SYNTRA_STATUS_INVALID_ARGUMENT;

    *snapshot = threads[(unsigned int)slot];
    return SYNTRA_STATUS_OK;
}

static syntra_status_t scheduler_set_thread_state_by_handle_unlocked(
    syntra_thread_handle_t handle, thread_state_t new_state)
{
    int slot = scheduler_resolve_handle_unlocked(handle);
    if (slot < 0)
        return SYNTRA_STATUS_INVALID_ARGUMENT;

    return scheduler_set_thread_state_unlocked(
        threads[(unsigned int)slot].thread_id, new_state);
}

static syntra_status_t scheduler_get_thread_snapshot_unlocked(
    unsigned int thread_id,
    syntra_thread_t *snapshot)
{
    if (snapshot == NULL || thread_id == 0U)
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    if (!scheduler_initialized)
        return SYNTRA_STATUS_ERROR;
    int slot = scheduler_find_slot(thread_id);
    if (slot < 0)
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    *snapshot = threads[(unsigned int)slot];
    return SYNTRA_STATUS_OK;
}

static syntra_status_t scheduler_get_current_snapshot_unlocked(syntra_thread_t *snapshot)
{
    if (snapshot == NULL)
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    if (!scheduler_initialized || current_slot < 0)
        return SYNTRA_STATUS_ERROR;
    *snapshot = threads[(unsigned int)current_slot];
    return SYNTRA_STATUS_OK;
}

static syntra_status_t scheduler_set_thread_context_unlocked(
    unsigned int thread_id, void *cpu_context)
{
    if (!scheduler_initialized)
        return SYNTRA_STATUS_ERROR;
    int slot = scheduler_find_slot(thread_id);
    if (slot < 0)
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    threads[(unsigned int)slot].cpu_context = cpu_context;
    return SYNTRA_STATUS_OK;
}

static syntra_status_t scheduler_schedule_next_context_unlocked(void **cpu_context_out)
{
    syntra_thread_t *selected;
    syntra_thread_handle_t outgoing_handle = {0U, 0U};
    int had_outgoing = 0;

    if (cpu_context_out == NULL)
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    *cpu_context_out = NULL;

    if (pending_switch.valid &&
        switch_event_count >= SYNTRA_SWITCH_COMPLETION_CAPACITY)
        return SYNTRA_STATUS_ERROR;

    if (!pending_switch.valid && current_slot >= 0 &&
        threads[(unsigned int)current_slot].state == THREAD_RUNNING)
    {
        if (switch_event_count >= SYNTRA_SWITCH_COMPLETION_CAPACITY ||
            switch_sequence == UINT64_MAX)
            return SYNTRA_STATUS_ERROR;
        outgoing_handle = threads[(unsigned int)current_slot].handle;
        had_outgoing = 1;
    }

    selected = scheduler_schedule_next_with_context_unlocked();
    if (selected == NULL)
        return SYNTRA_STATUS_ERROR;

    /*
     * A blocked thread may be woken before the architecture has switched
     * away from its stack. If round-robin selects that exact generation,
     * cancel the pending ticket: the outgoing stack is still active.
     */
    if (pending_switch.valid &&
        scheduler_thread_handle_equal(pending_switch.handle, selected->handle))
    {
        pending_switch.valid = 0;
        pending_switch.handle.slot = 0U;
        pending_switch.handle.generation = 0U;
        pending_switch.sequence = 0U;
    }

    if (!pending_switch.valid && had_outgoing &&
        !scheduler_thread_handle_equal(outgoing_handle, selected->handle))
    {
        /* Preconditions were checked before changing current_slot. */
        if (scheduler_stage_switch_unlocked(outgoing_handle) != SYNTRA_STATUS_OK)
            return SYNTRA_STATUS_ERROR;
    }

    *cpu_context_out = selected->cpu_context;
    return SYNTRA_STATUS_OK;
}

static syntra_thread_t *scheduler_get_thread_unlocked(unsigned int thread_id)
{
    if (!scheduler_initialized || thread_id == 0U)
    {
        return NULL;
    }

    int slot = scheduler_find_slot(thread_id);
    return slot < 0 ? NULL : &threads[(unsigned int)slot];
}

static unsigned int scheduler_get_thread_count_unlocked(void)
{
    return scheduler_initialized ? thread_count : 0U;
}

static syntra_status_t scheduler_validate_unlocked(void)
{
    unsigned int running_count = 0U;

    if (!scheduler_initialized || thread_count > SYNTRA_MAX_THREADS)
    {
        return SYNTRA_STATUS_ERROR;
    }

    for (unsigned int i = 0U; i < thread_count; ++i)
    {
        const syntra_thread_t *thread = &threads[i];

        if (thread->handle.slot != i || thread->handle.generation == 0U ||
            thread->thread_id == 0U ||
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

        if (thread->is_user_thread > 1U || thread->has_process_owner > 1U)
            return SYNTRA_STATUS_ERROR;

        if (thread->stack_ownership == SYNTRA_THREAD_STACK_BORROWED)
        {
            if (thread->stack_allocation.slot != 0U ||
                thread->stack_allocation.generation != 0U)
                return SYNTRA_STATUS_ERROR;
        }
        else if (thread->stack_ownership == SYNTRA_THREAD_STACK_OWNED)
        {
            if (thread->stack_allocation.generation == 0U ||
                thread->stack_base == 0U ||
                thread->stack_size < SYNTRA_THREAD_MIN_STACK_SIZE)
                return SYNTRA_STATUS_ERROR;
        }
        else
            return SYNTRA_STATUS_ERROR;
        if (thread->has_process_owner == 0U &&
            (thread->owner_process.slot != 0U ||
             thread->owner_process.generation != 0U))
            return SYNTRA_STATUS_ERROR;
        if (thread->has_process_owner != 0U &&
            (thread->owner_process.generation == 0U ||
             thread->owner_process.slot >= 64U))
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

    if (reserved_thread_valid)
    {
        if (reserved_thread.handle.slot != thread_count ||
            reserved_thread.handle.generation == 0U ||
            reserved_thread.thread_id == 0U ||
            reserved_thread.state != THREAD_BLOCKED ||
            reserved_thread.has_process_owner != 1U ||
            reserved_thread.owner_process.generation == 0U ||
            reserved_thread.owner_process.slot >= SYNTRA_MAX_PROCESSES ||
            reserved_thread.is_user_thread != 0U ||
            reserved_thread.user_stack_base != 0U ||
            reserved_thread.user_stack_size != 0U ||
            reserved_thread.cpu_context != NULL ||
            reserved_thread.stack_base == 0U ||
            reserved_thread.stack_size < SYNTRA_THREAD_MIN_STACK_SIZE)
            return SYNTRA_STATUS_ERROR;

        if (reserved_thread.stack_ownership == SYNTRA_THREAD_STACK_BORROWED)
        {
            if (reserved_thread.stack_allocation.slot != 0U ||
                reserved_thread.stack_allocation.generation != 0U)
                return SYNTRA_STATUS_ERROR;
        }
        else if (reserved_thread.stack_ownership == SYNTRA_THREAD_STACK_OWNED)
        {
            if (reserved_thread.stack_allocation.generation == 0U)
                return SYNTRA_STATUS_ERROR;
        }
        else
            return SYNTRA_STATUS_ERROR;

        if (scheduler_find_slot(reserved_thread.thread_id) >= 0 ||
            scheduler_stack_range_conflicts(
                reserved_thread.stack_base, reserved_thread.stack_size))
            return SYNTRA_STATUS_ERROR;
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


/*
 * Scheduler metadata is shared with the ARM64 timer/exception path. On the
 * current single-core target, local IRQ masking makes each public operation
 * atomic with respect to IRQ re-entry. This is not an SMP lock. Legacy pointer
 * accessors remain unsafe after return; use snapshot APIs in new code.
 */
syntra_status_t scheduler_init(void)
{
    syntra_irq_state_t irq = syntra_irq_save();
    syntra_status_t status = scheduler_init_unlocked();
    syntra_irq_restore(irq);
    return status;
}

syntra_status_t scheduler_add_thread(unsigned int id, unsigned int priority)
{
    syntra_irq_state_t irq = syntra_irq_save();
    syntra_status_t status = scheduler_add_thread_unlocked(id, priority);
    syntra_irq_restore(irq);
    return status;
}

syntra_status_t scheduler_create_thread_with_handle(
    unsigned int id, unsigned int priority, syntra_thread_entry_t entry,
    void *argument, void *stack, size_t stack_size,
    syntra_thread_handle_t *handle_out)
{
    syntra_irq_state_t irq;
    syntra_status_t status;
    int slot;

    if (handle_out == NULL)
        return SYNTRA_STATUS_INVALID_ARGUMENT;

    irq = syntra_irq_save();
    status = scheduler_create_thread_unlocked(
        id, priority, entry, argument, stack, stack_size);
    if (status == SYNTRA_STATUS_OK)
    {
        slot = scheduler_find_slot(id);
        if (slot < 0)
            status = SYNTRA_STATUS_ERROR;
        else
            *handle_out = threads[(unsigned int)slot].handle;
    }
    syntra_irq_restore(irq);
    return status;
}

syntra_status_t scheduler_create_thread(
    unsigned int id, unsigned int priority, syntra_thread_entry_t entry,
    void *argument, void *stack, size_t stack_size)
{
    syntra_irq_state_t irq = syntra_irq_save();
    syntra_status_t status = scheduler_create_thread_unlocked(
        id, priority, entry, argument, stack, stack_size);
    syntra_irq_restore(irq);
    return status;
}

syntra_status_t scheduler_create_user_thread_with_handle(
    unsigned int id, unsigned int priority, syntra_thread_entry_t entry,
    void *argument, void *kernel_stack, size_t kernel_stack_size,
    void *user_stack, size_t user_stack_size, void *cpu_context,
    syntra_thread_handle_t *handle_out)
{
    syntra_irq_state_t irq;
    syntra_status_t status;
    int slot;
    if (handle_out == NULL) return SYNTRA_STATUS_INVALID_ARGUMENT;
    irq = syntra_irq_save();
    status = scheduler_create_user_thread_unlocked(
        id, priority, entry, argument, kernel_stack, kernel_stack_size,
        user_stack, user_stack_size, cpu_context);
    if (status == SYNTRA_STATUS_OK)
    {
        slot = scheduler_find_slot(id);
        if (slot < 0) status = SYNTRA_STATUS_ERROR;
        else *handle_out = threads[(unsigned int)slot].handle;
    }
    syntra_irq_restore(irq);
    return status;
}

syntra_status_t scheduler_create_user_thread(
    unsigned int id, unsigned int priority, syntra_thread_entry_t entry,
    void *argument, void *kernel_stack, size_t kernel_stack_size,
    void *user_stack, size_t user_stack_size, void *cpu_context)
{
    syntra_irq_state_t irq = syntra_irq_save();
    syntra_status_t status = scheduler_create_user_thread_unlocked(
        id, priority, entry, argument, kernel_stack, kernel_stack_size,
        user_stack, user_stack_size, cpu_context);
    syntra_irq_restore(irq);
    return status;
}


/*
 * Ownership-aware scheduler entry points. The process/thread coordinator
 * validates the handle while holding the outer local-IRQ critical section.
 * These functions retain their own nested save/restore for safe standalone
 * scheduler access and commit ownership only after successful insertion.
 */
syntra_status_t scheduler_create_owned_thread_with_handle(
    syntra_process_handle_t owner, unsigned int id, unsigned int priority,
    syntra_thread_entry_t entry, void *argument, void *stack, size_t stack_size,
    syntra_thread_handle_t *handle_out)
{
    syntra_irq_state_t irq = syntra_irq_save();
    syntra_status_t status;
    int slot;
    if (owner.generation == 0U || owner.slot >= SYNTRA_MAX_PROCESSES ||
        handle_out == NULL)
    {
        syntra_irq_restore(irq);
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }
    status = scheduler_create_thread_unlocked(
        id, priority, entry, argument, stack, stack_size);
    if (status == SYNTRA_STATUS_OK)
    {
        slot = scheduler_find_slot(id);
        if (slot < 0) status = SYNTRA_STATUS_ERROR;
        else
        {
            threads[(unsigned int)slot].owner_process = owner;
            threads[(unsigned int)slot].has_process_owner = 1U;
            *handle_out = threads[(unsigned int)slot].handle;
        }
    }
    syntra_irq_restore(irq);
    return status;
}

syntra_status_t scheduler_create_owned_user_thread_with_handle(
    syntra_process_handle_t owner, unsigned int id, unsigned int priority,
    syntra_thread_entry_t entry, void *argument,
    void *kernel_stack, size_t kernel_stack_size,
    void *user_stack, size_t user_stack_size, void *cpu_context,
    syntra_thread_handle_t *handle_out)
{
    syntra_irq_state_t irq = syntra_irq_save();
    syntra_status_t status;
    int slot;
    if (owner.generation == 0U || owner.slot >= SYNTRA_MAX_PROCESSES ||
        handle_out == NULL)
    {
        syntra_irq_restore(irq);
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }
    status = scheduler_create_user_thread_unlocked(
        id, priority, entry, argument, kernel_stack, kernel_stack_size,
        user_stack, user_stack_size, cpu_context);
    if (status == SYNTRA_STATUS_OK)
    {
        slot = scheduler_find_slot(id);
        if (slot < 0) status = SYNTRA_STATUS_ERROR;
        else
        {
            threads[(unsigned int)slot].owner_process = owner;
            threads[(unsigned int)slot].has_process_owner = 1U;
            *handle_out = threads[(unsigned int)slot].handle;
        }
    }
    syntra_irq_restore(irq);
    return status;
}

/*
 * Private reserve/attach/publish protocol. The reserved TCB lives outside the
 * published array, so neither scheduler selection path nor public snapshots
 * can observe it. The single pending slot matches the current single-core,
 * append-only scheduler model; this is not an SMP reservation primitive.
 */
syntra_status_t scheduler_reserve_owned_thread(
    syntra_process_handle_t owner, unsigned int id, unsigned int priority,
    syntra_thread_entry_t entry, void *argument, void *stack,
    size_t stack_size, syntra_thread_handle_t *handle_out)
{
    syntra_irq_state_t irq = syntra_irq_save();
    uintptr_t base = (uintptr_t)stack;
    syntra_status_t status = SYNTRA_STATUS_INVALID_ARGUMENT;

    if (owner.generation == 0U || owner.slot >= SYNTRA_MAX_PROCESSES ||
        id == 0U || entry == NULL || stack == NULL || handle_out == NULL ||
        stack_size < SYNTRA_THREAD_MIN_STACK_SIZE ||
        (base & (SYNTRA_THREAD_STACK_ALIGNMENT - 1U)) != 0U ||
        base > UINTPTR_MAX - stack_size)
        goto done;

    if (!scheduler_initialized || reserved_thread_valid)
    {
        status = SYNTRA_STATUS_ERROR;
        goto done;
    }
    if (scheduler_find_slot(id) >= 0)
        goto done;
    if (thread_count >= SYNTRA_MAX_THREADS)
    {
        status = SYNTRA_STATUS_OUT_OF_MEMORY;
        goto done;
    }
    if (scheduler_stack_range_conflicts(base, stack_size))
        goto done;

    scheduler_clear_reserved_thread();
    reserved_thread.handle.slot = thread_count;
    reserved_thread.handle.generation = 1U;
    reserved_thread.thread_id = id;
    reserved_thread.priority = priority;
    reserved_thread.state = THREAD_BLOCKED;
    reserved_thread.entry = entry;
    reserved_thread.argument = argument;
    reserved_thread.stack_base = base;
    reserved_thread.stack_size = stack_size;
    reserved_thread.stack_ownership = SYNTRA_THREAD_STACK_BORROWED;
    reserved_thread.owner_process = owner;
    reserved_thread.has_process_owner = 1U;
    reserved_thread_valid = 1;
    *handle_out = reserved_thread.handle;
    status = SYNTRA_STATUS_OK;

done:
    syntra_irq_restore(irq);
    return status;
}

syntra_status_t scheduler_attach_owned_stack(
    syntra_thread_handle_t handle,
    syntra_memory_allocation_handle_t allocation)
{
    syntra_irq_state_t irq = syntra_irq_save();
    syntra_memory_allocation_t snapshot;
    syntra_memory_owner_handle_t expected_owner;
    syntra_status_t status = SYNTRA_STATUS_INVALID_ARGUMENT;

    if (!reserved_thread_valid ||
        !scheduler_thread_handle_equal(handle, reserved_thread.handle) ||
        allocation.generation == 0U)
        goto done;

    expected_owner.slot = handle.slot;
    expected_owner.generation = handle.generation;
    status = memory_get_allocation_by_handle(allocation, &snapshot);
    if (status != SYNTRA_STATUS_OK)
        goto done;

    if (snapshot.ownership_state != SYNTRA_MEMORY_ALLOCATION_RESERVED ||
        snapshot.owner.slot != expected_owner.slot ||
        snapshot.owner.generation != expected_owner.generation ||
        snapshot.address != (void *)reserved_thread.stack_base ||
        snapshot.block_count >
            UINT32_MAX / SYNTRA_MEMORY_BLOCK_SIZE ||
        snapshot.block_count * SYNTRA_MEMORY_BLOCK_SIZE <
            reserved_thread.stack_size)
    {
        status = SYNTRA_STATUS_PERMISSION_DENIED;
        goto done;
    }

    reserved_thread.stack_ownership = SYNTRA_THREAD_STACK_OWNED;
    reserved_thread.stack_allocation = allocation;
    status = SYNTRA_STATUS_OK;

done:
    syntra_irq_restore(irq);
    return status;
}

syntra_status_t scheduler_publish_owned_thread(syntra_thread_handle_t handle)
{
    syntra_irq_state_t irq = syntra_irq_save();
    syntra_memory_allocation_t snapshot;
    syntra_memory_owner_handle_t expected_owner;
    syntra_status_t status = SYNTRA_STATUS_INVALID_ARGUMENT;

    if (!reserved_thread_valid ||
        !scheduler_thread_handle_equal(handle, reserved_thread.handle) ||
        reserved_thread.stack_ownership != SYNTRA_THREAD_STACK_OWNED ||
        reserved_thread.stack_allocation.generation == 0U ||
        thread_count >= SYNTRA_MAX_THREADS)
        goto done;

    expected_owner.slot = handle.slot;
    expected_owner.generation = handle.generation;
    status = memory_get_allocation_by_handle(
        reserved_thread.stack_allocation, &snapshot);
    if (status != SYNTRA_STATUS_OK)
        goto done;

    if (snapshot.ownership_state != SYNTRA_MEMORY_ALLOCATION_THREAD_OWNED ||
        snapshot.owner.slot != expected_owner.slot ||
        snapshot.owner.generation != expected_owner.generation ||
        snapshot.address != (void *)reserved_thread.stack_base ||
        snapshot.block_count >
            UINT32_MAX / SYNTRA_MEMORY_BLOCK_SIZE ||
        snapshot.block_count * SYNTRA_MEMORY_BLOCK_SIZE <
            reserved_thread.stack_size)
    {
        status = SYNTRA_STATUS_PERMISSION_DENIED;
        goto done;
    }

    /* Final visibility point: copy complete TCB, then increment published count. */
    reserved_thread.state = THREAD_READY;
    threads[thread_count] = reserved_thread;
    ++thread_count;
    scheduler_clear_reserved_thread();
    reserved_thread_valid = 0;
    status = SYNTRA_STATUS_OK;

done:
    syntra_irq_restore(irq);
    return status;
}

syntra_status_t scheduler_abort_reserved_thread(syntra_thread_handle_t handle)
{
    syntra_irq_state_t irq = syntra_irq_save();
    syntra_status_t status = SYNTRA_STATUS_INVALID_ARGUMENT;

    if (reserved_thread_valid &&
        scheduler_thread_handle_equal(handle, reserved_thread.handle))
    {
        scheduler_clear_reserved_thread();
        reserved_thread_valid = 0;
        status = SYNTRA_STATUS_OK;
    }

    syntra_irq_restore(irq);
    return status;
}

/*
 * Mark every thread owned by the exact process generation as terminated.
 * Thread records and caller-owned stacks remain reserved; no reaping occurs.
 * If the current thread belongs to this process, clear current_slot so the
 * scheduler cannot resume it. Caller holds the outer coordinator IRQ guard.
 */
syntra_status_t scheduler_terminate_process_threads(
    syntra_process_handle_t owner)
{
    syntra_irq_state_t irq = syntra_irq_save();
    if (owner.generation == 0U || owner.slot >= SYNTRA_MAX_PROCESSES)
    {
        syntra_irq_restore(irq);
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }
    if (!scheduler_initialized)
    {
        syntra_irq_restore(irq);
        return SYNTRA_STATUS_ERROR;
    }

    if (current_slot >= 0)
    {
        syntra_thread_t *current = &threads[(unsigned int)current_slot];
        if (current->has_process_owner != 0U &&
            current->owner_process.slot == owner.slot &&
            current->owner_process.generation == owner.generation)
        {
            if (scheduler_stage_switch_unlocked(current->handle) != SYNTRA_STATUS_OK)
            {
                syntra_irq_restore(irq);
                return SYNTRA_STATUS_ERROR;
            }
        }
    }

    for (unsigned int i = 0U; i < thread_count; ++i)
    {
        syntra_thread_t *thread = &threads[i];
        if (thread->has_process_owner != 0U &&
            thread->owner_process.slot == owner.slot &&
            thread->owner_process.generation == owner.generation)
        {
            thread->state = THREAD_TERMINATED;
            if (current_slot == (int)i)
                current_slot = -1;
        }
    }

    syntra_irq_restore(irq);
    return SYNTRA_STATUS_OK;
}

syntra_status_t scheduler_get_process_thread_count(
    syntra_process_handle_t owner, unsigned int *count_out)
{
    syntra_irq_state_t irq = syntra_irq_save();
    if (count_out == NULL || owner.generation == 0U || owner.slot >= 64U)
    {
        syntra_irq_restore(irq);
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }
    if (!scheduler_initialized)
    {
        syntra_irq_restore(irq);
        return SYNTRA_STATUS_ERROR;
    }
    unsigned int count = 0U;
    for (unsigned int i = 0U; i < thread_count; ++i)
        if (threads[i].has_process_owner != 0U &&
            threads[i].owner_process.slot == owner.slot &&
            threads[i].owner_process.generation == owner.generation)
            ++count;
    *count_out = count;
    syntra_irq_restore(irq);
    return SYNTRA_STATUS_OK;
}

syntra_status_t scheduler_get_process_thread_snapshot(
    syntra_process_handle_t owner, unsigned int index,
    syntra_thread_t *snapshot)
{
    syntra_irq_state_t irq = syntra_irq_save();
    if (snapshot == NULL || owner.generation == 0U || owner.slot >= 64U)
    {
        syntra_irq_restore(irq);
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }
    if (!scheduler_initialized)
    {
        syntra_irq_restore(irq);
        return SYNTRA_STATUS_ERROR;
    }
    unsigned int found = 0U;
    for (unsigned int i = 0U; i < thread_count; ++i)
    {
        if (threads[i].has_process_owner != 0U &&
            threads[i].owner_process.slot == owner.slot &&
            threads[i].owner_process.generation == owner.generation)
        {
            if (found == index)
            {
                *snapshot = threads[i];
                syntra_irq_restore(irq);
                return SYNTRA_STATUS_OK;
            }
            ++found;
        }
    }
    syntra_irq_restore(irq);
    return SYNTRA_STATUS_INVALID_ARGUMENT;
}

syntra_thread_t *scheduler_get_next_ready(void)
{
    syntra_irq_state_t irq = syntra_irq_save();
    syntra_thread_t *thread = scheduler_get_next_ready_unlocked();
    syntra_irq_restore(irq);
    return thread;
}

syntra_thread_t *scheduler_schedule_next(void)
{
    syntra_irq_state_t irq = syntra_irq_save();
    syntra_thread_t *thread = scheduler_schedule_next_unlocked();
    syntra_irq_restore(irq);
    return thread;
}

syntra_thread_t *scheduler_schedule_next_with_context(void)
{
    syntra_irq_state_t irq = syntra_irq_save();
    syntra_thread_t *thread = scheduler_schedule_next_with_context_unlocked();
    syntra_irq_restore(irq);
    return thread;
}

syntra_thread_t *scheduler_get_current(void)
{
    syntra_irq_state_t irq = syntra_irq_save();
    syntra_thread_t *thread = scheduler_get_current_unlocked();
    syntra_irq_restore(irq);
    return thread;
}

syntra_status_t scheduler_set_thread_state(unsigned int id, thread_state_t state)
{
    syntra_irq_state_t irq = syntra_irq_save();
    syntra_status_t status = scheduler_set_thread_state_unlocked(id, state);
    syntra_irq_restore(irq);
    return status;
}

syntra_status_t scheduler_wake_thread(unsigned int id)
{
    syntra_irq_state_t irq = syntra_irq_save();
    syntra_status_t status = scheduler_wake_thread_unlocked(id);
    syntra_irq_restore(irq);
    return status;
}

syntra_status_t scheduler_block_current(void)
{
    syntra_irq_state_t irq = syntra_irq_save();
    syntra_status_t status = scheduler_block_current_unlocked();
    syntra_irq_restore(irq);
    return status;
}

syntra_status_t scheduler_terminate_current(void)
{
    syntra_irq_state_t irq = syntra_irq_save();
    syntra_status_t status = scheduler_terminate_current_unlocked();
    syntra_irq_restore(irq);
    return status;
}

syntra_status_t scheduler_prepare_switch_away(
    syntra_thread_handle_t handle)
{
    syntra_irq_state_t irq = syntra_irq_save();
    int slot = scheduler_resolve_handle_unlocked(handle);
    syntra_status_t status;

    if (slot < 0 ||
        (threads[(unsigned int)slot].state != THREAD_BLOCKED &&
         threads[(unsigned int)slot].state != THREAD_TERMINATED) ||
        current_slot == slot)
    {
        syntra_irq_restore(irq);
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    status = scheduler_stage_switch_unlocked(handle);
    syntra_irq_restore(irq);
    return status;
}

syntra_status_t scheduler_complete_switch_away(void)
{
    syntra_irq_state_t irq = syntra_irq_save();
    int outgoing_slot;

    if (!pending_switch.valid ||
        switch_event_count >= SYNTRA_SWITCH_COMPLETION_CAPACITY ||
        current_slot < 0 ||
        (unsigned int)current_slot >= thread_count)
    {
        syntra_irq_restore(irq);
        return SYNTRA_STATUS_ERROR;
    }

    outgoing_slot = scheduler_resolve_handle_unlocked(pending_switch.handle);
    if (outgoing_slot < 0 || outgoing_slot == current_slot ||
        threads[(unsigned int)current_slot].state != THREAD_RUNNING ||
        threads[(unsigned int)outgoing_slot].state == THREAD_RUNNING)
    {
        syntra_irq_restore(irq);
        return SYNTRA_STATUS_ERROR;
    }

    switch_events[switch_event_tail].handle = pending_switch.handle;
    switch_events[switch_event_tail].sequence = pending_switch.sequence;
    switch_event_tail =
        (switch_event_tail + 1U) % SYNTRA_SWITCH_COMPLETION_CAPACITY;
    ++switch_event_count;

    pending_switch.valid = 0;
    pending_switch.handle.slot = 0U;
    pending_switch.handle.generation = 0U;
    pending_switch.sequence = 0U;

    syntra_irq_restore(irq);
    return SYNTRA_STATUS_OK;
}

syntra_status_t scheduler_take_switch_away_event(
    syntra_thread_switch_event_t *event_out)
{
    syntra_irq_state_t irq = syntra_irq_save();

    if (event_out == NULL)
    {
        syntra_irq_restore(irq);
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }
    if (switch_event_count == 0U)
    {
        syntra_irq_restore(irq);
        return SYNTRA_STATUS_ERROR;
    }

    *event_out = switch_events[switch_event_head];
    switch_event_head =
        (switch_event_head + 1U) % SYNTRA_SWITCH_COMPLETION_CAPACITY;
    --switch_event_count;

    syntra_irq_restore(irq);
    return SYNTRA_STATUS_OK;
}

syntra_status_t scheduler_get_thread_snapshot_by_handle(
    syntra_thread_handle_t handle, syntra_thread_t *out)
{
    syntra_irq_state_t irq = syntra_irq_save();
    syntra_status_t status =
        scheduler_get_thread_snapshot_by_handle_unlocked(handle, out);
    syntra_irq_restore(irq);
    return status;
}

syntra_status_t scheduler_set_thread_state_by_handle(
    syntra_thread_handle_t handle, thread_state_t state)
{
    syntra_irq_state_t irq = syntra_irq_save();
    syntra_status_t status =
        scheduler_set_thread_state_by_handle_unlocked(handle, state);
    syntra_irq_restore(irq);
    return status;
}

int scheduler_thread_handle_equal(
    syntra_thread_handle_t left, syntra_thread_handle_t right)
{
    return left.slot == right.slot &&
        left.generation == right.generation;
}

syntra_status_t scheduler_get_thread_snapshot(unsigned int id, syntra_thread_t *out)
{
    syntra_irq_state_t irq = syntra_irq_save();
    syntra_status_t status = scheduler_get_thread_snapshot_unlocked(id, out);
    syntra_irq_restore(irq);
    return status;
}

syntra_status_t scheduler_get_current_handle(
    syntra_thread_handle_t *handle_out)
{
    syntra_irq_state_t irq;
    if (handle_out == NULL) return SYNTRA_STATUS_INVALID_ARGUMENT;
    irq = syntra_irq_save();
    if (!scheduler_initialized || current_slot < 0)
    {
        syntra_irq_restore(irq);
        return SYNTRA_STATUS_ERROR;
    }
    *handle_out = threads[(unsigned int)current_slot].handle;
    syntra_irq_restore(irq);
    return SYNTRA_STATUS_OK;
}

syntra_status_t scheduler_get_current_snapshot(syntra_thread_t *out)
{
    syntra_irq_state_t irq = syntra_irq_save();
    syntra_status_t status = scheduler_get_current_snapshot_unlocked(out);
    syntra_irq_restore(irq);
    return status;
}

syntra_status_t scheduler_set_thread_context(unsigned int id, void *context)
{
    syntra_irq_state_t irq = syntra_irq_save();
    syntra_status_t status = scheduler_set_thread_context_unlocked(id, context);
    syntra_irq_restore(irq);
    return status;
}

syntra_status_t scheduler_schedule_next_context(void **context_out)
{
    syntra_irq_state_t irq = syntra_irq_save();
    syntra_status_t status = scheduler_schedule_next_context_unlocked(context_out);
    syntra_irq_restore(irq);
    return status;
}

syntra_thread_t *scheduler_get_thread(unsigned int id)
{
    syntra_irq_state_t irq = syntra_irq_save();
    syntra_thread_t *thread = scheduler_get_thread_unlocked(id);
    syntra_irq_restore(irq);
    return thread;
}

unsigned int scheduler_get_thread_count(void)
{
    syntra_irq_state_t irq = syntra_irq_save();
    unsigned int count = scheduler_get_thread_count_unlocked();
    syntra_irq_restore(irq);
    return count;
}

syntra_status_t scheduler_validate(void)
{
    syntra_irq_state_t irq = syntra_irq_save();
    syntra_status_t status = scheduler_validate_unlocked();
    syntra_irq_restore(irq);
    return status;
}