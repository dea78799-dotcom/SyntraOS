#include "address_space.h"

#include <stddef.h>

#include "../../memory/page_allocator.h"

static syntra_arm64_address_space_t *registered_spaces[
    SYNTRA_ARM64_ADDRESS_SPACE_MAX_INSTANCES];

/* Single-core V1 active-root tracking; no SMP coordination is provided. */
static uint64_t registered_kernel_root_physical_address;
static uint64_t active_root_physical_address;
static syntra_arm64_address_space_t *active_address_space;

#if defined(SYNTRA_ARM64_ADDRESS_SPACE_TESTING)
static syntra_arm64_test_read_ttbr0_fn test_read_ttbr0;
static syntra_arm64_test_write_ttbr0_fn test_write_ttbr0;
static void *test_mmu_context;

void syntra_arm64_address_space_test_set_mmu_hooks(
    syntra_arm64_test_read_ttbr0_fn read_ttbr0,
    syntra_arm64_test_write_ttbr0_fn write_ttbr0,
    void *context)
{
    test_read_ttbr0 = read_ttbr0;
    test_write_ttbr0 = write_ttbr0;
    test_mmu_context = context;
}
#endif

static syntra_status_t write_ttbr0_and_flush(uint64_t root_physical_address)
{
#if defined(SYNTRA_ARM64_ADDRESS_SPACE_TESTING)
    if (test_write_ttbr0 == NULL)
        return SYNTRA_STATUS_NOT_IMPLEMENTED;
    return test_write_ttbr0(root_physical_address, test_mmu_context);
#elif defined(__aarch64__)
    __asm__ volatile("dsb ishst" ::: "memory");
    __asm__ volatile("msr ttbr0_el1, %0" ::
        "r"(root_physical_address) : "memory");
    __asm__ volatile("isb" ::: "memory");
    __asm__ volatile("tlbi vmalle1" ::: "memory");
    __asm__ volatile("dsb ish" ::: "memory");
    __asm__ volatile("isb" ::: "memory");
    return SYNTRA_STATUS_OK;
#else
    (void)root_physical_address;
    return SYNTRA_STATUS_NOT_IMPLEMENTED;
#endif
}

static int address_space_registry_slot(
    const syntra_arm64_address_space_t *space, size_t *free_slot)
{
    size_t i;
    for (i = 0U; i < SYNTRA_ARM64_ADDRESS_SPACE_MAX_INSTANCES; ++i)
    {
        if (registered_spaces[i] == space)
            return 1;
        if (registered_spaces[i] == NULL && free_slot != NULL &&
            *free_slot == SYNTRA_ARM64_ADDRESS_SPACE_MAX_INSTANCES)
            *free_slot = i;
    }
    return 0;
}

static int address_space_is_registered(
    const syntra_arm64_address_space_t *space)
{
    size_t unused = SYNTRA_ARM64_ADDRESS_SPACE_MAX_INSTANCES;
    return address_space_registry_slot(space, &unused);
}

static int physical_page_is_mapped(
    uint64_t physical_address,
    const syntra_arm64_address_space_t *except_space)
{
    for (size_t i = 0U; i < SYNTRA_ARM64_ADDRESS_SPACE_MAX_INSTANCES; ++i)
    {
        const syntra_arm64_address_space_t *candidate = registered_spaces[i];
        if (candidate == NULL || candidate == except_space)
            continue;
        for (size_t j = 0U; j < candidate->mapping_count; ++j)
            if (candidate->mappings[j].physical_address == physical_address)
                return 1;
    }
    return 0;
}

static void address_space_unregister(
    const syntra_arm64_address_space_t *space)
{
    for (size_t i = 0U; i < SYNTRA_ARM64_ADDRESS_SPACE_MAX_INSTANCES; ++i)
        if (registered_spaces[i] == space)
        {
            registered_spaces[i] = NULL;
            return;
        }
}

#define DESC_VALID (1ULL << 0U)
#define DESC_TYPE  (1ULL << 1U)
#define DESC_ADDR  SYNTRA_ARM64_MMU_PA_MASK

static syntra_status_t allocate_page_table_page(
    void *context, uint64_t *physical_address)
{
    syntra_arm64_address_space_t *space =
        (syntra_arm64_address_space_t *)context;
    uintptr_t page;
    syntra_status_t status;

    if (space == NULL || space->resolve == NULL || physical_address == NULL)
        return SYNTRA_STATUS_INVALID_ARGUMENT;

    status = syntra_page_alloc_owned(&page, SYNTRA_PAGE_OWNER_PAGE_TABLE);
    if (status != SYNTRA_STATUS_OK)
        return status;

    /*
     * Validate the allocator's physical-to-virtual mapping before handing the
     * page to the generic page-table builder. Otherwise its resolver failure
     * would leave an allocated page with no owner able to reclaim it.
     */
    if (space->resolve((uint64_t)page, space->resolve_context) == NULL)
    {
        (void)syntra_page_free_owned(page, SYNTRA_PAGE_OWNER_PAGE_TABLE);
        return SYNTRA_STATUS_ERROR;
    }

    *physical_address = (uint64_t)page;
    return SYNTRA_STATUS_OK;
}

