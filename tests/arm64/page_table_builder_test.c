#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "page_table_builder.h"

#define TABLES 8U
#define BASE_PA 0x1000ULL
static uint64_t storage[TABLES][512];
static unsigned int used;
static unsigned int released;
static unsigned int release_attempts;
static unsigned int allocation_limit = TABLES;
static int fail_release;

static syntra_status_t alloc_cb(void *ctx, uint64_t *pa)
{
    (void)ctx;
    if (used >= allocation_limit || pa == NULL) return SYNTRA_STATUS_OUT_OF_MEMORY;
    *pa = BASE_PA + (uint64_t)used * 4096ULL;
    ++used;
    return SYNTRA_STATUS_OK;
}

static uint64_t *resolve_cb(uint64_t pa, void *ctx)
{
    (void)ctx;
    if (pa < BASE_PA || pa >= BASE_PA + (uint64_t)TABLES * 4096ULL ||
        ((pa - BASE_PA) & 4095ULL) != 0ULL)
        return NULL;
    return storage[(pa - BASE_PA) / 4096ULL];
}

static void reset(void)
{
    used = 0U;
    allocation_limit = TABLES;
    fail_release = 0;
    for (unsigned int i = 0U; i < TABLES; ++i)
        for (unsigned int j = 0U; j < 512U; ++j)
            storage[i][j] = 0ULL;
}


static syntra_status_t release_cb(void *ctx, uint64_t pa)
{
    (void)ctx;
    ++release_attempts;
    if (pa < BASE_PA || pa >= BASE_PA + (uint64_t)TABLES * 4096ULL ||
        ((pa - BASE_PA) & 4095ULL) != 0ULL)
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    if (fail_release) return SYNTRA_STATUS_ERROR;
    ++released;
    return SYNTRA_STATUS_OK;
}

static syntra_status_t alloc_invalid_pa_cb(void *ctx, uint64_t *pa)
{
    (void)ctx;
    if (pa == NULL) return SYNTRA_STATUS_INVALID_ARGUMENT;
    *pa = BASE_PA + 1ULL; /* allocator contract violation: unaligned PA */
    return SYNTRA_STATUS_OK;
}

static uint64_t *resolve_fail_cb(uint64_t pa, void *ctx)
{
    (void)pa;
    (void)ctx;
    return NULL;
}

static void test_init_releases_invalid_allocator_result(void)
{
    syntra_arm64_page_table_t pt;
    reset();
    released = 0U;
    release_attempts = 0U;
    assert(syntra_arm64_page_table_init_ex(
        &pt, alloc_invalid_pa_cb, release_cb, resolve_cb, NULL) ==
        SYNTRA_STATUS_ERROR);
    assert(release_attempts == 1U);
    assert(released == 0U);
    assert(pt.root_physical_address == 0ULL);
}

static void test_init_reclaims_unresolvable_root(void)
{
    syntra_arm64_page_table_t pt;
    reset();
    released = 0U;
    assert(syntra_arm64_page_table_init_ex(
        &pt, alloc_cb, release_cb, resolve_fail_cb, NULL) ==
        SYNTRA_STATUS_ERROR);
    assert(used == 1U);
    assert(released == 1U);
    assert(pt.root_physical_address == 0ULL);
}

