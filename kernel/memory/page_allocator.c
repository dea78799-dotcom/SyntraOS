#include "page_allocator.h"

#include <stdint.h>

#include "memory_region.h"

#define SYNTRA_PAGE_COUNT \
    ((size_t)(SYNTRA_PAGE_ALLOCATOR_MAX_PHYSICAL_ADDRESS / SYNTRA_PAGE_SIZE))
#define SYNTRA_BITMAP_BYTES ((SYNTRA_PAGE_COUNT + 7U) / 8U)

static uint8_t free_bitmap[SYNTRA_BITMAP_BYTES];
static uint8_t allocated_bitmap[SYNTRA_BITMAP_BYTES];
static size_t total_pages;
static size_t free_pages;
static int initialized;

static int bitmap_test(const uint8_t *bitmap, size_t index)
{
    return (bitmap[index / 8U] & (uint8_t)(1U << (index % 8U))) != 0U;
}

static void bitmap_set(uint8_t *bitmap, size_t index)
{
    bitmap[index / 8U] |= (uint8_t)(1U << (index % 8U));
}

static void bitmap_clear(uint8_t *bitmap, size_t index)
{
    bitmap[index / 8U] &= (uint8_t)~(uint8_t)(1U << (index % 8U));
}

static uintptr_t align_up_page(uintptr_t address)
{
    uintptr_t remainder = address % (uintptr_t)SYNTRA_PAGE_SIZE;

    return remainder == 0U
        ? address
        : address + ((uintptr_t)SYNTRA_PAGE_SIZE - remainder);
}

syntra_status_t syntra_page_allocator_init(void)
{
    size_t index;

    for (index = 0U; index < SYNTRA_PAGE_COUNT / 8U; ++index)
    {
        free_bitmap[index] = 0U;
        allocated_bitmap[index] = 0U;
    }

    total_pages = 0U;
    free_pages = 0U;
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
        if ((uint64_t)end > SYNTRA_PAGE_ALLOCATOR_MAX_PHYSICAL_ADDRESS)
        {
            return SYNTRA_STATUS_NOT_IMPLEMENTED;
        }

        /*
         * Aligning a high physical base upward must not wrap uintptr_t.
         * Reject an unrepresentable alignment instead of creating low pages.
         */
        if (region.base > UINTPTR_MAX -
                ((uintptr_t)SYNTRA_PAGE_SIZE - 1U))
        {
            return SYNTRA_STATUS_INVALID_ARGUMENT;
        }

        first = align_up_page(region.base);
        limit = end - (end % (uintptr_t)SYNTRA_PAGE_SIZE);

        for (address = first; address < limit;
             address += (uintptr_t)SYNTRA_PAGE_SIZE)
        {
            size_t page_index =
                (size_t)(address / (uintptr_t)SYNTRA_PAGE_SIZE);

            bitmap_set(free_bitmap, page_index);
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

    for (index = 0U; index < SYNTRA_PAGE_COUNT; ++index)
    {
        if (bitmap_test(free_bitmap, index))
        {
            bitmap_clear(free_bitmap, index);
            bitmap_set(allocated_bitmap, index);
            free_pages--;
            *physical_address =
                (uintptr_t)(index * SYNTRA_PAGE_SIZE);
            return SYNTRA_STATUS_OK;
        }
    }

    return SYNTRA_STATUS_ERROR;
}

syntra_status_t syntra_page_free(uintptr_t physical_address)
{
    size_t index;

    if (!initialized ||
        physical_address % (uintptr_t)SYNTRA_PAGE_SIZE != 0U ||
        (uint64_t)physical_address >=
            SYNTRA_PAGE_ALLOCATOR_MAX_PHYSICAL_ADDRESS)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    index = (size_t)(physical_address / (uintptr_t)SYNTRA_PAGE_SIZE);
    if (!bitmap_test(allocated_bitmap, index))
    {
        return SYNTRA_STATUS_ERROR;
    }

    bitmap_clear(allocated_bitmap, index);
    bitmap_set(free_bitmap, index);
    free_pages++;
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
