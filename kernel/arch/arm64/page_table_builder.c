#include "page_table_builder.h"

#include <stddef.h>

#define DESC_VALID (1ULL << 0U)
#define DESC_TYPE  (1ULL << 1U)
#define DESC_ADDR  SYNTRA_ARM64_MMU_PA_MASK
#define PA_LIMIT   (1ULL << SYNTRA_ARM64_MMU_PA_BITS)

#define SYNTRA_PT_TXN_MAX_NEW_TABLES 64U

typedef struct
{
    uint64_t *parent;
    unsigned int index;
    uint64_t child_pa;
    uint64_t descriptor;
} syntra_pt_txn_entry_t;

typedef struct
{
    syntra_pt_txn_entry_t entries[SYNTRA_PT_TXN_MAX_NEW_TABLES];
    size_t count;
} syntra_pt_txn_t;

static void rollback_transaction(
    syntra_arm64_page_table_t *builder, syntra_pt_txn_t *transaction)
{
    while (transaction->count != 0U)
    {
        syntra_pt_txn_entry_t *entry =
            &transaction->entries[transaction->count - 1U];

        /*
         * Without a release hook, keep the hierarchy attached so its owner
         * can discover and reclaim every allocated page.
         */
        if (builder->release == NULL)
        {
            --transaction->count;
            continue;
        }

        /*
         * Release can fail (for example, an ownership mismatch). In that
         * case restore the descriptor and stop: its ancestors must remain
         * attached too, otherwise the still-live child becomes unreachable.
         */
        if (entry->parent[entry->index] != entry->descriptor)
            return;

        entry->parent[entry->index] = 0ULL;
        if (builder->release(builder->context, entry->child_pa) !=
            SYNTRA_STATUS_OK)
        {
            entry->parent[entry->index] = entry->descriptor;
            return;
        }

        --transaction->count;
    }
}


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
    if (!pa_valid(*physical))
    {
        /*
         * The allocator reported success, so give its paired release hook a
         * chance to undo the allocation. The production owner-aware allocator
         * independently validates address alignment and ownership.
         */
        if (builder->release != NULL)
            (void)builder->release(builder->context, *physical);
        return SYNTRA_STATUS_ERROR;
    }

    memory = get_table(builder, *physical);
    if (memory == NULL)
    {
        if (builder->release != NULL &&
            builder->release(builder->context, *physical) != SYNTRA_STATUS_OK)
            return SYNTRA_STATUS_ERROR;
        return SYNTRA_STATUS_ERROR;
    }

    for (size_t i = 0U; i < SYNTRA_ARM64_MMU_TABLE_ENTRIES; ++i)
        memory[i] = 0ULL;

    *virtual_table = memory;
    return SYNTRA_STATUS_OK;
}

syntra_status_t syntra_arm64_page_table_init_ex(
    syntra_arm64_page_table_t *table,
    syntra_arm64_pt_alloc_fn allocate,
    syntra_arm64_pt_release_fn release,
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
    table->release = release;
    table->context = context;
    table->root_physical_address = 0ULL;

    status = allocate_table(table, &root_pa, &root);
    if (status != SYNTRA_STATUS_OK) return status;

    table->root_physical_address = root_pa;
    return SYNTRA_STATUS_OK;
}

syntra_status_t syntra_arm64_page_table_init(
    syntra_arm64_page_table_t *table,
    syntra_arm64_pt_alloc_fn allocate,
    syntra_arm64_pt_resolve_fn resolve,
    void *context)
{
    return syntra_arm64_page_table_init_ex(
        table, allocate, NULL, resolve, context);
}

static syntra_status_t ensure_child(
    syntra_arm64_page_table_t *builder,
    uint64_t *parent,
    unsigned int index,
    uint64_t **child,
    syntra_pt_txn_t *transaction)
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

    if (transaction == NULL ||
        transaction->count >= SYNTRA_PT_TXN_MAX_NEW_TABLES)
        return SYNTRA_STATUS_OUT_OF_MEMORY;

    status = allocate_table(builder, &child_pa, child);
    if (status != SYNTRA_STATUS_OK) return status;

    status = syntra_arm64_mmu_make_table_descriptor(child_pa, &descriptor);
    if (status != SYNTRA_STATUS_OK)
    {
        if (builder->release != NULL &&
            builder->release(builder->context, child_pa) != SYNTRA_STATUS_OK)
            return SYNTRA_STATUS_ERROR;
        *child = NULL;
        return status;
    }

    transaction->entries[transaction->count].parent = parent;
    transaction->entries[transaction->count].index = index;
    transaction->entries[transaction->count].child_pa = child_pa;
    transaction->entries[transaction->count].descriptor = descriptor;
    ++transaction->count;
    parent[index] = descriptor;
    return SYNTRA_STATUS_OK;
}


