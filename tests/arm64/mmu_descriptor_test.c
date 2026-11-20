#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "../../kernel/arch/arm64/mmu.h"

#define DESC_VALID       (1ULL << 0)
#define DESC_TYPE        (1ULL << 1)
#define DESC_ATTR_SHIFT  2U
#define DESC_AP_RO       (1ULL << 7)
#define DESC_SH_SHIFT    8U
#define DESC_AF          (1ULL << 10)
#define DESC_PXN         (1ULL << 53)
#define DESC_UXN         (1ULL << 54)

static void test_table_descriptor(void)
{
    uint64_t descriptor = 0U;

    assert(syntra_arm64_mmu_make_table_descriptor(
        0x0000000040001000ULL, &descriptor) == SYNTRA_STATUS_OK);
    assert((descriptor & (DESC_VALID | DESC_TYPE)) ==
           (DESC_VALID | DESC_TYPE));
    assert((descriptor & SYNTRA_ARM64_MMU_PA_MASK) ==
           0x0000000040001000ULL);

    assert(syntra_arm64_mmu_make_table_descriptor(
        0x40000001ULL, &descriptor) == SYNTRA_STATUS_INVALID_ARGUMENT);
    assert(syntra_arm64_mmu_make_table_descriptor(
        0x0001000000000000ULL, &descriptor) ==
        SYNTRA_STATUS_INVALID_ARGUMENT);
    assert(syntra_arm64_mmu_make_table_descriptor(
        0x40000000ULL, NULL) == SYNTRA_STATUS_INVALID_ARGUMENT);
}

static void test_normal_page_descriptors(void)
{
    uint64_t descriptor = 0U;

    assert(syntra_arm64_mmu_make_page_descriptor(
        0x40200000ULL,
        SYNTRA_ARM64_MMU_MEMORY_NORMAL,
        SYNTRA_ARM64_MMU_ACCESS_READ_WRITE,
        1,
        &descriptor) == SYNTRA_STATUS_OK);

    assert((descriptor & (DESC_VALID | DESC_TYPE)) ==
           (DESC_VALID | DESC_TYPE));
    assert((descriptor & SYNTRA_ARM64_MMU_PA_MASK) == 0x40200000ULL);
    assert(((descriptor >> DESC_ATTR_SHIFT) & 7ULL) == 0ULL);
    assert(((descriptor >> DESC_SH_SHIFT) & 3ULL) == 3ULL);
    assert((descriptor & DESC_AF) != 0ULL);
    assert((descriptor & DESC_PXN) == 0ULL);
    assert((descriptor & DESC_UXN) != 0ULL);
    assert((descriptor & DESC_AP_RO) == 0ULL);

    assert(syntra_arm64_mmu_make_page_descriptor(
        0x40201000ULL,
        SYNTRA_ARM64_MMU_MEMORY_NORMAL,
        SYNTRA_ARM64_MMU_ACCESS_READ_ONLY,
        0,
        &descriptor) == SYNTRA_STATUS_OK);
    assert((descriptor & DESC_AP_RO) != 0ULL);
    assert((descriptor & DESC_PXN) != 0ULL);
    assert((descriptor & DESC_UXN) != 0ULL);
}

static void test_device_page_descriptor(void)
{
    uint64_t descriptor = 0U;

    assert(syntra_arm64_mmu_make_page_descriptor(
        0x08000000ULL,
        SYNTRA_ARM64_MMU_MEMORY_DEVICE,
        SYNTRA_ARM64_MMU_ACCESS_READ_WRITE,
        0,
        &descriptor) == SYNTRA_STATUS_OK);
    assert(((descriptor >> DESC_ATTR_SHIFT) & 7ULL) == 1ULL);
    assert(((descriptor >> DESC_SH_SHIFT) & 3ULL) == 2ULL);
    assert((descriptor & (DESC_PXN | DESC_UXN)) ==
           (DESC_PXN | DESC_UXN));

    assert(syntra_arm64_mmu_make_page_descriptor(
        0x08000000ULL,
        SYNTRA_ARM64_MMU_MEMORY_DEVICE,
        SYNTRA_ARM64_MMU_ACCESS_READ_WRITE,
        1,
        &descriptor) == SYNTRA_STATUS_INVALID_ARGUMENT);
}

