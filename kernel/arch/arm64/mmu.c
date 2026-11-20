#include <stddef.h>
#include "mmu.h"

#define DESC_VALID       (1ULL << 0U)
#define DESC_TYPE        (1ULL << 1U)
#define DESC_ATTR_SHIFT  2U
#define DESC_AP_SHIFT    6U
#define DESC_SH_SHIFT    8U
#define DESC_AF          (1ULL << 10U)
#define DESC_PXN         (1ULL << 53U)
#define DESC_UXN         (1ULL << 54U)
#define DESC_PXN_TABLE   (1ULL << 59U)
#define DESC_UXN_TABLE   (1ULL << 60U)
#define DESC_AP_TABLE_RO (1ULL << 61U)
#define DESC_AP_TABLE_NO_EL0 (1ULL << 62U)

#define DESC_ADDR_MASK SYNTRA_ARM64_MMU_PA_MASK
#define DESC_LEAF_FEATURE_MASK (((1ULL << 59U) - 1ULL) & ~((1ULL << 50U) - 1ULL))
#define DESC_LEAF_ALLOWED (0xFFFULL | DESC_ADDR_MASK | DESC_LEAF_FEATURE_MASK)
#define DESC_TABLE_ALLOWED (0x3ULL | DESC_ADDR_MASK | (0xFULL << 55U) | (0x1FULL << 59U))

#define MMU_ATTR_INDEX_NORMAL 0U
#define MMU_ATTR_INDEX_DEVICE 1U
#define MMU_SH_NON_SHAREABLE 0U
#define MMU_SH_OUTER_SHAREABLE 2U
#define MMU_SH_INNER_SHAREABLE 3U
#define MMU_VA_MASK ((1ULL << SYNTRA_ARM64_MMU_VA_BITS) - 1ULL)
#define MMU_PA_LIMIT (1ULL << SYNTRA_ARM64_MMU_PA_BITS)

#define MMU_MAIR_VALUE 0x00000000000000FFULL
#define MMU_TCR_T0SZ       25ULL
#define MMU_TCR_EPD0       (0ULL << 7U)
#define MMU_TCR_IRGN0_WBWA (1ULL << 8U)
#define MMU_TCR_ORGN0_WBWA (1ULL << 10U)
#define MMU_TCR_SH0_INNER  (3ULL << 12U)
#define MMU_TCR_TG0_4K     (0ULL << 14U)
#define MMU_TCR_IPS_SHIFT  32U
#define MMU_ID_AA64MMFR0_PARANGE_MASK 0xFULL
#define MMU_ID_AA64MMFR0_PARANGE_48_BITS 5ULL
#define MMU_SCTLR_M (1ULL << 0U)

static uint64_t mmu_configured_tcr_value;

static int physical_address_is_valid(uint64_t address)
{
    return (address & (SYNTRA_ARM64_MMU_PAGE_SIZE - 1ULL)) == 0ULL &&
           address < MMU_PA_LIMIT;
}

syntra_status_t syntra_arm64_mmu_make_table_descriptor(
    uint64_t table_physical_address, uint64_t *descriptor)
{
    if (descriptor == NULL || !physical_address_is_valid(table_physical_address))
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    *descriptor = (table_physical_address & DESC_ADDR_MASK) | DESC_VALID | DESC_TYPE;
    return SYNTRA_STATUS_OK;
}

syntra_status_t syntra_arm64_mmu_make_page_descriptor(
    uint64_t physical_address, syntra_arm64_mmu_memory_type_t memory_type,
    syntra_arm64_mmu_access_t access, int executable, uint64_t *descriptor)
{
    uint64_t value, attribute_index, shareability;
    if (descriptor == NULL || !physical_address_is_valid(physical_address) ||
        (memory_type != SYNTRA_ARM64_MMU_MEMORY_NORMAL &&
         memory_type != SYNTRA_ARM64_MMU_MEMORY_DEVICE) ||
        (access != SYNTRA_ARM64_MMU_ACCESS_READ_WRITE &&
         access != SYNTRA_ARM64_MMU_ACCESS_READ_ONLY) ||
        (executable != 0 && executable != 1) ||
        (memory_type == SYNTRA_ARM64_MMU_MEMORY_DEVICE && executable))
        return SYNTRA_STATUS_INVALID_ARGUMENT;

    attribute_index = memory_type == SYNTRA_ARM64_MMU_MEMORY_NORMAL
        ? MMU_ATTR_INDEX_NORMAL : MMU_ATTR_INDEX_DEVICE;
    shareability = memory_type == SYNTRA_ARM64_MMU_MEMORY_NORMAL
        ? MMU_SH_INNER_SHAREABLE : MMU_SH_OUTER_SHAREABLE;
    value = (physical_address & DESC_ADDR_MASK) | DESC_VALID | DESC_TYPE |
            (attribute_index << DESC_ATTR_SHIFT) |
            (shareability << DESC_SH_SHIFT) | DESC_AF | DESC_UXN;
    if (access == SYNTRA_ARM64_MMU_ACCESS_READ_ONLY) value |= (1ULL << 7U);
    if (!executable) value |= DESC_PXN;
    *descriptor = value;
    return SYNTRA_STATUS_OK;
}