static void test_build_and_walk_verify(void)
{
    syntra_arm64_page_table_t pt;
    reset();
    assert(syntra_arm64_page_table_init_ex(
        &pt, alloc_cb, release_cb, resolve_cb, NULL) == SYNTRA_STATUS_OK);
    assert(used == 1U);

    assert(syntra_arm64_page_table_map(
        &pt, 0x40000000ULL, 0x80000000ULL, 0x3000ULL,
        SYNTRA_ARM64_MMU_MEMORY_NORMAL,
        SYNTRA_ARM64_MMU_ACCESS_READ_WRITE, 1) == SYNTRA_STATUS_OK);

    assert(syntra_arm64_page_table_verify(
        &pt, 0x40000000ULL, 0x80000000ULL, 0x3000ULL,
        SYNTRA_ARM64_MMU_MEMORY_NORMAL,
        SYNTRA_ARM64_MMU_ACCESS_READ_WRITE, 1) == SYNTRA_STATUS_OK);

    /* Reject remapping a populated page. */
    assert(syntra_arm64_page_table_map(
        &pt, 0x40001000ULL, 0x90000000ULL, 0x1000ULL,
        SYNTRA_ARM64_MMU_MEMORY_DEVICE,
        SYNTRA_ARM64_MMU_ACCESS_READ_WRITE, 0) == SYNTRA_STATUS_ERROR);

    /* A permission mismatch must fail the verifier. */
    assert(syntra_arm64_page_table_verify(
        &pt, 0x40000000ULL, 0x80000000ULL, 0x1000ULL,
        SYNTRA_ARM64_MMU_MEMORY_NORMAL,
        SYNTRA_ARM64_MMU_ACCESS_READ_ONLY, 1) == SYNTRA_STATUS_ERROR);
}


static void test_map_rolls_back_on_allocation_failure(void)
{
    syntra_arm64_page_table_t pt;
    syntra_arm64_mmu_translation_t translation;
    reset();
    released = 0U;
    release_attempts = 0U;
    assert(syntra_arm64_page_table_init_ex(
        &pt, alloc_cb, release_cb, resolve_cb, NULL) == SYNTRA_STATUS_OK);

    /* Root succeeds; the next allocation creates L2; L3 allocation fails. */
    allocation_limit = 2U;
    assert(syntra_arm64_page_table_map(
        &pt, 0x51000000ULL, 0x81000000ULL, 0x1000ULL,
        SYNTRA_ARM64_MMU_MEMORY_NORMAL,
        SYNTRA_ARM64_MMU_ACCESS_READ_WRITE, 0) ==
        SYNTRA_STATUS_OUT_OF_MEMORY);
    assert(used == 2U);
    assert(released == 1U);
    assert(release_attempts == 1U);
    assert(storage[0][(0x51000000ULL >> 30U) & 0x1ffU] == 0ULL);
    assert(syntra_arm64_mmu_walk(pt.root_physical_address, 0x51000000ULL,
        (syntra_arm64_mmu_table_resolver_t)resolve_cb, NULL,
        &translation) == SYNTRA_STATUS_ERROR);

    /* The same VA can be mapped after the failed transaction was rolled back. */
    allocation_limit = TABLES;
    assert(syntra_arm64_page_table_map(
        &pt, 0x51000000ULL, 0x81000000ULL, 0x1000ULL,
        SYNTRA_ARM64_MMU_MEMORY_NORMAL,
        SYNTRA_ARM64_MMU_ACCESS_READ_WRITE, 0) == SYNTRA_STATUS_OK);
}

static void test_rollback_preserves_hierarchy_when_release_fails(void)
{
    syntra_arm64_page_table_t pt;
    syntra_arm64_mmu_translation_t translation;
    uint64_t l1_descriptor;

    reset();
    released = 0U;
    release_attempts = 0U;
    assert(syntra_arm64_page_table_init_ex(
        &pt, alloc_cb, release_cb, resolve_cb, NULL) == SYNTRA_STATUS_OK);

    /* Force L3 allocation to fail, then make reclamation of L2 fail too. */
    allocation_limit = 2U;
    fail_release = 1;
    assert(syntra_arm64_page_table_map(
        &pt, 0x53000000ULL, 0x83000000ULL, 0x1000ULL,
        SYNTRA_ARM64_MMU_MEMORY_NORMAL,
        SYNTRA_ARM64_MMU_ACCESS_READ_WRITE, 0) ==
        SYNTRA_STATUS_OUT_OF_MEMORY);

    l1_descriptor = storage[0][(0x53000000ULL >> 30U) & 0x1ffU];
    assert(l1_descriptor != 0ULL);
    assert(release_attempts == 1U);
    assert(released == 0U);
    assert(resolve_cb(l1_descriptor & 0x0000fffffffff000ULL, NULL) != NULL);

    /* Once ownership release works, the retained empty hierarchy is reusable. */
    fail_release = 0;
    allocation_limit = TABLES;
    assert(syntra_arm64_page_table_map(
        &pt, 0x53000000ULL, 0x83000000ULL, 0x1000ULL,
        SYNTRA_ARM64_MMU_MEMORY_NORMAL,
        SYNTRA_ARM64_MMU_ACCESS_READ_WRITE, 0) == SYNTRA_STATUS_OK);
    assert(syntra_arm64_mmu_walk(pt.root_physical_address, 0x53000000ULL,
        (syntra_arm64_mmu_table_resolver_t)resolve_cb, NULL,
        &translation) == SYNTRA_STATUS_OK);
}

