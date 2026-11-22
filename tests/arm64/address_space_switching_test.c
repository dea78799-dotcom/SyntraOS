#ifdef NDEBUG
#undef NDEBUG
#endif

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "../../kernel/arch/arm64/address_space.h"
#include "../../kernel/memory/memory_region.h"
#include "../../kernel/memory/page_allocator.h"

#define TEST_RAM_BASE 0x18000000ULL
#define TEST_RAM_PAGES 16U
#define TEST_KERNEL_VA 0x40000000ULL

static uint64_t table_storage[TEST_RAM_PAGES][SYNTRA_ARM64_MMU_TABLE_ENTRIES];

typedef struct
{
    uint64_t ttbr0;
    size_t write_count;
    int fail_next_write;
} mock_mmu_t;

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

static uint64_t mock_read_ttbr0(void *context)
{
    mock_mmu_t *mmu = (mock_mmu_t *)context;
    return mmu->ttbr0;
}

static syntra_status_t mock_write_ttbr0(uint64_t root_physical_address, void *context)
{
    mock_mmu_t *mmu = (mock_mmu_t *)context;

    if (mmu->fail_next_write)
    {
        mmu->fail_next_write = 0;
        return SYNTRA_STATUS_ERROR;
    }

    mmu->ttbr0 = root_physical_address;
    mmu->write_count++;
    return SYNTRA_STATUS_OK;
}

