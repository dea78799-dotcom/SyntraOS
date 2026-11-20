#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "../../kernel/arch/arm64/mmu_mapping_plan.h"

static syntra_arm64_boot_memory_metadata_t fixture(void)
{
    syntra_arm64_boot_memory_metadata_t m = {0};
    m.kernel_start = 0x40200000; m.kernel_end = 0x40208000;
    m.text_start = 0x40200000; m.text_end = 0x40201000;
    m.rodata_start = 0x40201000; m.rodata_end = 0x40202000;
    m.data_start = 0x40202000; m.data_end = 0x40203000;
    m.bss_start = 0x40203000; m.bss_end = 0x40204000;
    m.stack_bottom = 0x40204000; m.stack_top = 0x40208000;
    m.dtb_start = 0x40000000; m.dtb_size = 0x1800;
    return m;
}

static syntra_arm64_gic_config_t fixture_gic(void)
{
    syntra_arm64_gic_config_t g = {0};
    g.version = SYNTRA_ARM64_GIC_VERSION_3;
    g.distributor_base = 0x08000000; g.distributor_size = 0x10000;
    g.redistributor_base = 0x080A0000; g.redistributor_size = 0xF60000;
    return g;
}

static void test_attributes(void)
{
    syntra_memory_region_t rs[] = {
        {0x40002000,0x001FE000,SYNTRA_MEMORY_REGION_RAM},
        {0x40200000,0x8000,SYNTRA_MEMORY_REGION_KERNEL},
        {0x40208000,0x1FDF8000,SYNTRA_MEMORY_REGION_RAM},
        {0x40000000,0x1800,SYNTRA_MEMORY_REGION_DTB}
    };
    syntra_arm64_gic_config_t g = fixture_gic();
    syntra_arm64_mmu_mapping_plan_t p;
    syntra_arm64_boot_memory_metadata_t m = fixture();
    int flags[10] = {0};
    size_t i;

    assert(syntra_arm64_mmu_mapping_plan_build(
        &m, rs, 4, &g, 0x09000000, 0x1000,
        0x09020000, 0x18, &p) == SYNTRA_STATUS_OK);

    for (i = 0; i < p.count; ++i)
    {
        const syntra_arm64_mmu_mapping_t *x = &p.mappings[i];
        if (x->virtual_address == m.text_start)
            flags[0] = x->access == SYNTRA_ARM64_MMU_ACCESS_READ_ONLY &&
                x->executable && x->memory_type == SYNTRA_ARM64_MMU_MEMORY_NORMAL;
        if (x->virtual_address == m.rodata_start)
            flags[1] = x->access == SYNTRA_ARM64_MMU_ACCESS_READ_ONLY && !x->executable;
        if (x->virtual_address == m.data_start)
            flags[2] = x->access == SYNTRA_ARM64_MMU_ACCESS_READ_WRITE && !x->executable;
        if (x->virtual_address == m.bss_start)
            flags[3] = x->access == SYNTRA_ARM64_MMU_ACCESS_READ_WRITE && !x->executable;
        if (x->virtual_address == m.stack_bottom)
            flags[4] = x->access == SYNTRA_ARM64_MMU_ACCESS_READ_WRITE && !x->executable;
        if (x->virtual_address == 0x40000000)
            flags[5] = x->access == SYNTRA_ARM64_MMU_ACCESS_READ_ONLY && !x->executable;
        if (x->virtual_address == 0x09000000)
            flags[6] = x->memory_type == SYNTRA_ARM64_MMU_MEMORY_DEVICE && !x->executable;
        if (x->virtual_address == 0x08000000)
            flags[7] = x->memory_type == SYNTRA_ARM64_MMU_MEMORY_DEVICE;
        if (x->virtual_address == 0x080A0000)
            flags[8] = x->memory_type == SYNTRA_ARM64_MMU_MEMORY_DEVICE;
        if (x->virtual_address == 0x09020000)
            flags[9] = x->memory_type == SYNTRA_ARM64_MMU_MEMORY_DEVICE &&
                x->access == SYNTRA_ARM64_MMU_ACCESS_READ_WRITE && !x->executable;
    }
    for (i = 0; i < 10; ++i) assert(flags[i]);
}

static void test_rejections(void)
{
    syntra_memory_region_t rs[] = {
        {0x40002000,0x001FE000,SYNTRA_MEMORY_REGION_RAM},
        {0x40200000,0x8000,SYNTRA_MEMORY_REGION_KERNEL},
        {0x40208000,0x1FDF8000,SYNTRA_MEMORY_REGION_RAM}
    };
    syntra_arm64_gic_config_t g = fixture_gic();
    syntra_arm64_mmu_mapping_plan_t p;
    syntra_arm64_boot_memory_metadata_t m = fixture();

    assert(syntra_arm64_mmu_mapping_plan_build(
        &m, rs, 3, &g, 0x40200000, 0x1000, 0, 0, &p) == SYNTRA_STATUS_ERROR);
    m.text_start++;
    assert(syntra_arm64_mmu_mapping_plan_build(
        &m, rs, 3, &g, 0x09000000, 0x1000, 0, 0, &p) ==
        SYNTRA_STATUS_INVALID_ARGUMENT);
    m = fixture();
    assert(syntra_arm64_mmu_mapping_plan_build(
        &m, rs, 3, &g, 0x09000000, 0x1000, 0x09020000, 0, &p) ==
        SYNTRA_STATUS_INVALID_ARGUMENT);
    assert(syntra_arm64_mmu_mapping_plan_build(
        &m, rs, 3, &g, 0x09000000, 0x1000, 0x09000000, 0x1000, &p) ==
        SYNTRA_STATUS_ERROR);
    g.version = SYNTRA_ARM64_GIC_VERSION_NONE;
    assert(syntra_arm64_mmu_mapping_plan_build(
        &m, rs, 3, &g, 0x09000000, 0x1000, 0, 0, &p) ==
        SYNTRA_STATUS_INVALID_ARGUMENT);
}

int main(void)
{
    test_attributes();
    test_rejections();
    puts("ARM64 MMU mapping-plan tests passed");
    return 0;
}
