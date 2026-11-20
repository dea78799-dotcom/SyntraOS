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

#define MMU_MAIR_VALUE 0x00000000000000FFULL
#define MMU_TCR_T0SZ       25ULL
#define MMU_TCR_EPD0       (0ULL << 7U)
#define MMU_TCR_IRGN0_WBWA (1ULL << 8U)
#define MMU_TCR_ORGN0_WBWA (1ULL << 10U)
#define MMU_TCR_SH0_INNER  (3ULL << 12U)
#define MMU_TCR_TG0_4K     (0ULL << 14U)
#define MMU_TCR_IPS_SHIFT  32U
#define MMU_TCR_IPS_MASK   (7ULL << MMU_TCR_IPS_SHIFT)
#define MMU_ID_AA64MMFR0_PARANGE_MASK 0xFULL
#define MMU_ID_AA64MMFR0_PARANGE_48_BITS 5ULL
#define MMU_SCTLR_M    (1ULL << 0U)

static uint64_t mmu_configured_tcr_value;

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


syntra_status_t syntra_arm64_mmu_walk(
    uint64_t root_table_physical_address,
    uint64_t virtual_address,
    syntra_arm64_mmu_table_resolver_t resolver,
    void *context,
    syntra_arm64_mmu_translation_t *translation
)
{
    uint16_t indices[4];
    const uint64_t *table;
    uint64_t descriptor;
    uint64_t table_address;
    unsigned int level;

    if (resolver == NULL || translation == NULL ||
        !physical_address_is_valid(root_table_physical_address))
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (syntra_arm64_mmu_get_table_indices(virtual_address, indices) !=
        SYNTRA_STATUS_OK)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    table = resolver(root_table_physical_address, context);
    if (table == NULL)
    {
        return SYNTRA_STATUS_ERROR;
    }

    for (level = 0U; level < 3U; ++level)
    {
        descriptor = table[indices[level]];
        if ((descriptor & MMU_DESCRIPTOR_VALID) == 0ULL)
        {
            return SYNTRA_STATUS_ERROR;
        }

        if (level < 2U)
        {
            if ((descriptor & MMU_DESCRIPTOR_TABLE_PAGE) == 0ULL)
            {
                return SYNTRA_STATUS_NOT_IMPLEMENTED;
            }

            table_address = descriptor & SYNTRA_ARM64_MMU_PA_MASK;
            if (!physical_address_is_valid(table_address))
            {
                return SYNTRA_STATUS_ERROR;
            }

            table = resolver(table_address, context);
            if (table == NULL)
            {
                return SYNTRA_STATUS_ERROR;
            }
        }
        else
        {
            uint64_t page_base;

            if ((descriptor & MMU_DESCRIPTOR_TABLE_PAGE) == 0ULL)
            {
                return SYNTRA_STATUS_ERROR;
            }

            page_base = descriptor & SYNTRA_ARM64_MMU_PA_MASK;
            translation->physical_address =
                page_base + (uint64_t)indices[3];
            translation->page_descriptor = descriptor;
            translation->level = 3U;
            return SYNTRA_STATUS_OK;
        }
    }

    return SYNTRA_STATUS_ERROR;
}

