
#include "hal_backend.h"

#include <stddef.h>
#include <string.h>

static int backend_type_is_valid(
    syntra_hal_backend_type_t type
)
{
    return type == SYNTRA_HAL_BACKEND_SOFTWARE ||
           type == SYNTRA_HAL_BACKEND_ARM64 ||
           type == SYNTRA_HAL_BACKEND_X86_64 ||
           type == SYNTRA_HAL_BACKEND_RISCV64;
}

syntra_status_t hal_backend_init(
    syntra_hal_backend_t *backend,
    unsigned int backend_id,
    const char *name,
    syntra_hal_backend_type_t type
)
{
    size_t name_length;

    if (backend == NULL ||
        name == NULL ||
        backend_id == 0U ||
        !backend_type_is_valid(type))
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    name_length = strlen(name);

    if (name_length == 0U ||
        name_length >= SYNTRA_HAL_BACKEND_NAME_MAX)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    memset(backend, 0, sizeof(*backend));

    backend->backend_id = backend_id;

    memcpy(
        backend->name,
        name,
        name_length
    );

    backend->type = type;
    backend->initialized = 1;

    /*
     * Backend types are identifiers only.
     * Hardware backends are not connected yet.
     */
    backend->available =
        (type == SYNTRA_HAL_BACKEND_SOFTWARE) ? 1 : 0;

    return SYNTRA_STATUS_OK;
}

syntra_status_t hal_backend_reset(
    syntra_hal_backend_t *backend
)
{
    if (backend == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    memset(backend, 0, sizeof(*backend));

    return SYNTRA_STATUS_OK;
}

syntra_status_t hal_backend_validate(
    const syntra_hal_backend_t *backend
)
{
    if (backend == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (backend->initialized != 1 ||
        backend->backend_id == 0U ||
        !backend_type_is_valid(backend->type) ||
        backend->name[0] == '\0' ||
        memchr(
            backend->name,
            '\0',
            SYNTRA_HAL_BACKEND_NAME_MAX
        ) == NULL)
    {
        return SYNTRA_STATUS_ERROR;
    }

    if (backend->available !=
        ((backend->type == SYNTRA_HAL_BACKEND_SOFTWARE) ? 1 : 0))
    {
        return SYNTRA_STATUS_ERROR;
    }

    return SYNTRA_STATUS_OK;
}

syntra_status_t hal_backend_is_available(
    const syntra_hal_backend_t *backend,
    int *result
)
{
    if (backend == NULL || result == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (hal_backend_validate(backend) != SYNTRA_STATUS_OK)
    {
        return SYNTRA_STATUS_ERROR;
    }

    *result = backend->available;

    return SYNTRA_STATUS_OK;
}
