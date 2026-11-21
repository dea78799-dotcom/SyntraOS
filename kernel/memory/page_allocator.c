#include "page_allocator.h"

#include <stdint.h>

#include "memory_region.h"

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
            candidates &= (UINT64_C(1) << valid_bits) - 1U;
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
                page_candidates &= (UINT64_C(1) << valid_bits) - 1U;
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

syntra_status_t syntra_page_allocator_init(void)
{
    size_t index;

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

            free_page_set(page_index);
            total_pages++;
            free_pages++;
        }
    }

    if (total_pages == 0U)
    {
        return SYNTRA_STATUS_OUT_OF_MEMORY;
    }

    initialized = 1;
    return SYNTRA_STATUS_OK;
}

syntra_status_t syntra_page_alloc(uintptr_t *physical_address)
{
    size_t index;

    if (!initialized || physical_address == NULL)
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
    free_pages--;

    allocation_cursor = index + 1U;
    if (allocation_cursor == SYNTRA_PAGE_COUNT)
    {
        allocation_cursor = 0U;
    }

    *physical_address = (uintptr_t)(index * SYNTRA_PAGE_SIZE);
    return SYNTRA_STATUS_OK;
}

syntra_status_t syntra_page_free(uintptr_t physical_address)
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

    bitmap_clear(allocated_bitmap, index);
    free_page_set(index);
    free_pages++;
    if (index < allocation_cursor)
    {
        allocation_cursor = index;
    }

    return SYNTRA_STATUS_OK;
}

size_t syntra_page_allocator_free_count(void)
{
    return initialized ? free_pages : 0U;
}

size_t syntra_page_allocator_total_count(void)
{
    return initialized ? total_pages : 0U;
}
