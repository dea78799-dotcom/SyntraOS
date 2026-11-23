#include "mmu_mapping_plan.h"

#include <stdint.h>

#define PAGE_MASK (SYNTRA_ARM64_MMU_PAGE_SIZE - 1ULL)
#define VIRTIO_MMIO_MIN_REGION_SIZE 0x200U

static int range_end(uint64_t base, uint64_t size, uint64_t *end)
{
    if (size == 0U || base > UINT64_MAX - size)
        return 0;

    *end = base + size;
    return 1;
}

static uint64_t align_down(uint64_t value)
{
    return value & ~PAGE_MASK;
}

static int align_up(uint64_t value, uint64_t *aligned)
{
    if (value > UINT64_MAX - PAGE_MASK)
        return 0;

    *aligned = (value + PAGE_MASK) & ~PAGE_MASK;
    return 1;
}

static syntra_status_t append(
    syntra_arm64_mmu_mapping_plan_t *plan,
    uint64_t base,
    uint64_t size,
    syntra_arm64_mmu_memory_type_t memory_type,
    syntra_arm64_mmu_access_t access,
    int executable)
{
    uint64_t end;
    size_t index;

    if (plan == NULL || size == 0U ||
        syntra_arm64_mmu_validate_mapping_range(base, base, size) !=
            SYNTRA_STATUS_OK ||
        !range_end(base, size, &end))
        return SYNTRA_STATUS_INVALID_ARGUMENT;

    for (index = 0U; index < plan->count; ++index)
    {
        uint64_t existing_end;
        const syntra_arm64_mmu_mapping_t *mapping =
            &plan->mappings[index];

        if (!range_end(mapping->virtual_address, mapping->size,
                       &existing_end))
            return SYNTRA_STATUS_ERROR;

        if (base < existing_end && mapping->virtual_address < end)
            return SYNTRA_STATUS_ERROR;
    }

    if (plan->count >= SYNTRA_ARM64_MMU_MAPPING_PLAN_MAX)
        return SYNTRA_STATUS_ERROR;

    plan->mappings[plan->count] = (syntra_arm64_mmu_mapping_t) {
        base, base, size, memory_type, access, (uint8_t)executable
    };
    ++plan->count;
    return SYNTRA_STATUS_OK;
}

static syntra_status_t append_region(
    syntra_arm64_mmu_mapping_plan_t *plan,
    uint64_t base,
    uint64_t size,
    syntra_arm64_mmu_memory_type_t memory_type,
    syntra_arm64_mmu_access_t access,
    int executable,
    int round_outward)
{
    uint64_t end;
    uint64_t aligned_end;

    if (!range_end(base, size, &end))
        return SYNTRA_STATUS_INVALID_ARGUMENT;

    if (round_outward)
    {
        base = align_down(base);
        if (!align_up(end, &aligned_end))
            return SYNTRA_STATUS_INVALID_ARGUMENT;
        end = aligned_end;
    }
    else
    {
        if (!align_up(base, &base))
            return SYNTRA_STATUS_INVALID_ARGUMENT;
        end = align_down(end);
    }

    if (end <= base)
        return SYNTRA_STATUS_OK;

    return append(plan, base, end - base, memory_type, access, executable);
}

/*
 * no-map exclusions are page-rounded outward: ARM64 cannot leave only part
 * of a translation granule unmapped. Reject any explicit mapping that would
 * cover even one page intersecting a no-map reservation.
 */
static syntra_status_t validate_no_map_exclusions(
    const syntra_arm64_mmu_mapping_plan_t *plan,
    const syntra_memory_region_t *regions,
    size_t region_count)
{
    size_t region_index;

    for (region_index = 0U; region_index < region_count; ++region_index)
    {
        uint64_t excluded_start;
        uint64_t excluded_end;
        uint64_t raw_end;
        size_t mapping_index;

        if (regions[region_index].type != SYNTRA_MEMORY_REGION_NOMAP)
        {
            continue;
        }

        if (!range_end((uint64_t)regions[region_index].base,
                       (uint64_t)regions[region_index].size, &raw_end))
        {
            return SYNTRA_STATUS_INVALID_ARGUMENT;
        }

        excluded_start = align_down((uint64_t)regions[region_index].base);
        if (!align_up(raw_end, &excluded_end))
        {
            return SYNTRA_STATUS_INVALID_ARGUMENT;
        }

        for (mapping_index = 0U;
             mapping_index < plan->count;
             ++mapping_index)
        {
            const syntra_arm64_mmu_mapping_t *mapping =
                &plan->mappings[mapping_index];
            uint64_t mapping_end;

            if (!range_end(mapping->physical_address,
                           mapping->size, &mapping_end))
            {
                return SYNTRA_STATUS_ERROR;
            }

            if (excluded_start < mapping_end &&
                mapping->virtual_address < excluded_end)
            {
                return SYNTRA_STATUS_ERROR;
            }
        }
    }

    return SYNTRA_STATUS_OK;
}

