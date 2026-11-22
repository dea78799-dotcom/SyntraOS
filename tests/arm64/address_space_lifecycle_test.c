#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "../../kernel/arch/arm64/address_space.h"
#include "../../kernel/memory/memory_region.h"
#include "../../kernel/memory/page_allocator.h"

#define TEST_RAM_BASE 0x10000000ULL
#define TEST_RAM_PAGES 16U
#define TEST_USER_VA (SYNTRA_ARM64_USER_VA_BASE)
#define TEST_KERNEL_VA 0x40000000ULL

static uint64_t table_storage[TEST_RAM_PAGES][SYNTRA_ARM64_MMU_TABLE_ENTRIES];

static uint64_t *resolve_page(uint64_t physical_address, void *context)
{
    (void)context;
    if (physical_address < TEST_RAM_BASE ||
        physical_address >= TEST_RAM_BASE +
            TEST_RAM_PAGES * SYNTRA_PAGE_SIZE ||
        ((physical_address - TEST_RAM_BASE) % SYNTRA_PAGE_SIZE) != 0ULL)
        return NULL;

    return table_storage[
        (physical_address - TEST_RAM_BASE) / SYNTRA_PAGE_SIZE];
}

static const uint64_t *resolve_const_page(
    uint64_t physical_address, void *context)
{
    return resolve_page(physical_address, context);
}

static syntra_status_t allocate_table_page(void *context, uint64_t *physical_address)
{
    uintptr_t page;
    syntra_status_t status;
    (void)context;

    if (physical_address == NULL)
        return SYNTRA_STATUS_INVALID_ARGUMENT;

    status = syntra_page_alloc_owned(&page, SYNTRA_PAGE_OWNER_PAGE_TABLE);
    if (status == SYNTRA_STATUS_OK)
        *physical_address = (uint64_t)page;
    return status;
}

static void assert_owner_count(
    syntra_page_owner_t owner, size_t expected)
{
    syntra_page_allocator_stats_t stats;
    assert(syntra_page_allocator_get_stats(&stats) == SYNTRA_STATUS_OK);
    assert(stats.valid == 1U);
    assert(stats.owner_pages[owner] == expected);
}