static uint64_t *resolve_page_table_page(
    uint64_t physical_address, void *context)
{
    syntra_arm64_address_space_t *space =
        (syntra_arm64_address_space_t *)context;

    if (space == NULL || space->resolve == NULL)
        return NULL;
    return space->resolve(physical_address, space->resolve_context);
}

static const uint64_t *walk_resolve(
    uint64_t physical_address, void *context)
{
    return resolve_page_table_page(physical_address, context);
}

static syntra_status_t page_owner_is(
    uint64_t physical_address, syntra_page_owner_t expected)
{
    syntra_page_owner_t actual = SYNTRA_PAGE_OWNER_NONE;
    syntra_status_t status;

    if (physical_address > (uint64_t)UINTPTR_MAX)
        return SYNTRA_STATUS_INVALID_ARGUMENT;

    status = syntra_page_get_owner((uintptr_t)physical_address, &actual);
    if (status != SYNTRA_STATUS_OK)
        return status;
    return actual == expected
        ? SYNTRA_STATUS_OK : SYNTRA_STATUS_PERMISSION_DENIED;
}

syntra_status_t syntra_arm64_address_space_create(
    syntra_arm64_address_space_t *space,
    uint32_t address_space_id,
    syntra_arm64_address_space_resolve_fn resolve,
    void *resolve_context)
{
    syntra_status_t status;
    size_t registry_slot = SYNTRA_ARM64_ADDRESS_SPACE_MAX_INSTANCES;

    if (space == NULL || address_space_id == 0U || resolve == NULL)
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    if (space->initialized ||
        address_space_registry_slot(space, &registry_slot))
        return SYNTRA_STATUS_ERROR;
    if (registry_slot == SYNTRA_ARM64_ADDRESS_SPACE_MAX_INSTANCES)
        return SYNTRA_STATUS_OUT_OF_MEMORY;

    space->address_space_id = address_space_id;
    space->initialized = 0U;
    space->resolve = resolve;
    space->resolve_context = resolve_context;
    space->mapping_count = 0U;
    space->kernel_root_physical_address = 0ULL;
    space->page_table.root_physical_address = 0ULL;
    space->page_table.allocate = allocate_page_table_page;
    space->page_table.resolve = resolve_page_table_page;
    space->page_table.context = space;

    status = syntra_arm64_page_table_init(
        &space->page_table,
        allocate_page_table_page,
        resolve_page_table_page,
        space);
    if (status != SYNTRA_STATUS_OK)
    {
        space->address_space_id = 0U;
        space->resolve = NULL;
        space->resolve_context = NULL;
        return status;
    }

    status = page_owner_is(
        space->page_table.root_physical_address,
        SYNTRA_PAGE_OWNER_PAGE_TABLE);
    if (status != SYNTRA_STATUS_OK)
    {
        (void)syntra_page_free_owned(
            (uintptr_t)space->page_table.root_physical_address,
            SYNTRA_PAGE_OWNER_PAGE_TABLE);
        space->page_table.root_physical_address = 0ULL;
        return status;
    }

    registered_spaces[registry_slot] = space;
    space->initialized = 1U;
    return SYNTRA_STATUS_OK;
}

static int root_entry_is_borrowed(
    const syntra_arm64_address_space_t *space, size_t index)
{
    return (space->borrowed_root_entries[index / 64U] &
        (1ULL << (index % 64U))) != 0ULL;
}

static void root_entry_mark_borrowed(
    syntra_arm64_address_space_t *space, size_t index)
{
    space->borrowed_root_entries[index / 64U] |=
        1ULL << (index % 64U);
}

