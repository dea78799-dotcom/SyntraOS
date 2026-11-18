
#include "config.h"

#include <stddef.h>
#include <string.h>

static syntra_system_config_t system_config;

static syntra_status_t copy_config_string(
    char *destination,
    size_t destination_size,
    const char *source
)
{
    if (destination == NULL ||
        source == NULL ||
        destination_size == 0U)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    size_t length = strlen(source);

    if (length == 0U || length >= destination_size)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    memcpy(destination, source, length + 1U);

    return SYNTRA_STATUS_OK;
}

syntra_status_t config_init(void)
{
    if (system_config.initialized)
    {
        return SYNTRA_STATUS_OK;
    }

    memset(&system_config, 0, sizeof(system_config));

    if (copy_config_string(
            system_config.os_name,
            sizeof(system_config.os_name),
            "SyntraOS") != SYNTRA_STATUS_OK)
    {
        return SYNTRA_STATUS_ERROR;
    }

    if (copy_config_string(
            system_config.version,
            sizeof(system_config.version),
            "0.1.0") != SYNTRA_STATUS_OK)
    {
        return SYNTRA_STATUS_ERROR;
    }

    if (copy_config_string(
            system_config.architecture,
            sizeof(system_config.architecture),
            "unknown") != SYNTRA_STATUS_OK)
    {
        return SYNTRA_STATUS_ERROR;
    }

    system_config.memory_size = 0ULL;
    system_config.cpu_count = 1U;
    system_config.debug_mode = 0;
    system_config.initialized = 1;
    system_config.valid = 1;

    return SYNTRA_STATUS_OK;
}

syntra_status_t config_reset(void)
{
    memset(&system_config, 0, sizeof(system_config));

    return config_init();
}

syntra_status_t config_set_name(const char *name)
{
    if (!system_config.initialized)
    {
        return SYNTRA_STATUS_ERROR;
    }

    return copy_config_string(
        system_config.os_name,
        sizeof(system_config.os_name),
        name
    );
}

syntra_status_t config_set_version(const char *version)
{
    if (!system_config.initialized)
    {
        return SYNTRA_STATUS_ERROR;
    }

    return copy_config_string(
        system_config.version,
        sizeof(system_config.version),
        version
    );
}

syntra_status_t config_set_architecture(
    const char *architecture
)
{
    if (!system_config.initialized)
    {
        return SYNTRA_STATUS_ERROR;
    }

    return copy_config_string(
        system_config.architecture,
        sizeof(system_config.architecture),
        architecture
    );
}

syntra_status_t config_set_memory_size(
    unsigned long long memory_size
)
{
    if (!system_config.initialized)
    {
        return SYNTRA_STATUS_ERROR;
    }

    if (memory_size == 0ULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    system_config.memory_size = memory_size;

    return SYNTRA_STATUS_OK;
}

syntra_status_t config_set_cpu_count(
    unsigned int cpu_count
)
{
    if (!system_config.initialized)
    {
        return SYNTRA_STATUS_ERROR;
    }

    if (cpu_count == 0U)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    system_config.cpu_count = cpu_count;

    return SYNTRA_STATUS_OK;
}

syntra_status_t config_set_debug_mode(int enabled)
{
    if (!system_config.initialized)
    {
        return SYNTRA_STATUS_ERROR;
    }

    if (enabled != 0 && enabled != 1)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    system_config.debug_mode = enabled;

    return SYNTRA_STATUS_OK;
}

syntra_status_t config_get_info(
    syntra_system_config_t *config
)
{
    if (config == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (!system_config.initialized)
    {
        return SYNTRA_STATUS_ERROR;
    }

    *config = system_config;

    config->valid =
        (config_validate() == SYNTRA_STATUS_OK);

    return SYNTRA_STATUS_OK;
}

syntra_status_t config_validate(void)
{
    if (!system_config.initialized)
    {
        return SYNTRA_STATUS_ERROR;
    }

    if (system_config.os_name[0] == '\0' ||
        system_config.version[0] == '\0' ||
        system_config.architecture[0] == '\0')
    {
        return SYNTRA_STATUS_ERROR;
    }

    if (system_config.cpu_count == 0U)
    {
        return SYNTRA_STATUS_ERROR;
    }

    if (system_config.debug_mode != 0 &&
        system_config.debug_mode != 1)
    {
        return SYNTRA_STATUS_ERROR;
    }

    return SYNTRA_STATUS_OK;
}