int main(void)
{
    syntra_arm64_address_space_t space = {0};
    syntra_arm64_page_table_t kernel_table = {0};
    syntra_arm64_mmu_translation_t translation;
    syntra_page_allocator_stats_t before, after_unmap, after;
    uintptr_t process_page_a, process_page_b, kernel_data_page;
    uint64_t kernel_root, kernel_l2, kernel_l3;
    uint64_t *kernel_root_entries, *kernel_l2_entries;
    size_t initial_free;

    assert(syntra_memory_regions_init() == SYNTRA_STATUS_OK);
    assert(syntra_memory_region_add(
        (uintptr_t)TEST_RAM_BASE,
        TEST_RAM_PAGES * SYNTRA_PAGE_SIZE,
        SYNTRA_MEMORY_REGION_RAM) == SYNTRA_STATUS_OK);
    assert(syntra_page_allocator_init() == SYNTRA_STATUS_OK);
    assert(syntra_page_allocator_get_stats(&before) == SYNTRA_STATUS_OK);
    initial_free = before.free_pages;
    assert(initial_free == TEST_RAM_PAGES);

    /*
     * Create a real kernel subtree. The address space must borrow this root
     * entry and must not release the shared L2/L3 pages when it is destroyed.
     */
    assert(syntra_arm64_page_table_init(
        &kernel_table, allocate_table_page, resolve_page, NULL) ==
        SYNTRA_STATUS_OK);
    kernel_root = kernel_table.root_physical_address;
    assert(syntra_page_alloc_owned(
        &kernel_data_page, SYNTRA_PAGE_OWNER_DRIVER) == SYNTRA_STATUS_OK);
    assert(syntra_arm64_page_table_map(
        &kernel_table, TEST_KERNEL_VA, (uint64_t)kernel_data_page, SYNTRA_PAGE_SIZE,
        SYNTRA_ARM64_MMU_MEMORY_NORMAL,
        SYNTRA_ARM64_MMU_ACCESS_READ_WRITE, 0) == SYNTRA_STATUS_OK);

    kernel_root_entries = resolve_page(kernel_root, NULL);
    assert(kernel_root_entries != NULL);
    kernel_l2 = kernel_root_entries[1] & SYNTRA_ARM64_MMU_PA_MASK;
    kernel_l2_entries = resolve_page(kernel_l2, NULL);
    assert(kernel_l2_entries != NULL);
    kernel_l3 = kernel_l2_entries[0] & SYNTRA_ARM64_MMU_PA_MASK;
    assert(resolve_page(kernel_l3, NULL) != NULL);

    assert(syntra_arm64_address_space_create(
        &space, 7U, resolve_page, NULL) == SYNTRA_STATUS_OK);
    assert(space.initialized == 1U);
    assert(space.address_space_id == 7U);
    assert(space.page_table.root_physical_address != kernel_root);
    assert(syntra_arm64_address_space_inherit_kernel_mappings(
        &space, kernel_root) == SYNTRA_STATUS_OK);
    assert(space.kernel_mappings_inherited == 1U);

    /* Borrowed kernel mapping remains visible but is not EL0-accessible. */
    assert(syntra_arm64_address_space_translate(
        &space, TEST_KERNEL_VA, &translation) == SYNTRA_STATUS_OK);
    assert(translation.physical_address == (uint64_t)kernel_data_page);
    assert(translation.user_read == 0U);
    assert(translation.user_write == 0U);

    assert(syntra_page_alloc_owned(
        &process_page_a, SYNTRA_PAGE_OWNER_PROCESS) == SYNTRA_STATUS_OK);
    assert(syntra_page_alloc_owned(
        &process_page_b, SYNTRA_PAGE_OWNER_PROCESS) == SYNTRA_STATUS_OK);

    assert(syntra_arm64_address_space_map_user_page(
        &space, TEST_USER_VA, (uint64_t)process_page_a,
        SYNTRA_ARM64_MMU_ACCESS_READ_WRITE, 0) == SYNTRA_STATUS_OK);
    assert(syntra_arm64_address_space_map_user_page(
        &space, TEST_USER_VA + SYNTRA_PAGE_SIZE,
        (uint64_t)process_page_b,
        SYNTRA_ARM64_MMU_ACCESS_READ_ONLY, 1) == SYNTRA_STATUS_OK);
    assert(space.mapping_count == 2U);
    assert(syntra_arm64_address_space_validate(&space) == SYNTRA_STATUS_OK);

    assert(syntra_arm64_address_space_translate(
        &space, TEST_USER_VA, &translation) == SYNTRA_STATUS_OK);
    assert(translation.physical_address == (uint64_t)process_page_a);
    assert(translation.user_read == 1U);
    assert(translation.user_write == 1U);
    assert(translation.user_execute == 0U);

    assert(syntra_arm64_address_space_translate(
        &space, TEST_USER_VA + SYNTRA_PAGE_SIZE, &translation) ==
        SYNTRA_STATUS_OK);
    assert(translation.physical_address == (uint64_t)process_page_b);
    assert(translation.user_read == 1U);
    assert(translation.user_write == 0U);
    assert(translation.user_execute == 1U);

    /* Reject duplicate VA and a mapping outside the reserved EL0 range. */
    assert(syntra_arm64_address_space_map_user_page(
        &space, TEST_USER_VA, (uint64_t)process_page_b,
        SYNTRA_ARM64_MMU_ACCESS_READ_ONLY, 0) != SYNTRA_STATUS_OK);
    assert(syntra_arm64_address_space_map_user_page(
        &space, 0x20000000ULL, (uint64_t)process_page_b,
        SYNTRA_ARM64_MMU_ACCESS_READ_ONLY, 0) ==
        SYNTRA_STATUS_INVALID_ARGUMENT);

    /* Removing one of two pages must keep their shared L3 table alive. */
    assert(syntra_arm64_address_space_unmap_user_page(
        &space, TEST_USER_VA) == SYNTRA_STATUS_OK);
    assert(space.mapping_count == 1U);
    assert(syntra_arm64_address_space_validate(&space) == SYNTRA_STATUS_OK);
    assert(syntra_arm64_address_space_translate(
        &space, TEST_USER_VA, &translation) != SYNTRA_STATUS_OK);

    /* Removing the final page must reclaim the now-empty L3 and L2 tables. */
    assert(syntra_arm64_address_space_unmap_user_page(
        &space, TEST_USER_VA + SYNTRA_PAGE_SIZE) == SYNTRA_STATUS_OK);
    assert(space.mapping_count == 0U);
    assert(syntra_arm64_address_space_validate(&space) == SYNTRA_STATUS_OK);
    assert(syntra_arm64_address_space_translate(
        &space, TEST_USER_VA + SYNTRA_PAGE_SIZE, &translation) !=
        SYNTRA_STATUS_OK);
    assert(syntra_page_allocator_get_stats(&after_unmap) == SYNTRA_STATUS_OK);
    assert(after_unmap.owner_pages[SYNTRA_PAGE_OWNER_PROCESS] == 0U);
    /* Kernel root+L2+L3 and private address-space root remain allocated. */
    assert(after_unmap.owner_pages[SYNTRA_PAGE_OWNER_PAGE_TABLE] == 4U);

    /* Destroy releases the private root, but preserves the borrowed subtree. */
    assert(syntra_arm64_address_space_destroy(&space) == SYNTRA_STATUS_OK);
    assert(space.initialized == 0U);
    assert(space.mapping_count == 0U);
    assert(space.page_table.root_physical_address == 0ULL);

    assert(syntra_page_allocator_get_stats(&after) == SYNTRA_STATUS_OK);
    assert(after.owner_pages[SYNTRA_PAGE_OWNER_PAGE_TABLE] == 3U);
    assert(after.owner_pages[SYNTRA_PAGE_OWNER_PROCESS] == 0U);
    assert(kernel_root_entries[1] != 0ULL);
    assert((kernel_root_entries[1] & SYNTRA_ARM64_MMU_PA_MASK) == kernel_l2);
    assert(kernel_l2_entries[0] != 0ULL);
    assert((kernel_l2_entries[0] & SYNTRA_ARM64_MMU_PA_MASK) == kernel_l3);
    assert(syntra_arm64_mmu_walk(
        kernel_root, TEST_KERNEL_VA, resolve_const_page, NULL, &translation) ==
        SYNTRA_STATUS_OK);
    assert(translation.physical_address == (uint64_t)kernel_data_page);
    assert_owner_count(SYNTRA_PAGE_OWNER_PAGE_TABLE, 3U);

    /* Release the kernel-owned subtree explicitly after the borrower is gone. */
    assert(syntra_page_free_owned(
        (uintptr_t)kernel_l3, SYNTRA_PAGE_OWNER_PAGE_TABLE) == SYNTRA_STATUS_OK);
    assert(syntra_page_free_owned(
        (uintptr_t)kernel_l2, SYNTRA_PAGE_OWNER_PAGE_TABLE) == SYNTRA_STATUS_OK);
    assert(syntra_page_free_owned(
        (uintptr_t)kernel_root, SYNTRA_PAGE_OWNER_PAGE_TABLE) == SYNTRA_STATUS_OK);
    assert(syntra_page_free_owned(
        kernel_data_page, SYNTRA_PAGE_OWNER_DRIVER) == SYNTRA_STATUS_OK);

    assert(syntra_page_allocator_get_stats(&after) == SYNTRA_STATUS_OK);
    assert(after.free_pages == initial_free);
    assert(after.allocated_pages == 0U);
    assert(after.owner_pages[SYNTRA_PAGE_OWNER_PROCESS] == 0U);
    assert(after.owner_pages[SYNTRA_PAGE_OWNER_PAGE_TABLE] == 0U);
    assert(after.owner_pages[SYNTRA_PAGE_OWNER_DRIVER] == 0U);
    assert(syntra_page_allocator_validate() == SYNTRA_STATUS_OK);
    assert_owner_count(SYNTRA_PAGE_OWNER_PROCESS, 0U);
    assert_owner_count(SYNTRA_PAGE_OWNER_PAGE_TABLE, 0U);
    assert_owner_count(SYNTRA_PAGE_OWNER_DRIVER, 0U);

    puts("ARM64 address-space lifecycle test passed");
    return 0;
}
