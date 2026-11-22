#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "../../kernel/arch/arm64/address_space.h"
#include "../../kernel/memory/memory_region.h"
#include "../../kernel/memory/page_allocator.h"
#include "../../kernel/process/process.h"
#include "../../kernel/scheduler/scheduler.h"

#define TEST_RAM_BASE 0x10000000ULL
#define TEST_RAM_PAGES 32U

static uint64_t table_storage[TEST_RAM_PAGES][SYNTRA_ARM64_MMU_TABLE_ENTRIES];
static _Alignas(16) uint8_t process_thread_stack[2048];

static void dummy_thread(void *argument)
{
    (void)argument;
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
    static syntra_arm64_page_table_t kernel_table;
    static syntra_arm64_address_space_t process_space;
    syntra_process_t *process;
    uintptr_t kernel_data_page, process_data_page;
    size_t initial_free;
    uint64_t *kernel_root, *kernel_l2;
    uint16_t indices[4];

    assert(syntra_memory_regions_init() == SYNTRA_STATUS_OK);
    assert(syntra_memory_region_add(
        (uintptr_t)TEST_RAM_BASE,
        TEST_RAM_PAGES * SYNTRA_PAGE_SIZE,
        SYNTRA_MEMORY_REGION_RAM) == SYNTRA_STATUS_OK);
    assert(syntra_page_allocator_init() == SYNTRA_STATUS_OK);
    assert(scheduler_init() == SYNTRA_STATUS_OK);
    assert(process_init() == SYNTRA_STATUS_OK);
    initial_free = syntra_page_allocator_free_count();

    assert(syntra_page_alloc_owned(
        &kernel_data_page, SYNTRA_PAGE_OWNER_KERNEL) == SYNTRA_STATUS_OK);
    assert(syntra_arm64_page_table_init(
        &kernel_table, allocate_table_page, resolve_page, NULL) ==
        SYNTRA_STATUS_OK);
    assert(syntra_arm64_page_table_map(
        &kernel_table, 0x10000000ULL, kernel_data_page,
        SYNTRA_ARM64_MMU_PAGE_SIZE, SYNTRA_ARM64_MMU_MEMORY_NORMAL,
        SYNTRA_ARM64_MMU_ACCESS_READ_WRITE, 0) == SYNTRA_STATUS_OK);

    assert(syntra_arm64_address_space_create(
        &process_space, 1U, resolve_page, NULL) == SYNTRA_STATUS_OK);
    assert(syntra_arm64_address_space_inherit_kernel_mappings(
        &process_space, kernel_table.root_physical_address) ==
        SYNTRA_STATUS_OK);
    assert(syntra_page_alloc_owned(
        &process_data_page, SYNTRA_PAGE_OWNER_PROCESS) == SYNTRA_STATUS_OK);
    assert(syntra_arm64_address_space_map_user_page(
        &process_space, SYNTRA_ARM64_USER_VA_BASE,
        process_data_page, SYNTRA_ARM64_MMU_ACCESS_READ_WRITE, 0) ==
        SYNTRA_STATUS_OK);

    assert(process_create(1U, 5U) == SYNTRA_STATUS_OK);
    assert(process_create(2U, 4U) == SYNTRA_STATUS_OK);
    assert(scheduler_create_thread(101U, 5U, dummy_thread, NULL,
        process_thread_stack, sizeof(process_thread_stack)) == SYNTRA_STATUS_OK);
    assert(process_attach_address_space(1U, &process_space) ==
        SYNTRA_STATUS_OK);
    process = process_get(1U);
    assert(process != NULL && process->address_space == &process_space);
    assert(process_attach_address_space(2U, &process_space) ==
        SYNTRA_STATUS_PERMISSION_DENIED);
    assert(process_attach_thread(1U, 101U) == SYNTRA_STATUS_OK);
    assert(process_get_address_space_for_thread(101U) == &process_space);
    assert(process_get_address_space_for_thread(999U) == NULL);
    assert(process_set_state(1U, PROCESS_TERMINATED) ==
        SYNTRA_STATUS_PERMISSION_DENIED);
    assert(process_validate() == SYNTRA_STATUS_OK);

    assert(scheduler_set_thread_state(101U, THREAD_TERMINATED) ==
        SYNTRA_STATUS_OK);
    assert(process_refresh_state(1U) == SYNTRA_STATUS_OK);
    assert(process_terminate(1U) == SYNTRA_STATUS_OK);
    assert(process->state == PROCESS_TERMINATED);
    assert(process->address_space == NULL);
    assert(process_get_address_space_for_thread(101U) == NULL);
    assert(process_attach_address_space(2U, &process_space) ==
        SYNTRA_STATUS_INVALID_ARGUMENT);
    assert(process_validate() == SYNTRA_STATUS_OK);

    /* The kernel root and its borrowed subtree remain owned by the kernel. */
    kernel_root = resolve_page(kernel_table.root_physical_address, NULL);
    assert(kernel_root != NULL);
    assert(syntra_arm64_mmu_get_table_indices(
        0x10000000ULL, indices) == SYNTRA_STATUS_OK);
    kernel_l2 = resolve_page(
        kernel_root[indices[0]] & SYNTRA_ARM64_MMU_PA_MASK, NULL);
    assert(kernel_l2 != NULL);

    assert(syntra_page_free_owned(
        (uintptr_t)(kernel_l2[indices[1]] & SYNTRA_ARM64_MMU_PA_MASK),
        SYNTRA_PAGE_OWNER_PAGE_TABLE) == SYNTRA_STATUS_OK);
    assert(syntra_page_free_owned(
        (uintptr_t)(kernel_root[indices[0]] & SYNTRA_ARM64_MMU_PA_MASK),
        SYNTRA_PAGE_OWNER_PAGE_TABLE) == SYNTRA_STATUS_OK);
    assert(syntra_page_free_owned(
        (uintptr_t)kernel_table.root_physical_address,
        SYNTRA_PAGE_OWNER_PAGE_TABLE) == SYNTRA_STATUS_OK);
    assert(syntra_page_free_owned(
        kernel_data_page, SYNTRA_PAGE_OWNER_KERNEL) == SYNTRA_STATUS_OK);
    assert(syntra_page_allocator_free_count() == initial_free);
    assert(syntra_page_allocator_validate() == SYNTRA_STATUS_OK);

    puts("Process Manager address-space ownership and termination cleanup passed");
    return 0;
}
