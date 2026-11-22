#include "kernel_va_registry.h"

#include "../../arch/irq.h"

typedef struct
{
    uint8_t active;
    syntra_arm64_kernel_va_reservation_t reservation;
} registry_slot_t;

static registry_slot_t registry[SYNTRA_ARM64_KERNEL_VA_REGISTRY_CAPACITY];
static uint64_t next_allocation_id = 1U;
static size_t live_count;
static int initialized;

static int range_end(uint64_t base, uint64_t size, uint64_t *end)
{
    if (size == 0U || base > UINT64_MAX - size)
        return 0;
    *end = base + size;
    return 1;
}

static int reservation_equal(
    const syntra_arm64_kernel_va_reservation_t *left,
    const syntra_arm64_kernel_va_reservation_t *right)
{
    return left->allocation_id == right->allocation_id &&
        left->owner == right->owner &&
        left->guard_pages == right->guard_pages &&
        left->requested_size == right->requested_size &&
        left->reservation_base == right->reservation_base &&
        left->reservation_size == right->reservation_size &&
        left->usable_base == right->usable_base &&
        left->usable_size == right->usable_size;
}

syntra_status_t syntra_arm64_kernel_va_registry_init(void)
{
    size_t slot;

    if (initialized)
        return SYNTRA_STATUS_OK;

    for (slot = 0U; slot < SYNTRA_ARM64_KERNEL_VA_REGISTRY_CAPACITY; ++slot)
    {
        registry[slot].active = 0U;
        registry[slot].reservation = (syntra_arm64_kernel_va_reservation_t){0};
    }

    live_count = 0U;
    next_allocation_id = 1U;
    initialized = 1;
    return SYNTRA_STATUS_OK;
}

static syntra_status_t find_first_fit(
    uint64_t total_size,
    uint64_t *base_out)
{
    uint64_t candidate = SYNTRA_ARM64_KERNEL_VA_BASE;
    const uint64_t arena_end = SYNTRA_ARM64_KERNEL_VA_LIMIT;
    size_t iteration;

    if (base_out == NULL || total_size == 0U ||
        total_size > SYNTRA_ARM64_KERNEL_VA_SIZE)
        return SYNTRA_STATUS_INVALID_ARGUMENT;

    /* Bounded restart: each collision advances candidate past a live range. */
    for (iteration = 0U;
         iteration < SYNTRA_ARM64_KERNEL_VA_REGISTRY_CAPACITY;
         ++iteration)
    {
        uint64_t candidate_end;
        size_t slot;
        int collision = 0;

        if (candidate > arena_end - total_size)
            return SYNTRA_STATUS_OUT_OF_MEMORY;
        candidate_end = candidate + total_size;

        for (slot = 0U; slot < SYNTRA_ARM64_KERNEL_VA_REGISTRY_CAPACITY;
             ++slot)
        {
            uint64_t existing_end;
            const syntra_arm64_kernel_va_reservation_t *existing;

            if (!registry[slot].active)
                continue;

            existing = &registry[slot].reservation;
            if (!range_end(existing->reservation_base,
                           existing->reservation_size, &existing_end))
                return SYNTRA_STATUS_ERROR;

            if (candidate < existing_end &&
                existing->reservation_base < candidate_end)
            {
                candidate = existing_end;
                collision = 1;
                break;
            }
        }

        if (!collision)
        {
            *base_out = candidate;
            return SYNTRA_STATUS_OK;
        }
    }

    return SYNTRA_STATUS_OUT_OF_MEMORY;
}

