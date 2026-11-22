#ifdef NDEBUG
#undef NDEBUG
#endif

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "../../kernel/arch/arm64/address_space.h"
#include "../../kernel/arch/arm64/context.h"
#include "../../kernel/memory/memory_region.h"
#include "../../kernel/memory/page_allocator.h"
#include "../../kernel/process/process.h"
#include "../../kernel/scheduler/scheduler.h"

#define TEST_RAM_BASE 0x20000000ULL
#define TEST_RAM_PAGES 128U
#define TEST_KERNEL_VA 0x40000000ULL

static uint64_t table_storage[TEST_RAM_PAGES][SYNTRA_ARM64_MMU_TABLE_ENTRIES];

static _Alignas(16) uint8_t stack_a1[2048];
static _Alignas(16) uint8_t stack_a2[2048];
static _Alignas(16) uint8_t stack_b1[2048];
static _Alignas(16) uint8_t stack_b2[2048];
static _Alignas(16) uint8_t user_stack_a1[2048];
static _Alignas(16) uint8_t user_stack_a2[2048];
static _Alignas(16) uint8_t user_stack_b1[2048];
static _Alignas(16) uint8_t user_stack_b2[2048];
static _Alignas(16) uint8_t context_a1[320];
static _Alignas(16) uint8_t context_a2[320];
static _Alignas(16) uint8_t context_b1[320];
static _Alignas(16) uint8_t context_b2[320];

typedef struct
{
    uint64_t ttbr0;
    size_t write_count;
    int fail_next_write;
} mock_mmu_t;

static void thread_entry(void *argument)
{
    (void)argument;
}

static syntra_status_t allocate_table_page(
    void *context, uint64_t *physical_address)
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
        ((physical_address - TEST_RAM_BASE) % SYNTRA_PAGE_SIZE) != 0ULL)
        return NULL;

    return table_storage[
        (physical_address - TEST_RAM_BASE) / SYNTRA_PAGE_SIZE];
}

static uint64_t mock_read_ttbr0(void *context)
{
    return ((mock_mmu_t *)context)->ttbr0;
}

static syntra_status_t mock_write_ttbr0(
    uint64_t root_physical_address, void *context)
{
    mock_mmu_t *mmu = (mock_mmu_t *)context;

    if (mmu->fail_next_write != 0)
    {
        mmu->fail_next_write = 0;
        return SYNTRA_STATUS_ERROR;
    }

    mmu->ttbr0 = root_physical_address;
    ++mmu->write_count;
    return SYNTRA_STATUS_OK;
}

static void create_process_address_space(
    syntra_arm64_address_space_t *space,
    uint32_t address_space_id,
    uint64_t kernel_root,
    uint64_t user_virtual_address)
{
    uintptr_t user_page;

    assert(syntra_arm64_address_space_create(
        space, address_space_id, resolve_page, NULL) == SYNTRA_STATUS_OK);
    assert(syntra_arm64_address_space_inherit_kernel_mappings(
        space, kernel_root) == SYNTRA_STATUS_OK);
    assert(syntra_page_alloc_owned(
        &user_page, SYNTRA_PAGE_OWNER_PROCESS) == SYNTRA_STATUS_OK);
    assert(syntra_arm64_address_space_map_user_page(
        space, user_virtual_address, (uint64_t)user_page,
        SYNTRA_ARM64_MMU_ACCESS_READ_WRITE, 0) == SYNTRA_STATUS_OK);
}

static void create_user_thread(
    unsigned int thread_id,
    unsigned int process_id,
    void *kernel_stack,
    void *user_stack,
    void *cpu_context)
{
    assert(scheduler_create_user_thread(
        thread_id, 5U, thread_entry, NULL,
        kernel_stack, 2048U, user_stack, 2048U, cpu_context) ==
        SYNTRA_STATUS_OK);
    assert(process_attach_thread(process_id, thread_id) == SYNTRA_STATUS_OK);
}

static syntra_thread_t *schedule_and_activate(
    unsigned int expected_thread_id,
    syntra_arm64_address_space_t *expected_space,
    mock_mmu_t *mmu)
{
    syntra_thread_t *selected =
        scheduler_schedule_next_with_context();
    assert(selected != NULL);
    assert(selected->thread_id == expected_thread_id);

    assert(process_get_address_space_for_thread(selected->thread_id) ==
        expected_space);
    assert(syntra_arm64_address_space_activate(expected_space) ==
        SYNTRA_STATUS_OK);
    assert(mmu->ttbr0 == expected_space->page_table.root_physical_address);

    assert(process_refresh_all_states() == SYNTRA_STATUS_OK);
    assert(process_reap_terminated() == SYNTRA_STATUS_OK);
    assert(process_validate() == SYNTRA_STATUS_OK);
    return selected;
}

