#ifdef NDEBUG
#undef NDEBUG
#endif

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "../../kernel/arch/arm64/address_space.h"
#include "../../kernel/memory/memory_region.h"
#include "../../kernel/memory/page_allocator.h"
#include "../../kernel/process/process.h"
#include "../../kernel/scheduler/scheduler.h"

#define TEST_RAM_BASE 0x20000000ULL
#define TEST_RAM_PAGES 48U
#define TEST_KERNEL_VA 0x40000000ULL
#define PROCESS_A_ID 11U
#define PROCESS_B_ID 22U
#define THREAD_A_ID 201U
#define THREAD_B_ID 202U

static uint64_t table_storage[TEST_RAM_PAGES][SYNTRA_ARM64_MMU_TABLE_ENTRIES];
static _Alignas(16) uint8_t kernel_stack_a[2048];
static _Alignas(16) uint8_t kernel_stack_b[2048];
static _Alignas(16) uint8_t user_stack_a[2048];
static _Alignas(16) uint8_t user_stack_b[2048];
static _Alignas(16) uint8_t thread_context_a[16];
static _Alignas(16) uint8_t thread_context_b[16];

typedef struct
{
    uint64_t ttbr0;
    size_t writes;
    int fail_next_write;
} mock_mmu_t;

static void user_entry(void *argument)
{
    (void)argument;
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

static syntra_status_t allocate_table_page(
    void *context,
    uint64_t *physical_address)
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
    return ((mock_mmu_t *)context)->ttbr0;
}

static syntra_status_t mock_write_ttbr0(
    uint64_t root_physical_address,
    void *context)
{
    mock_mmu_t *mmu = (mock_mmu_t *)context;

    if (mmu->fail_next_write != 0)
    {
        mmu->fail_next_write = 0;
        return SYNTRA_STATUS_ERROR;
    }

    mmu->ttbr0 = root_physical_address;
    mmu->writes++;
    return SYNTRA_STATUS_OK;
}

/* Mirrors the production exception bridge's thread -> process -> TTBR policy. */
static syntra_status_t activate_thread_address_space(unsigned int thread_id)
{
    syntra_thread_t *thread = scheduler_get_thread(thread_id);
    syntra_arm64_address_space_t *space =
        process_get_address_space_for_thread(thread_id);

    if (thread == NULL)
        return SYNTRA_STATUS_INVALID_ARGUMENT;

    if (thread->is_user_thread != 0U && space == NULL)
        return SYNTRA_STATUS_PERMISSION_DENIED;

    if (space != NULL)
        return syntra_arm64_address_space_activate(space);

    return syntra_arm64_address_space_activate_kernel_root();
}

static void refresh_process_states(void)
{
    assert(process_refresh_state(PROCESS_A_ID) == SYNTRA_STATUS_OK);
    assert(process_refresh_state(PROCESS_B_ID) == SYNTRA_STATUS_OK);
}

static syntra_thread_t *schedule_and_activate(void)
{
    syntra_thread_t *next = scheduler_schedule_next_with_context();
    assert(next != NULL);
    assert(activate_thread_address_space(next->thread_id) == SYNTRA_STATUS_OK);
    refresh_process_states();
    assert(scheduler_validate() == SYNTRA_STATUS_OK);
    assert(process_validate() == SYNTRA_STATUS_OK);
    return next;
}