syntra_status_t syntra_arm64_mmu_get_table_indices(
    uint64_t virtual_address, uint16_t indices[4])
{
    if (indices == NULL || (virtual_address & ~MMU_VA_MASK) != 0ULL)
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    /* T0SZ=25 gives a 39-bit VA: translation starts at L1, not L0. */
    indices[0] = (uint16_t)((virtual_address >> 30U) & 0x1FFU);
    indices[1] = (uint16_t)((virtual_address >> 21U) & 0x1FFU);
    indices[2] = (uint16_t)((virtual_address >> 12U) & 0x1FFU);
    indices[3] = (uint16_t)(virtual_address & 0xFFFU);
    return SYNTRA_STATUS_OK;
}

static syntra_status_t decode_leaf(
    uint64_t descriptor, uint64_t virtual_address, unsigned int level,
    uint64_t inherited, syntra_arm64_mmu_translation_t *translation)
{
    uint64_t page_size, address_mask, output_base, ap, sh, attr_index;
    uint64_t allowed = DESC_LEAF_ALLOWED;
    int user_allowed = 1, table_read_only = 0;
    int pxn_table = 0, uxn_table = 0;

    if ((descriptor & ~allowed) != 0ULL) return SYNTRA_STATUS_ERROR;
    if ((descriptor & DESC_VALID) == 0ULL || (descriptor & DESC_TYPE) == 0ULL)
        return SYNTRA_STATUS_ERROR;
    if (level == 1U) page_size = 1ULL << 30U;
    else if (level == 2U) page_size = 1ULL << 21U;
    else if (level == 3U) page_size = SYNTRA_ARM64_MMU_PAGE_SIZE;
    else return SYNTRA_STATUS_ERROR;

    address_mask = DESC_ADDR_MASK & ~(page_size - 1ULL);
    if ((descriptor & DESC_ADDR_MASK & (page_size - 1ULL)) != 0ULL)
        return SYNTRA_STATUS_ERROR;
    output_base = descriptor & address_mask;
    if (output_base >= MMU_PA_LIMIT) return SYNTRA_STATUS_ERROR;

    sh = (descriptor >> DESC_SH_SHIFT) & 3ULL;
    if (sh == 1ULL) return SYNTRA_STATUS_ERROR;
    if ((descriptor & DESC_AF) == 0ULL) return SYNTRA_STATUS_ERROR;

    ap = (descriptor >> DESC_AP_SHIFT) & 3ULL;
    attr_index = (descriptor >> DESC_ATTR_SHIFT) & 7ULL;
    user_allowed = (ap & 1ULL) != 0ULL;
    table_read_only = (ap & 2ULL) != 0ULL;
    if ((inherited & DESC_AP_TABLE_NO_EL0) != 0ULL) user_allowed = 0;
    if ((inherited & DESC_AP_TABLE_RO) != 0ULL) table_read_only = 1;
    pxn_table = (inherited & DESC_PXN_TABLE) != 0ULL;
    uxn_table = (inherited & DESC_UXN_TABLE) != 0ULL;

    translation->physical_address = output_base |
        (virtual_address & (page_size - 1ULL));
    translation->page_descriptor = descriptor;
    translation->level = (uint8_t)level;
    translation->page_size = page_size;
    translation->attribute_index = (uint8_t)attr_index;
    translation->memory_type = attr_index == MMU_ATTR_INDEX_NORMAL
        ? SYNTRA_ARM64_MMU_MEMORY_NORMAL
        : (attr_index == MMU_ATTR_INDEX_DEVICE
            ? SYNTRA_ARM64_MMU_MEMORY_DEVICE
            : SYNTRA_ARM64_MMU_MEMORY_UNKNOWN);
    translation->shareability = (uint8_t)sh;
    translation->access_flag = 1U;
    translation->ap = (uint8_t)ap;
    translation->privileged_read = 1U;
    translation->privileged_write = (uint8_t)(!table_read_only);
    translation->user_read = (uint8_t)user_allowed;
    translation->user_write = (uint8_t)(user_allowed && !table_read_only);
    translation->privileged_execute = (uint8_t)(
        (descriptor & DESC_PXN) == 0ULL && !pxn_table);
    translation->user_execute = (uint8_t)(
        (descriptor & DESC_UXN) == 0ULL && !uxn_table && user_allowed);
    return SYNTRA_STATUS_OK;
}

