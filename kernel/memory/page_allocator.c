#include "page_allocator.h"

#include <stdint.h>

#include "memory_region.h"
#include "../arch/irq.h"

#define SYNTRA_PAGE_COUNT \
    ((size_t)(SYNTRA_PAGE_ALLOCATOR_MAX_PHYSICAL_ADDRESS / SYNTRA_PAGE_SIZE))
#define SYNTRA_BITMAP_WORD_BITS 64U
#define SYNTRA_BITMAP_WORD_COUNT (SYNTRA_PAGE_COUNT / SYNTRA_BITMAP_WORD_BITS)
#define SYNTRA_SUMMARY_WORD_COUNT \
    ((SYNTRA_BITMAP_WORD_COUNT + SYNTRA_BITMAP_WORD_BITS - 1U) / SYNTRA_BITMAP_WORD_BITS)

static uint64_t free_bitmap[SYNTRA_BITMAP_WORD_COUNT];
static uint64_t allocated_bitmap[SYNTRA_BITMAP_WORD_COUNT];
/* Each summary bit marks one free_bitmap word that contains at least one free page. */
static uint64_t free_summary[SYNTRA_SUMMARY_WORD_COUNT];
static size_t total_pages;
static size_t free_pages;
static size_t allocation_cursor;
static int initialized;
/* One byte per managed physical page; zero means the page is free. */
static uint8_t page_owners[SYNTRA_PAGE_COUNT];
static size_t owner_page_counts[SYNTRA_PAGE_OWNER_MAX];

static unsigned int bit_index(uint64_t value)
{
#if defined(__GNUC__) || defined(__clang__)
    return (unsigned int)__builtin_ctzll(value);
#else
    unsigned int index = 0U;
    while ((value & UINT64_C(1)) == 0U)
    {
        value >>= 1U;
        index++;
    }
    return index;
#endif
}

static int bitmap_test(const uint64_t *bitmap, size_t index)
{
    return (bitmap[index / SYNTRA_BITMAP_WORD_BITS] &
            (UINT64_C(1) << (index % SYNTRA_BITMAP_WORD_BITS))) != 0U;
}

static void bitmap_set(uint64_t *bitmap, size_t index)
{
    bitmap[index / SYNTRA_BITMAP_WORD_BITS] |=
        UINT64_C(1) << (index % SYNTRA_BITMAP_WORD_BITS);
}

static void bitmap_clear(uint64_t *bitmap, size_t index)
{
    bitmap[index / SYNTRA_BITMAP_WORD_BITS] &=
        ~(UINT64_C(1) << (index % SYNTRA_BITMAP_WORD_BITS));
}

static void free_page_set(size_t page_index)
{
    size_t word_index = page_index / SYNTRA_BITMAP_WORD_BITS;
    uint64_t old_word = free_bitmap[word_index];

    bitmap_set(free_bitmap, page_index);
    if (old_word == 0U)
    {
        bitmap_set(free_summary, word_index);
    }
}

static void free_page_clear(size_t page_index)
{
    size_t word_index = page_index / SYNTRA_BITMAP_WORD_BITS;

    bitmap_clear(free_bitmap, page_index);
    if (free_bitmap[word_index] == 0U)
    {
        bitmap_clear(free_summary, word_index);
    }
}

static uintptr_t align_up_page(uintptr_t address)
{
    uintptr_t remainder = address % (uintptr_t)SYNTRA_PAGE_SIZE;

    return remainder == 0U
        ? address
        : address + ((uintptr_t)SYNTRA_PAGE_SIZE - remainder);
}

