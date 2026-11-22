#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "../../kernel/arch/arm64/address_space.h"
#include "../../kernel/memory/memory_region.h"
#include "../../kernel/memory/page_allocator.h"

#define TEST_RAM_BASE 0x10000000ULL
#define TEST_RAM_PAGES 8U

static uint64_t table_storage[TEST_RAM_PAGES][SYNTRA_ARM64_MMU_TABLE_ENTRIES];

static uint64_t *resolve_page(uint64_t physical_address, void *context)
{
    (void)context;
    if (physical_address < TEST_RAM_BASE ||
        physical_address >= TEST_RAM_BASE +
            TEST_RAM_PAGES * SYNTRA_PAGE_SIZE ||
        ((physical_address - TEST_RAM_BASE) % SYNTRA_PAGE_SIZE) != 0U)
        return NULL;
    return table_storage[
        (physical_address - TEST_RAM_BASE) / SYNTRA_PAGE_SIZE];
}

int main(void)
{
    syntra_arm64_address_space_t first = {0};
    syntra_arm64_address_space_t second = {0};
    syntra_arm64_mmu_translation_t translation;
    uintptr_t first_process_page;
    uintptr_t second_process_page;
    size_t initial_free;

    assert(syntra_memory_regions_init() == SYNTRA_STATUS_OK);
    assert(syntra_memory_region_add(
        (uintptr_t)TEST_RAM_BASE,
        TEST_RAM_PAGES * SYNTRA_PAGE_SIZE,
        SYNTRA_MEMORY_REGION_RAM) == SYNTRA_STATUS_OK);
    assert(syntra_page_allocator_init() == SYNTRA_STATUS_OK);
    initial_free = syntra_page_allocator_free_count();
    assert(initial_free == TEST_RAM_PAGES);

    assert(syntra_arm64_address_space_create(
        &first, 1U, resolve_page, NULL) == SYNTRA_STATUS_OK);
    assert(syntra_arm64_address_space_create(
        &second, 2U, resolve_page, NULL) == SYNTRA_STATUS_OK);
    assert(first.page_table.root_physical_address !=
           second.page_table.root_physical_address);

    assert(syntra_page_alloc_owned(
        &first_process_page, SYNTRA_PAGE_OWNER_PROCESS) == SYNTRA_STATUS_OK);
    assert(syntra_page_alloc_owned(
        &second_process_page, SYNTRA_PAGE_OWNER_PROCESS) == SYNTRA_STATUS_OK);

    assert(syntra_arm64_address_space_map_user_page(
        &first, 0x40000000ULL, first_process_page,
        SYNTRA_ARM64_MMU_ACCESS_READ_WRITE, 0) == SYNTRA_STATUS_OK);
    assert(syntra_arm64_address_space_map_user_page(
        &second, 0x40000000ULL, second_process_page,
        SYNTRA_ARM64_MMU_ACCESS_READ_ONLY, 0) == SYNTRA_STATUS_OK);

    assert(syntra_arm64_address_space_translate(
        &first, 0x40000034ULL, &translation) == SYNTRA_STATUS_OK);
    assert(translation.physical_address == first_process_page + 0x34ULL);
    assert(translation.user_read && translation.user_write);
    assert(!translation.user_execute);

    assert(syntra_arm64_address_space_translate(
        &second, 0x40000034ULL, &translation) == SYNTRA_STATUS_OK);
    assert(translation.physical_address == second_process_page + 0x34ULL);
    assert(translation.user_read && !translation.user_write);
    assert(!translation.user_execute);

    assert(syntra_arm64_address_space_map_user_page(
        &first, 0x40000000ULL, second_process_page,
        SYNTRA_ARM64_MMU_ACCESS_READ_WRITE, 0) != SYNTRA_STATUS_OK);
    assert(syntra_arm64_address_space_validate(&first) == SYNTRA_STATUS_OK);
    assert(syntra_arm64_address_space_validate(&second) == SYNTRA_STATUS_OK);
    assert(syntra_page_allocator_validate() == SYNTRA_STATUS_OK);

    assert(syntra_arm64_address_space_destroy(&first) == SYNTRA_STATUS_OK);
    assert(syntra_arm64_address_space_destroy(&second) == SYNTRA_STATUS_OK);
    assert(syntra_page_allocator_free_count() == initial_free);
    assert(syntra_page_allocator_validate() == SYNTRA_STATUS_OK);

    puts("ARM64 independent address-space ownership and translation passed");
    return 0;
}