static void test_user_map_rolls_back_on_allocation_failure(void)
{
    syntra_arm64_page_table_t pt;
    syntra_arm64_mmu_translation_t translation;
    reset();
    released = 0U;
    release_attempts = 0U;
    assert(syntra_arm64_page_table_init_ex(
        &pt, alloc_cb, release_cb, resolve_cb, NULL) == SYNTRA_STATUS_OK);

    /* Root succeeds, L2 succeeds, and the required L3 allocation fails. */
    allocation_limit = 2U;
    assert(syntra_arm64_page_table_map_user(
        &pt, 0x52000000ULL, 0x82000000ULL, 0x1000ULL,
        SYNTRA_ARM64_MMU_ACCESS_READ_WRITE, 0) ==
        SYNTRA_STATUS_OUT_OF_MEMORY);
    assert(used == 2U);
    assert(released == 1U);
    assert(release_attempts == 1U);
    assert(storage[0][(0x52000000ULL >> 30U) & 0x1ffU] == 0ULL);
    assert(syntra_arm64_mmu_walk(pt.root_physical_address, 0x52000000ULL,
        (syntra_arm64_mmu_table_resolver_t)resolve_cb, NULL,
        &translation) == SYNTRA_STATUS_ERROR);

    /* The failed user-map setup leaves the VA available for a retry. */
    allocation_limit = TABLES;
    assert(syntra_arm64_page_table_map_user(
        &pt, 0x52000000ULL, 0x82000000ULL, 0x1000ULL,
        SYNTRA_ARM64_MMU_ACCESS_READ_WRITE, 0) == SYNTRA_STATUS_OK);
    assert(syntra_arm64_page_table_verify_user(
        &pt, 0x52000000ULL, 0x82000000ULL, 0x1000ULL,
        SYNTRA_ARM64_MMU_ACCESS_READ_WRITE, 0) == SYNTRA_STATUS_OK);
}