/* Find a free page in [start_page, end_page), using summary bits to skip empty words. */
static int find_free_page_range(size_t start_page, size_t end_page, size_t *found_page)
{
    size_t first_word;
    size_t last_word;
    size_t summary_index;

    if (start_page >= end_page)
    {
        return 0;
    }

    first_word = start_page / SYNTRA_BITMAP_WORD_BITS;
    last_word = (end_page - 1U) / SYNTRA_BITMAP_WORD_BITS;

    for (summary_index = first_word / SYNTRA_BITMAP_WORD_BITS;
         summary_index <= last_word / SYNTRA_BITMAP_WORD_BITS;
         ++summary_index)
    {
        uint64_t candidates = free_summary[summary_index];
        size_t summary_first_word = summary_index * SYNTRA_BITMAP_WORD_BITS;
        size_t summary_last_word = summary_first_word + SYNTRA_BITMAP_WORD_BITS - 1U;

        if (first_word > summary_first_word)
        {
            candidates &= UINT64_MAX << (first_word - summary_first_word);
        }
        if (last_word < summary_last_word)
        {
            size_t valid_bits = last_word - summary_first_word + 1U;
            if (valid_bits < SYNTRA_BITMAP_WORD_BITS)
            {
                candidates &= (UINT64_C(1) << valid_bits) - 1U;
            }
        }

        while (candidates != 0U)
        {
            size_t word_index = summary_first_word + bit_index(candidates);
            uint64_t page_candidates = free_bitmap[word_index];
            size_t word_page_start = word_index * SYNTRA_BITMAP_WORD_BITS;
            size_t word_page_end = word_page_start + SYNTRA_BITMAP_WORD_BITS;

            if (start_page > word_page_start)
            {
                page_candidates &= UINT64_MAX << (start_page - word_page_start);
            }
            if (end_page < word_page_end)
            {
                size_t valid_bits = end_page - word_page_start;
                if (valid_bits < SYNTRA_BITMAP_WORD_BITS)
                {
                    page_candidates &= (UINT64_C(1) << valid_bits) - 1U;
                }
            }

            if (page_candidates != 0U)
            {
                *found_page = word_page_start + bit_index(page_candidates);
                return 1;
            }

            candidates &= candidates - 1U;
        }
    }

    return 0;
}

/*
 * Initialization rebuilds the full bitmap and is intentionally a boot-time
 * operation. Call it before enabling interrupts and before publishing the
 * allocator to other kernel subsystems; it is not an IRQ-safe reinit path.
 */
syntra_status_t syntra_page_allocator_init(void)
{
    size_t index;

    for (index = 0U; index < SYNTRA_PAGE_COUNT; ++index)
    {
        page_owners[index] = (uint8_t)SYNTRA_PAGE_OWNER_NONE;
    }
    for (index = 0U; index < SYNTRA_PAGE_OWNER_MAX; ++index)
    {
        owner_page_counts[index] = 0U;
    }

    for (index = 0U; index < SYNTRA_BITMAP_WORD_COUNT; ++index)
    {
        free_bitmap[index] = 0U;
        allocated_bitmap[index] = 0U;
    }
    for (index = 0U; index < SYNTRA_SUMMARY_WORD_COUNT; ++index)
    {
        free_summary[index] = 0U;
    }

    total_pages = 0U;
    free_pages = 0U;
    allocation_cursor = 0U;
    initialized = 0;

    for (index = 0U; index < syntra_memory_region_count(); ++index)
    {
        syntra_memory_region_t region;
        uintptr_t end;
        uintptr_t first;
        uintptr_t limit;
        uintptr_t address;

        if (syntra_memory_region_get(index, &region) != SYNTRA_STATUS_OK)
        {
            return SYNTRA_STATUS_ERROR;
        }

        if (region.type != SYNTRA_MEMORY_REGION_RAM)
        {
            continue;
        }

        if (region.size == 0U || region.base > UINTPTR_MAX - region.size)
        {
            return SYNTRA_STATUS_INVALID_ARGUMENT;
        }

        end = region.base + region.size;

        /* V1 manages only physical addresses below 4 GiB. */
        if (region.base >= (uintptr_t)SYNTRA_PAGE_ALLOCATOR_MAX_PHYSICAL_ADDRESS)
        {
            continue;
        }
        if ((uint64_t)end > SYNTRA_PAGE_ALLOCATOR_MAX_PHYSICAL_ADDRESS)
        {
            end = (uintptr_t)SYNTRA_PAGE_ALLOCATOR_MAX_PHYSICAL_ADDRESS;
        }

        if (region.base > UINTPTR_MAX - ((uintptr_t)SYNTRA_PAGE_SIZE - 1U))
        {
            return SYNTRA_STATUS_INVALID_ARGUMENT;
        }

        first = align_up_page(region.base);
        limit = end - (end % (uintptr_t)SYNTRA_PAGE_SIZE);
        if (first >= limit)
        {
            continue;
        }

        for (address = first; address < limit;
             address += (uintptr_t)SYNTRA_PAGE_SIZE)
        {
            size_t page_index = (size_t)(address / (uintptr_t)SYNTRA_PAGE_SIZE);

            /*
             * Physical address zero is the null/sentinel root value across
             * the page-table APIs, so it must never be returned as a frame.
             */
            if (page_index == 0U)
            {
                continue;
            }

            if (!bitmap_test(free_bitmap, page_index))
            {
                free_page_set(page_index);
                total_pages++;
                free_pages++;
            }
        }
    }

    if (total_pages == 0U)
    {
        return SYNTRA_STATUS_OUT_OF_MEMORY;
    }

    initialized = 1;
    return SYNTRA_STATUS_OK;
}