int main(void)
{
    static syntra_arm64_page_table_t kernel_table;
    static syntra_arm64_address_space_t process_a_space;
    static syntra_arm64_address_space_t process_b_space;
    mock_mmu_t mmu = {0};
    syntra_page_allocator_stats_t baseline;
    syntra_page_allocator_stats_t stats;
    syntra_thread_t *selected;
    uintptr_t kernel_data_page;
    uint64_t kernel_root;
    size_t initial_free;

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
        &kernel_table, TEST_KERNEL_VA, (uint64_t)kernel_data_page,
        SYNTRA_PAGE_SIZE, SYNTRA_ARM64_MMU_MEMORY_NORMAL,
        SYNTRA_ARM64_MMU_ACCESS_READ_WRITE, 0) == SYNTRA_STATUS_OK);
    kernel_root = kernel_table.root_physical_address;

    create_process_address_space(
        &process_a_space, 1U, kernel_root,
        SYNTRA_ARM64_USER_VA_BASE);
    create_process_address_space(
        &process_b_space, 2U, kernel_root,
        SYNTRA_ARM64_USER_VA_BASE + SYNTRA_PAGE_SIZE);

    assert(process_create(1U, 5U) == SYNTRA_STATUS_OK);
    assert(process_create(2U, 5U) == SYNTRA_STATUS_OK);
    assert(process_attach_address_space(1U, &process_a_space) ==
        SYNTRA_STATUS_OK);
    assert(process_attach_address_space(2U, &process_b_space) ==
        SYNTRA_STATUS_OK);

    create_user_thread(11U, 1U, stack_a1, user_stack_a1, context_a1);
    create_user_thread(12U, 1U, stack_a2, user_stack_a2, context_a2);
    create_user_thread(21U, 2U, stack_b1, user_stack_b1, context_b1);
    create_user_thread(22U, 2U, stack_b2, user_stack_b2, context_b2);

    /* Thread ownership lookup is the bridge required by EL0 process exit. */
    assert(process_get_id_for_thread(11U) == 1U);
    assert(process_get_id_for_thread(12U) == 1U);
    assert(process_get_id_for_thread(21U) == 2U);
    assert(process_get_id_for_thread(22U) == 2U);
    assert(process_get_id_for_thread(999U) == 0U);
    assert(process_get_id_for_thread(0U) == 0U);

    mmu.ttbr0 = kernel_root;
    syntra_arm64_address_space_test_set_mmu_hooks(
        mock_read_ttbr0, mock_write_ttbr0, &mmu);
    assert(syntra_arm64_address_space_register_kernel_root(kernel_root) ==
        SYNTRA_STATUS_OK);
    assert(syntra_page_allocator_get_stats(&baseline) == SYNTRA_STATUS_OK);
    assert(process_validate() == SYNTRA_STATUS_OK);

    /* A1 -> A2 keeps Process A's TTBR0 root; then force A -> B failure. */
    selected = schedule_and_activate(11U, &process_a_space, &mmu);
    assert(process_get(1U)->state == PROCESS_RUNNING);
    assert(process_get(2U)->state == PROCESS_READY);
    assert(mmu.write_count == 1U);

    selected = schedule_and_activate(12U, &process_a_space, &mmu);
    assert(selected->thread_id == 12U);
    assert(mmu.write_count == 1U);

    selected = scheduler_schedule_next_with_context();
    assert(selected != NULL && selected->thread_id == 21U);
    mmu.fail_next_write = 1;
    assert(syntra_arm64_address_space_activate(&process_b_space) ==
        SYNTRA_STATUS_ERROR);
    assert(scheduler_rollback_context_switch(12U, 21U) == SYNTRA_STATUS_OK);
    assert(process_refresh_all_states() == SYNTRA_STATUS_OK);
    assert(scheduler_get_current() != NULL);
    assert(scheduler_get_current()->thread_id == 12U);
    assert(process_get(1U)->state == PROCESS_RUNNING);
    assert(process_get(2U)->state == PROCESS_READY);
    assert(mmu.ttbr0 == process_a_space.page_table.root_physical_address);
    assert(process_validate() == SYNTRA_STATUS_OK);

    /* Deterministic retry enters Process B, then repeated B thread stays there. */
    selected = schedule_and_activate(21U, &process_b_space, &mmu);
    assert(selected->thread_id == 21U);
    assert(mmu.write_count == 2U);
    selected = schedule_and_activate(22U, &process_b_space, &mmu);
    assert(selected->thread_id == 22U);
    assert(mmu.write_count == 2U);

    /* Switch back to A and terminate its threads one at a time. */
    selected = schedule_and_activate(11U, &process_a_space, &mmu);
    assert(selected->thread_id == 11U);
    assert(mmu.write_count == 3U);
    assert(scheduler_terminate_current() == SYNTRA_STATUS_OK);
    assert(process_refresh_all_states() == SYNTRA_STATUS_OK);
    assert(process_get(1U)->state == PROCESS_READY);

    selected = schedule_and_activate(12U, &process_a_space, &mmu);
    assert(selected->thread_id == 12U);
    assert(process_get(1U)->state == PROCESS_RUNNING);

    /* The architecture exit trampoline terminates the complete owning process. */
    syntra_arm64_thread_exit();
    assert(scheduler_get_current() == NULL);
    assert(scheduler_get_thread(11U)->state == THREAD_TERMINATED);
    assert(scheduler_get_thread(12U)->state == THREAD_TERMINATED);
    assert(process_refresh_all_states() == SYNTRA_STATUS_OK);
    assert(process_get(1U)->state == PROCESS_TERMINATED);
    assert(process_get(1U)->address_space == &process_a_space);
    assert(process_validate() == SYNTRA_STATUS_OK);

    /* B is selected before A is reaped, so A's active root is never freed. */
    selected = schedule_and_activate(21U, &process_b_space, &mmu);
    assert(selected->thread_id == 21U);
    assert(process_get(1U)->state == PROCESS_TERMINATED);
    assert(process_get(1U)->address_space == NULL);
    assert(process_get(2U)->state == PROCESS_RUNNING);
    assert(process_get_address_space_for_thread(11U) == NULL);
    assert(process_get_address_space_for_thread(12U) == NULL);
    assert(mmu.ttbr0 == process_b_space.page_table.root_physical_address);
    assert(process_validate() == SYNTRA_STATUS_OK);

    assert(scheduler_terminate_current() == SYNTRA_STATUS_OK);
    assert(process_refresh_all_states() == SYNTRA_STATUS_OK);
    assert(process_get(2U)->state == PROCESS_READY);
    selected = schedule_and_activate(22U, &process_b_space, &mmu);
    assert(selected->thread_id == 22U);
    assert(scheduler_terminate_current() == SYNTRA_STATUS_OK);

    /* No runnable thread: move to kernel TTBR0 before reaping Process B. */
    assert(scheduler_schedule_next_with_context() == NULL);
    assert(syntra_arm64_address_space_activate_kernel_root() ==
        SYNTRA_STATUS_OK);
    assert(mmu.ttbr0 == kernel_root);
    assert(process_refresh_all_states() == SYNTRA_STATUS_OK);
    assert(process_reap_terminated() == SYNTRA_STATUS_OK);
    assert(process_get(2U)->state == PROCESS_TERMINATED);
    assert(process_get(2U)->address_space == NULL);
    assert(process_get_address_space_for_thread(21U) == NULL);
    assert(process_get_address_space_for_thread(22U) == NULL);
    assert(process_validate() == SYNTRA_STATUS_OK);

    assert(syntra_page_allocator_get_stats(&stats) == SYNTRA_STATUS_OK);
    assert(stats.owner_pages[SYNTRA_PAGE_OWNER_PROCESS] == 0U);
    assert(stats.owner_pages[SYNTRA_PAGE_OWNER_PAGE_TABLE] == 3U);
    assert(stats.owner_pages[SYNTRA_PAGE_OWNER_KERNEL] ==
        baseline.owner_pages[SYNTRA_PAGE_OWNER_KERNEL]);
    assert(syntra_page_allocator_validate() == SYNTRA_STATUS_OK);

    /* Release the kernel's private L2/L3 tables and root after all users exit. */
    {
        uint64_t *root_entries = resolve_page(kernel_root, NULL);
        uint16_t indices[4];
        uint64_t *l2_entries;
        assert(root_entries != NULL);
        assert(syntra_arm64_mmu_get_table_indices(TEST_KERNEL_VA, indices) ==
            SYNTRA_STATUS_OK);
        l2_entries = resolve_page(
            root_entries[indices[0]] & SYNTRA_ARM64_MMU_PA_MASK, NULL);
        assert(l2_entries != NULL);
        assert(syntra_page_free_owned(
            (uintptr_t)(l2_entries[indices[1]] &
                SYNTRA_ARM64_MMU_PA_MASK),
            SYNTRA_PAGE_OWNER_PAGE_TABLE) == SYNTRA_STATUS_OK);
        assert(syntra_page_free_owned(
            (uintptr_t)(root_entries[indices[0]] &
                SYNTRA_ARM64_MMU_PA_MASK),
            SYNTRA_PAGE_OWNER_PAGE_TABLE) == SYNTRA_STATUS_OK);
    }
    assert(syntra_page_free_owned(
        (uintptr_t)kernel_root, SYNTRA_PAGE_OWNER_PAGE_TABLE) ==
        SYNTRA_STATUS_OK);
    assert(syntra_page_free_owned(
        kernel_data_page, SYNTRA_PAGE_OWNER_KERNEL) == SYNTRA_STATUS_OK);
    assert(syntra_page_allocator_free_count() == initial_free);
    assert(syntra_page_allocator_validate() == SYNTRA_STATUS_OK);

    puts("Process/Scheduler/ARM64 address-space integration test passed");
    return 0;
}
