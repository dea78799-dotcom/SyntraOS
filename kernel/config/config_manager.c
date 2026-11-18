
#include "config_manager.h"

#include <string.h>

static syntra_system_config_t managed_config;
static syntra_config_manager_t manager_state;

static int configs_are_equal(
    const syntra_system_config_t *a,
    const syntra_system_config_t *b
)
{
    return
        strcmp(a->os_name, b->os_name) == 0 &&
        strcmp(a->version, b->version) == 0 &&
        strcmp(a->architecture, b->architecture) == 0 &&
        a->memory_size == b->memory_size &&
        a->cpu_count == b->cpu_count &&
        a->debug_mode == b->debug_mode &&
        a->initialized == b->initialized &&
        a->valid == b->valid;
}

syntra_status_t config_manager_init(void)
{
    if (manager_state.initialized)
    {
        return SYNTRA_STATUS_OK;
    }

    memset(&managed_config, 0, sizeof(managed_config));
    memset(&manager_state, 0, sizeof(manager_state));

    if (config_init() != SYNTRA_STATUS_OK)
    {
        return SYNTRA_STATUS_ERROR;
    }

    if (config_get_info(&managed_config) != SYNTRA_STATUS_OK)
    {
        return SYNTRA_STATUS_ERROR;
    }

    manager_state.update_count = 0UL;
    manager_state.dirty = 0;
    manager_state.initialized = 1;

    return SYNTRA_STATUS_OK;
}

syntra_status_t config_manager_reset(void)
{
    memset(&managed_config, 0, sizeof(managed_config));
    memset(&manager_state, 0, sizeof(manager_state));

    return config_manager_init();
}

syntra_status_t config_manager_update(
    const syntra_system_config_t *config
)
{
    if (!manager_state.initialized)
    {
        return SYNTRA_STATUS_ERROR;
    }

    if (config == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (memchr(config->os_name, '\0',
               sizeof(config->os_name)) == NULL ||
        memchr(config->version, '\0',
               sizeof(config->version)) == NULL ||
        memchr(config->architecture, '\0',
               sizeof(config->architecture)) == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (!config->initialized ||
        config->valid != 1 ||
        config->cpu_count == 0U ||
        config->os_name[0] == '\0' ||
        config->version[0] == '\0' ||
        config->architecture[0] == '\0' ||
        (config->debug_mode != 0 &&
         config->debug_mode != 1))
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (!configs_are_equal(&managed_config, config))
    {
        managed_config = *config;
        manager_state.update_count++;
        manager_state.dirty = 1;
    }

    return SYNTRA_STATUS_OK;
}

syntra_status_t config_manager_get_config(
    syntra_system_config_t *config
)
{
    if (config == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (!manager_state.initialized)
    {
        return SYNTRA_STATUS_ERROR;
    }

    *config = managed_config;

    return SYNTRA_STATUS_OK;
}

syntra_status_t config_manager_get_info(
    syntra_config_manager_t *manager
)
{
    if (manager == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (!manager_state.initialized)
    {
        return SYNTRA_STATUS_ERROR;
    }

    *manager = manager_state;

    return SYNTRA_STATUS_OK;
}

syntra_status_t config_manager_mark_clean(void)
{
    if (!manager_state.initialized)
    {
        return SYNTRA_STATUS_ERROR;
    }

    manager_state.dirty = 0;

    return SYNTRA_STATUS_OK;
}

syntra_status_t config_manager_is_dirty(int *dirty)
{
    if (dirty == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (!manager_state.initialized)
    {
        return SYNTRA_STATUS_ERROR;
    }

    *dirty = manager_state.dirty;

    return SYNTRA_STATUS_OK;
}