static syntra_status_t page_alloc_unlocked(
    uintptr_t *physical_address, syntra_page_owner_t owner)
{
    size_t index;

    if (!initialized || physical_address == NULL ||
        owner <= SYNTRA_PAGE_OWNER_NONE || owner >= SYNTRA_PAGE_OWNER_MAX)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (free_pages == 0U)
    {
        return SYNTRA_STATUS_OUT_OF_MEMORY;
    }

    if (!find_free_page_range(allocation_cursor, SYNTRA_PAGE_COUNT, &index) &&
        !find_free_page_range(0U, allocation_cursor, &index))
    {
        return SYNTRA_STATUS_ERROR;
    }

    free_page_clear(index);
    bitmap_set(allocated_bitmap, index);
    page_owners[index] = (uint8_t)owner;
    owner_page_counts[owner]++;
    free_pages--;

    allocation_cursor = index + 1U;
    if (allocation_cursor == SYNTRA_PAGE_COUNT)
    {
        allocation_cursor = 0U;
    }

    *physical_address = (uintptr_t)(index * SYNTRA_PAGE_SIZE);
    return SYNTRA_STATUS_OK;
}


static int find_free_contiguous_range(
    size_t start_page,
    size_t end_page,
    size_t page_count,
    size_t *found_page)
{
    size_t candidate;

    if (page_count == 0U || start_page >= end_page ||
        page_count > end_page - start_page)
        return 0;

    candidate = start_page;
    while (candidate <= end_page - page_count)
    {
        size_t offset;
        for (offset = 0U; offset < page_count; ++offset)
        {
            if (!bitmap_test(free_bitmap, candidate + offset))
            {
                candidate += offset + 1U;
                break;
            }
        }

        if (offset == page_count)
        {
            *found_page = candidate;
            return 1;
        }
    }

    return 0;
}

static syntra_status_t page_alloc_contiguous_unlocked(
    size_t page_count,
    uintptr_t *physical_address,
    syntra_page_owner_t owner)
{
    size_t index;

    if (!initialized || physical_address == NULL || page_count == 0U ||
        page_count > SYNTRA_PAGE_COUNT ||
        owner <= SYNTRA_PAGE_OWNER_NONE || owner >= SYNTRA_PAGE_OWNER_MAX)
        return SYNTRA_STATUS_INVALID_ARGUMENT;

    if (free_pages < page_count)
        return SYNTRA_STATUS_OUT_OF_MEMORY;

    /* The helper receives the exclusive physical scan limit, not the last
     * candidate-start limit; it accounts for page_count itself. */
    if (!find_free_contiguous_range(
            allocation_cursor, SYNTRA_PAGE_COUNT, page_count, &index) &&
        !find_free_contiguous_range(
            0U, allocation_cursor, page_count, &index))
        return SYNTRA_STATUS_OUT_OF_MEMORY;

    {
        size_t offset;
        for (offset = 0U; offset < page_count; ++offset)
        {
            free_page_clear(index + offset);
            bitmap_set(allocated_bitmap, index + offset);
            page_owners[index + offset] = (uint8_t)owner;
            owner_page_counts[owner]++;
        }
    }

    free_pages -= page_count;
    allocation_cursor = index + page_count;
    if (allocation_cursor >= SYNTRA_PAGE_COUNT)
        allocation_cursor = 0U;

    *physical_address = (uintptr_t)(index * SYNTRA_PAGE_SIZE);
    return SYNTRA_STATUS_OK;
}