static void test_user_page_mappings(void)
{
    syntra_arm64_page_table_t pt;
    syntra_arm64_mmu_translation_t translation;
    reset();
    assert(syntra_arm64_page_table_init_ex(
        &pt, alloc_cb, release_cb, resolve_cb, NULL) == SYNTRA_STATUS_OK);

    /* User code: EL0 read/execute, never EL1 execute, read-only. */
    assert(syntra_arm64_page_table_map_user(
        &pt, 0x50000000ULL, 0x80000000ULL, 0x1000ULL,
        SYNTRA_ARM64_MMU_ACCESS_READ_ONLY, 1) == SYNTRA_STATUS_OK);
    assert(syntra_arm64_page_table_verify_user(
        &pt, 0x50000000ULL, 0x80000000ULL, 0x1000ULL,
        SYNTRA_ARM64_MMU_ACCESS_READ_ONLY, 1) == SYNTRA_STATUS_OK);
    assert(syntra_arm64_mmu_walk(pt.root_physical_address, 0x50000000ULL,
        (syntra_arm64_mmu_table_resolver_t)resolve_cb, NULL,
        &translation) == SYNTRA_STATUS_OK);
    assert(translation.user_read == 1U && translation.user_write == 0U);
    assert(translation.user_execute == 1U);
    assert(translation.privileged_execute == 0U);

    /* User stack: EL0 read/write, execute-never. */
    assert(syntra_arm64_page_table_map_user(
        &pt, 0x50001000ULL, 0x80001000ULL, 0x2000ULL,
        SYNTRA_ARM64_MMU_ACCESS_READ_WRITE, 0) == SYNTRA_STATUS_OK);
    assert(syntra_arm64_page_table_verify_user(
        &pt, 0x50001000ULL, 0x80001000ULL, 0x2000ULL,
        SYNTRA_ARM64_MMU_ACCESS_READ_WRITE, 0) == SYNTRA_STATUS_OK);
    assert(syntra_arm64_mmu_walk(pt.root_physical_address, 0x50001000ULL,
        (syntra_arm64_mmu_table_resolver_t)resolve_cb, NULL,
        &translation) == SYNTRA_STATUS_OK);
    assert(translation.user_read == 1U && translation.user_write == 1U);
    assert(translation.user_execute == 0U);
    assert(translation.privileged_execute == 0U);

    /* Kernel-only verifier must reject a user-accessible leaf. */
    assert(syntra_arm64_page_table_verify(
        &pt, 0x50000000ULL, 0x80000000ULL, 0x1000ULL,
        SYNTRA_ARM64_MMU_MEMORY_NORMAL,
        SYNTRA_ARM64_MMU_ACCESS_READ_ONLY, 1) == SYNTRA_STATUS_ERROR);
    assert(syntra_arm64_page_table_map_user(
        &pt, 0x50000000ULL, 0x90000000ULL, 0x1000ULL,
        SYNTRA_ARM64_MMU_ACCESS_READ_ONLY, 1) == SYNTRA_STATUS_ERROR);
}

static void test_invalid_attributes_do_not_allocate_tables(void)
{
    syntra_arm64_page_table_t pt;
    reset();
    assert(syntra_arm64_page_table_init_ex(
        &pt, alloc_cb, release_cb, resolve_cb, NULL) == SYNTRA_STATUS_OK);
    assert(used == 1U);

    assert(syntra_arm64_page_table_map(
        &pt, 0x40000000ULL, 0x80000000ULL, 0x1000ULL,
        SYNTRA_ARM64_MMU_MEMORY_UNKNOWN,
        SYNTRA_ARM64_MMU_ACCESS_READ_WRITE, 0) ==
        SYNTRA_STATUS_INVALID_ARGUMENT);
    assert(syntra_arm64_page_table_map(
        &pt, 0x40000000ULL, 0x80000000ULL, 0x1000ULL,
        SYNTRA_ARM64_MMU_MEMORY_NORMAL,
        (syntra_arm64_mmu_access_t)99, 0) ==
        SYNTRA_STATUS_INVALID_ARGUMENT);
    assert(syntra_arm64_page_table_map(
        &pt, 0x40000000ULL, 0x80000000ULL, 0x1000ULL,
        SYNTRA_ARM64_MMU_MEMORY_NORMAL,
        SYNTRA_ARM64_MMU_ACCESS_READ_WRITE, 2) ==
        SYNTRA_STATUS_INVALID_ARGUMENT);
    assert(syntra_arm64_page_table_map(
        &pt, 0x40000000ULL, 0x80000000ULL, 0x1000ULL,
        SYNTRA_ARM64_MMU_MEMORY_DEVICE,
        SYNTRA_ARM64_MMU_ACCESS_READ_WRITE, 1) ==
        SYNTRA_STATUS_INVALID_ARGUMENT);

    /* Root remains the only allocation: invalid input has no side effects. */
    assert(used == 1U);
    assert(syntra_arm64_page_table_verify(
        &pt, 0x40000000ULL, 0x80000000ULL, 0x1000ULL,
        SYNTRA_ARM64_MMU_MEMORY_NORMAL,
        SYNTRA_ARM64_MMU_ACCESS_READ_WRITE, 0) == SYNTRA_STATUS_ERROR);

    /* Verification APIs reject invalid attributes instead of interpreting them. */
    assert(syntra_arm64_page_table_verify(
        &pt, 0x40000000ULL, 0x80000000ULL, 0x1000ULL,
        SYNTRA_ARM64_MMU_MEMORY_UNKNOWN,
        SYNTRA_ARM64_MMU_ACCESS_READ_WRITE, 0) ==
        SYNTRA_STATUS_INVALID_ARGUMENT);
    assert(syntra_arm64_page_table_verify(
        &pt, 0x40000000ULL, 0x80000000ULL, 0x1000ULL,
        SYNTRA_ARM64_MMU_MEMORY_NORMAL,
        (syntra_arm64_mmu_access_t)99, 0) ==
        SYNTRA_STATUS_INVALID_ARGUMENT);
    assert(syntra_arm64_page_table_verify(
        &pt, 0x40000000ULL, 0x80000000ULL, 0x1000ULL,
        SYNTRA_ARM64_MMU_MEMORY_NORMAL,
        SYNTRA_ARM64_MMU_ACCESS_READ_WRITE, 2) ==
        SYNTRA_STATUS_INVALID_ARGUMENT);
    assert(syntra_arm64_page_table_verify(
        &pt, 0x40000000ULL, 0x80000000ULL, 0x1000ULL,
        SYNTRA_ARM64_MMU_MEMORY_DEVICE,
        SYNTRA_ARM64_MMU_ACCESS_READ_WRITE, 1) ==
        SYNTRA_STATUS_INVALID_ARGUMENT);
}

