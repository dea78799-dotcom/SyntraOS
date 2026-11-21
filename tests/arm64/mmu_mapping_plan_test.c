#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "../../kernel/arch/arm64/mmu_mapping_plan.h"

#ifdef NDEBUG
#undef NDEBUG
#endif

static syntra_arm64_boot_memory_metadata_t fixture(void)
{
    syntra_arm64_boot_memory_metadata_t metadata = {0};
    metadata.kernel_start = 0x40200000U;
    metadata.kernel_end = 0x40208000U;
    metadata.text_start = 0x40200000U;
    metadata.text_end = 0x40201000U;
    metadata.rodata_start = 0x40201000U;
    metadata.rodata_end = 0x40202000U;
    metadata.data_start = 0x40202000U;
    metadata.data_end = 0x40203000U;
    metadata.bss_start = 0x40203000U;
    metadata.bss_end = 0x40204000U;
    metadata.stack_bottom = 0x40204000U;
    metadata.stack_top = 0x40208000U;
    metadata.dtb_start = 0x40000000U;
    metadata.dtb_size = 0x1800U;
    return metadata;
}

static syntra_arm64_gic_config_t fixture_gic(void)
{
    syntra_arm64_gic_config_t gic = {0};
    gic.version = SYNTRA_ARM64_GIC_VERSION_3;
    gic.distributor_base = 0x08000000U;
    gic.distributor_size = 0x10000U;
    gic.redistributor_base = 0x080A0000U;
    gic.redistributor_size = 0xF60000U;
    return gic;
}

static syntra_memory_region_t fixture_regions[] = {
    {0x40002000U, 0x001FE000U, SYNTRA_MEMORY_REGION_RAM},
    {0x40200000U, 0x8000U, SYNTRA_MEMORY_REGION_KERNEL},
    {0x40208000U, 0x1FDF8000U, SYNTRA_MEMORY_REGION_RAM},
    {0x40000000U, 0x1800U, SYNTRA_MEMORY_REGION_DTB}
};

static void test_existing_mapping_attributes(void)
{
    syntra_arm64_gic_config_t gic = fixture_gic();
    syntra_arm64_mmu_mapping_plan_t plan;
    syntra_arm64_boot_memory_metadata_t metadata = fixture();
    int flags[10] = {0};
    size_t index;

    assert(syntra_arm64_mmu_mapping_plan_build(
        &metadata, fixture_regions, 4U, &gic,
        0x09000000U, 0x1000U, 0x09020000U, 0x18U,
        &plan) == SYNTRA_STATUS_OK);

    for (index = 0U; index < plan.count; ++index)
    {
        const syntra_arm64_mmu_mapping_t *mapping =
            &plan.mappings[index];

        if (mapping->virtual_address == metadata.text_start)
            flags[0] = mapping->access == SYNTRA_ARM64_MMU_ACCESS_READ_ONLY &&
                mapping->executable &&
                mapping->memory_type == SYNTRA_ARM64_MMU_MEMORY_NORMAL;
        if (mapping->virtual_address == metadata.rodata_start)
            flags[1] = mapping->access == SYNTRA_ARM64_MMU_ACCESS_READ_ONLY &&
                !mapping->executable;
        if (mapping->virtual_address == metadata.data_start)
            flags[2] = mapping->access == SYNTRA_ARM64_MMU_ACCESS_READ_WRITE &&
                !mapping->executable;
        if (mapping->virtual_address == metadata.bss_start)
            flags[3] = mapping->access == SYNTRA_ARM64_MMU_ACCESS_READ_WRITE &&
                !mapping->executable;
        if (mapping->virtual_address == metadata.stack_bottom)
            flags[4] = mapping->access == SYNTRA_ARM64_MMU_ACCESS_READ_WRITE &&
                !mapping->executable;
        if (mapping->virtual_address == 0x40000000U)
            flags[5] = mapping->access == SYNTRA_ARM64_MMU_ACCESS_READ_ONLY &&
                !mapping->executable;
        if (mapping->virtual_address == 0x09000000U)
            flags[6] = mapping->memory_type == SYNTRA_ARM64_MMU_MEMORY_DEVICE &&
                !mapping->executable;
        if (mapping->virtual_address == 0x08000000U)
            flags[7] = mapping->memory_type == SYNTRA_ARM64_MMU_MEMORY_DEVICE;
        if (mapping->virtual_address == 0x080A0000U)
            flags[8] = mapping->memory_type == SYNTRA_ARM64_MMU_MEMORY_DEVICE;
        if (mapping->virtual_address == 0x09020000U)
            flags[9] = mapping->memory_type == SYNTRA_ARM64_MMU_MEMORY_DEVICE &&
                mapping->access == SYNTRA_ARM64_MMU_ACCESS_READ_WRITE &&
                !mapping->executable;
    }

    for (index = 0U; index < 10U; ++index)
        assert(flags[index]);
}