static syntra_status_t page_free_unlocked(
    uintptr_t physical_address,
    syntra_page_owner_t expected_owner,
    int enforce_owner)
{
    size_t index;

    if (!initialized ||
        physical_address % (uintptr_t)SYNTRA_PAGE_SIZE != 0U ||
        (uint64_t)physical_address >= SYNTRA_PAGE_ALLOCATOR_MAX_PHYSICAL_ADDRESS)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    index = (size_t)(physical_address / (uintptr_t)SYNTRA_PAGE_SIZE);
    if (!bitmap_test(allocated_bitmap, index))
    {
        return SYNTRA_STATUS_ERROR;
    }
    if (page_owners[index] <= (uint8_t)SYNTRA_PAGE_OWNER_NONE ||
        page_owners[index] >= (uint8_t)SYNTRA_PAGE_OWNER_MAX ||
        owner_page_counts[page_owners[index]] == 0U)
    {
        return SYNTRA_STATUS_ERROR;
    }
    if (enforce_owner && page_owners[index] != (uint8_t)expected_owner)
    {
        return SYNTRA_STATUS_PERMISSION_DENIED;
    }

    bitmap_clear(allocated_bitmap, index);
    owner_page_counts[page_owners[index]]--;
    page_owners[index] = (uint8_t)SYNTRA_PAGE_OWNER_NONE;
    free_page_set(index);
    free_pages++;
    if (index < allocation_cursor)
    {
        allocation_cursor = index;
    }

    return SYNTRA_STATUS_OK;
}

static size_t page_allocator_free_count_unlocked(void)
{
    return initialized ? free_pages : 0U;
}

static size_t page_allocator_total_count_unlocked(void)
{
    return initialized ? total_pages : 0U;
}

/*
 * Full metadata audit for diagnostics and tests. This is deliberately not
 * called on the allocation fast path: it scans the complete 4 GiB bitmap.
 */
static unsigned int count_set_bits(uint64_t value)
{
    unsigned int count = 0U;
    while (value != 0U)
    {
        value &= value - 1U;
        ++count;
    }
    return count;
}

static syntra_status_t page_allocator_validate_unlocked(void)
{
    size_t word_index;
    size_t observed_free = 0U;
    size_t observed_allocated = 0U;
    size_t observed_owner_pages[SYNTRA_PAGE_OWNER_MAX] = {0U};

    if (!initialized)
        return SYNTRA_STATUS_INVALID_ARGUMENT;

    for (word_index = 0U; word_index < SYNTRA_BITMAP_WORD_COUNT; ++word_index)
    {
        uint64_t free_word = free_bitmap[word_index];
        uint64_t allocated_word = allocated_bitmap[word_index];
        size_t summary_word = word_index / SYNTRA_BITMAP_WORD_BITS;
        size_t summary_bit = word_index % SYNTRA_BITMAP_WORD_BITS;
        int summary_marks_free =
            (free_summary[summary_word] &
             (UINT64_C(1) << summary_bit)) != 0U;

        if ((free_word & allocated_word) != 0U ||
            summary_marks_free != (free_word != 0U))
            return SYNTRA_STATUS_ERROR;

        observed_free += (size_t)count_set_bits(free_word);
        observed_allocated += (size_t)count_set_bits(allocated_word);
    }

    for (word_index = 0U; word_index < SYNTRA_PAGE_COUNT; ++word_index)
    {
        int allocated = bitmap_test(allocated_bitmap, word_index);
        uint8_t owner = page_owners[word_index];
        if ((allocated && (owner <= (uint8_t)SYNTRA_PAGE_OWNER_NONE ||
                           owner >= (uint8_t)SYNTRA_PAGE_OWNER_MAX)) ||
            (!allocated && owner != (uint8_t)SYNTRA_PAGE_OWNER_NONE))
            return SYNTRA_STATUS_ERROR;
        if (allocated)
            observed_owner_pages[owner]++;
    }

    {
        size_t owner_index;
        for (owner_index = 0U; owner_index < SYNTRA_PAGE_OWNER_MAX; ++owner_index)
        {
            if (observed_owner_pages[owner_index] != owner_page_counts[owner_index])
                return SYNTRA_STATUS_ERROR;
        }
    }

    if (observed_free != free_pages ||
        observed_free + observed_allocated != total_pages ||
        total_pages > SYNTRA_PAGE_COUNT)
        return SYNTRA_STATUS_ERROR;

    return SYNTRA_STATUS_OK;
}

/*
 * These operations mutate or inspect shared allocator metadata. On the
 * current single-core ARM64 target, masking local IRQ exceptions prevents an
 * interrupt handler from re-entering the allocator halfway through an
 * update. This is not an SMP lock and does not make the allocator safe for
 * concurrent access from multiple CPU cores.
 */
syntra_status_t syntra_page_alloc_owned(
    uintptr_t *physical_address, syntra_page_owner_t owner)
{
    syntra_irq_state_t irq_state = syntra_irq_save();
    syntra_status_t status = page_alloc_unlocked(physical_address, owner);
    syntra_irq_restore(irq_state);
    return status;
}

