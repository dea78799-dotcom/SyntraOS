#include "mmu_mapping_plan.h"
#include <stdint.h>
#define PAGE_MASK (SYNTRA_ARM64_MMU_PAGE_SIZE - 1ULL)
static int range_end(uint64_t base,uint64_t size,uint64_t *end){if(!size||base>UINT64_MAX-size)return 0;*end=base+size;return 1;}
static uint64_t align_down(uint64_t v){return v&~PAGE_MASK;}
static int align_up(uint64_t v,uint64_t *a){if(v>UINT64_MAX-PAGE_MASK)return 0;*a=(v+PAGE_MASK)&~PAGE_MASK;return 1;}
static syntra_status_t append(syntra_arm64_mmu_mapping_plan_t *p,uint64_t base,uint64_t size,syntra_arm64_mmu_memory_type_t type,syntra_arm64_mmu_access_t access,int exec){
 uint64_t end;size_t i;
 if(!p||!size||syntra_arm64_mmu_validate_mapping_range(base,base,size)!=SYNTRA_STATUS_OK||!range_end(base,size,&end))return SYNTRA_STATUS_INVALID_ARGUMENT;
 for(i=0;i<p->count;i++){uint64_t ee;const syntra_arm64_mmu_mapping_t *m=&p->mappings[i];if(!range_end(m->virtual_address,m->size,&ee))return SYNTRA_STATUS_ERROR;if(base<ee&&m->virtual_address<end)return SYNTRA_STATUS_ERROR;}
 if(p->count>=SYNTRA_ARM64_MMU_MAPPING_PLAN_MAX)return SYNTRA_STATUS_ERROR;
 p->mappings[p->count]=(syntra_arm64_mmu_mapping_t){base,base,size,type,access,(uint8_t)exec};p->count++;return SYNTRA_STATUS_OK;
}
static syntra_status_t append_region(syntra_arm64_mmu_mapping_plan_t *p,uint64_t base,uint64_t size,syntra_arm64_mmu_memory_type_t type,syntra_arm64_mmu_access_t access,int exec,int outward){
 uint64_t end,ae;if(!range_end(base,size,&end))return SYNTRA_STATUS_INVALID_ARGUMENT;
 if(outward){base=align_down(base);if(!align_up(end,&ae))return SYNTRA_STATUS_INVALID_ARGUMENT;end=ae;}
 else {if(!align_up(base,&base))return SYNTRA_STATUS_INVALID_ARGUMENT;end=align_down(end);}
 if(end<=base)return SYNTRA_STATUS_OK;return append(p,base,end-base,type,access,exec);
}
static syntra_status_t append_section(syntra_arm64_mmu_mapping_plan_t *p,uint64_t s,uint64_t e,syntra_arm64_mmu_access_t a,int x,uint64_t ks,uint64_t ke){
 if(s>e||(s&PAGE_MASK)||(e&PAGE_MASK)||s<ks||e>ke)return SYNTRA_STATUS_INVALID_ARGUMENT;
 if(s==e)return SYNTRA_STATUS_OK;return append(p,s,e-s,SYNTRA_ARM64_MMU_MEMORY_NORMAL,a,x);
}
syntra_status_t syntra_arm64_mmu_mapping_plan_build(const syntra_arm64_boot_memory_metadata_t *m,const syntra_memory_region_t *r,size_t n,const syntra_arm64_gic_config_t *g,uintptr_t ub,size_t us,syntra_arm64_mmu_mapping_plan_t *p){
 syntra_status_t st;size_t i;uint64_t end;
 if(!m||!r||!n||!g||!p||!ub||!us||!m->kernel_start||m->kernel_end<=m->kernel_start||!m->dtb_start||!m->dtb_size||
 m->text_end>m->rodata_start||m->rodata_end>m->data_start||m->data_end>m->bss_start||m->bss_end>m->stack_bottom||m->stack_top<=m->stack_bottom)return SYNTRA_STATUS_INVALID_ARGUMENT;
 p->count=0;
 for(i=0;i<n;i++){if(!r[i].size||!range_end(r[i].base,r[i].size,&end))return SYNTRA_STATUS_INVALID_ARGUMENT;
 if(r[i].type==SYNTRA_MEMORY_REGION_RAM||r[i].type==SYNTRA_MEMORY_REGION_RESERVED||r[i].type==SYNTRA_MEMORY_REGION_RECLAIMABLE){st=append_region(p,r[i].base,r[i].size,SYNTRA_ARM64_MMU_MEMORY_NORMAL,SYNTRA_ARM64_MMU_ACCESS_READ_WRITE,0,0);if(st!=SYNTRA_STATUS_OK)return st;}}
 st=append_section(p,m->text_start,m->text_end,SYNTRA_ARM64_MMU_ACCESS_READ_ONLY,1,m->kernel_start,m->kernel_end);if(st!=SYNTRA_STATUS_OK)return st;
 st=append_section(p,m->rodata_start,m->rodata_end,SYNTRA_ARM64_MMU_ACCESS_READ_ONLY,0,m->kernel_start,m->kernel_end);if(st!=SYNTRA_STATUS_OK)return st;
 st=append_section(p,m->data_start,m->data_end,SYNTRA_ARM64_MMU_ACCESS_READ_WRITE,0,m->kernel_start,m->kernel_end);if(st!=SYNTRA_STATUS_OK)return st;
 st=append_section(p,m->bss_start,m->bss_end,SYNTRA_ARM64_MMU_ACCESS_READ_WRITE,0,m->kernel_start,m->kernel_end);if(st!=SYNTRA_STATUS_OK)return st;
 st=append_section(p,m->stack_bottom,m->stack_top,SYNTRA_ARM64_MMU_ACCESS_READ_WRITE,0,m->kernel_start,m->kernel_end);if(st!=SYNTRA_STATUS_OK)return st;
 st=append_region(p,m->dtb_start,m->dtb_size,SYNTRA_ARM64_MMU_MEMORY_NORMAL,SYNTRA_ARM64_MMU_ACCESS_READ_ONLY,0,1);if(st!=SYNTRA_STATUS_OK)return st;
 st=append_region(p,ub,us,SYNTRA_ARM64_MMU_MEMORY_DEVICE,SYNTRA_ARM64_MMU_ACCESS_READ_WRITE,0,1);if(st!=SYNTRA_STATUS_OK)return st;
 if(g->version==SYNTRA_ARM64_GIC_VERSION_3){st=append_region(p,g->distributor_base,g->distributor_size,SYNTRA_ARM64_MMU_MEMORY_DEVICE,SYNTRA_ARM64_MMU_ACCESS_READ_WRITE,0,1);if(st!=SYNTRA_STATUS_OK)return st;st=append_region(p,g->redistributor_base,g->redistributor_size,SYNTRA_ARM64_MMU_MEMORY_DEVICE,SYNTRA_ARM64_MMU_ACCESS_READ_WRITE,0,1);if(st!=SYNTRA_STATUS_OK)return st;}
 else if(g->version==SYNTRA_ARM64_GIC_VERSION_2){st=append_region(p,g->distributor_base,g->distributor_size,SYNTRA_ARM64_MMU_MEMORY_DEVICE,SYNTRA_ARM64_MMU_ACCESS_READ_WRITE,0,1);if(st!=SYNTRA_STATUS_OK)return st;st=append_region(p,g->cpu_interface_base,g->cpu_interface_size,SYNTRA_ARM64_MMU_MEMORY_DEVICE,SYNTRA_ARM64_MMU_ACCESS_READ_WRITE,0,1);}
 else return SYNTRA_STATUS_INVALID_ARGUMENT;
 return SYNTRA_STATUS_OK;
}