static void test_invalid_page_arguments(void)
{
    uint64_t descriptor = 0U;

    assert(syntra_arm64_mmu_make_page_descriptor(
        0x40200001ULL, SYNTRA_ARM64_MMU_MEMORY_NORMAL,
        SYNTRA_ARM64_MMU_ACCESS_READ_WRITE, 0,
        &descriptor) == SYNTRA_STATUS_INVALID_ARGUMENT);
    assert(syntra_arm64_mmu_make_page_descriptor(
        0x0001000000000000ULL, SYNTRA_ARM64_MMU_MEMORY_NORMAL,
        SYNTRA_ARM64_MMU_ACCESS_READ_WRITE, 0,
        &descriptor) == SYNTRA_STATUS_INVALID_ARGUMENT);
    assert(syntra_arm64_mmu_make_page_descriptor(
        0x40200000ULL, (syntra_arm64_mmu_memory_type_t)9,
        SYNTRA_ARM64_MMU_ACCESS_READ_WRITE, 0,
        &descriptor) == SYNTRA_STATUS_INVALID_ARGUMENT);
    assert(syntra_arm64_mmu_make_page_descriptor(
        0x40200000ULL, SYNTRA_ARM64_MMU_MEMORY_NORMAL,
        (syntra_arm64_mmu_access_t)9, 0,
        &descriptor) == SYNTRA_STATUS_INVALID_ARGUMENT);
    assert(syntra_arm64_mmu_make_page_descriptor(
        0x40200000ULL, SYNTRA_ARM64_MMU_MEMORY_NORMAL,
        SYNTRA_ARM64_MMU_ACCESS_READ_WRITE, 2,
        &descriptor) == SYNTRA_STATUS_INVALID_ARGUMENT);
    assert(syntra_arm64_mmu_make_page_descriptor(
        0x40200000ULL, SYNTRA_ARM64_MMU_MEMORY_NORMAL,
        SYNTRA_ARM64_MMU_ACCESS_READ_WRITE, 0,
        NULL) == SYNTRA_STATUS_INVALID_ARGUMENT);
}

static void test_translation_indices(void)
{
    uint16_t indices[4] = {0U, 0U, 0U, 0U};

    assert(syntra_arm64_mmu_get_table_indices(
        0x0000000040201234ULL, indices) == SYNTRA_STATUS_OK);
    assert(indices[0] == 1U);
    assert(indices[1] == 1U);
    assert(indices[2] == 1U);
    assert(indices[3] == 0x234U);

    assert(syntra_arm64_mmu_get_table_indices(
        1ULL << SYNTRA_ARM64_MMU_VA_BITS, indices) ==
        SYNTRA_STATUS_INVALID_ARGUMENT);
    assert(syntra_arm64_mmu_get_table_indices(
        0x40000000ULL, NULL) == SYNTRA_STATUS_INVALID_ARGUMENT);
}


static void test_mapping_range_validation(void)
{
    const uint64_t page = SYNTRA_ARM64_MMU_PAGE_SIZE;

    assert(syntra_arm64_mmu_validate_mapping_range(
        0x40000000ULL, 0x40000000ULL, page) == SYNTRA_STATUS_OK);
    assert(syntra_arm64_mmu_validate_mapping_range(
        0x40000000ULL, 0x40000000ULL, 2ULL * page) == SYNTRA_STATUS_OK);

    /* The last page inside the supported 39-bit VA space is valid. */
    assert(syntra_arm64_mmu_validate_mapping_range(
        (1ULL << SYNTRA_ARM64_MMU_VA_BITS) - page,
        0x0000FFFFFFFFF000ULL, page) == SYNTRA_STATUS_OK);

    assert(syntra_arm64_mmu_validate_mapping_range(
        0x40000000ULL, 0x40000000ULL, 0ULL) ==
        SYNTRA_STATUS_INVALID_ARGUMENT);
    assert(syntra_arm64_mmu_validate_mapping_range(
        0x40000001ULL, 0x40000000ULL, page) ==
        SYNTRA_STATUS_INVALID_ARGUMENT);
    assert(syntra_arm64_mmu_validate_mapping_range(
        0x40000000ULL, 0x40000001ULL, page) ==
        SYNTRA_STATUS_INVALID_ARGUMENT);
    assert(syntra_arm64_mmu_validate_mapping_range(
        0x40000000ULL, 0x40000000ULL, page + 1ULL) ==
        SYNTRA_STATUS_INVALID_ARGUMENT);

    /* Reject ranges crossing either address-space boundary. */
    assert(syntra_arm64_mmu_validate_mapping_range(
        (1ULL << SYNTRA_ARM64_MMU_VA_BITS) - page,
        0x40000000ULL, 2ULL * page) ==
        SYNTRA_STATUS_INVALID_ARGUMENT);
    assert(syntra_arm64_mmu_validate_mapping_range(
        0x40000000ULL, 0x0000FFFFFFFFF000ULL, 2ULL * page) ==
        SYNTRA_STATUS_INVALID_ARGUMENT);
}

int main(void)
{
    test_table_descriptor();
    test_normal_page_descriptors();
    test_device_page_descriptor();
    test_invalid_page_arguments();
    test_translation_indices();
    test_mapping_range_validation();
    puts("ARM64 MMU descriptor tests passed");
    return 0;
}