static void test_virtio_device_mapping(void)
{
    syntra_arm64_gic_config_t gic = fixture_gic();
    syntra_arm64_mmu_mapping_plan_t plan;
    syntra_arm64_boot_memory_metadata_t metadata = fixture();
    int found = 0;
    size_t index;

    assert(syntra_arm64_mmu_mapping_plan_build_with_virtio(
        &metadata, fixture_regions, 4U, &gic,
        0x09000000U, 0x1000U, 0U, 0U,
        0x0A000000U, 0x200U, &plan) == SYNTRA_STATUS_OK);

    for (index = 0U; index < plan.count; ++index)
    {
        const syntra_arm64_mmu_mapping_t *mapping =
            &plan.mappings[index];

        if (mapping->virtual_address == 0x0A000000U)
        {
            found = 1;
            assert(mapping->physical_address == 0x0A000000U);
            assert(mapping->size == SYNTRA_ARM64_MMU_PAGE_SIZE);
            assert(mapping->memory_type == SYNTRA_ARM64_MMU_MEMORY_DEVICE);
            assert(mapping->access == SYNTRA_ARM64_MMU_ACCESS_READ_WRITE);
            assert(mapping->executable == 0U);
        }
    }
    assert(found);
}

static void test_virtio_rejections(void)
{
    syntra_arm64_gic_config_t gic = fixture_gic();
    syntra_arm64_mmu_mapping_plan_t plan;
    syntra_arm64_boot_memory_metadata_t metadata = fixture();

    assert(syntra_arm64_mmu_mapping_plan_build_with_virtio(
        &metadata, fixture_regions, 4U, &gic,
        0x09000000U, 0x1000U, 0U, 0U,
        0x0A000000U, 0x1FFU, &plan) ==
        SYNTRA_STATUS_INVALID_ARGUMENT);

    assert(syntra_arm64_mmu_mapping_plan_build_with_virtio(
        &metadata, fixture_regions, 4U, &gic,
        0x09000000U, 0x1000U, 0U, 0U,
        0x0A000000U, 0U, &plan) ==
        SYNTRA_STATUS_INVALID_ARGUMENT);

    assert(syntra_arm64_mmu_mapping_plan_build_with_virtio(
        &metadata, fixture_regions, 4U, &gic,
        0x09000000U, 0x1000U, 0U, 0U,
        0x09000000U, 0x200U, &plan) == SYNTRA_STATUS_ERROR);

    assert(syntra_arm64_mmu_mapping_plan_build_with_virtio(
        &metadata, fixture_regions, 4U, &gic,
        0x09000000U, 0x1000U, 0U, 0U,
        UINTPTR_MAX - 0x100U, 0x200U, &plan) ==
        SYNTRA_STATUS_INVALID_ARGUMENT);
}

static void test_existing_rejections(void)
{
    syntra_arm64_gic_config_t gic = fixture_gic();
    syntra_arm64_mmu_mapping_plan_t plan;
    syntra_arm64_boot_memory_metadata_t metadata = fixture();

    assert(syntra_arm64_mmu_mapping_plan_build(
        &metadata, fixture_regions, 4U, &gic,
        0x40200000U, 0x1000U, 0U, 0U, &plan) ==
        SYNTRA_STATUS_ERROR);

    metadata.text_start++;
    assert(syntra_arm64_mmu_mapping_plan_build(
        &metadata, fixture_regions, 4U, &gic,
        0x09000000U, 0x1000U, 0U, 0U, &plan) ==
        SYNTRA_STATUS_INVALID_ARGUMENT);

    metadata = fixture();
    assert(syntra_arm64_mmu_mapping_plan_build(
        &metadata, fixture_regions, 4U, &gic,
        0x09000000U, 0x1000U, 0x09020000U, 0U, &plan) ==
        SYNTRA_STATUS_INVALID_ARGUMENT);

    assert(syntra_arm64_mmu_mapping_plan_build(
        &metadata, fixture_regions, 4U, &gic,
        0x09000000U, 0x1000U, 0x09000000U, 0x1000U, &plan) ==
        SYNTRA_STATUS_ERROR);

    gic.version = SYNTRA_ARM64_GIC_VERSION_NONE;
    assert(syntra_arm64_mmu_mapping_plan_build(
        &metadata, fixture_regions, 4U, &gic,
        0x09000000U, 0x1000U, 0U, 0U, &plan) ==
        SYNTRA_STATUS_INVALID_ARGUMENT);
}

int main(void)
{
    test_existing_mapping_attributes();
    test_virtio_device_mapping();
    test_virtio_rejections();
    test_existing_rejections();
    puts("ARM64 MMU mapping-plan tests passed");
    return 0;
}