syntra_status_t syntra_arm64_mmu_walk(
    uint64_t root_table_physical_address, uint64_t virtual_address,
    syntra_arm64_mmu_table_resolver_t resolver, void *context,
    syntra_arm64_mmu_translation_t *translation)
{
    uint16_t indices[4];
    const uint64_t *table;
    uint64_t descriptor, table_address, inherited = 0ULL;
    unsigned int level;

    if (resolver == NULL || translation == NULL ||
        !physical_address_is_valid(root_table_physical_address))
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    if (syntra_arm64_mmu_get_table_indices(virtual_address, indices) != SYNTRA_STATUS_OK)
        return SYNTRA_STATUS_INVALID_ARGUMENT;

    table = resolver(root_table_physical_address, context);
    if (table == NULL) return SYNTRA_STATUS_ERROR;

    for (level = 1U; level <= 3U; ++level)
    {
        descriptor = table[indices[level - 1U]];
        if ((descriptor & DESC_VALID) == 0ULL) return SYNTRA_STATUS_ERROR;

        if (level == 3U)
            return decode_leaf(descriptor, virtual_address, level, inherited, translation);

        if ((descriptor & DESC_TYPE) == 0ULL)
            return decode_leaf(descriptor, virtual_address, level, inherited, translation);

        if ((descriptor & ~DESC_TABLE_ALLOWED) != 0ULL) return SYNTRA_STATUS_ERROR;
        inherited |= descriptor & (DESC_PXN_TABLE | DESC_UXN_TABLE |
                                   DESC_AP_TABLE_RO | DESC_AP_TABLE_NO_EL0);
        table_address = descriptor & DESC_ADDR_MASK;
        if (!physical_address_is_valid(table_address)) return SYNTRA_STATUS_ERROR;
        table = resolver(table_address, context);
        if (table == NULL) return SYNTRA_STATUS_ERROR;
    }
    return SYNTRA_STATUS_ERROR;
}

syntra_status_t syntra_arm64_mmu_read_registers(syntra_arm64_mmu_registers_t *registers)
{
    if (registers == NULL) return SYNTRA_STATUS_INVALID_ARGUMENT;
#if defined(__aarch64__)
    __asm__ volatile("mrs %0, sctlr_el1" : "=r"(registers->sctlr_el1));
    __asm__ volatile("mrs %0, mair_el1" : "=r"(registers->mair_el1));
    __asm__ volatile("mrs %0, tcr_el1" : "=r"(registers->tcr_el1));
    __asm__ volatile("mrs %0, ttbr0_el1" : "=r"(registers->ttbr0_el1));
    __asm__ volatile("mrs %0, id_aa64mmfr0_el1" : "=r"(registers->id_aa64mmfr0_el1));
    return SYNTRA_STATUS_OK;
#else
    registers->sctlr_el1 = 0ULL; registers->mair_el1 = 0ULL;
    registers->tcr_el1 = 0ULL; registers->ttbr0_el1 = 0ULL;
    registers->id_aa64mmfr0_el1 = 0ULL;
    return SYNTRA_STATUS_NOT_IMPLEMENTED;
#endif
}

