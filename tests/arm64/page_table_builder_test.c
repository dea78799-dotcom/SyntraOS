#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "page_table_builder.h"

#define TABLES 160U
#define BASE_PA 0x1000ULL
static uint64_t storage[TABLES][512];
static unsigned int used;
static unsigned int released;
static unsigned int release_attempts;
static unsigned int allocation_limit = TABLES;
static int fail_release;

typedef struct
{
    uint64_t fail_physical_address;
} resolve_failure_context_t;

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

static uint64_t *resolve_fail_pa_cb(uint64_t pa, void *ctx)
{
    const resolve_failure_context_t *failure =
        (const resolve_failure_context_t *)ctx;
    if (failure != NULL && pa == failure->fail_physical_address)
        return NULL;
    return resolve_cb(pa, NULL);
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

static void test_map_preallocates_before_publishing_any_leaf(void)
{
    syntra_arm64_page_table_t pt;
    syntra_arm64_mmu_translation_t translation;

    reset();
    released = 0U;
    release_attempts = 0U;
    assert(syntra_arm64_page_table_init_ex(
        &pt, alloc_cb, release_cb, resolve_cb, NULL) == SYNTRA_STATUS_OK);

    /* Seed the first 2-MiB region so its first leaf already has a table. */
    assert(syntra_arm64_page_table_map(
        &pt, 0x40000000ULL, 0x80000000ULL, 0x1000ULL,
        SYNTRA_ARM64_MMU_MEMORY_NORMAL,
        SYNTRA_ARM64_MMU_ACCESS_READ_WRITE, 0) == SYNTRA_STATUS_OK);
    assert(used == 3U);

    /*
     * First requested page fits in the existing L3 table. The second page
     * crosses a 2-MiB boundary and needs another L3 table; force that
     * allocation to fail. Phase one must fail before publishing page one.
     */
    allocation_limit = used;
    assert(syntra_arm64_page_table_map(
        &pt, 0x401ff000ULL, 0x801ff000ULL, 0x2000ULL,
        SYNTRA_ARM64_MMU_MEMORY_NORMAL,
        SYNTRA_ARM64_MMU_ACCESS_READ_WRITE, 0) ==
        SYNTRA_STATUS_OUT_OF_MEMORY);

    assert(syntra_arm64_mmu_walk(pt.root_physical_address, 0x401ff000ULL,
        (syntra_arm64_mmu_table_resolver_t)resolve_cb, NULL,
        &translation) == SYNTRA_STATUS_ERROR);
    assert(syntra_arm64_mmu_walk(pt.root_physical_address, 0x40200000ULL,
        (syntra_arm64_mmu_table_resolver_t)resolve_cb, NULL,
        &translation) == SYNTRA_STATUS_ERROR);

    allocation_limit = TABLES;
    assert(syntra_arm64_page_table_map(
        &pt, 0x401ff000ULL, 0x801ff000ULL, 0x2000ULL,
        SYNTRA_ARM64_MMU_MEMORY_NORMAL,
        SYNTRA_ARM64_MMU_ACCESS_READ_WRITE, 0) == SYNTRA_STATUS_OK);
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

static void test_user_map_preallocates_before_publishing_any_leaf(void)
{
    syntra_arm64_page_table_t pt;
    syntra_arm64_mmu_translation_t translation;

    reset();
    released = 0U;
    release_attempts = 0U;
    assert(syntra_arm64_page_table_init_ex(
        &pt, alloc_cb, release_cb, resolve_cb, NULL) == SYNTRA_STATUS_OK);

    /* Seed an L3 table in the first 2-MiB region of this L1 slot. */
    assert(syntra_arm64_page_table_map_user(
        &pt, 0x50000000ULL, 0x80000000ULL, 0x1000ULL,
        SYNTRA_ARM64_MMU_ACCESS_READ_WRITE, 0) == SYNTRA_STATUS_OK);

    /*
     * The first requested page reuses that L3 table; the next page crosses
     * into a new L2 slot and needs another L3 table. Fail that allocation.
     * Preparation must fail before publishing the first requested leaf.
     */
    allocation_limit = used;
    assert(syntra_arm64_page_table_map_user(
        &pt, 0x501ff000ULL, 0x801ff000ULL, 0x2000ULL,
        SYNTRA_ARM64_MMU_ACCESS_READ_WRITE, 0) ==
        SYNTRA_STATUS_OUT_OF_MEMORY);

    assert(syntra_arm64_mmu_walk(pt.root_physical_address, 0x501ff000ULL,
        (syntra_arm64_mmu_table_resolver_t)resolve_cb, NULL,
        &translation) == SYNTRA_STATUS_ERROR);
    assert(syntra_arm64_mmu_walk(pt.root_physical_address, 0x50200000ULL,
        (syntra_arm64_mmu_table_resolver_t)resolve_cb, NULL,
        &translation) == SYNTRA_STATUS_ERROR);

    allocation_limit = TABLES;
    assert(syntra_arm64_page_table_map_user(
        &pt, 0x501ff000ULL, 0x801ff000ULL, 0x2000ULL,
        SYNTRA_ARM64_MMU_ACCESS_READ_WRITE, 0) == SYNTRA_STATUS_OK);
    assert(syntra_arm64_page_table_verify_user(
        &pt, 0x501ff000ULL, 0x801ff000ULL, 0x2000ULL,
        SYNTRA_ARM64_MMU_ACCESS_READ_WRITE, 0) == SYNTRA_STATUS_OK);
}

static void test_user_map_reclaims_tables_when_child_resolve_fails(void)
{
    syntra_arm64_page_table_t pt;
    syntra_arm64_mmu_translation_t translation;
    resolve_failure_context_t failure;

    reset();
    released = 0U;
    release_attempts = 0U;
    failure.fail_physical_address = BASE_PA + 2ULL * 4096ULL;

    assert(syntra_arm64_page_table_init_ex(
        &pt, alloc_cb, release_cb, resolve_fail_pa_cb, &failure) ==
        SYNTRA_STATUS_OK);

    /*
     * Root and L2 resolve normally. L3 allocation succeeds, but resolving
     * its physical page fails. The L3 page must be released immediately and
     * the already-attached L2 page must be removed by transaction rollback.
     */
    assert(syntra_arm64_page_table_map_user(
        &pt, 0x54000000ULL, 0x84000000ULL, 0x1000ULL,
        SYNTRA_ARM64_MMU_ACCESS_READ_WRITE, 0) == SYNTRA_STATUS_ERROR);
    assert(used == 3U);
    assert(release_attempts == 2U);
    assert(released == 2U);
    assert(storage[0][(0x54000000ULL >> 30U) & 0x1ffU] == 0ULL);
    assert(syntra_arm64_mmu_walk(pt.root_physical_address, 0x54000000ULL,
        (syntra_arm64_mmu_table_resolver_t)resolve_cb, NULL,
        &translation) == SYNTRA_STATUS_ERROR);

    /* Once the resolver recovers, the same VA can be mapped successfully. */
    failure.fail_physical_address = 0ULL;
    assert(syntra_arm64_page_table_map_user(
        &pt, 0x54000000ULL, 0x84000000ULL, 0x1000ULL,
        SYNTRA_ARM64_MMU_ACCESS_READ_WRITE, 0) == SYNTRA_STATUS_OK);
    assert(syntra_arm64_page_table_verify_user(
        &pt, 0x54000000ULL, 0x84000000ULL, 0x1000ULL,
        SYNTRA_ARM64_MMU_ACCESS_READ_WRITE, 0) == SYNTRA_STATUS_OK);
}

static void test_user_map_preserves_hierarchy_when_release_fails(void)
{
    syntra_arm64_page_table_t pt;
    syntra_arm64_mmu_translation_t translation;
    uint64_t *root;
    uint64_t next_l1_descriptor;

    reset();
    released = 0U;
    release_attempts = 0U;
    assert(syntra_arm64_page_table_init_ex(
        &pt, alloc_cb, release_cb, resolve_cb, NULL) == SYNTRA_STATUS_OK);

    /* Seed the final L1 slot so the first requested page already has tables. */
    assert(syntra_arm64_page_table_map_user(
        &pt, 0x7fffe000ULL, 0x8fffe000ULL, 0x1000ULL,
        SYNTRA_ARM64_MMU_ACCESS_READ_WRITE, 0) == SYNTRA_STATUS_OK);

    /*
     * The range crosses an L1 boundary. Permit allocation of the next L2
     * table, fail its L3 allocation, and then fail release of that L2 page.
     * Rollback must restore the root descriptor so the owned page stays
     * reachable and can be reused by a later retry.
     */
    allocation_limit = used + 1U;
    fail_release = 1;
    assert(syntra_arm64_page_table_map_user(
        &pt, 0x7ffff000ULL, 0x8ffff000ULL, 0x2000ULL,
        SYNTRA_ARM64_MMU_ACCESS_READ_WRITE, 0) ==
        SYNTRA_STATUS_OUT_OF_MEMORY);

    root = resolve_cb(pt.root_physical_address, NULL);
    assert(root != NULL);
    next_l1_descriptor = root[(0x80000000ULL >> 30U) & 0x1ffU];
    assert(next_l1_descriptor != 0ULL);
    assert((next_l1_descriptor & 3ULL) == 3ULL);
    assert(resolve_cb(next_l1_descriptor & 0x0000fffffffff000ULL, NULL) != NULL);
    assert(release_attempts == 1U);
    assert(released == 0U);

    /* Neither requested leaf may have been published by the failed setup. */
    assert(syntra_arm64_mmu_walk(pt.root_physical_address, 0x7ffff000ULL,
        (syntra_arm64_mmu_table_resolver_t)resolve_cb, NULL,
        &translation) == SYNTRA_STATUS_ERROR);
    assert(syntra_arm64_mmu_walk(pt.root_physical_address, 0x80000000ULL,
        (syntra_arm64_mmu_table_resolver_t)resolve_cb, NULL,
        &translation) == SYNTRA_STATUS_ERROR);

    /* Re-enable reclamation/allocation and prove the retained hierarchy retries. */
    fail_release = 0;
    allocation_limit = TABLES;
    assert(syntra_arm64_page_table_map_user(
        &pt, 0x7ffff000ULL, 0x8ffff000ULL, 0x2000ULL,
        SYNTRA_ARM64_MMU_ACCESS_READ_WRITE, 0) == SYNTRA_STATUS_OK);
    assert(syntra_arm64_page_table_verify_user(
        &pt, 0x7ffff000ULL, 0x8ffff000ULL, 0x2000ULL,
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

static void test_mapping_journal_capacity_exhaustion(void)
{
    syntra_arm64_page_table_t pt;
    syntra_arm64_mmu_translation_t translation;
    const uint64_t map_va = 0x40001000ULL;
    const uint64_t map_pa = 0x80001000ULL;
    const uint64_t map_size = 64ULL * SYNTRA_ARM64_MMU_BLOCK_SIZE;
    const unsigned int journal_capacity = 64U;

    reset();
    released = 0U;
    release_attempts = 0U;
    assert(syntra_arm64_page_table_init_ex(
        &pt, alloc_cb, release_cb, resolve_cb, NULL) == SYNTRA_STATUS_OK);

    /*
     * This page-only range needs one L2 table plus 65 L3 tables. The bounded journal
     * can retain only 64 new-table records, so preparation must fail before
     * publishing any leaf and reclaim every table it did attach.
     */
    assert(syntra_arm64_page_table_map_user(
        &pt, map_va, map_pa, map_size,
        SYNTRA_ARM64_MMU_ACCESS_READ_WRITE, 0) ==
        SYNTRA_STATUS_OUT_OF_MEMORY);

    assert(used == journal_capacity + 1U); /* root + 64 attached tables */
    assert(release_attempts == journal_capacity);
    assert(released == journal_capacity);
    assert(storage[0][(map_va >> 30U) & 0x1ffU] == 0ULL);
    assert(syntra_arm64_mmu_walk(pt.root_physical_address, map_va,
        (syntra_arm64_mmu_table_resolver_t)resolve_cb, NULL,
        &translation) == SYNTRA_STATUS_ERROR);
    assert(syntra_arm64_mmu_walk(
        pt.root_physical_address,
        map_va + map_size - SYNTRA_ARM64_MMU_PAGE_SIZE,
        (syntra_arm64_mmu_table_resolver_t)resolve_cb, NULL,
        &translation) == SYNTRA_STATUS_ERROR);

    /*
     * Retry a small prefix: the original oversized range inherently exceeds
     * journal capacity even when empty, but its first pages must remain usable.
     */
    const retry_size = 2ULL * SYNTRA_ARM64_MMU_PAGE_SIZE;
    assert(syntra_arm64_page_table_map_user(
        &pt, map_va, map_pa, retry_size,
        SYNTRA_ARM64_MMU_ACCESS_READ_WRITE, 0) == SYNTRA_STATUS_OK);
    assert(syntra_arm64_page_table_verify_user(
        &pt, map_va, map_pa, retry_size,
        SYNTRA_ARM64_MMU_ACCESS_READ_WRITE, 0) == SYNTRA_STATUS_OK);
    assert(syntra_arm64_mmu_walk(pt.root_physical_address, map_va,
        (syntra_arm64_mmu_table_resolver_t)resolve_cb, NULL,
        &translation) == SYNTRA_STATUS_OK);
    assert(translation.physical_address == map_pa);
    assert(syntra_arm64_mmu_walk(
        pt.root_physical_address,
        map_va + retry_size - SYNTRA_ARM64_MMU_PAGE_SIZE,
        (syntra_arm64_mmu_table_resolver_t)resolve_cb, NULL,
        &translation) == SYNTRA_STATUS_OK);
    assert(translation.physical_address ==
        map_pa + retry_size - SYNTRA_ARM64_MMU_PAGE_SIZE);
}

int main(void)
{
    test_init_releases_invalid_allocator_result();
    test_init_reclaims_unresolvable_root();
    test_build_and_walk_verify();
    test_map_rolls_back_on_allocation_failure();
    test_map_preallocates_before_publishing_any_leaf();
    test_mapping_journal_capacity_exhaustion();
    test_rollback_preserves_hierarchy_when_release_fails();
    test_user_map_rolls_back_on_allocation_failure();
    test_user_map_preallocates_before_publishing_any_leaf();
    test_user_map_reclaims_tables_when_child_resolve_fails();
    test_user_map_preserves_hierarchy_when_release_fails();
    test_user_page_mappings();
    test_block_mapping();
    test_block_page_conflict();
    test_partial_overlap_preflight();
    test_invalid_attributes_do_not_allocate_tables();
    test_device_mapping_and_invalid_inputs();
    puts("ARM64 page-table builder tests passed");
    return 0;
}