/* Preflight a 2-MiB L2 slot; any existing block or table is a conflict. */
static syntra_status_t block_slot_is_unmapped(
    syntra_arm64_page_table_t *builder, uint64_t va)
{
    uint16_t indices[4];
    uint64_t *l1, *l2;
    uint64_t descriptor;
    if (syntra_arm64_mmu_get_table_indices(va, indices) != SYNTRA_STATUS_OK)
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    l1 = get_table(builder, builder->root_physical_address);
    if (l1 == NULL) return SYNTRA_STATUS_ERROR;
    descriptor = l1[indices[0]];
    if ((descriptor & DESC_VALID) == 0ULL) return SYNTRA_STATUS_OK;
    if ((descriptor & DESC_TYPE) == 0ULL ||
        (descriptor & ~(DESC_ADDR | 3ULL)) != 0ULL)
        return SYNTRA_STATUS_ERROR;
    l2 = get_table(builder, descriptor & DESC_ADDR);
    if (l2 == NULL) return SYNTRA_STATUS_ERROR;
    return (l2[indices[1]] & DESC_VALID) == 0ULL
        ? SYNTRA_STATUS_OK : SYNTRA_STATUS_ERROR;
}

/* Preflight existing hierarchy so overlaps never cause partial range maps. */
static syntra_status_t page_is_unmapped(
    syntra_arm64_page_table_t *builder, uint64_t va)
{
    uint16_t indices[4];
    uint64_t *l1, *l2, *l3;
    uint64_t descriptor;

    if (syntra_arm64_mmu_get_table_indices(va, indices) != SYNTRA_STATUS_OK)
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    l1 = get_table(builder, builder->root_physical_address);
    if (l1 == NULL) return SYNTRA_STATUS_ERROR;
    descriptor = l1[indices[0]];
    if ((descriptor & DESC_VALID) == 0ULL) return SYNTRA_STATUS_OK;
    if ((descriptor & DESC_TYPE) == 0ULL ||
        (descriptor & ~(DESC_ADDR | 3ULL)) != 0ULL)
        return SYNTRA_STATUS_ERROR;
    l2 = get_table(builder, descriptor & DESC_ADDR);
    if (l2 == NULL) return SYNTRA_STATUS_ERROR;

    descriptor = l2[indices[1]];
    if ((descriptor & DESC_VALID) == 0ULL) return SYNTRA_STATUS_OK;
    if ((descriptor & DESC_TYPE) == 0ULL ||
        (descriptor & ~(DESC_ADDR | 3ULL)) != 0ULL)
        return SYNTRA_STATUS_ERROR;
    l3 = get_table(builder, descriptor & DESC_ADDR);
    if (l3 == NULL) return SYNTRA_STATUS_ERROR;

    return (l3[indices[2]] & DESC_VALID) == 0ULL
        ? SYNTRA_STATUS_OK : SYNTRA_STATUS_ERROR;
}

/*
 * Phase one: allocate every intermediate table needed by the range without
 * publishing any leaf mapping. If allocation fails, the caller can roll back
 * the complete set of newly attached tables while no requested page is mapped.
 */