int main(void)
{
    syntra_arm64_address_space_t process_a = {0};
    syntra_arm64_address_space_t process_b = {0};
    syntra_arm64_address_space_t process_c = {0};
    syntra_arm64_page_table_t kernel_table = {0};
    mock_mmu_t mock_mmu = {0};
    uintptr_t kernel_data_page;
    uint64_t kernel_root;
    uint64_t *kernel_root_entries;
    uint64_t *kernel_l2_entries;
    uint16_t indices[4];
    size_t initial_free;

    assert(syntra_memory_regions_init() == SYNTRA_STATUS_OK);
    assert(syntra_memory_region_add(
        (uintptr_t)TEST_RAM_BASE,
        TEST_RAM_PAGES * SYNTRA_PAGE_SIZE,
        SYNTRA_MEMORY_REGION_RAM) == SYNTRA_STATUS_OK);
    assert(syntra_page_allocator_init() == SYNTRA_STATUS_OK);
    initial_free = syntra_page_allocator_free_count();
    assert(initial_free == TEST_RAM_PAGES);

    /* Build a valid kernel root and a borrowed kernel mapping subtree. */
    assert(syntra_arm64_page_table_init(
        &kernel_table, allocate_table_page, resolve_page, NULL) ==
        SYNTRA_STATUS_OK);
    kernel_root = kernel_table.root_physical_address;
    assert(syntra_page_alloc_owned(
        &kernel_data_page, SYNTRA_PAGE_OWNER_DRIVER) == SYNTRA_STATUS_OK);
    assert(syntra_arm64_page_table_map(
        &kernel_table, TEST_KERNEL_VA, (uint64_t)kernel_data_page,
        SYNTRA_PAGE_SIZE, SYNTRA_ARM64_MMU_MEMORY_NORMAL,
        SYNTRA_ARM64_MMU_ACCESS_READ_WRITE, 0) == SYNTRA_STATUS_OK);

    assert(syntra_arm64_address_space_create(
        &process_a, 1U, resolve_page, NULL) == SYNTRA_STATUS_OK);
    assert(syntra_arm64_address_space_create(
        &process_b, 2U, resolve_page, NULL) == SYNTRA_STATUS_OK);
    assert(syntra_arm64_address_space_create(
        &process_c, 3U, resolve_page, NULL) == SYNTRA_STATUS_OK);
    assert(syntra_arm64_address_space_inherit_kernel_mappings(
        &process_a, kernel_root) == SYNTRA_STATUS_OK);
    assert(syntra_arm64_address_space_inherit_kernel_mappings(
        &process_b, kernel_root) == SYNTRA_STATUS_OK);
    assert(syntra_arm64_address_space_inherit_kernel_mappings(
        &process_c, kernel_root) == SYNTRA_STATUS_OK);

    mock_mmu.ttbr0 = kernel_root;
    syntra_arm64_address_space_test_set_mmu_hooks(
        mock_read_ttbr0, mock_write_ttbr0, &mock_mmu);

    /* Registration must verify the simulated hardware's current TTBR0. */
    assert(syntra_arm64_address_space_register_kernel_root(
        kernel_root) == SYNTRA_STATUS_OK);
    assert(syntra_arm64_address_space_kernel_root_registered());

    /* Kernel -> A; repeated activation of A must not issue another write. */
    assert(syntra_arm64_address_space_activate(
        &process_a) == SYNTRA_STATUS_OK);
    assert(mock_mmu.ttbr0 == process_a.page_table.root_physical_address);
    assert(mock_mmu.write_count == 1U);
    assert(syntra_arm64_address_space_activate(
        &process_a) == SYNTRA_STATUS_OK);
    assert(mock_mmu.write_count == 1U);

    /* An active root cannot be destroyed or have its backing page tables freed. */
    assert(syntra_arm64_address_space_destroy(
        &process_a) == SYNTRA_STATUS_PERMISSION_DENIED);
    assert(process_a.initialized == 1U);

    /* A -> B -> Kernel transitions each update the simulated TTBR0 once. */
    assert(syntra_arm64_address_space_activate(
        &process_b) == SYNTRA_STATUS_OK);
    assert(mock_mmu.ttbr0 == process_b.page_table.root_physical_address);
    assert(mock_mmu.write_count == 2U);
    assert(syntra_arm64_address_space_activate_kernel_root() ==
        SYNTRA_STATUS_OK);
    assert(mock_mmu.ttbr0 == kernel_root);
    assert(mock_mmu.write_count == 3U);
    assert(syntra_arm64_address_space_activate_kernel_root() ==
        SYNTRA_STATUS_OK);
    assert(mock_mmu.write_count == 3U);

    /* Failed hardware transition must not publish the target as active. */
    mock_mmu.fail_next_write = 1;
    assert(syntra_arm64_address_space_activate(
        &process_c) == SYNTRA_STATUS_ERROR);
    assert(mock_mmu.ttbr0 == kernel_root);
    assert(mock_mmu.write_count == 3U);
    assert(syntra_arm64_address_space_destroy(
        &process_c) == SYNTRA_STATUS_OK);
    assert(process_c.initialized == 0U);

    /* Once switched away, A and B can be destroyed without freeing kernel tables. */
    assert(syntra_arm64_address_space_destroy(
        &process_a) == SYNTRA_STATUS_OK);
    assert(syntra_arm64_address_space_activate(
        &process_b) == SYNTRA_STATUS_OK);
    assert(mock_mmu.ttbr0 == process_b.page_table.root_physical_address);
    assert(mock_mmu.write_count == 4U);
    assert(syntra_arm64_address_space_activate_kernel_root() ==
        SYNTRA_STATUS_OK);
    assert(mock_mmu.ttbr0 == kernel_root);
    assert(mock_mmu.write_count == 5U);
    assert(syntra_arm64_address_space_destroy(
        &process_b) == SYNTRA_STATUS_OK);

    /* Verify the borrowed kernel L2/L3 subtree remains intact. */
    kernel_root_entries = resolve_page(kernel_root, NULL);
    assert(kernel_root_entries != NULL);
    assert(syntra_arm64_mmu_get_table_indices(TEST_KERNEL_VA, indices) ==
        SYNTRA_STATUS_OK);
    kernel_l2_entries = resolve_page(
        kernel_root_entries[indices[0]] & SYNTRA_ARM64_MMU_PA_MASK, NULL);
    assert(kernel_l2_entries != NULL);
    assert((kernel_l2_entries[indices[1]] & SYNTRA_ARM64_MMU_PA_MASK) != 0ULL);

    assert(syntra_page_free_owned(
        (uintptr_t)(kernel_l2_entries[indices[1]] &
            SYNTRA_ARM64_MMU_PA_MASK),
        SYNTRA_PAGE_OWNER_PAGE_TABLE) == SYNTRA_STATUS_OK);
    assert(syntra_page_free_owned(
        (uintptr_t)(kernel_root_entries[indices[0]] &
            SYNTRA_ARM64_MMU_PA_MASK),
        SYNTRA_PAGE_OWNER_PAGE_TABLE) == SYNTRA_STATUS_OK);
    assert(syntra_page_free_owned(
        (uintptr_t)kernel_root, SYNTRA_PAGE_OWNER_PAGE_TABLE) ==
        SYNTRA_STATUS_OK);
    assert(syntra_page_free_owned(
        kernel_data_page, SYNTRA_PAGE_OWNER_DRIVER) == SYNTRA_STATUS_OK);

    assert(syntra_page_allocator_free_count() == initial_free);
    assert(syntra_page_allocator_validate() == SYNTRA_STATUS_OK);

    puts("ARM64 address-space switching and active-root safety test passed");
    return 0;
}
