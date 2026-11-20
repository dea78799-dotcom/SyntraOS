#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "../../kernel/arch/arm64/mmu.h"

#define ROOT_PA 0x1000ULL
#define L2_PA 0x2000ULL
#define L3_PA 0x3000ULL
#define VA 0x0000000012345123ULL
#define DESC_VALID (1ULL << 0U)
#define DESC_TYPE (1ULL << 1U)
#define DESC_AF (1ULL << 10U)
#define DESC_PXN (1ULL << 53U)
#define DESC_UXN (1ULL << 54U)
#define DESC_AP_SHIFT 6U
#define DESC_SH_SHIFT 8U
#define DESC_ATTR_SHIFT 2U

static uint64_t root[512], l2[512], l3[512];
static const uint64_t *resolve(uint64_t pa, void *ctx)
{
    (void)ctx;
    if (pa == ROOT_PA) return root;
    if (pa == L2_PA) return l2;
    if (pa == L3_PA) return l3;
    return NULL;
}
static void reset(void)
{
    unsigned int i;
    for (i=0; i<512U; ++i) root[i]=l2[i]=l3[i]=0ULL;
}
static void setup_page(uint64_t descriptor)
{
    uint16_t ix[4];
    assert(syntra_arm64_mmu_get_table_indices(VA, ix)==SYNTRA_STATUS_OK);
    root[ix[0]]=L2_PA|DESC_VALID|DESC_TYPE;
    l2[ix[1]]=L3_PA|DESC_VALID|DESC_TYPE;
    l3[ix[2]]=descriptor;
}
static void test_l1_l2_l3_indices_and_page(void)
{
    uint16_t ix[4];
    uint64_t d;
    syntra_arm64_mmu_translation_t out;
    assert(syntra_arm64_mmu_get_table_indices(VA,ix)==SYNTRA_STATUS_OK);
    assert(ix[0]==((VA>>30U)&0x1FFU));
    assert(ix[1]==((VA>>21U)&0x1FFU));
    assert(ix[2]==((VA>>12U)&0x1FFU));
    assert(ix[3]==(VA&0xFFFU));
    assert(syntra_arm64_mmu_make_page_descriptor(0x40000000ULL,
        SYNTRA_ARM64_MMU_MEMORY_NORMAL,SYNTRA_ARM64_MMU_ACCESS_READ_WRITE,1,&d)==SYNTRA_STATUS_OK);
    reset(); setup_page(d);
    assert(syntra_arm64_mmu_walk(ROOT_PA,VA,resolve,NULL,&out)==SYNTRA_STATUS_OK);
    assert(out.physical_address==0x40000123ULL);
    assert(out.page_size==4096ULL && out.level==3U);
    assert(out.memory_type==SYNTRA_ARM64_MMU_MEMORY_NORMAL);
    assert(out.privileged_read && out.privileged_write);
    assert(!out.user_read && !out.user_write);
    assert(out.privileged_execute && !out.user_execute);
    assert(out.shareability==3U && out.access_flag);
}
static void test_missing_mapping_and_reserved_bits(void)
{
    syntra_arm64_mmu_translation_t out;
    uint64_t d;
    uint16_t ix[4];
    reset();
    assert(syntra_arm64_mmu_walk(ROOT_PA,VA,resolve,NULL,&out)==SYNTRA_STATUS_ERROR);
    assert(syntra_arm64_mmu_get_table_indices(VA,ix)==SYNTRA_STATUS_OK);
    root[ix[0]]=L2_PA|DESC_VALID|DESC_TYPE;
    l2[ix[1]]=L3_PA|DESC_VALID|DESC_TYPE;
    l3[ix[2]]=0ULL; /* Valid bit clear: mapping absent. */
    assert(syntra_arm64_mmu_walk(ROOT_PA,VA,resolve,NULL,&out)==SYNTRA_STATUS_ERROR);
    assert(syntra_arm64_mmu_make_page_descriptor(0x40000000ULL,
        SYNTRA_ARM64_MMU_MEMORY_NORMAL,SYNTRA_ARM64_MMU_ACCESS_READ_WRITE,0,&d)==SYNTRA_STATUS_OK);
    setup_page(d | (1ULL<<59U)); /* Reserved high descriptor bit. */
    assert(syntra_arm64_mmu_walk(ROOT_PA,VA,resolve,NULL,&out)==SYNTRA_STATUS_ERROR);
    setup_page(d & ~DESC_AF); /* AF=0 must be rejected. */
    assert(syntra_arm64_mmu_walk(ROOT_PA,VA,resolve,NULL,&out)==SYNTRA_STATUS_ERROR);
}
static void test_l1_one_gib_block(void)
{
    syntra_arm64_mmu_translation_t out;
    uint16_t ix[4];
    const uint64_t va=0x0000000045678123ULL;
    reset();
    assert(syntra_arm64_mmu_get_table_indices(va,ix)==SYNTRA_STATUS_OK);
    root[ix[0]]=0x80000000ULL|DESC_VALID|DESC_AF| (3ULL<<DESC_SH_SHIFT);
    assert(syntra_arm64_mmu_walk(ROOT_PA,va,resolve,NULL,&out)==SYNTRA_STATUS_OK);
    assert(out.level==1U && out.page_size==(1ULL<<30U));
    assert(out.physical_address==0x845678123ULL);
}
static void test_l2_two_mib_block(void)
{
    syntra_arm64_mmu_translation_t out;
    uint16_t ix[4];
    const uint64_t va=0x0000000012345123ULL;
    reset();
    assert(syntra_arm64_mmu_get_table_indices(va,ix)==SYNTRA_STATUS_OK);
    root[ix[0]]=L2_PA|DESC_VALID|DESC_TYPE;
    l2[ix[1]]=0x60000000ULL|DESC_VALID|DESC_AF| (1ULL<<DESC_ATTR_SHIFT) |
        (2ULL<<DESC_SH_SHIFT) | DESC_PXN | DESC_UXN;
    assert(syntra_arm64_mmu_walk(ROOT_PA,va,resolve,NULL,&out)==SYNTRA_STATUS_OK);
    assert(out.level==2U && out.page_size==(1ULL<<21U));
    assert(out.physical_address==0x60045123ULL);
    assert(out.memory_type==SYNTRA_ARM64_MMU_MEMORY_DEVICE);
    assert(!out.privileged_execute && !out.user_execute);
}
static void test_ap_pxn_uxn_shareability_and_table_restrictions(void)
{
    syntra_arm64_mmu_translation_t out;
    uint64_t d;
    uint16_t ix[4];
    reset();
    assert(syntra_arm64_mmu_make_page_descriptor(0x40000000ULL,
        SYNTRA_ARM64_MMU_MEMORY_NORMAL,SYNTRA_ARM64_MMU_ACCESS_READ_ONLY,0,&d)==SYNTRA_STATUS_OK);
    setup_page(d | (3ULL<<DESC_AP_SHIFT) | (1ULL<<DESC_UXN));
    assert(syntra_arm64_mmu_walk(ROOT_PA,VA,resolve,NULL,&out)==SYNTRA_STATUS_OK);
    assert(out.privileged_read && !out.privileged_write);
    assert(out.user_read && !out.user_write);
    assert(!out.privileged_execute && !out.user_execute);
    reset();
    assert(syntra_arm64_mmu_get_table_indices(VA,ix)==SYNTRA_STATUS_OK);
    root[ix[0]]=L2_PA|DESC_VALID|DESC_TYPE|(1ULL<<62U)|(1ULL<<59U);
    l2[ix[1]]=L3_PA|DESC_VALID|DESC_TYPE;
    l3[ix[2]]=0x40000000ULL|DESC_VALID|DESC_TYPE|DESC_AF|(3ULL<<DESC_SH_SHIFT);
    assert(syntra_arm64_mmu_walk(ROOT_PA,VA,resolve,NULL,&out)==SYNTRA_STATUS_OK);
    assert(!out.user_read && !out.user_write && !out.privileged_execute);
    assert(out.shareability==3U);
    l3[ix[2]] &= ~(3ULL<<DESC_SH_SHIFT);
    l3[ix[2]] |= (1ULL<<DESC_SH_SHIFT); /* Reserved SH encoding. */
    assert(syntra_arm64_mmu_walk(ROOT_PA,VA,resolve,NULL,&out)==SYNTRA_STATUS_ERROR);
}
static void test_invalid_arguments_and_host_registers(void)
{
    syntra_arm64_mmu_translation_t out;
    syntra_arm64_mmu_registers_t regs;
    assert(syntra_arm64_mmu_walk(ROOT_PA,VA,NULL,NULL,&out)==SYNTRA_STATUS_INVALID_ARGUMENT);
    assert(syntra_arm64_mmu_walk(ROOT_PA+1ULL,VA,resolve,NULL,&out)==SYNTRA_STATUS_INVALID_ARGUMENT);
    assert(syntra_arm64_mmu_read_registers(&regs)==SYNTRA_STATUS_NOT_IMPLEMENTED);
    assert(syntra_arm64_mmu_read_registers(NULL)==SYNTRA_STATUS_INVALID_ARGUMENT);
    assert(syntra_arm64_mmu_prepare(ROOT_PA)==SYNTRA_STATUS_NOT_IMPLEMENTED);
    assert(syntra_arm64_mmu_enable()==SYNTRA_STATUS_NOT_IMPLEMENTED);
}
int main(void)
{
    test_l1_l2_l3_indices_and_page();
    test_missing_mapping_and_reserved_bits();
    test_l1_one_gib_block();
    test_l2_two_mib_block();
    test_ap_pxn_uxn_shareability_and_table_restrictions();
    test_invalid_arguments_and_host_registers();
    puts("ARM64 MMU software walker tests passed");
    return 0;
}