static syntra_status_t prepare_mapping_tables(
    syntra_arm64_page_table_t *builder,
    uint64_t va,
    uint64_t pa,
    uint64_t size,
    int page_only,
    syntra_pt_txn_t *transaction)
{
    uint64_t end = va + size;

    while (va < end)
    {
        uint16_t indices[4];
        uint64_t *l1, *l2, *l3;
        syntra_status_t status;
        uint64_t remaining = end - va;
        int use_block = page_only == 0 &&
            (va & (SYNTRA_ARM64_MMU_BLOCK_SIZE - 1ULL)) == 0ULL &&
            (pa & (SYNTRA_ARM64_MMU_BLOCK_SIZE - 1ULL)) == 0ULL &&
            remaining >= SYNTRA_ARM64_MMU_BLOCK_SIZE;

        status = syntra_arm64_mmu_get_table_indices(va, indices);
        if (status != SYNTRA_STATUS_OK)
            return status;

        l1 = get_table(builder, builder->root_physical_address);
        if (l1 == NULL)
            return SYNTRA_STATUS_ERROR;

        status = ensure_child(builder, l1, indices[0], &l2, transaction);
        if (status != SYNTRA_STATUS_OK)
            return status;

        if (!use_block)
        {
            status = ensure_child(builder, l2, indices[1], &l3, transaction);
            if (status != SYNTRA_STATUS_OK)
                return status;
            (void)l3;
            va += SYNTRA_ARM64_MMU_PAGE_SIZE;
            pa += SYNTRA_ARM64_MMU_PAGE_SIZE;
        }
        else
        {
            va += SYNTRA_ARM64_MMU_BLOCK_SIZE;
            pa += SYNTRA_ARM64_MMU_BLOCK_SIZE;
        }
    }

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
    syntra_pt_txn_t transaction;

    transaction.count = 0U;
    /*
     * Validate every descriptor attribute before allocating intermediate
     * tables. Descriptor constructors repeat these checks at the leaf level,
     * but doing them here prevents invalid requests from leaving empty tables
     * behind in an otherwise unchanged address space.
     */
    if ((type != SYNTRA_ARM64_MMU_MEMORY_NORMAL &&
         type != SYNTRA_ARM64_MMU_MEMORY_DEVICE) ||
        (access != SYNTRA_ARM64_MMU_ACCESS_READ_WRITE &&
         access != SYNTRA_ARM64_MMU_ACCESS_READ_ONLY) ||
        (executable != 0 && executable != 1) ||
        (type == SYNTRA_ARM64_MMU_MEMORY_DEVICE && executable != 0))
        return SYNTRA_STATUS_INVALID_ARGUMENT;

    if (table == NULL || table->root_physical_address == 0ULL ||
        table->allocate == NULL || table->resolve == NULL ||
        syntra_arm64_mmu_validate_mapping_range(va, pa, size) !=
            SYNTRA_STATUS_OK)
        return SYNTRA_STATUS_INVALID_ARGUMENT;

    end = va + size;
    /* Preflight whole range, using 2-MiB slot checks for block chunks. */
    {
        uint64_t check_va = va, check_pa = pa;
        while (check_va < end)
        {
            syntra_status_t check_status;
            uint64_t remaining = end - check_va;
            if ((check_va & (SYNTRA_ARM64_MMU_BLOCK_SIZE - 1ULL)) == 0ULL &&
                (check_pa & (SYNTRA_ARM64_MMU_BLOCK_SIZE - 1ULL)) == 0ULL &&
                remaining >= SYNTRA_ARM64_MMU_BLOCK_SIZE)
            {
                check_status = block_slot_is_unmapped(table, check_va);
                if (check_status != SYNTRA_STATUS_OK) return check_status;
                check_va += SYNTRA_ARM64_MMU_BLOCK_SIZE;
                check_pa += SYNTRA_ARM64_MMU_BLOCK_SIZE;
            }
            else
            {
                check_status = page_is_unmapped(table, check_va);
                if (check_status != SYNTRA_STATUS_OK) return check_status;
                check_va += SYNTRA_ARM64_MMU_PAGE_SIZE;
                check_pa += SYNTRA_ARM64_MMU_PAGE_SIZE;
            }
        }
    }
    {
        syntra_status_t prepare_status = prepare_mapping_tables(
            table, va, pa, size, 0, &transaction);
        if (prepare_status != SYNTRA_STATUS_OK)
        {
            rollback_transaction(table, &transaction);
            return prepare_status;
        }
    }

    while (va < end)
    {
        uint16_t indices[4];
        uint64_t *l1, *l2, *l3, descriptor;
        syntra_status_t status;
        uint64_t remaining = end - va;

        status = syntra_arm64_mmu_get_table_indices(va, indices);
        if (status != SYNTRA_STATUS_OK)
        {
            rollback_transaction(table, &transaction);
            return status;
        }
        l1 = get_table(table, table->root_physical_address);
        if (l1 == NULL)
        {
            rollback_transaction(table, &transaction);
            return SYNTRA_STATUS_ERROR;
        }
        status = ensure_child(table, l1, indices[0], &l2, &transaction);
        if (status != SYNTRA_STATUS_OK)
        {
            rollback_transaction(table, &transaction);
            return status;
        }

        if ((va & (SYNTRA_ARM64_MMU_BLOCK_SIZE - 1ULL)) == 0ULL &&
            (pa & (SYNTRA_ARM64_MMU_BLOCK_SIZE - 1ULL)) == 0ULL &&
            remaining >= SYNTRA_ARM64_MMU_BLOCK_SIZE)
        {
            if ((l2[indices[1]] & DESC_VALID) != 0ULL)
            {
                rollback_transaction(table, &transaction);
                return SYNTRA_STATUS_ERROR;
            }
            status = syntra_arm64_mmu_make_block_descriptor(
                pa, type, access, executable, &descriptor);
            if (status != SYNTRA_STATUS_OK)
            {
                rollback_transaction(table, &transaction);
                return status;
            }
            l2[indices[1]] = descriptor;
            va += SYNTRA_ARM64_MMU_BLOCK_SIZE;
            pa += SYNTRA_ARM64_MMU_BLOCK_SIZE;
        }
        else
        {
            status = ensure_child(table, l2, indices[1], &l3, &transaction);
            if (status != SYNTRA_STATUS_OK)
            {
                rollback_transaction(table, &transaction);
                return status;
            }
            status = syntra_arm64_mmu_make_page_descriptor(
                pa, type, access, executable, &descriptor);
            if (status != SYNTRA_STATUS_OK)
            {
                rollback_transaction(table, &transaction);
                return status;
            }
            if ((l3[indices[2]] & DESC_VALID) != 0ULL)
            {
                rollback_transaction(table, &transaction);
                return SYNTRA_STATUS_ERROR;
            }
            l3[indices[2]] = descriptor;
            va += SYNTRA_ARM64_MMU_PAGE_SIZE;
            pa += SYNTRA_ARM64_MMU_PAGE_SIZE;
        }
    }

    return SYNTRA_STATUS_OK;
}