syntra_status_t syntra_arm64_address_space_inherit_kernel_mappings(
    syntra_arm64_address_space_t *space,
    uint64_t kernel_root_physical_address)
{
    uint64_t *root, *kernel_root;

    if (space == NULL || !space->initialized ||
        !address_space_is_registered(space) ||
        space->kernel_mappings_inherited != 0U ||
        space->mapping_count != 0U ||
        kernel_root_physical_address == 0ULL ||
        kernel_root_physical_address ==
            space->page_table.root_physical_address)
        return SYNTRA_STATUS_INVALID_ARGUMENT;

    if (page_owner_is(kernel_root_physical_address,
            SYNTRA_PAGE_OWNER_PAGE_TABLE) != SYNTRA_STATUS_OK)
        return SYNTRA_STATUS_PERMISSION_DENIED;

    root = resolve_page_table_page(
        space->page_table.root_physical_address, space);
    kernel_root = resolve_page_table_page(kernel_root_physical_address, space);
    if (root == NULL || kernel_root == NULL)
        return SYNTRA_STATUS_ERROR;

    /* The upper half of the 39-bit TTBR0 range is reserved for EL0. */
    for (size_t i = 128U; i < SYNTRA_ARM64_MMU_TABLE_ENTRIES; ++i)
        if (kernel_root[i] != 0ULL)
            return SYNTRA_STATUS_ERROR;

    /* Preflight the complete source and destination before mutating the root. */
    for (size_t i = 0U; i < 128U; ++i)
    {
        uint64_t descriptor = kernel_root[i];
        if (root[i] != 0ULL)
            return SYNTRA_STATUS_ERROR;
        if (descriptor != 0ULL &&
            ((descriptor & DESC_VALID) == 0ULL ||
             (descriptor & ~(DESC_ADDR | 3ULL)) != 0ULL))
            return SYNTRA_STATUS_ERROR;
    }

    for (size_t i = 0U; i < 128U; ++i)
    {
        if (kernel_root[i] != 0ULL)
        {
            root[i] = kernel_root[i];
            root_entry_mark_borrowed(space, i);
        }
    }

    space->kernel_root_physical_address = kernel_root_physical_address;
    space->kernel_mappings_inherited = 1U;
    return SYNTRA_STATUS_OK;
}

/*
 * A failed user mapping may have allocated one or both private table levels.
 * Detach and release only empty tables, preserving any pre-existing siblings.
 */
static int table_is_empty(const uint64_t *table);

static syntra_status_t rollback_empty_user_tables(
    syntra_arm64_address_space_t *space,
    uint64_t virtual_address)
{
    uint16_t indices[4];
    uint64_t *root, *level2, *level3;
    uint64_t descriptor, root_descriptor, level2_pa, level3_pa;
    syntra_status_t status;

    status = syntra_arm64_mmu_get_table_indices(virtual_address, indices);
    if (status != SYNTRA_STATUS_OK)
        return status;

    root = resolve_page_table_page(
        space->page_table.root_physical_address, space);
    if (root == NULL)
        return SYNTRA_STATUS_ERROR;

    root_descriptor = root[indices[0]];
    descriptor = root_descriptor;
    if ((descriptor & (DESC_VALID | DESC_TYPE)) !=
        (DESC_VALID | DESC_TYPE))
        return SYNTRA_STATUS_OK;
    if ((descriptor & ~(DESC_ADDR | 3ULL)) != 0ULL)
        return SYNTRA_STATUS_ERROR;

    level2_pa = descriptor & DESC_ADDR;
    level2 = resolve_page_table_page(level2_pa, space);
    if (level2 == NULL)
        return SYNTRA_STATUS_ERROR;

    descriptor = level2[indices[1]];
    if ((descriptor & DESC_VALID) != 0ULL)
    {
        if ((descriptor & DESC_TYPE) == 0ULL ||
            (descriptor & ~(DESC_ADDR | 3ULL)) != 0ULL)
            return SYNTRA_STATUS_ERROR;

        level3_pa = descriptor & DESC_ADDR;
        level3 = resolve_page_table_page(level3_pa, space);
        if (level3 == NULL)
            return SYNTRA_STATUS_ERROR;

        if (table_is_empty(level3))
        {
            level2[indices[1]] = 0ULL;
            status = syntra_page_free_owned(
                (uintptr_t)level3_pa, SYNTRA_PAGE_OWNER_PAGE_TABLE);
            if (status != SYNTRA_STATUS_OK)
            {
                /* Keep the still-owned child reachable for a later retry. */
                level2[indices[1]] = descriptor;
                return status;
            }
        }
    }

    if (table_is_empty(level2))
    {
        root[indices[0]] = 0ULL;
        status = syntra_page_free_owned(
            (uintptr_t)level2_pa, SYNTRA_PAGE_OWNER_PAGE_TABLE);
        if (status != SYNTRA_STATUS_OK)
            root[indices[0]] = root_descriptor;
        return status;
    }

    return SYNTRA_STATUS_OK;
}