static syntra_status_t reserve_unlocked(
    uint64_t requested_size,
    uint32_t guard_pages,
    uint32_t owner,
    syntra_arm64_kernel_va_reservation_t *reservation)
{
    uint64_t rounded_size;
    uint64_t guard_size;
    uint64_t total_size;
    uint64_t base;
    uint64_t usable_base;
    size_t free_slot = SYNTRA_ARM64_KERNEL_VA_REGISTRY_CAPACITY;
    size_t slot;
    syntra_status_t status;
    syntra_arm64_kernel_va_reservation_t created;

    if (!initialized || reservation == NULL || requested_size == 0U ||
        owner == 0U)
        return SYNTRA_STATUS_INVALID_ARGUMENT;

    if (requested_size > UINT64_MAX -
        (SYNTRA_ARM64_KERNEL_VA_PAGE_SIZE - 1U))
        return SYNTRA_STATUS_INVALID_ARGUMENT;

    rounded_size = (requested_size +
        (SYNTRA_ARM64_KERNEL_VA_PAGE_SIZE - 1U)) &
        ~(SYNTRA_ARM64_KERNEL_VA_PAGE_SIZE - 1U);
    guard_size = (uint64_t)guard_pages *
        SYNTRA_ARM64_KERNEL_VA_PAGE_SIZE;

    if (guard_size > UINT64_MAX - rounded_size)
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    total_size = guard_size + rounded_size;

    if (total_size == 0U ||
        total_size > SYNTRA_ARM64_KERNEL_VA_SIZE)
        return SYNTRA_STATUS_OUT_OF_MEMORY;

    if (next_allocation_id == 0U)
        return SYNTRA_STATUS_OUT_OF_MEMORY;

    for (slot = 0U; slot < SYNTRA_ARM64_KERNEL_VA_REGISTRY_CAPACITY;
         ++slot)
    {
        if (!registry[slot].active)
        {
            free_slot = slot;
            break;
        }
    }

    if (free_slot == SYNTRA_ARM64_KERNEL_VA_REGISTRY_CAPACITY)
        return SYNTRA_STATUS_OUT_OF_MEMORY;

    status = find_first_fit(total_size, &base);
    if (status != SYNTRA_STATUS_OK)
        return status;

    usable_base = base + guard_size;
    created = (syntra_arm64_kernel_va_reservation_t) {
        next_allocation_id,
        owner,
        guard_pages,
        requested_size,
        base,
        total_size,
        usable_base,
        rounded_size
    };

    /* Commit state only after every validation and placement step succeeds. */
    registry[free_slot].reservation = created;
    registry[free_slot].active = 1U;
    ++live_count;
    ++next_allocation_id;
    *reservation = created;

    return SYNTRA_STATUS_OK;
}

static syntra_status_t release_unlocked(
    const syntra_arm64_kernel_va_reservation_t *reservation)
{
    size_t slot;

    if (!initialized || reservation == NULL ||
        reservation->allocation_id == 0U)
        return SYNTRA_STATUS_INVALID_ARGUMENT;

    for (slot = 0U; slot < SYNTRA_ARM64_KERNEL_VA_REGISTRY_CAPACITY;
         ++slot)
    {
        if (registry[slot].active &&
            registry[slot].reservation.allocation_id ==
                reservation->allocation_id)
        {
            if (!reservation_equal(
                    &registry[slot].reservation, reservation))
                return SYNTRA_STATUS_INVALID_ARGUMENT;

            registry[slot].active = 0U;
            registry[slot].reservation =
                (syntra_arm64_kernel_va_reservation_t){0};
            --live_count;
            return SYNTRA_STATUS_OK;
        }
    }

    return SYNTRA_STATUS_INVALID_ARGUMENT;
}