syntra_status_t syntra_arm64_page_table_map_user(
    syntra_arm64_page_table_t *table,
    uint64_t va, uint64_t pa, uint64_t size,
    syntra_arm64_mmu_access_t access, int executable)
{
    uint64_t end;
    syntra_pt_txn_t transaction;

    transaction.count = 0U;
    if (table == NULL || table->root_physical_address == 0ULL ||
        table->allocate == NULL || table->resolve == NULL ||
        (access != SYNTRA_ARM64_MMU_ACCESS_READ_WRITE &&
         access != SYNTRA_ARM64_MMU_ACCESS_READ_ONLY) ||
        (executable != 0 && executable != 1) ||
        syntra_arm64_mmu_validate_mapping_range(va, pa, size) !=
            SYNTRA_STATUS_OK)
        return SYNTRA_STATUS_INVALID_ARGUMENT;

    end = va + size;
    /* User mappings are deliberately page-only; preflight prevents overlaps. */
    for (uint64_t check_va = va; check_va < end;
         check_va += SYNTRA_ARM64_MMU_PAGE_SIZE)
    {
        syntra_status_t status = page_is_unmapped(table, check_va);
        if (status != SYNTRA_STATUS_OK) return status;
    }

    {
        syntra_status_t prepare_status = prepare_mapping_tables(
            table, va, pa, size, 1, &transaction);
        if (prepare_status != SYNTRA_STATUS_OK)
        {
            rollback_transaction(table, &transaction);
            return prepare_status;
        }
    }

    while (va < end)
    {
        uint16_t indices[4];
        uint64_t *l1, *l2, *l3, descriptor;
        syntra_status_t status = syntra_arm64_mmu_get_table_indices(va, indices);
        if (status != SYNTRA_STATUS_OK)
        {
            rollback_transaction(table, &transaction);
            return status;
        }
        l1 = get_table(table, table->root_physical_address);
        if (l1 == NULL)
        {
            rollback_transaction(table, &transaction);
            return SYNTRA_STATUS_ERROR;
        }
        status = ensure_child(table, l1, indices[0], &l2, &transaction);
        if (status != SYNTRA_STATUS_OK)
        {
            rollback_transaction(table, &transaction);
            return status;
        }
        status = ensure_child(table, l2, indices[1], &l3, &transaction);
        if (status != SYNTRA_STATUS_OK)
        {
            rollback_transaction(table, &transaction);
            return status;
        }
        status = syntra_arm64_mmu_make_user_page_descriptor(
            pa, access, executable, &descriptor);
        if (status != SYNTRA_STATUS_OK)
        {
            rollback_transaction(table, &transaction);
            return status;
        }
        if ((l3[indices[2]] & DESC_VALID) != 0ULL)
        {
            rollback_transaction(table, &transaction);
            return SYNTRA_STATUS_ERROR;
        }
        l3[indices[2]] = descriptor;
        va += SYNTRA_ARM64_MMU_PAGE_SIZE;
        pa += SYNTRA_ARM64_MMU_PAGE_SIZE;
    }
    return SYNTRA_STATUS_OK;
}