static void test_device_mapping_and_invalid_inputs(void)
{
    syntra_arm64_page_table_t pt;
    reset();
    assert(syntra_arm64_page_table_init_ex(
        &pt, alloc_cb, release_cb, resolve_cb, NULL) == SYNTRA_STATUS_OK);

    assert(syntra_arm64_page_table_map(
        &pt, 0x08000000ULL, 0x08000000ULL, 0x2000ULL,
        SYNTRA_ARM64_MMU_MEMORY_DEVICE,
        SYNTRA_ARM64_MMU_ACCESS_READ_WRITE, 0) == SYNTRA_STATUS_OK);
    assert(syntra_arm64_page_table_verify(
        &pt, 0x08000000ULL, 0x08000000ULL, 0x2000ULL,
        SYNTRA_ARM64_MMU_MEMORY_DEVICE,
        SYNTRA_ARM64_MMU_ACCESS_READ_WRITE, 0) == SYNTRA_STATUS_OK);

    assert(syntra_arm64_page_table_map(
        &pt, 0x08000001ULL, 0x08000000ULL, 0x1000ULL,
        SYNTRA_ARM64_MMU_MEMORY_DEVICE,
        SYNTRA_ARM64_MMU_ACCESS_READ_WRITE, 0) ==
        SYNTRA_STATUS_INVALID_ARGUMENT);
    assert(syntra_arm64_page_table_map(
        &pt, 0x1000ULL, 0x2000ULL, 0x1000ULL,
        SYNTRA_ARM64_MMU_MEMORY_DEVICE,
        SYNTRA_ARM64_MMU_ACCESS_READ_WRITE, 1) ==
        SYNTRA_STATUS_INVALID_ARGUMENT);
    assert(syntra_arm64_page_table_init_ex(
        NULL, alloc_cb, release_cb, resolve_cb, NULL) == SYNTRA_STATUS_INVALID_ARGUMENT);
}


