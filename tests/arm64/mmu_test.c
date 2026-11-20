#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "../../kernel/arch/arm64/mmu.h"

#define ROOT_PA  0x1000ULL
#define L2_PA    0x2000ULL
#define L3_PA    0x3000ULL
#define TARGET_PA 0x40000000ULL

static uint64_t root_table[SYNTRA_ARM64_MMU_TABLE_ENTRIES];
static uint64_t level2_table[SYNTRA_ARM64_MMU_TABLE_ENTRIES];
static uint64_t level3_table[SYNTRA_ARM64_MMU_TABLE_ENTRIES];

static const uint64_t *resolve_table(uint64_t physical_address, void *context)
{
    (void)context;
    switch (physical_address)
    {
        case ROOT_PA: return root_table;
        case L2_PA: return level2_table;
        case L3_PA: return level3_table;
        default: return NULL;
    }
}

static void clear_tables(void)
{
    unsigned int i;
    for (i = 0U; i < SYNTRA_ARM64_MMU_TABLE_ENTRIES; ++i)
    {
        root_table[i] = 0ULL;
        level2_table[i] = 0ULL;
        level3_table[i] = 0ULL;
    }
}

static void test_page_table_walk(void)
{
    syntra_arm64_mmu_translation_t result;
    uint64_t descriptor;
    const uint64_t virtual_address = 0x0000000012345123ULL;
    uint16_t indices[4];

    clear_tables();
    assert(syntra_arm64_mmu_get_table_indices(
        virtual_address, indices) == SYNTRA_STATUS_OK);
    assert(syntra_arm64_mmu_make_table_descriptor(
        L2_PA, &descriptor) == SYNTRA_STATUS_OK);
    root_table[indices[0]] = descriptor;
    assert(syntra_arm64_mmu_make_table_descriptor(
        L3_PA, &descriptor) == SYNTRA_STATUS_OK);
    level2_table[indices[1]] = descriptor;
    assert(syntra_arm64_mmu_make_page_descriptor(
        TARGET_PA, SYNTRA_ARM64_MMU_MEMORY_NORMAL,
        SYNTRA_ARM64_MMU_ACCESS_READ_WRITE, 0, &descriptor) ==
        SYNTRA_STATUS_OK);
    level3_table[indices[2]] = descriptor;

    assert(syntra_arm64_mmu_walk(
        ROOT_PA, virtual_address, resolve_table, NULL, &result) ==
        SYNTRA_STATUS_OK);
    assert(result.physical_address == TARGET_PA + 0x123ULL);
    assert(result.page_descriptor == descriptor);
    assert(result.level == 3U);
}

static void test_walker_rejects_invalid_paths(void)
{
    syntra_arm64_mmu_translation_t result;
    uint64_t descriptor;
    uint16_t indices[4];
    const uint64_t va = 0x0000000000400123ULL;

    clear_tables();
    assert(syntra_arm64_mmu_walk(
        ROOT_PA, va, resolve_table, NULL, &result) == SYNTRA_STATUS_ERROR);

    assert(syntra_arm64_mmu_get_table_indices(va, indices) == SYNTRA_STATUS_OK);
    root_table[indices[0]] = 1ULL; /* Valid L1 block, unsupported. */
    assert(syntra_arm64_mmu_walk(
        ROOT_PA, va, resolve_table, NULL, &result) ==
        SYNTRA_STATUS_NOT_IMPLEMENTED);

    assert(syntra_arm64_mmu_make_table_descriptor(
        L2_PA, &descriptor) == SYNTRA_STATUS_OK);
    root_table[indices[0]] = descriptor;
    level2_table[indices[1]] = descriptor;
    assert(syntra_arm64_mmu_walk(
        ROOT_PA, va, resolve_table, NULL, &result) == SYNTRA_STATUS_ERROR);

    assert(syntra_arm64_mmu_walk(
        ROOT_PA, va, NULL, NULL, &result) ==
        SYNTRA_STATUS_INVALID_ARGUMENT);
    assert(syntra_arm64_mmu_walk(
        ROOT_PA + 1ULL, va, resolve_table, NULL, &result) ==
        SYNTRA_STATUS_INVALID_ARGUMENT);
    assert(syntra_arm64_mmu_walk(
        ROOT_PA, 1ULL << SYNTRA_ARM64_MMU_VA_BITS,
        resolve_table, NULL, &result) == SYNTRA_STATUS_INVALID_ARGUMENT);
}

static void test_register_api_is_host_safe(void)
{
    syntra_arm64_mmu_registers_t registers;
    assert(syntra_arm64_mmu_read_registers(&registers) ==
        SYNTRA_STATUS_NOT_IMPLEMENTED);
    assert(syntra_arm64_mmu_read_registers(NULL) ==
        SYNTRA_STATUS_INVALID_ARGUMENT);
    assert(syntra_arm64_mmu_prepare(ROOT_PA) ==
        SYNTRA_STATUS_NOT_IMPLEMENTED);
    assert(syntra_arm64_mmu_enable() == SYNTRA_STATUS_NOT_IMPLEMENTED);
}

int main(void)
{
    test_page_table_walk();
    test_walker_rejects_invalid_paths();
    test_register_api_is_host_safe();
    puts("ARM64 MMU tests passed");
    return 0;
}