syntra_status_t syntra_arm64_page_table_verify_user(
    syntra_arm64_page_table_t *table,
    uint64_t va, uint64_t pa, uint64_t size,
    syntra_arm64_mmu_access_t access, int executable)
{
    uint64_t end;
    if (table == NULL || table->root_physical_address == 0ULL ||
        table->resolve == NULL ||
        (access != SYNTRA_ARM64_MMU_ACCESS_READ_WRITE &&
         access != SYNTRA_ARM64_MMU_ACCESS_READ_ONLY) ||
        (executable != 0 && executable != 1) ||
        syntra_arm64_mmu_validate_mapping_range(va, pa, size) !=
            SYNTRA_STATUS_OK)
        return SYNTRA_STATUS_INVALID_ARGUMENT;

    end = va + size;
    while (va < end)
    {
        syntra_arm64_mmu_translation_t translation;
        if (syntra_arm64_mmu_walk(table->root_physical_address, va,
                walk_resolve, table, &translation) != SYNTRA_STATUS_OK)
            return SYNTRA_STATUS_ERROR;
        if (translation.physical_address != pa ||
            translation.page_size != SYNTRA_ARM64_MMU_PAGE_SIZE ||
            translation.memory_type != SYNTRA_ARM64_MMU_MEMORY_NORMAL ||
            translation.privileged_read == 0U ||
            translation.privileged_write != (uint8_t)(
                access == SYNTRA_ARM64_MMU_ACCESS_READ_WRITE) ||
            translation.privileged_execute != 0U ||
            translation.user_read != 1U ||
            translation.user_write != (uint8_t)(
                access == SYNTRA_ARM64_MMU_ACCESS_READ_WRITE) ||
            translation.user_execute != (uint8_t)(executable != 0))
            return SYNTRA_STATUS_ERROR;
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
        (type != SYNTRA_ARM64_MMU_MEMORY_NORMAL &&
         type != SYNTRA_ARM64_MMU_MEMORY_DEVICE) ||
        (access != SYNTRA_ARM64_MMU_ACCESS_READ_WRITE &&
         access != SYNTRA_ARM64_MMU_ACCESS_READ_ONLY) ||
        (executable != 0 && executable != 1) ||
        (type == SYNTRA_ARM64_MMU_MEMORY_DEVICE && executable != 0) ||
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
            translation.page_size < SYNTRA_ARM64_MMU_PAGE_SIZE ||
            translation.memory_type != (uint8_t)type ||
            translation.privileged_read == 0U ||
            translation.privileged_write != (uint8_t)(
                access == SYNTRA_ARM64_MMU_ACCESS_READ_WRITE) ||
            translation.privileged_execute != (uint8_t)(executable != 0) ||
            translation.user_read != 0U ||
            translation.user_write != 0U ||
            translation.user_execute != 0U)
            return SYNTRA_STATUS_ERROR;

        {
            uint64_t step = translation.page_size -
                (va & (translation.page_size - 1ULL));
            uint64_t remaining = end - va;
            if (step > remaining) step = remaining;
            if (step == 0ULL ||
                (step & (SYNTRA_ARM64_MMU_PAGE_SIZE - 1ULL)) != 0ULL)
                return SYNTRA_STATUS_ERROR;
            va += step;
            pa += step;
        }
    }

    return SYNTRA_STATUS_OK;
}
