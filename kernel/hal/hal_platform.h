
#ifndef SYNTRA_HAL_PLATFORM_H
#define SYNTRA_HAL_PLATFORM_H

#include "hal.h"

#define SYNTRA_HAL_PLATFORM_NAME_MAX 64U

typedef enum
{
    SYNTRA_HAL_ARCH_UNKNOWN = 0,
    SYNTRA_HAL_ARCH_ARM64,
    SYNTRA_HAL_ARCH_X86_64,
    SYNTRA_HAL_ARCH_RISCV64

} syntra_hal_architecture_t;

typedef enum
{
    SYNTRA_HAL_PLATFORM_UNKNOWN = 0,
    SYNTRA_HAL_PLATFORM_GENERIC,
    SYNTRA_HAL_PLATFORM_EMULATOR,
    SYNTRA_HAL_PLATFORM_DEVICE

} syntra_hal_platform_type_t;

typedef struct
{
    char name[SYNTRA_HAL_PLATFORM_NAME_MAX];

    syntra_hal_architecture_t architecture;

    syntra_hal_platform_type_t type;

    unsigned int cpu_count;

    unsigned int memory_size_mb;

    int initialized;

} syntra_hal_platform_t;

syntra_status_t hal_platform_init(
    syntra_hal_platform_t *platform,
    const char *name,
    syntra_hal_architecture_t architecture,
    syntra_hal_platform_type_t type,
    unsigned int cpu_count,
    unsigned int memory_size_mb
);

syntra_status_t hal_platform_reset(
    syntra_hal_platform_t *platform
);

syntra_status_t hal_platform_validate(
    const syntra_hal_platform_t *platform
);

syntra_status_t hal_platform_get_architecture(
    const syntra_hal_platform_t *platform,
    syntra_hal_architecture_t *architecture
);

syntra_status_t hal_platform_get_type(
    const syntra_hal_platform_t *platform,
    syntra_hal_platform_type_t *type
);

#endif
