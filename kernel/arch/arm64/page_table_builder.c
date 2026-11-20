#include "page_table_builder.h"

#include <stddef.h>

#define DESC_VALID (1ULL << 0U)
#define DESC_TYPE  (1ULL << 1U)
#define DESC_ADDR  SYNTRA_ARM64_MMU_PA_MASK
#define PA_LIMIT   (1ULL << SYNTRA_ARM64_MMU_PA_BITS)

static int pa_valid(uint64_t pa)
{
    return pa != 0ULL &&
        (pa & (SYNTRA_ARM64_MMU_PAGE_SIZE - 1ULL)) == 0ULL &&
        pa < PA_LIMIT;
}

static uint64_t *get_table(
    syntra_arm64_page_table_t *builder, uint64_t pa)
{
    if (!pa_valid(pa) || builder->resolve == NULL) return NULL;
    return builder->resolve(pa, builder->context);
}

static const uint64_t *walk_resolve(uint64_t pa, void *context)
{
    syntra_arm64_page_table_t *builder =
        (syntra_arm64_page_table_t *)context;
    return get_table(builder, pa);
}

static syntra_status_t allocate_table(
    syntra_arm64_page_table_t *builder,
    uint64_t *physical,
    uint64_t **virtual_table)
{
    syntra_status_t status;
    uint64_t *memory;

    if (builder->allocate == NULL || physical == NULL ||
        virtual_table == NULL)
        return SYNTRA_STATUS_INVALID_ARGUMENT;

    status = builder->allocate(builder->context, physical);
    if (status != SYNTRA_STATUS_OK) return status;
    if (!pa_valid(*physical)) return SYNTRA_STATUS_ERROR;

    memory = get_table(builder, *physical);
    if (memory == NULL) return SYNTRA_STATUS_ERROR;

    for (size_t i = 0U; i < SYNTRA_ARM64_MMU_TABLE_ENTRIES; ++i)
        memory[i] = 0ULL;

    *virtual_table = memory;
    return SYNTRA_STATUS_OK;
}

syntra_status_t syntra_arm64_page_table_init(
    syntra_arm64_page_table_t *table,
    syntra_arm64_pt_alloc_fn allocate,
    syntra_arm64_pt_resolve_fn resolve,
    void *context)
{
    uint64_t root_pa;
    uint64_t *root;
    syntra_status_t status;

    if (table == NULL || allocate == NULL || resolve == NULL)
        return SYNTRA_STATUS_INVALID_ARGUMENT;

    table->allocate = allocate;
    table->resolve = resolve;
    table->context = context;
    table->root_physical_address = 0ULL;

    status = allocate_table(table, &root_pa, &root);
    if (status != SYNTRA_STATUS_OK) return status;

    table->root_physical_address = root_pa;
    return SYNTRA_STATUS_OK;
}

static syntra_status_t ensure_child(
    syntra_arm64_page_table_t *builder,
    uint64_t *parent,
    unsigned int index,
    uint64_t **child)
{
    uint64_t descriptor = parent[index];
    uint64_t child_pa;
    syntra_status_t status;

    if ((descriptor & DESC_VALID) != 0ULL)
    {
        if ((descriptor & DESC_TYPE) == 0ULL ||
            (descriptor & ~(DESC_ADDR | 3ULL)) != 0ULL)
            return SYNTRA_STATUS_ERROR;

        child_pa = descriptor & DESC_ADDR;
        *child = get_table(builder, child_pa);
        return *child == NULL ? SYNTRA_STATUS_ERROR : SYNTRA_STATUS_OK;
    }

    status = allocate_table(builder, &child_pa, child);
    if (status != SYNTRA_STATUS_OK) return status;

    status = syntra_arm64_mmu_make_table_descriptor(child_pa, &descriptor);
    if (status != SYNTRA_STATUS_OK) return status;

    parent[index] = descriptor;
    return SYNTRA_STATUS_OK;
}

syntra_status_t syntra_arm64_page_table_map(
    syntra_arm64_page_table_t *table,
    uint64_t va,
    uint64_t pa,
    uint64_t size,
    syntra_arm64_mmu_memory_type_t type,
    syntra_arm64_mmu_access_t access,
    int executable)
{
    uint64_t end;

    if (table == NULL || table->root_physical_address == 0ULL ||
        table->allocate == NULL || table->resolve == NULL ||
        syntra_arm64_mmu_validate_mapping_range(va, pa, size) !=
            SYNTRA_STATUS_OK)
        return SYNTRA_STATUS_INVALID_ARGUMENT;

    end = va + size;
    while (va < end)
    {
        uint16_t indices[4];
        uint64_t *l1, *l2, *l3, descriptor;
        syntra_status_t status;

        status = syntra_arm64_mmu_make_page_descriptor(
            pa, type, access, executable, &descriptor);
        if (status != SYNTRA_STATUS_OK) return status;

        status = syntra_arm64_mmu_get_table_indices(va, indices);
        if (status != SYNTRA_STATUS_OK) return status;

        l1 = get_table(table, table->root_physical_address);
        if (l1 == NULL) return SYNTRA_STATUS_ERROR;

        status = ensure_child(table, l1, indices[0], &l2);
        if (status != SYNTRA_STATUS_OK) return status;
        status = ensure_child(table, l2, indices[1], &l3);
        if (status != SYNTRA_STATUS_OK) return status;

        if ((l3[indices[2]] & DESC_VALID) != 0ULL)
            return SYNTRA_STATUS_ERROR;

        l3[indices[2]] = descriptor;
        va += SYNTRA_ARM64_MMU_PAGE_SIZE;
        pa += SYNTRA_ARM64_MMU_PAGE_SIZE;
    }

    return SYNTRA_STATUS_OK;
}

syntra_status_t syntra_arm64_page_table_verify(
    syntra_arm64_page_table_t *table,
    uint64_t va,
    uint64_t pa,
    uint64_t size,
    syntra_arm64_mmu_memory_type_t type,
    syntra_arm64_mmu_access_t access,
    int executable)
{
    uint64_t end;

    if (table == NULL || table->root_physical_address == 0ULL ||
        table->resolve == NULL ||
        syntra_arm64_mmu_validate_mapping_range(va, pa, size) !=
            SYNTRA_STATUS_OK)
        return SYNTRA_STATUS_INVALID_ARGUMENT;

    end = va + size;
    while (va < end)
    {
        syntra_arm64_mmu_translation_t translation;

        if (syntra_arm64_mmu_walk(
                table->root_physical_address, va, walk_resolve,
                table, &translation) != SYNTRA_STATUS_OK)
            return SYNTRA_STATUS_ERROR;

        if (translation.physical_address != pa ||
            translation.page_size != SYNTRA_ARM64_MMU_PAGE_SIZE ||
            translation.memory_type != (uint8_t)type ||
            translation.privileged_read == 0U ||
            translation.privileged_write != (uint8_t)(
                access == SYNTRA_ARM64_MMU_ACCESS_READ_WRITE) ||
            translation.privileged_execute != (uint8_t)(executable != 0) ||
            translation.user_read != 0U ||
            translation.user_write != 0U ||
            translation.user_execute != 0U)
            return SYNTRA_STATUS_ERROR;

        va += SYNTRA_ARM64_MMU_PAGE_SIZE;
        pa += SYNTRA_ARM64_MMU_PAGE_SIZE;
    }

    return SYNTRA_STATUS_OK;
}