syntra_status_t syntra_arm64_mmu_read_registers(
    syntra_arm64_mmu_registers_t *registers
)
{
    if (registers == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

#if defined(__aarch64__)
    __asm__ volatile("mrs %0, sctlr_el1" : "=r"(registers->sctlr_el1));
    __asm__ volatile("mrs %0, mair_el1" : "=r"(registers->mair_el1));
    __asm__ volatile("mrs %0, tcr_el1" : "=r"(registers->tcr_el1));
    __asm__ volatile("mrs %0, ttbr0_el1" : "=r"(registers->ttbr0_el1));
    __asm__ volatile("mrs %0, id_aa64mmfr0_el1"
                     : "=r"(registers->id_aa64mmfr0_el1));
    return SYNTRA_STATUS_OK;
#else
    registers->sctlr_el1 = 0ULL;
    registers->mair_el1 = 0ULL;
    registers->tcr_el1 = 0ULL;
    registers->ttbr0_el1 = 0ULL;
    registers->id_aa64mmfr0_el1 = 0ULL;
    return SYNTRA_STATUS_NOT_IMPLEMENTED;
#endif
}

syntra_status_t syntra_arm64_mmu_prepare(uint64_t root_table_physical_address)
{
#if defined(__aarch64__)
    syntra_arm64_mmu_registers_t registers;
    uint64_t parange;
    uint64_t tcr_value;

    if (!physical_address_is_valid(root_table_physical_address))
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (syntra_arm64_mmu_read_registers(&registers) != SYNTRA_STATUS_OK)
    {
        return SYNTRA_STATUS_ERROR;
    }

    if ((registers.sctlr_el1 & MMU_SCTLR_M) != 0ULL)
    {
        return SYNTRA_STATUS_PERMISSION_DENIED;
    }

    parange = registers.id_aa64mmfr0_el1 &
              MMU_ID_AA64MMFR0_PARANGE_MASK;
    if (parange > MMU_ID_AA64MMFR0_PARANGE_48_BITS)
    {
        /* Descriptor helpers currently support at most 48-bit PA. */
        parange = MMU_ID_AA64MMFR0_PARANGE_48_BITS;
    }

    tcr_value = MMU_TCR_T0SZ |
                MMU_TCR_EPD0 |
                MMU_TCR_IRGN0_WBWA |
                MMU_TCR_ORGN0_WBWA |
                MMU_TCR_SH0_INNER |
                MMU_TCR_TG0_4K |
                (parange << MMU_TCR_IPS_SHIFT);

    __asm__ volatile("msr mair_el1, %0" :: "r"(MMU_MAIR_VALUE) : "memory");
    __asm__ volatile("msr tcr_el1, %0" :: "r"(tcr_value) : "memory");
    __asm__ volatile("dsb ish" ::: "memory");
    __asm__ volatile("isb" ::: "memory");
    __asm__ volatile("dsb ish" ::: "memory");
    __asm__ volatile("isb" ::: "memory");
    __asm__ volatile("msr ttbr0_el1, %0"
                     :: "r"(root_table_physical_address) : "memory");
    __asm__ volatile("dsb ish" ::: "memory");
    __asm__ volatile("isb" ::: "memory");
    __asm__ volatile("dsb ish" ::: "memory");
    __asm__ volatile("tlbi vmalle1" ::: "memory");
    __asm__ volatile("dsb ish" ::: "memory");
    __asm__ volatile("isb" ::: "memory");
    mmu_configured_tcr_value = tcr_value;
    return SYNTRA_STATUS_OK;
#else
    (void)root_table_physical_address;
    return SYNTRA_STATUS_NOT_IMPLEMENTED;
#endif
}

syntra_status_t syntra_arm64_mmu_enable(void)
{
#if defined(__aarch64__)
    syntra_arm64_mmu_registers_t registers;
    uint64_t sctlr;

    if (syntra_arm64_mmu_read_registers(&registers) != SYNTRA_STATUS_OK)
    {
        return SYNTRA_STATUS_ERROR;
    }

    if ((registers.sctlr_el1 & MMU_SCTLR_M) != 0ULL)
    {
        return SYNTRA_STATUS_PERMISSION_DENIED;
    }

    if ((registers.ttbr0_el1 & SYNTRA_ARM64_MMU_PA_MASK) == 0ULL ||
        registers.mair_el1 != MMU_MAIR_VALUE ||
        mmu_configured_tcr_value == 0ULL ||
        registers.tcr_el1 != mmu_configured_tcr_value)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    __asm__ volatile("dsb ish" ::: "memory");
    __asm__ volatile("isb" ::: "memory");
    __asm__ volatile("mrs %0, sctlr_el1" : "=r"(sctlr));
    sctlr |= MMU_SCTLR_M;
    __asm__ volatile("msr sctlr_el1, %0" :: "r"(sctlr) : "memory");
    __asm__ volatile("dsb ish" ::: "memory");
    __asm__ volatile("isb" ::: "memory");
    return SYNTRA_STATUS_OK;
#else
    return SYNTRA_STATUS_NOT_IMPLEMENTED;
#endif
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