int main(void)
{
    syntra_arm64_page_table_t kernel_table = {0};
    syntra_arm64_address_space_t process_space_a = {0};
    syntra_arm64_address_space_t process_space_b = {0};
    mock_mmu_t mmu = {0};
    uintptr_t kernel_data_page, process_data_page_a, process_data_page_b;
    uint64_t kernel_root;
    uint64_t *kernel_root_entries, *kernel_l2_entries;
    uint16_t indices[4];
    size_t initial_free;
    syntra_thread_t *selected;

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
        &kernel_data_page, SYNTRA_PAGE_OWNER_DRIVER) == SYNTRA_STATUS_OK);
    assert(syntra_arm64_page_table_init(
        &kernel_table, allocate_table_page, resolve_page, NULL) ==
        SYNTRA_STATUS_OK);
    kernel_root = kernel_table.root_physical_address;
    assert(syntra_arm64_page_table_map(
        &kernel_table, TEST_KERNEL_VA, kernel_data_page,
        SYNTRA_PAGE_SIZE, SYNTRA_ARM64_MMU_MEMORY_NORMAL,
        SYNTRA_ARM64_MMU_ACCESS_READ_WRITE, 0) == SYNTRA_STATUS_OK);

    assert(syntra_arm64_address_space_create(
        &process_space_a, 1U, resolve_page, NULL) == SYNTRA_STATUS_OK);
    assert(syntra_arm64_address_space_create(
        &process_space_b, 2U, resolve_page, NULL) == SYNTRA_STATUS_OK);
    assert(syntra_arm64_address_space_inherit_kernel_mappings(
        &process_space_a, kernel_root) == SYNTRA_STATUS_OK);
    assert(syntra_arm64_address_space_inherit_kernel_mappings(
        &process_space_b, kernel_root) == SYNTRA_STATUS_OK);

    assert(syntra_page_alloc_owned(
        &process_data_page_a, SYNTRA_PAGE_OWNER_PROCESS) == SYNTRA_STATUS_OK);
    assert(syntra_page_alloc_owned(
        &process_data_page_b, SYNTRA_PAGE_OWNER_PROCESS) == SYNTRA_STATUS_OK);
    assert(syntra_arm64_address_space_map_user_page(
        &process_space_a, SYNTRA_ARM64_USER_VA_BASE,
        process_data_page_a, SYNTRA_ARM64_MMU_ACCESS_READ_WRITE, 0) ==
        SYNTRA_STATUS_OK);
    assert(syntra_arm64_address_space_map_user_page(
        &process_space_b, SYNTRA_ARM64_USER_VA_BASE,
        process_data_page_b, SYNTRA_ARM64_MMU_ACCESS_READ_WRITE, 0) ==
        SYNTRA_STATUS_OK);

    mmu.ttbr0 = kernel_root;
    syntra_arm64_address_space_test_set_mmu_hooks(
        mock_read_ttbr0, mock_write_ttbr0, &mmu);
    assert(syntra_arm64_address_space_register_kernel_root(kernel_root) ==
        SYNTRA_STATUS_OK);

    assert(process_create(PROCESS_A_ID, 5U) == SYNTRA_STATUS_OK);
    assert(process_create(PROCESS_B_ID, 5U) == SYNTRA_STATUS_OK);
    assert(process_attach_address_space(PROCESS_A_ID, &process_space_a) ==
        SYNTRA_STATUS_OK);
    assert(process_attach_address_space(PROCESS_B_ID, &process_space_b) ==
        SYNTRA_STATUS_OK);

    assert(scheduler_create_user_thread(
        THREAD_A_ID, 5U, user_entry, NULL,
        kernel_stack_a, sizeof(kernel_stack_a),
        user_stack_a, sizeof(user_stack_a), thread_context_a) ==
        SYNTRA_STATUS_OK);
    assert(scheduler_create_user_thread(
        THREAD_B_ID, 5U, user_entry, NULL,
        kernel_stack_b, sizeof(kernel_stack_b),
        user_stack_b, sizeof(user_stack_b), thread_context_b) ==
        SYNTRA_STATUS_OK);
    assert(process_attach_thread(PROCESS_A_ID, THREAD_A_ID) ==
        SYNTRA_STATUS_OK);
    assert(process_attach_thread(PROCESS_B_ID, THREAD_B_ID) ==
        SYNTRA_STATUS_OK);
    assert(process_get_address_space_for_thread(THREAD_A_ID) ==
        &process_space_a);
    assert(process_get_address_space_for_thread(THREAD_B_ID) ==
        &process_space_b);
    assert(process_validate() == SYNTRA_STATUS_OK);

    /*
     * Initial activation failure: no previous scheduler thread exists.
     * The selected user thread must be restored to READY and retried safely.
     */
    mmu.fail_next_write = 1;
    selected = scheduler_schedule_next_with_context();
    assert(selected != NULL && selected->thread_id == THREAD_A_ID);
    assert(activate_thread_address_space(selected->thread_id) ==
        SYNTRA_STATUS_ERROR);
    assert(scheduler_rollback_context_switch(0U, THREAD_A_ID) ==
        SYNTRA_STATUS_OK);
    assert(scheduler_get_current() == NULL);
    assert(scheduler_get_thread(THREAD_A_ID)->state == THREAD_READY);
    assert(mmu.ttbr0 == kernel_root);

    selected = schedule_and_activate();
    assert(selected->thread_id == THREAD_A_ID);
    assert(mmu.ttbr0 == process_space_a.page_table.root_physical_address);
    assert(process_get(PROCESS_A_ID)->state == PROCESS_RUNNING);
    assert(process_get(PROCESS_B_ID)->state == PROCESS_READY);

    /*
     * Fail A -> B activation. Scheduler must return to A and leave TTBR0
     * pointing at A; the process state snapshots must agree after refresh.
     */
    selected = scheduler_schedule_next_with_context();
    assert(selected != NULL && selected->thread_id == THREAD_B_ID);
    mmu.fail_next_write = 1;
    assert(activate_thread_address_space(selected->thread_id) ==
        SYNTRA_STATUS_ERROR);
    assert(scheduler_rollback_context_switch(THREAD_A_ID, THREAD_B_ID) ==
        SYNTRA_STATUS_OK);
    refresh_process_states();
    assert(scheduler_get_current()->thread_id == THREAD_A_ID);
    assert(scheduler_get_thread(THREAD_A_ID)->state == THREAD_RUNNING);
    assert(scheduler_get_thread(THREAD_B_ID)->state == THREAD_READY);
    assert(mmu.ttbr0 == process_space_a.page_table.root_physical_address);
    assert(process_get(PROCESS_A_ID)->state == PROCESS_RUNNING);
    assert(process_get(PROCESS_B_ID)->state == PROCESS_READY);
    assert(process_validate() == SYNTRA_STATUS_OK);

    /* Retry A -> B and then switch back to A; TTBR0 must follow each owner. */
    selected = schedule_and_activate();
    assert(selected->thread_id == THREAD_B_ID);
    assert(mmu.ttbr0 == process_space_b.page_table.root_physical_address);
    assert(process_get(PROCESS_B_ID)->state == PROCESS_RUNNING);
    assert(process_get(PROCESS_A_ID)->state == PROCESS_READY);

    selected = schedule_and_activate();
    assert(selected->thread_id == THREAD_A_ID);
    assert(mmu.ttbr0 == process_space_a.page_table.root_physical_address);

    /*
     * Terminate B while A is active. Its process can now be destroyed because
     * its root is inactive; A's root must remain protected.
     */
    assert(scheduler_set_thread_state(THREAD_B_ID, THREAD_TERMINATED) ==
        SYNTRA_STATUS_OK);
    assert(process_refresh_state(PROCESS_B_ID) == SYNTRA_STATUS_OK);
    assert(process_get(PROCESS_B_ID)->state == PROCESS_TERMINATED);
    assert(process_terminate(PROCESS_B_ID) == SYNTRA_STATUS_OK);
    assert(process_get(PROCESS_B_ID)->address_space == NULL);
    assert(process_get_address_space_for_thread(THREAD_B_ID) == NULL);
    assert(mmu.ttbr0 == process_space_a.page_table.root_physical_address);

    /* Terminate A; its active root cannot be freed until kernel root restored. */
    assert(scheduler_terminate_current() == SYNTRA_STATUS_OK);
    assert(process_refresh_state(PROCESS_A_ID) == SYNTRA_STATUS_OK);
    assert(process_get(PROCESS_A_ID)->state == PROCESS_TERMINATED);
    assert(process_terminate(PROCESS_A_ID) == SYNTRA_STATUS_PERMISSION_DENIED);
    assert(process_space_a.initialized == 1U);
    assert(scheduler_schedule_next_with_context() == NULL);

    assert(syntra_arm64_address_space_activate_kernel_root() ==
        SYNTRA_STATUS_OK);
    assert(mmu.ttbr0 == kernel_root);
    assert(process_terminate(PROCESS_A_ID) == SYNTRA_STATUS_OK);
    assert(process_get(PROCESS_A_ID)->address_space == NULL);
    assert(process_validate() == SYNTRA_STATUS_OK);

    /* Both process roots are gone; the borrowed kernel subtree still exists. */
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

    puts("Process/Scheduler/ARM64 address-space integration test passed");
    return 0;
}