syntra_status_t syntra_arm64_address_space_map_user_page(
    syntra_arm64_address_space_t *space,
    uint64_t virtual_address,
    uint64_t physical_address,
    syntra_arm64_mmu_access_t access,
    int executable)
{
    syntra_status_t status;

    if (space == NULL || !space->initialized ||
        !address_space_is_registered(space) ||
        space->kernel_mappings_inherited == 0U ||
        virtual_address < SYNTRA_ARM64_USER_VA_BASE ||
        virtual_address >= SYNTRA_ARM64_USER_VA_LIMIT ||
        (virtual_address & (SYNTRA_ARM64_MMU_PAGE_SIZE - 1ULL)) != 0ULL ||
        space->mapping_count >= SYNTRA_ARM64_ADDRESS_SPACE_MAX_MAPPINGS)
        return SYNTRA_STATUS_INVALID_ARGUMENT;

    if (physical_page_is_mapped(physical_address, NULL))
        return SYNTRA_STATUS_PERMISSION_DENIED;

    status = page_owner_is(physical_address, SYNTRA_PAGE_OWNER_PROCESS);
    if (status != SYNTRA_STATUS_OK)
        return status;

    for (size_t i = 0U; i < space->mapping_count; ++i)
    {
        if (space->mappings[i].virtual_address == virtual_address ||
            space->mappings[i].physical_address == physical_address)
            return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    status = syntra_arm64_page_table_map_user(
        &space->page_table,
        virtual_address,
        physical_address,
        SYNTRA_ARM64_MMU_PAGE_SIZE,
        access,
        executable);
    if (status != SYNTRA_STATUS_OK)
    {
        syntra_status_t rollback_status =
            rollback_empty_user_tables(space, virtual_address);
        return rollback_status == SYNTRA_STATUS_OK ? status : rollback_status;
    }

    space->mappings[space->mapping_count].virtual_address = virtual_address;
    space->mappings[space->mapping_count].physical_address = physical_address;
    space->mappings[space->mapping_count].access = access;
    space->mappings[space->mapping_count].executable = (uint8_t)executable;
    space->mapping_count++;
    return SYNTRA_STATUS_OK;
}

static int table_is_empty(const uint64_t *table)
{
    for (size_t i = 0U; i < SYNTRA_ARM64_MMU_TABLE_ENTRIES; ++i)
        if (table[i] != 0ULL)
            return 0;
    return 1;
}

syntra_status_t syntra_arm64_address_space_unmap_user_page(
    syntra_arm64_address_space_t *space,
    uint64_t virtual_address)
{
    uint16_t indices[4];
    uint64_t *root, *level2, *level3;
    uint64_t descriptor, root_descriptor, level2_descriptor;
    uint64_t level2_pa, level3_pa, process_pa, leaf_descriptor;
    size_t mapping_index;
    syntra_status_t status;

    if (space == NULL || !space->initialized ||
        !address_space_is_registered(space) ||
        virtual_address < SYNTRA_ARM64_USER_VA_BASE ||
        virtual_address >= SYNTRA_ARM64_USER_VA_LIMIT ||
        (virtual_address & (SYNTRA_ARM64_MMU_PAGE_SIZE - 1ULL)) != 0ULL)
        return SYNTRA_STATUS_INVALID_ARGUMENT;

    for (mapping_index = 0U; mapping_index < space->mapping_count;
         ++mapping_index)
        if (space->mappings[mapping_index].virtual_address == virtual_address)
            break;
    if (mapping_index == space->mapping_count)
        return SYNTRA_STATUS_INVALID_ARGUMENT;

    process_pa = space->mappings[mapping_index].physical_address;
    status = page_owner_is(process_pa, SYNTRA_PAGE_OWNER_PROCESS);
    if (status != SYNTRA_STATUS_OK)
        return status;

    status = syntra_arm64_mmu_get_table_indices(virtual_address, indices);
    if (status != SYNTRA_STATUS_OK)
        return status;

    root = resolve_page_table_page(
        space->page_table.root_physical_address, space);
    if (root == NULL)
        return SYNTRA_STATUS_ERROR;

    root_descriptor = root[indices[0]];
    descriptor = root_descriptor;
    if ((descriptor & (DESC_VALID | DESC_TYPE)) !=
        (DESC_VALID | DESC_TYPE) ||
        (descriptor & ~(DESC_ADDR | 3ULL)) != 0ULL)
        return SYNTRA_STATUS_ERROR;
    level2_pa = descriptor & DESC_ADDR;
    status = page_owner_is(level2_pa, SYNTRA_PAGE_OWNER_PAGE_TABLE);
    if (status != SYNTRA_STATUS_OK)
        return status;
    level2 = resolve_page_table_page(level2_pa, space);
    if (level2 == NULL)
        return SYNTRA_STATUS_ERROR;

    level2_descriptor = level2[indices[1]];
    descriptor = level2_descriptor;
    if ((descriptor & (DESC_VALID | DESC_TYPE)) !=
        (DESC_VALID | DESC_TYPE) ||
        (descriptor & ~(DESC_ADDR | 3ULL)) != 0ULL)
        return SYNTRA_STATUS_ERROR;
    level3_pa = descriptor & DESC_ADDR;
    status = page_owner_is(level3_pa, SYNTRA_PAGE_OWNER_PAGE_TABLE);
    if (status != SYNTRA_STATUS_OK)
        return status;
    level3 = resolve_page_table_page(level3_pa, space);
    if (level3 == NULL)
        return SYNTRA_STATUS_ERROR;

    leaf_descriptor = level3[indices[2]];
    if ((leaf_descriptor & (DESC_VALID | DESC_TYPE)) !=
        (DESC_VALID | DESC_TYPE) ||
        (leaf_descriptor & DESC_ADDR) != process_pa)
        return SYNTRA_STATUS_ERROR;

    level3[indices[2]] = 0ULL;
    status = syntra_page_free_owned(
        (uintptr_t)process_pa, SYNTRA_PAGE_OWNER_PROCESS);
    if (status != SYNTRA_STATUS_OK)
    {
        /* Restore the mapping if its backing frame could not be reclaimed. */
        level3[indices[2]] = leaf_descriptor;
        return status;
    }

    for (size_t i = mapping_index + 1U; i < space->mapping_count; ++i)
        space->mappings[i - 1U] = space->mappings[i];
    space->mapping_count--;

    if (table_is_empty(level3))
    {
        level2[indices[1]] = 0ULL;
        status = syntra_page_free_owned(
            (uintptr_t)level3_pa, SYNTRA_PAGE_OWNER_PAGE_TABLE);
        if (status != SYNTRA_STATUS_OK)
        {
            /* The L3 page is still owned; preserve its parent descriptor. */
            level2[indices[1]] = level2_descriptor;
            return status;
        }

        if (table_is_empty(level2))
        {
            root[indices[0]] = 0ULL;
            status = syntra_page_free_owned(
                (uintptr_t)level2_pa, SYNTRA_PAGE_OWNER_PAGE_TABLE);
            if (status != SYNTRA_STATUS_OK)
            {
                /* Keep the empty L2 table reachable if reclamation fails. */
                root[indices[0]] = root_descriptor;
                return status;
            }
        }
    }

    return SYNTRA_STATUS_OK;
}

syntra_status_t syntra_arm64_address_space_translate(
    syntra_arm64_address_space_t *space,
    uint64_t virtual_address,
    syntra_arm64_mmu_translation_t *translation)
{
    if (space == NULL || !space->initialized ||
        !address_space_is_registered(space) || translation == NULL)
        return SYNTRA_STATUS_INVALID_ARGUMENT;

    return syntra_arm64_mmu_walk(
        space->page_table.root_physical_address,
        virtual_address,
        walk_resolve,
        space,
        translation);
}

static syntra_status_t validate_address_space_root_layout(
    syntra_arm64_address_space_t *space);

syntra_status_t syntra_arm64_address_space_validate(
    syntra_arm64_address_space_t *space)
{
    syntra_status_t status;

    if (space == NULL || !space->initialized ||
        !address_space_is_registered(space) ||
        space->kernel_mappings_inherited == 0U ||
        space->address_space_id == 0U ||
        space->mapping_count > SYNTRA_ARM64_ADDRESS_SPACE_MAX_MAPPINGS ||
        space->page_table.root_physical_address == 0ULL)
        return SYNTRA_STATUS_INVALID_ARGUMENT;

    status = page_owner_is(
        space->page_table.root_physical_address,
        SYNTRA_PAGE_OWNER_PAGE_TABLE);
    if (status != SYNTRA_STATUS_OK)
        return status;

    status = validate_address_space_root_layout(space);
    if (status != SYNTRA_STATUS_OK)
        return status;

    for (size_t i = 0U; i < space->mapping_count; ++i)
    {
        const syntra_arm64_address_space_mapping_t *mapping =
            &space->mappings[i];
        syntra_arm64_mmu_translation_t translation;

        status = page_owner_is(
            mapping->physical_address, SYNTRA_PAGE_OWNER_PROCESS);
        if (status != SYNTRA_STATUS_OK)
            return status;

        status = syntra_arm64_page_table_verify_user(
            &space->page_table,
            mapping->virtual_address,
            mapping->physical_address,
            SYNTRA_ARM64_MMU_PAGE_SIZE,
            mapping->access,
            (int)mapping->executable);
        if (status != SYNTRA_STATUS_OK)
            return status;

        status = syntra_arm64_address_space_translate(
            space, mapping->virtual_address, &translation);
        if (status != SYNTRA_STATUS_OK ||
            translation.physical_address != mapping->physical_address ||
            !translation.user_read ||
            translation.user_write != (uint8_t)(
                mapping->access == SYNTRA_ARM64_MMU_ACCESS_READ_WRITE) ||
            translation.user_execute != mapping->executable)
            return SYNTRA_STATUS_ERROR;

        for (size_t j = i + 1U; j < space->mapping_count; ++j)
        {
            if (space->mappings[j].virtual_address == mapping->virtual_address ||
                space->mappings[j].physical_address == mapping->physical_address)
                return SYNTRA_STATUS_ERROR;
        }
    }

    return SYNTRA_STATUS_OK;
}

static syntra_status_t validate_table_tree(
    syntra_arm64_address_space_t *space,
    uint64_t table_physical_address,
    unsigned int level)
{
    uint64_t *table;
    syntra_status_t status;

    status = page_owner_is(
        table_physical_address, SYNTRA_PAGE_OWNER_PAGE_TABLE);
    if (status != SYNTRA_STATUS_OK)
        return status;

    table = resolve_page_table_page(table_physical_address, space);
    if (table == NULL)
        return SYNTRA_STATUS_ERROR;

    if (level >= 3U)
        return SYNTRA_STATUS_OK;

    for (size_t i = 0U; i < SYNTRA_ARM64_MMU_TABLE_ENTRIES; ++i)
    {
        uint64_t descriptor = table[i];
        if ((descriptor & DESC_VALID) == 0ULL ||
            (descriptor & DESC_TYPE) == 0ULL)
            continue;

        if ((descriptor & ~(DESC_ADDR | 3ULL)) != 0ULL)
            return SYNTRA_STATUS_ERROR;

        status = validate_table_tree(
            space, descriptor & DESC_ADDR, level + 1U);
        if (status != SYNTRA_STATUS_OK)
            return status;
    }

    return SYNTRA_STATUS_OK;
}

static syntra_status_t validate_user_table_tree(
    syntra_arm64_address_space_t *space,
    uint64_t table_physical_address,
    unsigned int level,
    uint64_t virtual_base)
{
    uint64_t *table;
    syntra_status_t status;

    status = page_owner_is(
        table_physical_address, SYNTRA_PAGE_OWNER_PAGE_TABLE);
    if (status != SYNTRA_STATUS_OK)
        return status;

    table = resolve_page_table_page(table_physical_address, space);
    if (table == NULL)
        return SYNTRA_STATUS_ERROR;

    for (size_t i = (level == 1U ? 256U : 0U);
         i < SYNTRA_ARM64_MMU_TABLE_ENTRIES; ++i)
    {
        uint64_t descriptor = table[i];
        uint64_t entry_va;
        uint64_t child_pa;

        if (descriptor == 0ULL)
            continue;
        if ((descriptor & DESC_VALID) == 0ULL)
            return SYNTRA_STATUS_ERROR;

        entry_va = virtual_base +
            ((uint64_t)i << (level == 1U ? 30U :
                             (level == 2U ? 21U : 12U)));

        if (level == 3U)
        {
            if ((descriptor & DESC_TYPE) == 0ULL ||
                (descriptor & ~(DESC_ADDR | 3ULL | (0xFFFULL << 2U))) != 0ULL)
                return SYNTRA_STATUS_ERROR;

            for (size_t mapping_index = 0U;
                 mapping_index < space->mapping_count; ++mapping_index)
            {
                const syntra_arm64_address_space_mapping_t *mapping =
                    &space->mappings[mapping_index];
                if (mapping->virtual_address == entry_va &&
                    mapping->physical_address == (descriptor & DESC_ADDR))
                    break;
                if (mapping_index + 1U == space->mapping_count)
                    return SYNTRA_STATUS_ERROR;
            }
            if (space->mapping_count == 0U)
                return SYNTRA_STATUS_ERROR;
            continue;
        }

        if ((descriptor & DESC_TYPE) == 0ULL ||
            (descriptor & ~(DESC_ADDR | 3ULL)) != 0ULL)
            return SYNTRA_STATUS_ERROR;

        child_pa = descriptor & DESC_ADDR;
        status = validate_user_table_tree(
            space, child_pa, level + 1U, entry_va);
        if (status != SYNTRA_STATUS_OK)
            return status;
    }

    return SYNTRA_STATUS_OK;
}

static syntra_status_t validate_address_space_root_layout(
    syntra_arm64_address_space_t *space)
{
    uint64_t *root, *kernel_root;
    syntra_status_t status;

    status = page_owner_is(
        space->kernel_root_physical_address,
        SYNTRA_PAGE_OWNER_PAGE_TABLE);
    if (status != SYNTRA_STATUS_OK)
        return status;

    root = resolve_page_table_page(
        space->page_table.root_physical_address, space);
    kernel_root = resolve_page_table_page(
        space->kernel_root_physical_address, space);
    if (root == NULL || kernel_root == NULL)
        return SYNTRA_STATUS_ERROR;

    /*
     * Entries 0..127 are borrowed from the kernel root and must remain
     * byte-for-byte identical. The bitmap must describe exactly those
     * non-empty borrowed entries so destruction never frees shared tables.
     */
    for (size_t i = 0U; i < 128U; ++i)
    {
        int expected_borrowed = kernel_root[i] != 0ULL;
        if (root[i] != kernel_root[i] ||
            root_entry_is_borrowed(space, i) != expected_borrowed)
            return SYNTRA_STATUS_ERROR;
    }

    /*
     * No root entry outside the inherited kernel window may be marked
     * borrowed. In particular, user L1 entries must remain privately owned.
     */
    for (size_t i = 128U; i < SYNTRA_ARM64_MMU_TABLE_ENTRIES; ++i)
        if (root_entry_is_borrowed(space, i))
            return SYNTRA_STATUS_ERROR;

    /* Entries 128..255 are the reserved gap before the EL0 user range. */
    for (size_t i = 128U; i < 256U; ++i)
        if (root[i] != 0ULL)
            return SYNTRA_STATUS_ERROR;

    /*
     * Audit every reachable table page, including private user tables and
     * borrowed kernel subtrees, before the address space is activated.
     */
    status = validate_table_tree(
        space, space->page_table.root_physical_address, 1U);
    if (status != SYNTRA_STATUS_OK)
        return status;

    /*
     * User tables may contain only private 4-KiB page mappings recorded in
     * the mapping registry. Reject block descriptors and untracked leaves.
     */
    return validate_user_table_tree(
        space, space->page_table.root_physical_address, 1U, 0ULL);
}

static syntra_status_t release_table_tree(
    syntra_arm64_address_space_t *space,
    uint64_t table_physical_address,
    unsigned int level)
{
    uint64_t *table =
        resolve_page_table_page(table_physical_address, space);

    if (table == NULL)
        return SYNTRA_STATUS_ERROR;

    if (level < 3U)
    {
        for (size_t i = 0U; i < SYNTRA_ARM64_MMU_TABLE_ENTRIES; ++i)
        {
            uint64_t descriptor = table[i];
            if (level == 1U && root_entry_is_borrowed(space, i))
                continue;
            syntra_status_t status;

            if ((descriptor & DESC_VALID) == 0ULL ||
                (descriptor & DESC_TYPE) == 0ULL)
                continue;

            status = release_table_tree(
                space, descriptor & DESC_ADDR, level + 1U);
            if (status != SYNTRA_STATUS_OK)
                return status;

            /*
             * The child is now released. Clear its parent link immediately so
             * a later failure leaves a valid, retryable partial tree.
             */
            table[i] = 0ULL;
        }
    }

    return syntra_page_free_owned(
        (uintptr_t)table_physical_address,
        SYNTRA_PAGE_OWNER_PAGE_TABLE);
}

syntra_status_t syntra_arm64_address_space_register_kernel_root(
    uint64_t kernel_root_physical_address)
{
    if (kernel_root_physical_address == 0ULL ||
        (kernel_root_physical_address &
            (SYNTRA_ARM64_MMU_PAGE_SIZE - 1ULL)) != 0ULL ||
        page_owner_is(kernel_root_physical_address,
            SYNTRA_PAGE_OWNER_PAGE_TABLE) != SYNTRA_STATUS_OK)
        return SYNTRA_STATUS_INVALID_ARGUMENT;

#if defined(SYNTRA_ARM64_ADDRESS_SPACE_TESTING)
    {
        uint64_t current_ttbr0;
        if (test_read_ttbr0 == NULL)
            return SYNTRA_STATUS_NOT_IMPLEMENTED;
        current_ttbr0 = test_read_ttbr0(test_mmu_context);
        if ((current_ttbr0 & SYNTRA_ARM64_MMU_PA_MASK) !=
            kernel_root_physical_address)
            return SYNTRA_STATUS_ERROR;
    }
#elif defined(__aarch64__)
    {
        uint64_t current_ttbr0;
        __asm__ volatile("mrs %0, ttbr0_el1" : "=r"(current_ttbr0));
        if ((current_ttbr0 & SYNTRA_ARM64_MMU_PA_MASK) !=
            kernel_root_physical_address)
            return SYNTRA_STATUS_ERROR;
    }
#else
    return SYNTRA_STATUS_NOT_IMPLEMENTED;
#endif

    if (registered_kernel_root_physical_address != 0ULL &&
        registered_kernel_root_physical_address != kernel_root_physical_address)
        return SYNTRA_STATUS_PERMISSION_DENIED;

    registered_kernel_root_physical_address = kernel_root_physical_address;
    active_root_physical_address = kernel_root_physical_address;
    active_address_space = NULL;
    return SYNTRA_STATUS_OK;
}

syntra_status_t syntra_arm64_address_space_activate(
    syntra_arm64_address_space_t *space)
{
    syntra_status_t status;

    if (space == NULL || !space->initialized ||
        !address_space_is_registered(space) ||
        space->kernel_mappings_inherited == 0U ||
        registered_kernel_root_physical_address == 0ULL ||
        space->kernel_root_physical_address !=
            registered_kernel_root_physical_address)
        return SYNTRA_STATUS_INVALID_ARGUMENT;

    status = syntra_arm64_address_space_validate(space);
    if (status != SYNTRA_STATUS_OK)
        return status;

    if (active_address_space == space &&
        active_root_physical_address == space->page_table.root_physical_address)
        return SYNTRA_STATUS_OK;

    status = write_ttbr0_and_flush(space->page_table.root_physical_address);
    if (status != SYNTRA_STATUS_OK)
        return status;

    active_root_physical_address = space->page_table.root_physical_address;
    active_address_space = space;
    return SYNTRA_STATUS_OK;
}

int syntra_arm64_address_space_kernel_root_registered(void)
{
    return registered_kernel_root_physical_address != 0ULL;
}

syntra_status_t syntra_arm64_address_space_activate_kernel_root(void)
{
    syntra_status_t status;

    if (registered_kernel_root_physical_address == 0ULL)
        return SYNTRA_STATUS_ERROR;

    if (active_address_space == NULL &&
        active_root_physical_address ==
            registered_kernel_root_physical_address)
        return SYNTRA_STATUS_OK;

    status = write_ttbr0_and_flush(
        registered_kernel_root_physical_address);
    if (status != SYNTRA_STATUS_OK)
        return status;

    active_root_physical_address =
        registered_kernel_root_physical_address;
    active_address_space = NULL;
    return SYNTRA_STATUS_OK;
}

syntra_status_t syntra_arm64_address_space_destroy(
    syntra_arm64_address_space_t *space)
{
    syntra_status_t status;
    uint64_t root;

    if (space == NULL || !space->initialized)
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    if (active_address_space == space ||
        active_root_physical_address == space->page_table.root_physical_address)
        return SYNTRA_STATUS_PERMISSION_DENIED;

    root = space->page_table.root_physical_address;
    status = validate_table_tree(space, root, 1U);
    if (status != SYNTRA_STATUS_OK)
        return status;

    for (size_t i = 0U; i < space->mapping_count; ++i)
    {
        status = page_owner_is(
            space->mappings[i].physical_address,
            SYNTRA_PAGE_OWNER_PROCESS);
        if (status != SYNTRA_STATUS_OK)
            return status;
    }

    /*
     * Reuse the checked unmap path so each process leaf is detached before
     * its backing frame is freed. The unmap path commits mapping_count only
     * after the frame release succeeds and preserves table links if table
     * reclamation fails, making a later destroy attempt safe.
     */
    while (space->mapping_count != 0U)
    {
        size_t last = space->mapping_count - 1U;
        uint64_t virtual_address = space->mappings[last].virtual_address;
        status = syntra_arm64_address_space_unmap_user_page(
            space, virtual_address);
        if (status != SYNTRA_STATUS_OK)
            return status;
    }

    /*
     * release_table_tree clears each parent descriptor after freeing its
     * child. A failure therefore leaves the remaining tree reachable and
     * permits a subsequent destroy attempt.
     */
    status = release_table_tree(space, root, 1U);
    if (status != SYNTRA_STATUS_OK)
        return status;

    space->initialized = 0U;
    space->address_space_id = 0U;
    space->page_table.root_physical_address = 0ULL;
    space->mapping_count = 0U;
    space->kernel_mappings_inherited = 0U;
    space->kernel_root_physical_address = 0ULL;
    for (size_t i = 0U; i < SYNTRA_ARM64_ADDRESS_SPACE_ROOT_BITMAP_WORDS; ++i)
        space->borrowed_root_entries[i] = 0ULL;
    address_space_unregister(space);
    space->resolve = NULL;
    space->resolve_context = NULL;
    return SYNTRA_STATUS_OK;
}
