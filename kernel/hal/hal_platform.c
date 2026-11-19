
#include "hal_platform.h"

#include <stddef.h>
#include <string.h>

static int architecture_is_valid(
    syntra_hal_architecture_t architecture
)
{
    return architecture == SYNTRA_HAL_ARCH_ARM64 ||
           architecture == SYNTRA_HAL_ARCH_X86_64 ||
           architecture == SYNTRA_HAL_ARCH_RISCV64;
}

static int platform_type_is_valid(
    syntra_hal_platform_type_t type
)
{
    return type == SYNTRA_HAL_PLATFORM_GENERIC ||
           type == SYNTRA_HAL_PLATFORM_EMULATOR ||
           type == SYNTRA_HAL_PLATFORM_DEVICE;
}

syntra_status_t hal_platform_init(
    syntra_hal_platform_t *platform,
    const char *name,
    syntra_hal_architecture_t architecture,
    syntra_hal_platform_type_t type,
    unsigned int cpu_count,
    unsigned int memory_size_mb
)
{
    size_t name_length;

    if (platform == NULL ||
        name == NULL ||
        !architecture_is_valid(architecture) ||
        !platform_type_is_valid(type) ||
        cpu_count == 0U ||
        memory_size_mb == 0U)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    name_length = strlen(name);

    if (name_length == 0U ||
        name_length >= SYNTRA_HAL_PLATFORM_NAME_MAX)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    memset(platform, 0, sizeof(*platform));

    memcpy(
        platform->name,
        name,
        name_length
    );

    platform->architecture = architecture;
    platform->type = type;
    platform->cpu_count = cpu_count;
    platform->memory_size_mb = memory_size_mb;
    platform->initialized = 1;

    return SYNTRA_STATUS_OK;
}

syntra_status_t hal_platform_reset(
    syntra_hal_platform_t *platform
)
{
    if (platform == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    memset(platform, 0, sizeof(*platform));

    return SYNTRA_STATUS_OK;
}

syntra_status_t hal_platform_validate(
    const syntra_hal_platform_t *platform
)
{
    if (platform == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (platform->initialized != 1 ||
        !architecture_is_valid(platform->architecture) ||
        !platform_type_is_valid(platform->type) ||
        platform->cpu_count == 0U ||
        platform->memory_size_mb == 0U)
    {
        return SYNTRA_STATUS_ERROR;
    }

    if (platform->name[0] == '\0' ||
        memchr(
            platform->name,
            '\0',
            SYNTRA_HAL_PLATFORM_NAME_MAX
        ) == NULL)
    {
        return SYNTRA_STATUS_ERROR;
    }

    return SYNTRA_STATUS_OK;
}

syntra_status_t hal_platform_get_architecture(
    const syntra_hal_platform_t *platform,
    syntra_hal_architecture_t *architecture
)
{
    if (platform == NULL || architecture == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (hal_platform_validate(platform) != SYNTRA_STATUS_OK)
    {
        return SYNTRA_STATUS_ERROR;
    }

    *architecture = platform->architecture;

    return SYNTRA_STATUS_OK;
}

syntra_status_t hal_platform_get_type(
    const syntra_hal_platform_t *platform,
    syntra_hal_platform_type_t *type
)
{
    if (platform == NULL || type == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (hal_platform_validate(platform) != SYNTRA_STATUS_OK)
    {
        return SYNTRA_STATUS_ERROR;
    }

    *type = platform->type;

    return SYNTRA_STATUS_OK;
}