syntra_status_t syntra_page_alloc(uintptr_t *physical_address)
{
    return syntra_page_alloc_owned(physical_address, SYNTRA_PAGE_OWNER_KERNEL);
}


syntra_status_t syntra_page_alloc_contiguous_owned(
    size_t page_count, uintptr_t *physical_address,
    syntra_page_owner_t owner)
{
    syntra_irq_state_t irq_state = syntra_irq_save();
    syntra_status_t status = page_alloc_contiguous_unlocked(
        page_count, physical_address, owner);
    syntra_irq_restore(irq_state);
    return status;
}

syntra_status_t syntra_page_alloc_contiguous(
    size_t page_count, uintptr_t *physical_address)
{
    return syntra_page_alloc_contiguous_owned(
        page_count, physical_address, SYNTRA_PAGE_OWNER_KERNEL);
}

syntra_status_t syntra_page_free_owned(
    uintptr_t physical_address, syntra_page_owner_t expected_owner)
{
    syntra_irq_state_t irq_state = syntra_irq_save();
    syntra_status_t status;
    if (expected_owner <= SYNTRA_PAGE_OWNER_NONE ||
        expected_owner >= SYNTRA_PAGE_OWNER_MAX)
        status = SYNTRA_STATUS_INVALID_ARGUMENT;
    else
        status = page_free_unlocked(physical_address, expected_owner, 1);
    syntra_irq_restore(irq_state);
    return status;
}

syntra_status_t syntra_page_free(uintptr_t physical_address)
{
    syntra_irq_state_t irq_state = syntra_irq_save();
    syntra_status_t status = page_free_unlocked(physical_address,
        SYNTRA_PAGE_OWNER_NONE, 0);
    syntra_irq_restore(irq_state);
    return status;
}

syntra_status_t syntra_page_get_owner(
    uintptr_t physical_address, syntra_page_owner_t *owner)
{
    syntra_irq_state_t irq_state;
    size_t index;
    syntra_status_t status = SYNTRA_STATUS_OK;
    if (owner == NULL ||
        physical_address % (uintptr_t)SYNTRA_PAGE_SIZE != 0U ||
        (uint64_t)physical_address >= SYNTRA_PAGE_ALLOCATOR_MAX_PHYSICAL_ADDRESS)
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    irq_state = syntra_irq_save();
    index = (size_t)(physical_address / (uintptr_t)SYNTRA_PAGE_SIZE);
    if (!initialized)
        status = SYNTRA_STATUS_INVALID_ARGUMENT;
    else if (!bitmap_test(allocated_bitmap, index))
        status = SYNTRA_STATUS_ERROR;
    else
        *owner = (syntra_page_owner_t)page_owners[index];
    syntra_irq_restore(irq_state);
    return status;
}

size_t syntra_page_allocator_free_count(void)
{
    syntra_irq_state_t irq_state = syntra_irq_save();
    size_t count = page_allocator_free_count_unlocked();
    syntra_irq_restore(irq_state);
    return count;
}

size_t syntra_page_allocator_total_count(void)
{
    syntra_irq_state_t irq_state = syntra_irq_save();
    size_t count = page_allocator_total_count_unlocked();
    syntra_irq_restore(irq_state);
    return count;
}


syntra_status_t syntra_page_allocator_get_stats(
    syntra_page_allocator_stats_t *stats)
{
    syntra_irq_state_t irq_state;
    size_t owner_index;

    if (stats == NULL)
        return SYNTRA_STATUS_INVALID_ARGUMENT;

    irq_state = syntra_irq_save();
    if (!initialized)
    {
        syntra_irq_restore(irq_state);
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    stats->total_pages = total_pages;
    stats->free_pages = free_pages;
    stats->allocated_pages = total_pages - free_pages;
    for (owner_index = 0U; owner_index < SYNTRA_PAGE_OWNER_MAX; ++owner_index)
        stats->owner_pages[owner_index] = owner_page_counts[owner_index];
    stats->valid = 1;
    syntra_irq_restore(irq_state);
    return SYNTRA_STATUS_OK;
}

syntra_status_t syntra_page_allocator_validate(void)
{
    syntra_irq_state_t irq_state = syntra_irq_save();
    syntra_status_t status = page_allocator_validate_unlocked();
    syntra_irq_restore(irq_state);
    return status;
}
