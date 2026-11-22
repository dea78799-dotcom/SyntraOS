#include "address_space.h"

#include <stddef.h>

#include "../../memory/page_allocator.h"

#define DESC_VALID (1ULL << 0U)
#define DESC_TYPE  (1ULL << 1U)
#define DESC_ADDR  SYNTRA_ARM64_MMU_PA_MASK

static syntra_status_t allocate_page_table_page(
    void *context, uint64_t *physical_address)
{
    uintptr_t page;
    syntra_status_t status;
    (void)context;

    if (physical_address == NULL)
        return SYNTRA_STATUS_INVALID_ARGUMENT;

    status = syntra_page_alloc_owned(&page, SYNTRA_PAGE_OWNER_PAGE_TABLE);
    if (status == SYNTRA_STATUS_OK)
        *physical_address = (uint64_t)page;
    return status;
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

    if (space == NULL || address_space_id == 0U || resolve == NULL)
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    if (space->initialized)
        return SYNTRA_STATUS_ERROR;

    space->address_space_id = address_space_id;
    space->initialized = 0U;
    space->resolve = resolve;
    space->resolve_context = resolve_context;
    space->mapping_count = 0U;
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
        return status;

    space->initialized = 1U;
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
        (virtual_address & (SYNTRA_ARM64_MMU_PAGE_SIZE - 1ULL)) != 0ULL ||
        space->mapping_count >= SYNTRA_ARM64_ADDRESS_SPACE_MAX_MAPPINGS)
        return SYNTRA_STATUS_INVALID_ARGUMENT;

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
        return status;

    space->mappings[space->mapping_count].virtual_address = virtual_address;
    space->mappings[space->mapping_count].physical_address = physical_address;
    space->mappings[space->mapping_count].access = access;
    space->mappings[space->mapping_count].executable = (uint8_t)executable;
    space->mapping_count++;
    return SYNTRA_STATUS_OK;
}

syntra_status_t syntra_arm64_address_space_translate(
    syntra_arm64_address_space_t *space,
    uint64_t virtual_address,
    syntra_arm64_mmu_translation_t *translation)
{
    if (space == NULL || !space->initialized || translation == NULL)
        return SYNTRA_STATUS_INVALID_ARGUMENT;

    return syntra_arm64_mmu_walk(
        space->page_table.root_physical_address,
        virtual_address,
        walk_resolve,
        space,
        translation);
}

syntra_status_t syntra_arm64_address_space_validate(
    syntra_arm64_address_space_t *space)
{
    syntra_status_t status;

    if (space == NULL || !space->initialized ||
        space->address_space_id == 0U ||
        space->mapping_count > SYNTRA_ARM64_ADDRESS_SPACE_MAX_MAPPINGS ||
        space->page_table.root_physical_address == 0ULL)
        return SYNTRA_STATUS_INVALID_ARGUMENT;

    status = page_owner_is(
        space->page_table.root_physical_address,
        SYNTRA_PAGE_OWNER_PAGE_TABLE);
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
            syntra_status_t status;

            if ((descriptor & DESC_VALID) == 0ULL ||
                (descriptor & DESC_TYPE) == 0ULL)
                continue;

            status = release_table_tree(
                space, descriptor & DESC_ADDR, level + 1U);
            if (status != SYNTRA_STATUS_OK)
                return status;
        }
    }

    return syntra_page_free_owned(
        (uintptr_t)table_physical_address,
        SYNTRA_PAGE_OWNER_PAGE_TABLE);
}

syntra_status_t syntra_arm64_address_space_destroy(
    syntra_arm64_address_space_t *space)
{
    syntra_status_t status;
    uint64_t root;

    if (space == NULL || !space->initialized)
        return SYNTRA_STATUS_INVALID_ARGUMENT;

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

    for (size_t i = 0U; i < space->mapping_count; ++i)
    {
        status = syntra_page_free_owned(
            (uintptr_t)space->mappings[i].physical_address,
            SYNTRA_PAGE_OWNER_PROCESS);
        if (status != SYNTRA_STATUS_OK)
            return status;
    }

    status = release_table_tree(space, root, 1U);
    if (status != SYNTRA_STATUS_OK)
        return status;

    space->initialized = 0U;
    space->address_space_id = 0U;
    space->page_table.root_physical_address = 0ULL;
    space->mapping_count = 0U;
    space->resolve = NULL;
    space->resolve_context = NULL;
    return SYNTRA_STATUS_OK;
}