static syntra_status_t append_section(
    syntra_arm64_mmu_mapping_plan_t *plan,
    uint64_t start,
    uint64_t end,
    syntra_arm64_mmu_access_t access,
    int executable,
    uint64_t kernel_start,
    uint64_t kernel_end)
{
    if (start > end || (start & PAGE_MASK) != 0U ||
        (end & PAGE_MASK) != 0U ||
        start < kernel_start || end > kernel_end)
        return SYNTRA_STATUS_INVALID_ARGUMENT;

    if (start == end)
        return SYNTRA_STATUS_OK;

    return append(plan, start, end - start,
                  SYNTRA_ARM64_MMU_MEMORY_NORMAL, access, executable);
}

syntra_status_t syntra_arm64_mmu_mapping_plan_build_with_virtio(
    const syntra_arm64_boot_memory_metadata_t *metadata,
    const syntra_memory_region_t *regions,
    size_t region_count,
    const syntra_arm64_gic_config_t *gic,
    uintptr_t uart_base,
    size_t uart_size,
    uintptr_t fw_cfg_base,
    size_t fw_cfg_size,
    uintptr_t virtio_base,
    size_t virtio_size,
    syntra_arm64_mmu_mapping_plan_t *plan)
{
    syntra_status_t status;
    size_t index;
    uint64_t region_end;

    if (metadata == NULL || regions == NULL || region_count == 0U ||
        gic == NULL || plan == NULL || uart_base == 0U || uart_size == 0U ||
        ((fw_cfg_base == 0U) != (fw_cfg_size == 0U)) ||
        ((virtio_base == 0U) != (virtio_size == 0U)) ||
        (virtio_base != 0U &&
         (virtio_size < VIRTIO_MMIO_MIN_REGION_SIZE ||
          virtio_base > UINTPTR_MAX - virtio_size)) ||
        metadata->kernel_start == 0U ||
        metadata->kernel_end <= metadata->kernel_start ||
        metadata->dtb_start == 0U || metadata->dtb_size == 0U ||
        metadata->text_end > metadata->rodata_start ||
        metadata->rodata_end > metadata->data_start ||
        metadata->data_end > metadata->bss_start ||
        metadata->bss_end > metadata->stack_bottom ||
        metadata->stack_top <= metadata->stack_bottom)
        return SYNTRA_STATUS_INVALID_ARGUMENT;

    plan->count = 0U;

    for (index = 0U; index < region_count; ++index)
    {
        if (!regions[index].size ||
            !range_end(regions[index].base, regions[index].size,
                       &region_end))
            return SYNTRA_STATUS_INVALID_ARGUMENT;

        if (regions[index].type == SYNTRA_MEMORY_REGION_RAM ||
            regions[index].type == SYNTRA_MEMORY_REGION_RESERVED ||
            regions[index].type == SYNTRA_MEMORY_REGION_RECLAIMABLE)
        {
            status = append_region(
                plan, regions[index].base, regions[index].size,
                SYNTRA_ARM64_MMU_MEMORY_NORMAL,
                SYNTRA_ARM64_MMU_ACCESS_READ_WRITE, 0, 0);
            if (status != SYNTRA_STATUS_OK)
                return status;
        }
    }

    status = append_section(
        plan, metadata->text_start, metadata->text_end,
        SYNTRA_ARM64_MMU_ACCESS_READ_ONLY, 1,
        metadata->kernel_start, metadata->kernel_end);
    if (status != SYNTRA_STATUS_OK) return status;

    status = append_section(
        plan, metadata->rodata_start, metadata->rodata_end,
        SYNTRA_ARM64_MMU_ACCESS_READ_ONLY, 0,
        metadata->kernel_start, metadata->kernel_end);
    if (status != SYNTRA_STATUS_OK) return status;

    status = append_section(
        plan, metadata->data_start, metadata->data_end,
        SYNTRA_ARM64_MMU_ACCESS_READ_WRITE, 0,
        metadata->kernel_start, metadata->kernel_end);
    if (status != SYNTRA_STATUS_OK) return status;

    status = append_section(
        plan, metadata->bss_start, metadata->bss_end,
        SYNTRA_ARM64_MMU_ACCESS_READ_WRITE, 0,
        metadata->kernel_start, metadata->kernel_end);
    if (status != SYNTRA_STATUS_OK) return status;

    status = append_section(
        plan, metadata->stack_bottom, metadata->stack_top,
        SYNTRA_ARM64_MMU_ACCESS_READ_WRITE, 0,
        metadata->kernel_start, metadata->kernel_end);
    if (status != SYNTRA_STATUS_OK) return status;

    status = append_region(
        plan, metadata->dtb_start, metadata->dtb_size,
        SYNTRA_ARM64_MMU_MEMORY_NORMAL,
        SYNTRA_ARM64_MMU_ACCESS_READ_ONLY, 0, 1);
    if (status != SYNTRA_STATUS_OK) return status;

    status = append_region(
        plan, uart_base, uart_size, SYNTRA_ARM64_MMU_MEMORY_DEVICE,
        SYNTRA_ARM64_MMU_ACCESS_READ_WRITE, 0, 1);
    if (status != SYNTRA_STATUS_OK) return status;

    if (fw_cfg_base != 0U)
    {
        if (fw_cfg_size < 0x18U)
            return SYNTRA_STATUS_INVALID_ARGUMENT;

        status = append_region(
            plan, fw_cfg_base, fw_cfg_size,
            SYNTRA_ARM64_MMU_MEMORY_DEVICE,
            SYNTRA_ARM64_MMU_ACCESS_READ_WRITE, 0, 1);
        if (status != SYNTRA_STATUS_OK) return status;
    }

    if (virtio_base != 0U)
    {
        status = append_region(
            plan, virtio_base, virtio_size,
            SYNTRA_ARM64_MMU_MEMORY_DEVICE,
            SYNTRA_ARM64_MMU_ACCESS_READ_WRITE, 0, 1);
        if (status != SYNTRA_STATUS_OK) return status;
    }

    if (gic->version == SYNTRA_ARM64_GIC_VERSION_3)
    {
        status = append_region(
            plan, gic->distributor_base, gic->distributor_size,
            SYNTRA_ARM64_MMU_MEMORY_DEVICE,
            SYNTRA_ARM64_MMU_ACCESS_READ_WRITE, 0, 1);
        if (status != SYNTRA_STATUS_OK) return status;

        status = append_region(
            plan, gic->redistributor_base, gic->redistributor_size,
            SYNTRA_ARM64_MMU_MEMORY_DEVICE,
            SYNTRA_ARM64_MMU_ACCESS_READ_WRITE, 0, 1);
        if (status != SYNTRA_STATUS_OK) return status;
    }
    else if (gic->version == SYNTRA_ARM64_GIC_VERSION_2)
    {
        status = append_region(
            plan, gic->distributor_base, gic->distributor_size,
            SYNTRA_ARM64_MMU_MEMORY_DEVICE,
            SYNTRA_ARM64_MMU_ACCESS_READ_WRITE, 0, 1);
        if (status != SYNTRA_STATUS_OK) return status;

        status = append_region(
            plan, gic->cpu_interface_base, gic->cpu_interface_size,
            SYNTRA_ARM64_MMU_MEMORY_DEVICE,
            SYNTRA_ARM64_MMU_ACCESS_READ_WRITE, 0, 1);
        if (status != SYNTRA_STATUS_OK) return status;
    }
    else
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    status = validate_no_map_exclusions(plan, regions, region_count);
    if (status != SYNTRA_STATUS_OK)
        return status;

    return SYNTRA_STATUS_OK;
}

syntra_status_t syntra_arm64_mmu_mapping_plan_build(
    const syntra_arm64_boot_memory_metadata_t *metadata,
    const syntra_memory_region_t *regions,
    size_t region_count,
    const syntra_arm64_gic_config_t *gic,
    uintptr_t uart_base,
    size_t uart_size,
    uintptr_t fw_cfg_base,
    size_t fw_cfg_size,
    syntra_arm64_mmu_mapping_plan_t *plan)
{
    return syntra_arm64_mmu_mapping_plan_build_with_virtio(
        metadata, regions, region_count, gic,
        uart_base, uart_size, fw_cfg_base, fw_cfg_size,
        0U, 0U, plan);
}
