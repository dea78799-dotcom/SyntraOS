#include "stack_allocator.h"

#include <stdint.h>

#include "../arch/irq.h"

typedef struct
{
    uintptr_t base;
    size_t size;
    size_t pages;
    uint8_t live;
} stack_record_t;

static stack_record_t stack_records[SYNTRA_STACK_ALLOCATOR_MAX_STACKS];
static size_t live_count;

static int stack_find_record(uintptr_t base, size_t size, size_t pages)
{
    for (size_t i = 0U; i < SYNTRA_STACK_ALLOCATOR_MAX_STACKS; ++i)
    {
        const stack_record_t *record = &stack_records[i];
        if (record->live != 0U && record->base == base &&
            record->size == size && record->pages == pages)
            return (int)i;
    }
    return -1;
}

syntra_status_t syntra_stack_alloc(
    size_t requested_size,
    syntra_stack_allocation_t *allocation)
{
    size_t pages, actual_size;
    uintptr_t base;
    size_t slot;
    syntra_status_t status;

    if (allocation == NULL || requested_size == 0U ||
        requested_size > SIZE_MAX - (SYNTRA_PAGE_SIZE - 1U))
        return SYNTRA_STATUS_INVALID_ARGUMENT;

    pages = (requested_size + SYNTRA_PAGE_SIZE - 1U) / SYNTRA_PAGE_SIZE;
    if (pages > SIZE_MAX / SYNTRA_PAGE_SIZE)
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    actual_size = pages * SYNTRA_PAGE_SIZE;

    for (slot = 0U; slot < SYNTRA_STACK_ALLOCATOR_MAX_STACKS; ++slot)
        if (stack_records[slot].live == 0U)
            break;
    if (slot == SYNTRA_STACK_ALLOCATOR_MAX_STACKS)
        return SYNTRA_STATUS_OUT_OF_MEMORY;

    {
        syntra_irq_state_t irq_state = syntra_irq_save();
        status = syntra_page_alloc_contiguous_owned(
            pages, &base, SYNTRA_PAGE_OWNER_KERNEL);
        if (status == SYNTRA_STATUS_OK)
        {
            stack_records[slot].base = base;
            stack_records[slot].size = actual_size;
            stack_records[slot].pages = pages;
            stack_records[slot].live = 1U;
            ++live_count;

            allocation->physical_base = base;
            allocation->size = actual_size;
            allocation->page_count = pages;
        }
        syntra_irq_restore(irq_state);
    }
    return status;
}

syntra_status_t syntra_stack_free(
    const syntra_stack_allocation_t *allocation)
{
    int record_index;
    syntra_page_owner_t owner;

    if (allocation == NULL || allocation->physical_base == 0U ||
        allocation->size == 0U || allocation->page_count == 0U ||
        allocation->page_count > SIZE_MAX / SYNTRA_PAGE_SIZE ||
        allocation->size != allocation->page_count * SYNTRA_PAGE_SIZE ||
        allocation->physical_base % SYNTRA_PAGE_SIZE != 0U ||
        allocation->physical_base > UINTPTR_MAX - allocation->size)
        return SYNTRA_STATUS_INVALID_ARGUMENT;

    {
        syntra_irq_state_t irq_state = syntra_irq_save();
        record_index = stack_find_record(allocation->physical_base,
            allocation->size, allocation->page_count);
        if (record_index < 0)
        {
            syntra_irq_restore(irq_state);
            return SYNTRA_STATUS_INVALID_ARGUMENT;
        }

        /* Preflight every page before mutating allocator state. */
        for (size_t i = 0U; i < allocation->page_count; ++i)
        {
            uintptr_t address = allocation->physical_base +
                i * SYNTRA_PAGE_SIZE;
            if (syntra_page_get_owner(address, &owner) != SYNTRA_STATUS_OK ||
                owner != SYNTRA_PAGE_OWNER_KERNEL)
            {
                syntra_irq_restore(irq_state);
                return SYNTRA_STATUS_ERROR;
            }
        }

        for (size_t i = 0U; i < allocation->page_count; ++i)
        {
            syntra_status_t status = syntra_page_free_owned(
                allocation->physical_base + i * SYNTRA_PAGE_SIZE,
                SYNTRA_PAGE_OWNER_KERNEL);
            if (status != SYNTRA_STATUS_OK)
            {
                syntra_irq_restore(irq_state);
                return status;
            }
        }

        stack_records[(size_t)record_index].base = 0U;
        stack_records[(size_t)record_index].size = 0U;
        stack_records[(size_t)record_index].pages = 0U;
        stack_records[(size_t)record_index].live = 0U;
        --live_count;
        syntra_irq_restore(irq_state);
    }
    return SYNTRA_STATUS_OK;
}

syntra_status_t syntra_stack_allocator_validate(void)
{
    size_t observed = 0U;

    if (live_count > SYNTRA_STACK_ALLOCATOR_MAX_STACKS)
        return SYNTRA_STATUS_ERROR;

    for (size_t i = 0U; i < SYNTRA_STACK_ALLOCATOR_MAX_STACKS; ++i)
    {
        const stack_record_t *record = &stack_records[i];
        if (record->live == 0U)
        {
            if (record->base != 0U || record->size != 0U ||
                record->pages != 0U)
                return SYNTRA_STATUS_ERROR;
            continue;
        }

        ++observed;
        if (record->base == 0U || record->size == 0U ||
            record->pages == 0U ||
            record->pages > SIZE_MAX / SYNTRA_PAGE_SIZE ||
            record->size != record->pages * SYNTRA_PAGE_SIZE ||
            record->base % SYNTRA_PAGE_SIZE != 0U ||
            record->base > UINTPTR_MAX - record->size)
            return SYNTRA_STATUS_ERROR;

        for (size_t page = 0U; page < record->pages; ++page)
        {
            syntra_page_owner_t owner;
            if (syntra_page_get_owner(record->base + page * SYNTRA_PAGE_SIZE,
                    &owner) != SYNTRA_STATUS_OK ||
                owner != SYNTRA_PAGE_OWNER_KERNEL)
                return SYNTRA_STATUS_ERROR;
        }

        for (size_t j = i + 1U; j < SYNTRA_STACK_ALLOCATOR_MAX_STACKS; ++j)
        {
            const stack_record_t *other = &stack_records[j];
            if (other->live != 0U &&
                record->base < other->base + other->size &&
                other->base < record->base + record->size)
                return SYNTRA_STATUS_ERROR;
        }
    }

    return observed == live_count
        ? SYNTRA_STATUS_OK : SYNTRA_STATUS_ERROR;
}

size_t syntra_stack_allocator_live_count(void)
{
    return live_count;
}
