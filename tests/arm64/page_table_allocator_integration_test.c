#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "../../kernel/memory/memory_region.h"
#include "../../kernel/memory/page_allocator.h"
#include "../../kernel/arch/arm64/page_table_builder.h"

#define TEST_RAM_BASE 0x10000000ULL
#define TEST_RAM_PAGES 8U

static uint64_t table_storage[TEST_RAM_PAGES][SYNTRA_ARM64_MMU_TABLE_ENTRIES];

static syntra_status_t allocate_page(void *context, uint64_t *physical_address)
{
    (void)context;
    if (physical_address == NULL) return SYNTRA_STATUS_INVALID_ARGUMENT;
    return syntra_page_alloc_owned((uintptr_t *)physical_address,
        SYNTRA_PAGE_OWNER_PAGE_TABLE);
}

static uint64_t *resolve_page(uint64_t physical_address, void *context)
{
    (void)context;
    if (physical_address < TEST_RAM_BASE ||
        physical_address >= TEST_RAM_BASE + TEST_RAM_PAGES * SYNTRA_PAGE_SIZE ||
        ((physical_address - TEST_RAM_BASE) % SYNTRA_PAGE_SIZE) != 0U)
        return NULL;
    return table_storage[(physical_address - TEST_RAM_BASE) / SYNTRA_PAGE_SIZE];
}

int main(void)
{
    syntra_arm64_page_table_t table;
    size_t initial_free;

    for (size_t i = 0; i < TEST_RAM_PAGES; ++i)
        for (size_t j = 0; j < SYNTRA_ARM64_MMU_TABLE_ENTRIES; ++j)
            table_storage[i][j] = 0U;

    assert(syntra_memory_regions_init() == SYNTRA_STATUS_OK);
    assert(syntra_memory_region_add((uintptr_t)TEST_RAM_BASE,
        TEST_RAM_PAGES * SYNTRA_PAGE_SIZE, SYNTRA_MEMORY_REGION_RAM) ==
        SYNTRA_STATUS_OK);
    assert(syntra_page_allocator_init() == SYNTRA_STATUS_OK);
    initial_free = syntra_page_allocator_free_count();
    assert(initial_free == TEST_RAM_PAGES);

    assert(syntra_arm64_page_table_init(&table, allocate_page,
        resolve_page, NULL) == SYNTRA_STATUS_OK);
    assert(table.root_physical_address == TEST_RAM_BASE);
    assert(syntra_page_allocator_free_count() == initial_free - 1U);

    /* The builder allocates its L2 and L3 tables from the physical allocator. */
    assert(syntra_arm64_page_table_map(&table, 0x40000000ULL,
        0x80000000ULL, 0x2000ULL, SYNTRA_ARM64_MMU_MEMORY_NORMAL,
        SYNTRA_ARM64_MMU_ACCESS_READ_WRITE, 0) == SYNTRA_STATUS_OK);
    assert(syntra_page_allocator_free_count() == initial_free - 3U);

    assert(syntra_arm64_page_table_verify(&table, 0x40000000ULL,
        0x80000000ULL, 0x2000ULL, SYNTRA_ARM64_MMU_MEMORY_NORMAL,
        SYNTRA_ARM64_MMU_ACCESS_READ_WRITE, 0) == SYNTRA_STATUS_OK);

    puts("ARM64 page-table builder + physical page allocator integration passed");
    return 0;
}
