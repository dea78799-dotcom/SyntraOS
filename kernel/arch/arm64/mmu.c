#include <stddef.h>

#include "mmu.h"

#define MMU_DESCRIPTOR_VALID       (1ULL << 0)
#define MMU_DESCRIPTOR_TABLE_PAGE  (1ULL << 1)
#define MMU_DESCRIPTOR_ATTR_SHIFT  2U
#define MMU_DESCRIPTOR_AP_RO       (1ULL << 7)
#define MMU_DESCRIPTOR_SH_SHIFT    8U
#define MMU_DESCRIPTOR_AF          (1ULL << 10)
#define MMU_DESCRIPTOR_PXN         (1ULL << 53)
#define MMU_DESCRIPTOR_UXN         (1ULL << 54)

#define MMU_ATTR_INDEX_NORMAL      0U
#define MMU_ATTR_INDEX_DEVICE      1U

#define MMU_SH_OUTER_SHAREABLE     2U
#define MMU_SH_INNER_SHAREABLE     3U

#define MMU_VA_MASK               ((1ULL << SYNTRA_ARM64_MMU_VA_BITS) - 1ULL)
#define MMU_PA_LIMIT              (1ULL << SYNTRA_ARM64_MMU_PA_BITS)

static int physical_address_is_valid(uint64_t address)
{
    return (address & (SYNTRA_ARM64_MMU_PAGE_SIZE - 1ULL)) == 0ULL &&
           address < MMU_PA_LIMIT;
}

syntra_status_t syntra_arm64_mmu_make_table_descriptor(
    uint64_t table_physical_address,
    uint64_t *descriptor
)
{
    if (descriptor == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (!physical_address_is_valid(table_physical_address))
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    *descriptor = (table_physical_address & SYNTRA_ARM64_MMU_PA_MASK) |
                  MMU_DESCRIPTOR_VALID |
                  MMU_DESCRIPTOR_TABLE_PAGE;

    return SYNTRA_STATUS_OK;
}

syntra_status_t syntra_arm64_mmu_make_page_descriptor(
    uint64_t physical_address,
    syntra_arm64_mmu_memory_type_t memory_type,
    syntra_arm64_mmu_access_t access,
    int executable,
    uint64_t *descriptor
)
{
    uint64_t value;
    uint64_t attribute_index;
    uint64_t shareability;

    if (descriptor == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (!physical_address_is_valid(physical_address) ||
        (memory_type != SYNTRA_ARM64_MMU_MEMORY_NORMAL &&
         memory_type != SYNTRA_ARM64_MMU_MEMORY_DEVICE) ||
        (access != SYNTRA_ARM64_MMU_ACCESS_READ_WRITE &&
         access != SYNTRA_ARM64_MMU_ACCESS_READ_ONLY) ||
        (executable != 0 && executable != 1))
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (memory_type == SYNTRA_ARM64_MMU_MEMORY_DEVICE && executable)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    attribute_index = memory_type == SYNTRA_ARM64_MMU_MEMORY_NORMAL
        ? MMU_ATTR_INDEX_NORMAL : MMU_ATTR_INDEX_DEVICE;
    shareability = memory_type == SYNTRA_ARM64_MMU_MEMORY_NORMAL
        ? MMU_SH_INNER_SHAREABLE : MMU_SH_OUTER_SHAREABLE;

    value = (physical_address & SYNTRA_ARM64_MMU_PA_MASK) |
            MMU_DESCRIPTOR_VALID |
            MMU_DESCRIPTOR_TABLE_PAGE |
            (attribute_index << MMU_DESCRIPTOR_ATTR_SHIFT) |
            (shareability << MMU_DESCRIPTOR_SH_SHIFT) |
            MMU_DESCRIPTOR_AF |
            MMU_DESCRIPTOR_UXN;

    if (access == SYNTRA_ARM64_MMU_ACCESS_READ_ONLY)
    {
        value |= MMU_DESCRIPTOR_AP_RO;
    }

    if (!executable)
    {
        value |= MMU_DESCRIPTOR_PXN;
    }

    *descriptor = value;
    return SYNTRA_STATUS_OK;
}

syntra_status_t syntra_arm64_mmu_get_table_indices(
    uint64_t virtual_address,
    uint16_t indices[4]
)
{
    if (indices == NULL || (virtual_address & ~MMU_VA_MASK) != 0ULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    indices[0] = (uint16_t)((virtual_address >> 30U) & 0x1FFU);
    indices[1] = (uint16_t)((virtual_address >> 21U) & 0x1FFU);
    indices[2] = (uint16_t)((virtual_address >> 12U) & 0x1FFU);
    indices[3] = (uint16_t)(virtual_address & 0xFFFU);

    return SYNTRA_STATUS_OK;
}

syntra_status_t syntra_arm64_mmu_validate_mapping_range(
    uint64_t virtual_address,
    uint64_t physical_address,
    uint64_t size
)
{
    const uint64_t virtual_limit = 1ULL << SYNTRA_ARM64_MMU_VA_BITS;
    const uint64_t physical_limit = 1ULL << SYNTRA_ARM64_MMU_PA_BITS;
    const uint64_t page_mask = SYNTRA_ARM64_MMU_PAGE_SIZE - 1ULL;

    if (size == 0ULL ||
        ((virtual_address | physical_address | size) & page_mask) != 0ULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (virtual_address >= virtual_limit ||
        physical_address >= physical_limit ||
        size > (virtual_limit - virtual_address) ||
        size > (physical_limit - physical_address))
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    return SYNTRA_STATUS_OK;
}