static void test_block_mapping(void)
{
    syntra_arm64_page_table_t pt;
    syntra_arm64_mmu_translation_t tr;
    reset();
    assert(syntra_arm64_page_table_init_ex(&pt, alloc_cb, release_cb, resolve_cb, NULL) == SYNTRA_STATUS_OK);
    assert(syntra_arm64_page_table_map(&pt, 0x40000000ULL, 0x80000000ULL,
        SYNTRA_ARM64_MMU_BLOCK_SIZE, SYNTRA_ARM64_MMU_MEMORY_NORMAL,
        SYNTRA_ARM64_MMU_ACCESS_READ_WRITE, 0) == SYNTRA_STATUS_OK);
    assert(used == 2U); /* root + L2; no L3 table needed */
    assert(syntra_arm64_mmu_walk(pt.root_physical_address, 0x40001234ULL,
        (syntra_arm64_mmu_table_resolver_t)resolve_cb, NULL, &tr) == SYNTRA_STATUS_OK);
    assert(tr.level == 2U && tr.page_size == SYNTRA_ARM64_MMU_BLOCK_SIZE);
    assert(tr.physical_address == 0x80001234ULL);
    assert(syntra_arm64_page_table_verify(&pt, 0x40000000ULL, 0x80000000ULL,
        SYNTRA_ARM64_MMU_BLOCK_SIZE, SYNTRA_ARM64_MMU_MEMORY_NORMAL,
        SYNTRA_ARM64_MMU_ACCESS_READ_WRITE, 0) == SYNTRA_STATUS_OK);
    assert(syntra_arm64_page_table_map(&pt, 0x40001000ULL, 0x90001000ULL,
        0x1000ULL, SYNTRA_ARM64_MMU_MEMORY_NORMAL,
        SYNTRA_ARM64_MMU_ACCESS_READ_WRITE, 0) == SYNTRA_STATUS_ERROR);
}

static void test_block_page_conflict(void)
{
    syntra_arm64_page_table_t pt;
    reset();
    assert(syntra_arm64_page_table_init_ex(&pt, alloc_cb, release_cb, resolve_cb, NULL) == SYNTRA_STATUS_OK);
    assert(syntra_arm64_page_table_map(&pt, 0x60001000ULL, 0x70001000ULL,
        0x1000ULL, SYNTRA_ARM64_MMU_MEMORY_NORMAL,
        SYNTRA_ARM64_MMU_ACCESS_READ_WRITE, 0) == SYNTRA_STATUS_OK);
    assert(syntra_arm64_page_table_map(&pt, 0x60000000ULL, 0x80000000ULL,
        SYNTRA_ARM64_MMU_BLOCK_SIZE, SYNTRA_ARM64_MMU_MEMORY_NORMAL,
        SYNTRA_ARM64_MMU_ACCESS_READ_WRITE, 0) == SYNTRA_STATUS_ERROR);
}

static void test_partial_overlap_preflight(void)
{
    syntra_arm64_page_table_t pt;
    reset();
    assert(syntra_arm64_page_table_init_ex(&pt, alloc_cb, release_cb, resolve_cb, NULL) == SYNTRA_STATUS_OK);
    assert(syntra_arm64_page_table_map(&pt, 0x50002000ULL, 0x60002000ULL, 0x1000ULL,
        SYNTRA_ARM64_MMU_MEMORY_NORMAL, SYNTRA_ARM64_MMU_ACCESS_READ_WRITE, 0) == SYNTRA_STATUS_OK);
    assert(syntra_arm64_page_table_map(&pt, 0x50001000ULL, 0x70001000ULL, 0x2000ULL,
        SYNTRA_ARM64_MMU_MEMORY_NORMAL, SYNTRA_ARM64_MMU_ACCESS_READ_WRITE, 0) == SYNTRA_STATUS_ERROR);
    assert(syntra_arm64_page_table_verify(&pt, 0x50001000ULL, 0x70001000ULL, 0x1000ULL,
        SYNTRA_ARM64_MMU_MEMORY_NORMAL, SYNTRA_ARM64_MMU_ACCESS_READ_WRITE, 0) == SYNTRA_STATUS_ERROR);
}

int main(void)
{
    test_init_releases_invalid_allocator_result();
    test_init_reclaims_unresolvable_root();
    test_build_and_walk_verify();
    test_map_rolls_back_on_allocation_failure();
    test_rollback_preserves_hierarchy_when_release_fails();
    test_user_map_rolls_back_on_allocation_failure();
    test_user_page_mappings();
    test_block_mapping();
    test_block_page_conflict();
    test_partial_overlap_preflight();
    test_invalid_attributes_do_not_allocate_tables();
    test_device_mapping_and_invalid_inputs();
    puts("ARM64 page-table builder tests passed");
    return 0;
}