syntra_status_t syntra_arm64_mmu_prepare(uint64_t root_table_physical_address)
{
#if defined(__aarch64__)
    syntra_arm64_mmu_registers_t registers;
    uint64_t parange, tcr_value;
    if (!physical_address_is_valid(root_table_physical_address)) return SYNTRA_STATUS_INVALID_ARGUMENT;
    if (syntra_arm64_mmu_read_registers(&registers) != SYNTRA_STATUS_OK) return SYNTRA_STATUS_ERROR;
    if ((registers.sctlr_el1 & MMU_SCTLR_M) != 0ULL) return SYNTRA_STATUS_PERMISSION_DENIED;
    parange = registers.id_aa64mmfr0_el1 & 0xFULL;
    if (parange > MMU_ID_AA64MMFR0_PARANGE_48_BITS) parange = MMU_ID_AA64MMFR0_PARANGE_48_BITS;
    tcr_value = MMU_TCR_T0SZ | MMU_TCR_EPD0 | MMU_TCR_IRGN0_WBWA |
        MMU_TCR_ORGN0_WBWA | MMU_TCR_SH0_INNER | MMU_TCR_TG0_4K |
        (parange << MMU_TCR_IPS_SHIFT);
    __asm__ volatile("msr mair_el1, %0" :: "r"(MMU_MAIR_VALUE) : "memory");
    __asm__ volatile("msr tcr_el1, %0" :: "r"(tcr_value) : "memory");
    __asm__ volatile("dsb ish" ::: "memory"); __asm__ volatile("isb" ::: "memory");
    __asm__ volatile("dsb ish" ::: "memory"); __asm__ volatile("isb" ::: "memory");
    __asm__ volatile("msr ttbr0_el1, %0" :: "r"(root_table_physical_address) : "memory");
    __asm__ volatile("dsb ish" ::: "memory"); __asm__ volatile("isb" ::: "memory");
    __asm__ volatile("dsb ish" ::: "memory"); __asm__ volatile("tlbi vmalle1" ::: "memory");
    __asm__ volatile("dsb ish" ::: "memory"); __asm__ volatile("isb" ::: "memory");
    mmu_configured_tcr_value = tcr_value;
    return SYNTRA_STATUS_OK;
#else
    (void)root_table_physical_address; return SYNTRA_STATUS_NOT_IMPLEMENTED;
#endif
}

syntra_status_t syntra_arm64_mmu_enable(void)
{
#if defined(__aarch64__)
    syntra_arm64_mmu_registers_t registers;
    uint64_t sctlr;
    if (syntra_arm64_mmu_read_registers(&registers) != SYNTRA_STATUS_OK) return SYNTRA_STATUS_ERROR;
    if ((registers.sctlr_el1 & MMU_SCTLR_M) != 0ULL) return SYNTRA_STATUS_PERMISSION_DENIED;
    if ((registers.ttbr0_el1 & DESC_ADDR_MASK) == 0ULL ||
        registers.mair_el1 != MMU_MAIR_VALUE || mmu_configured_tcr_value == 0ULL ||
        registers.tcr_el1 != mmu_configured_tcr_value) return SYNTRA_STATUS_INVALID_ARGUMENT;
    __asm__ volatile("dsb ish" ::: "memory"); __asm__ volatile("isb" ::: "memory");
    __asm__ volatile("mrs %0, sctlr_el1" : "=r"(sctlr));
    sctlr |= MMU_SCTLR_M;
    __asm__ volatile("msr sctlr_el1, %0" :: "r"(sctlr) : "memory");
    __asm__ volatile("dsb ish" ::: "memory"); __asm__ volatile("isb" ::: "memory");
    return SYNTRA_STATUS_OK;
#else
    return SYNTRA_STATUS_NOT_IMPLEMENTED;
#endif
}

syntra_status_t syntra_arm64_mmu_validate_mapping_range(
    uint64_t virtual_address, uint64_t physical_address, uint64_t size)
{
    const uint64_t virtual_limit = 1ULL << SYNTRA_ARM64_MMU_VA_BITS;
    const uint64_t physical_limit = 1ULL << SYNTRA_ARM64_MMU_PA_BITS;
    const uint64_t page_mask = SYNTRA_ARM64_MMU_PAGE_SIZE - 1ULL;
    if (size == 0ULL || ((virtual_address | physical_address | size) & page_mask) != 0ULL)
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    if (virtual_address >= virtual_limit || physical_address >= physical_limit ||
        size > virtual_limit - virtual_address || size > physical_limit - physical_address)
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    return SYNTRA_STATUS_OK;
}
