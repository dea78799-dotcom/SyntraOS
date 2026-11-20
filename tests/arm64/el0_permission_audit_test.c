#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "page_table_builder.h"

#define TABLE_COUNT 8U
#define TABLE_BASE_PA 0x1000ULL
#define PAGE_SIZE SYNTRA_ARM64_MMU_PAGE_SIZE

static uint64_t table_storage[TABLE_COUNT][SYNTRA_ARM64_MMU_TABLE_ENTRIES];
static unsigned int allocated_tables;

static syntra_status_t allocate_table(void *context, uint64_t *physical_address)
{
    (void)context;
    if (physical_address == NULL || allocated_tables >= TABLE_COUNT)
        return SYNTRA_STATUS_OUT_OF_MEMORY;
    *physical_address = TABLE_BASE_PA +
        (uint64_t)allocated_tables * PAGE_SIZE;
    ++allocated_tables;
    return SYNTRA_STATUS_OK;
}

static uint64_t *resolve_table(uint64_t physical_address, void *context)
{
    uint64_t offset;
    (void)context;
    if (physical_address < TABLE_BASE_PA)
        return NULL;
    offset = physical_address - TABLE_BASE_PA;
    if (offset >= (uint64_t)TABLE_COUNT * PAGE_SIZE ||
        (offset & (PAGE_SIZE - 1ULL)) != 0ULL)
        return NULL;
    return table_storage[offset / PAGE_SIZE];
}

static void reset_tables(void)
{
    allocated_tables = 0U;
    for (unsigned int table = 0U; table < TABLE_COUNT; ++table)
        for (unsigned int entry = 0U;
             entry < SYNTRA_ARM64_MMU_TABLE_ENTRIES; ++entry)
            table_storage[table][entry] = 0ULL;
}

static syntra_arm64_mmu_translation_t walk(
    syntra_arm64_page_table_t *page_table, uint64_t virtual_address)
{
    syntra_arm64_mmu_translation_t translation;
    assert(syntra_arm64_mmu_walk(
        page_table->root_physical_address,
        virtual_address,
        (syntra_arm64_mmu_table_resolver_t)resolve_table,
        NULL,
        &translation) == SYNTRA_STATUS_OK);
    return translation;
}

static void test_el0_is_denied_kernel_pages(void)
{
    syntra_arm64_page_table_t page_table;
    syntra_arm64_mmu_translation_t text, data;

    reset_tables();
    assert(syntra_arm64_page_table_init(
        &page_table, allocate_table, resolve_table, NULL) == SYNTRA_STATUS_OK);

    /* Kernel text: EL1 read/execute, EL0 completely denied. */
    assert(syntra_arm64_page_table_map(
        &page_table, 0x40000000ULL, 0x80000000ULL, PAGE_SIZE,
        SYNTRA_ARM64_MMU_MEMORY_NORMAL,
        SYNTRA_ARM64_MMU_ACCESS_READ_ONLY, 1) == SYNTRA_STATUS_OK);
    assert(syntra_arm64_page_table_verify(
        &page_table, 0x40000000ULL, 0x80000000ULL, PAGE_SIZE,
        SYNTRA_ARM64_MMU_MEMORY_NORMAL,
        SYNTRA_ARM64_MMU_ACCESS_READ_ONLY, 1) == SYNTRA_STATUS_OK);

    /* Kernel data: EL1 read/write, execute-never, EL0 completely denied. */
    assert(syntra_arm64_page_table_map(
        &page_table, 0x40001000ULL, 0x80001000ULL, PAGE_SIZE,
        SYNTRA_ARM64_MMU_MEMORY_NORMAL,
        SYNTRA_ARM64_MMU_ACCESS_READ_WRITE, 0) == SYNTRA_STATUS_OK);
    assert(syntra_arm64_page_table_verify(
        &page_table, 0x40001000ULL, 0x80001000ULL, PAGE_SIZE,
        SYNTRA_ARM64_MMU_MEMORY_NORMAL,
        SYNTRA_ARM64_MMU_ACCESS_READ_WRITE, 0) == SYNTRA_STATUS_OK);

    text = walk(&page_table, 0x40000000ULL);
    assert(text.privileged_read == 1U);
    assert(text.privileged_write == 0U);
    assert(text.privileged_execute == 1U);
    assert(text.user_read == 0U);
    assert(text.user_write == 0U);
    assert(text.user_execute == 0U);

    data = walk(&page_table, 0x40001000ULL);
    assert(data.privileged_read == 1U);
    assert(data.privileged_write == 1U);
    assert(data.privileged_execute == 0U);
    assert(data.user_read == 0U);
    assert(data.user_write == 0U);
    assert(data.user_execute == 0U);

    puts("PASS: EL0 has no read/write/execute permission on kernel text or data");
}

static void test_el0_code_is_executable_but_not_privileged(void)
{
    syntra_arm64_page_table_t page_table;
    syntra_arm64_mmu_translation_t code, stack;

    reset_tables();
    assert(syntra_arm64_page_table_init(
        &page_table, allocate_table, resolve_table, NULL) == SYNTRA_STATUS_OK);

    /* User code: EL0 read/execute, read-only, EL1 execute disabled by PXN. */
    assert(syntra_arm64_page_table_map_user(
        &page_table, 0x50000000ULL, 0x90000000ULL, PAGE_SIZE,
        SYNTRA_ARM64_MMU_ACCESS_READ_ONLY, 1) == SYNTRA_STATUS_OK);
    assert(syntra_arm64_page_table_verify_user(
        &page_table, 0x50000000ULL, 0x90000000ULL, PAGE_SIZE,
        SYNTRA_ARM64_MMU_ACCESS_READ_ONLY, 1) == SYNTRA_STATUS_OK);

    /* User stack: EL0 read/write, never execute at either privilege level. */
    assert(syntra_arm64_page_table_map_user(
        &page_table, 0x50001000ULL, 0x90001000ULL, PAGE_SIZE,
        SYNTRA_ARM64_MMU_ACCESS_READ_WRITE, 0) == SYNTRA_STATUS_OK);
    assert(syntra_arm64_page_table_verify_user(
        &page_table, 0x50001000ULL, 0x90001000ULL, PAGE_SIZE,
        SYNTRA_ARM64_MMU_ACCESS_READ_WRITE, 0) == SYNTRA_STATUS_OK);

    code = walk(&page_table, 0x50000000ULL);
    assert(code.user_read == 1U);
    assert(code.user_write == 0U);
    assert(code.user_execute == 1U);
    assert(code.privileged_read == 1U);
    assert(code.privileged_write == 0U);
    assert(code.privileged_execute == 0U);

    stack = walk(&page_table, 0x50001000ULL);
    assert(stack.user_read == 1U);
    assert(stack.user_write == 1U);
    assert(stack.user_execute == 0U);
    assert(stack.privileged_execute == 0U);

    puts("PASS: EL0 code is executable; EL1 cannot execute user code");
    puts("PASS: EL0 stack is writable and execute-never");
}

int main(void)
{
    test_el0_is_denied_kernel_pages();
    test_el0_code_is_executable_but_not_privileged();
    puts("ARM64 EL1/EL0 MMU permission audit passed");
    return 0;
}
