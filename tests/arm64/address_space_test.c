#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "../../kernel/arch/arm64/address_space.h"
#include "../../kernel/memory/memory_region.h"
#include "../../kernel/memory/page_allocator.h"

#define TEST_RAM_BASE 0x10000000ULL
#define TEST_RAM_PAGES 32U

static uint64_t table_storage[TEST_RAM_PAGES][SYNTRA_ARM64_MMU_TABLE_ENTRIES];

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
    syntra_arm64_page_table_t kernel_table = {0};
    uint64_t *kernel_root, *kernel_l2, *kernel_l3;
    uint64_t *first_root;
    uint64_t saved_kernel_entry;
    uint16_t kernel_indices[4];
    uintptr_t kernel_data_page;
    syntra_arm64_mmu_translation_t translation;
    uintptr_t first_process_page;
    uintptr_t second_process_page;
    uintptr_t extra_process_pages[3];
    size_t initial_free;

    assert(syntra_memory_regions_init() == SYNTRA_STATUS_OK);
    assert(syntra_memory_region_add(
        (uintptr_t)TEST_RAM_BASE,
        TEST_RAM_PAGES * SYNTRA_PAGE_SIZE,
        SYNTRA_MEMORY_REGION_RAM) == SYNTRA_STATUS_OK);
    assert(syntra_page_allocator_init() == SYNTRA_STATUS_OK);
    initial_free = syntra_page_allocator_free_count();
    assert(initial_free == TEST_RAM_PAGES);

    assert(syntra_page_alloc_owned(
        &kernel_data_page, SYNTRA_PAGE_OWNER_KERNEL) == SYNTRA_STATUS_OK);
    assert(syntra_arm64_page_table_init(
        &kernel_table, allocate_table_page,
        resolve_page, NULL) == SYNTRA_STATUS_OK);
    assert(syntra_arm64_page_table_map(
        &kernel_table, 0x10000000ULL, kernel_data_page,
        SYNTRA_ARM64_MMU_PAGE_SIZE, SYNTRA_ARM64_MMU_MEMORY_NORMAL,
        SYNTRA_ARM64_MMU_ACCESS_READ_WRITE, 0) == SYNTRA_STATUS_OK);

    assert(syntra_arm64_address_space_create(
        &first, 1U, resolve_page, NULL) == SYNTRA_STATUS_OK);
    assert(syntra_arm64_address_space_create(
        &second, 2U, resolve_page, NULL) == SYNTRA_STATUS_OK);
    assert(first.page_table.root_physical_address !=
           second.page_table.root_physical_address);
    assert(syntra_arm64_address_space_inherit_kernel_mappings(
        &first, kernel_table.root_physical_address) == SYNTRA_STATUS_OK);
    assert(syntra_arm64_address_space_inherit_kernel_mappings(
        &second, kernel_table.root_physical_address) == SYNTRA_STATUS_OK);
    assert(syntra_arm64_address_space_inherit_kernel_mappings(
        &first, kernel_table.root_physical_address) ==
        SYNTRA_STATUS_INVALID_ARGUMENT);

    /* Both roots see the kernel mapping, but retain separate user roots. */
    assert(syntra_arm64_address_space_translate(
        &first, 0x10000034ULL, &translation) == SYNTRA_STATUS_OK);
    assert(translation.physical_address == kernel_data_page + 0x34ULL);
    assert(!translation.user_read);
    assert(syntra_arm64_address_space_translate(
        &second, 0x10000034ULL, &translation) == SYNTRA_STATUS_OK);
    assert(translation.physical_address == kernel_data_page + 0x34ULL);
    assert(!translation.user_read);

    /*
     * Validation must detect a modified borrowed kernel entry and any
     * accidental mapping in the reserved 128..256 GiB root gap.
     */
    assert(syntra_arm64_mmu_get_table_indices(
        0x10000000ULL, kernel_indices) == SYNTRA_STATUS_OK);
    first_root = resolve_page(first.page_table.root_physical_address, NULL);
    assert(first_root != NULL);
    saved_kernel_entry = first_root[kernel_indices[0]];
    first_root[kernel_indices[0]] = 0ULL;
    assert(syntra_arm64_address_space_validate(&first) == SYNTRA_STATUS_ERROR);
    first_root[kernel_indices[0]] = saved_kernel_entry;
    first_root[200U] = 1ULL;
    assert(syntra_arm64_address_space_validate(&first) == SYNTRA_STATUS_ERROR);
    first_root[200U] = 0ULL;
    assert(syntra_arm64_address_space_validate(&first) == SYNTRA_STATUS_OK);

    /* Low addresses are reserved for inherited kernel mappings. */
    assert(syntra_page_alloc_owned(
        &first_process_page, SYNTRA_PAGE_OWNER_PROCESS) == SYNTRA_STATUS_OK);
    assert(syntra_arm64_address_space_map_user_page(
        &first, 0x40000000ULL, first_process_page,
        SYNTRA_ARM64_MMU_ACCESS_READ_WRITE, 0) ==
        SYNTRA_STATUS_INVALID_ARGUMENT);
    assert(syntra_page_free_owned(
        first_process_page, SYNTRA_PAGE_OWNER_PROCESS) == SYNTRA_STATUS_OK);
    assert(syntra_page_alloc_owned(
        &first_process_page, SYNTRA_PAGE_OWNER_PROCESS) == SYNTRA_STATUS_OK);
    assert(syntra_page_alloc_owned(
        &second_process_page, SYNTRA_PAGE_OWNER_PROCESS) == SYNTRA_STATUS_OK);

    assert(syntra_arm64_address_space_map_user_page(
        &first, 0x4000000000ULL, first_process_page,
        SYNTRA_ARM64_MMU_ACCESS_READ_WRITE, 0) == SYNTRA_STATUS_OK);
    /* A PROCESS page cannot be aliased into a second address space in V1. */
    assert(syntra_arm64_address_space_map_user_page(
        &second, 0x4000001000ULL, first_process_page,
        SYNTRA_ARM64_MMU_ACCESS_READ_ONLY, 0) ==
        SYNTRA_STATUS_PERMISSION_DENIED);
    assert(syntra_arm64_address_space_map_user_page(
        &second, 0x4000000000ULL, second_process_page,
        SYNTRA_ARM64_MMU_ACCESS_READ_ONLY, 0) == SYNTRA_STATUS_OK);

    assert(syntra_arm64_address_space_translate(
        &first, 0x4000000034ULL, &translation) == SYNTRA_STATUS_OK);
    assert(translation.physical_address == first_process_page + 0x34ULL);
    assert(translation.user_read && translation.user_write);
    assert(!translation.user_execute);

    assert(syntra_arm64_address_space_translate(
        &second, 0x4000000034ULL, &translation) == SYNTRA_STATUS_OK);
    assert(translation.physical_address == second_process_page + 0x34ULL);
    assert(translation.user_read && !translation.user_write);
    assert(!translation.user_execute);

    /* Same VA, separate backing pages: writes must remain private. */
    {
        uint64_t *first_backing = resolve_page(
            (uint64_t)first_process_page, NULL);
        uint64_t *second_backing = resolve_page(
            (uint64_t)second_process_page, NULL);
        assert(first_backing != NULL && second_backing != NULL);
        first_backing[0] = 0xA11CE001ULL;
        second_backing[0] = 0xB0B02002ULL;
        assert(first_backing[0] == 0xA11CE001ULL);
        assert(second_backing[0] == 0xB0B02002ULL);
        assert(first_backing[0] != second_backing[0]);
    }

    assert(syntra_arm64_address_space_map_user_page(
        &first, 0x4000000000ULL, second_process_page,
        SYNTRA_ARM64_MMU_ACCESS_READ_WRITE, 0) != SYNTRA_STATUS_OK);
    /*
     * Private VA test: an additional page exists only in Process A.
     * Process B must not translate the same VA, even though both spaces
     * share the same user VA range and independently map the adjacent page.
     */
    assert(syntra_page_alloc_owned(
        &first_process_page, SYNTRA_PAGE_OWNER_PROCESS) == SYNTRA_STATUS_OK);
    assert(syntra_arm64_address_space_map_user_page(
        &first, 0x4000002000ULL, first_process_page,
        SYNTRA_ARM64_MMU_ACCESS_READ_WRITE, 0) == SYNTRA_STATUS_OK);
    assert(syntra_arm64_address_space_translate(
        &first, 0x4000002000ULL, &translation) == SYNTRA_STATUS_OK);
    assert(translation.physical_address == first_process_page);
    assert(translation.user_read && translation.user_write);
    assert(syntra_arm64_address_space_translate(
        &second, 0x4000002000ULL, &translation) == SYNTRA_STATUS_ERROR);

    /* Re-check after both roots are populated: the same VA remains private. */
    {
        uint64_t *first_backing = resolve_page(
            (uint64_t)first_process_page, NULL);
        assert(first_backing != NULL);
        first_backing[0] = 0xCAFE0001ULL;
        assert(first_backing[0] == 0xCAFE0001ULL);
        assert(syntra_arm64_address_space_translate(
            &second, 0x4000002000ULL, &translation) ==
            SYNTRA_STATUS_ERROR);
    }

    assert(syntra_arm64_address_space_validate(&first) == SYNTRA_STATUS_OK);
    assert(syntra_arm64_address_space_validate(&second) == SYNTRA_STATUS_OK);
    assert(syntra_page_allocator_validate() == SYNTRA_STATUS_OK);

    /* Unmap reclaims the process frame and now-empty L3/L2 tables. */
    assert(syntra_arm64_address_space_unmap_user_page(
        &first, 0x4000000000ULL) == SYNTRA_STATUS_OK);
    assert(syntra_arm64_address_space_unmap_user_page(
        &first, 0x4000000000ULL) == SYNTRA_STATUS_INVALID_ARGUMENT);
    assert(syntra_arm64_address_space_translate(
        &first, 0x4000000000ULL, &translation) == SYNTRA_STATUS_ERROR);
    assert(syntra_arm64_address_space_validate(&first) == SYNTRA_STATUS_OK);

    /*
     * Reclaiming Process A's mapping must not invalidate Process B's mapping
     * at the identical VA or release B's independently owned backing page.
     */
    assert(syntra_arm64_address_space_translate(
        &second, 0x4000000000ULL, &translation) == SYNTRA_STATUS_OK);
    assert(translation.physical_address == second_process_page);
    assert(translation.user_read && !translation.user_write);
    assert(!translation.user_execute);
    {
        uint64_t *second_backing = resolve_page(
            (uint64_t)second_process_page, NULL);
        assert(second_backing != NULL);
        assert(second_backing[0] == 0xB0B02002ULL);
    }
    assert(syntra_arm64_address_space_validate(&second) == SYNTRA_STATUS_OK);

    /* The same virtual range can be populated again after reclamation. */
    assert(syntra_page_alloc_owned(
        &first_process_page, SYNTRA_PAGE_OWNER_PROCESS) == SYNTRA_STATUS_OK);
    assert(syntra_arm64_address_space_map_user_page(
        &first, 0x4000000000ULL, first_process_page,
        SYNTRA_ARM64_MMU_ACCESS_READ_WRITE, 0) == SYNTRA_STATUS_OK);

    /*
     * Populate two additional L2 slots. Destroy must walk and reclaim all
     * mappings, including multiple L3 tables under the same private L2.
     */
    assert(syntra_page_alloc_owned(
        &extra_process_pages[0], SYNTRA_PAGE_OWNER_PROCESS) == SYNTRA_STATUS_OK);
    assert(syntra_page_alloc_owned(
        &extra_process_pages[1], SYNTRA_PAGE_OWNER_PROCESS) == SYNTRA_STATUS_OK);
    assert(syntra_arm64_address_space_map_user_page(
        &first, 0x4000200000ULL, extra_process_pages[0],
        SYNTRA_ARM64_MMU_ACCESS_READ_WRITE, 0) == SYNTRA_STATUS_OK);
    assert(syntra_arm64_address_space_map_user_page(
        &first, 0x4000400000ULL, extra_process_pages[1],
        SYNTRA_ARM64_MMU_ACCESS_READ_ONLY, 0) == SYNTRA_STATUS_OK);

    /*
     * Exercise complete private-branch pruning: this mapping is the only
     * leaf in its L2 slot, so unmap must release both its L3 and L2 tables.
     * Re-map the same VA to prove the pruned hierarchy can be rebuilt.
     */
    assert(syntra_page_alloc_owned(
        &extra_process_pages[2], SYNTRA_PAGE_OWNER_PROCESS) == SYNTRA_STATUS_OK);
    assert(syntra_arm64_address_space_map_user_page(
        &first, 0x4000600000ULL, extra_process_pages[2],
        SYNTRA_ARM64_MMU_ACCESS_READ_WRITE, 0) == SYNTRA_STATUS_OK);
    assert(syntra_arm64_address_space_unmap_user_page(
        &first, 0x4000600000ULL) == SYNTRA_STATUS_OK);
    assert(first.mapping_count == 4U);
    assert(syntra_arm64_address_space_translate(
        &first, 0x4000600000ULL, &translation) == SYNTRA_STATUS_ERROR);
    assert(syntra_page_allocator_validate() == SYNTRA_STATUS_OK);

    assert(syntra_page_alloc_owned(
        &extra_process_pages[2], SYNTRA_PAGE_OWNER_PROCESS) == SYNTRA_STATUS_OK);
    assert(syntra_arm64_address_space_map_user_page(
        &first, 0x4000600000ULL, extra_process_pages[2],
        SYNTRA_ARM64_MMU_ACCESS_READ_WRITE, 0) == SYNTRA_STATUS_OK);
    assert(first.mapping_count == 5U);
    assert(syntra_arm64_address_space_validate(&first) == SYNTRA_STATUS_OK);
    assert(syntra_page_allocator_validate() == SYNTRA_STATUS_OK);

    assert(syntra_arm64_address_space_destroy(&first) == SYNTRA_STATUS_OK);
    assert(first.mapping_count == 0U);
    assert(first.initialized == 0U);
    assert(syntra_page_allocator_validate() == SYNTRA_STATUS_OK);
    assert(syntra_arm64_address_space_destroy(&second) == SYNTRA_STATUS_OK);

    /* Address-space destruction must not free the borrowed kernel subtree. */
    kernel_root = resolve_page(kernel_table.root_physical_address, NULL);
    assert(kernel_root != NULL);
    assert(syntra_arm64_mmu_get_table_indices(0x10000000ULL,
        kernel_indices) == SYNTRA_STATUS_OK);
    kernel_l2 = resolve_page(kernel_root[kernel_indices[0]] &
        SYNTRA_ARM64_MMU_PA_MASK, NULL);
    assert(kernel_l2 != NULL);
    kernel_l3 = resolve_page(kernel_l2[kernel_indices[1]] &
        SYNTRA_ARM64_MMU_PA_MASK, NULL);
    assert(kernel_l3 != NULL);
    assert(syntra_page_free_owned(
        (uintptr_t)(kernel_root[kernel_indices[0]] &
            SYNTRA_ARM64_MMU_PA_MASK), SYNTRA_PAGE_OWNER_PAGE_TABLE) ==
        SYNTRA_STATUS_OK);
    assert(syntra_page_free_owned(
        (uintptr_t)(kernel_l2[kernel_indices[1]] &
            SYNTRA_ARM64_MMU_PA_MASK), SYNTRA_PAGE_OWNER_PAGE_TABLE) ==
        SYNTRA_STATUS_OK);
    assert(syntra_page_free_owned(
        (uintptr_t)kernel_table.root_physical_address,
        SYNTRA_PAGE_OWNER_PAGE_TABLE) == SYNTRA_STATUS_OK);
    assert(syntra_page_free_owned(
        kernel_data_page, SYNTRA_PAGE_OWNER_KERNEL) == SYNTRA_STATUS_OK);
    assert(syntra_page_allocator_free_count() == initial_free);
    assert(syntra_page_allocator_validate() == SYNTRA_STATUS_OK);

    puts("ARM64 kernel mapping inheritance, user isolation, unmapping, and reclamation passed");
    return 0;
}