static syntra_status_t validate_unlocked(void)
{
    size_t slot;
    size_t counted = 0U;

    if (!initialized)
        return SYNTRA_STATUS_ERROR;

    for (slot = 0U; slot < SYNTRA_ARM64_KERNEL_VA_REGISTRY_CAPACITY;
         ++slot)
    {
        const syntra_arm64_kernel_va_reservation_t *current;
        uint64_t current_end;
        uint64_t guard_size;

        if (!registry[slot].active)
            continue;

        ++counted;
        current = &registry[slot].reservation;
        guard_size = (uint64_t)current->guard_pages *
            SYNTRA_ARM64_KERNEL_VA_PAGE_SIZE;

        uint64_t rounded_requested;

        if (current->requested_size > UINT64_MAX -
            (SYNTRA_ARM64_KERNEL_VA_PAGE_SIZE - 1U))
            return SYNTRA_STATUS_ERROR;
        rounded_requested = (current->requested_size +
            (SYNTRA_ARM64_KERNEL_VA_PAGE_SIZE - 1U)) &
            ~(SYNTRA_ARM64_KERNEL_VA_PAGE_SIZE - 1U);

        if (current->allocation_id == 0U || current->owner == 0U ||
            current->requested_size == 0U ||
            current->usable_size == 0U ||
            current->usable_size != rounded_requested ||
            (current->reservation_base &
                (SYNTRA_ARM64_KERNEL_VA_PAGE_SIZE - 1U)) != 0U ||
            (current->usable_base &
                (SYNTRA_ARM64_KERNEL_VA_PAGE_SIZE - 1U)) != 0U ||
            (current->usable_size &
                (SYNTRA_ARM64_KERNEL_VA_PAGE_SIZE - 1U)) != 0U ||
            guard_size > UINT64_MAX - current->usable_size ||
            current->reservation_base > UINT64_MAX - guard_size ||
            current->usable_base != current->reservation_base + guard_size ||
            current->reservation_size != guard_size + current->usable_size ||
            current->reservation_base < SYNTRA_ARM64_KERNEL_VA_BASE ||
            !range_end(current->reservation_base,
                       current->reservation_size, &current_end) ||
            current_end > SYNTRA_ARM64_KERNEL_VA_LIMIT)
            return SYNTRA_STATUS_ERROR;

        {
            size_t other;
            for (other = slot + 1U;
                 other < SYNTRA_ARM64_KERNEL_VA_REGISTRY_CAPACITY;
                 ++other)
            {
                uint64_t other_end;
                const syntra_arm64_kernel_va_reservation_t *next;

                if (!registry[other].active)
                    continue;
                next = &registry[other].reservation;

                if (next->allocation_id == current->allocation_id ||
                    !range_end(next->reservation_base,
                              next->reservation_size, &other_end) ||
                    (current->reservation_base < other_end &&
                     next->reservation_base < current_end))
                    return SYNTRA_STATUS_ERROR;
            }
        }
    }

    return counted == live_count
        ? SYNTRA_STATUS_OK
        : SYNTRA_STATUS_ERROR;
}

static size_t registry_count_unlocked(void)
{
    return initialized ? live_count : 0U;
}

static syntra_status_t registry_get_unlocked(
    size_t slot,
    syntra_arm64_kernel_va_reservation_t *reservation)
{
    if (!initialized || reservation == NULL ||
        slot >= SYNTRA_ARM64_KERNEL_VA_REGISTRY_CAPACITY)
        return SYNTRA_STATUS_INVALID_ARGUMENT;

    if (!registry[slot].active)
        return SYNTRA_STATUS_NOT_IMPLEMENTED;

    *reservation = registry[slot].reservation;
    return SYNTRA_STATUS_OK;
}

/* IRQ masking prevents same-core interrupt re-entry; this is not an SMP lock. */
syntra_status_t syntra_arm64_kernel_va_reserve(uint64_t requested_size, uint32_t guard_pages, uint32_t owner, syntra_arm64_kernel_va_reservation_t *reservation)
{
    syntra_irq_state_t state = syntra_irq_save();
    syntra_status_t status = reserve_unlocked(requested_size, guard_pages, owner, reservation);
    syntra_irq_restore(state);
    return status;
}
syntra_status_t syntra_arm64_kernel_va_release(const syntra_arm64_kernel_va_reservation_t *reservation)
{
    syntra_irq_state_t state = syntra_irq_save();
    syntra_status_t status = release_unlocked(reservation);
    syntra_irq_restore(state);
    return status;
}
syntra_status_t syntra_arm64_kernel_va_registry_validate(void)
{
    syntra_irq_state_t state = syntra_irq_save();
    syntra_status_t status = validate_unlocked();
    syntra_irq_restore(state);
    return status;
}
size_t syntra_arm64_kernel_va_registry_count(void)
{
    syntra_irq_state_t state = syntra_irq_save();
    size_t count = registry_count_unlocked();
    syntra_irq_restore(state);
    return count;
}
syntra_status_t syntra_arm64_kernel_va_registry_get(size_t slot, syntra_arm64_kernel_va_reservation_t *reservation)
{
    syntra_irq_state_t state = syntra_irq_save();
    syntra_status_t status = registry_get_unlocked(slot, reservation);
    syntra_irq_restore(state);
    return status;
}
