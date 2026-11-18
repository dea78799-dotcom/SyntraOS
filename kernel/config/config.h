
#ifndef SYNTRA_CONFIG_H
#define SYNTRA_CONFIG_H

#include "../core/status.h"

#define SYNTRA_CONFIG_NAME_MAX 64U
#define SYNTRA_CONFIG_VERSION_MAX 32U
#define SYNTRA_CONFIG_ARCH_MAX 16U

typedef struct
{
    char os_name[SYNTRA_CONFIG_NAME_MAX];

    char version[SYNTRA_CONFIG_VERSION_MAX];

    char architecture[SYNTRA_CONFIG_ARCH_MAX];

    unsigned long long memory_size;

    unsigned int cpu_count;

    int debug_mode;
    int initialized;
    int valid;

} syntra_system_config_t;

/* Core API */

syntra_status_t config_init(void);

syntra_status_t config_reset(void);

/* Configuration API */

syntra_status_t config_set_name(
    const char *name
);

syntra_status_t config_set_version(
    const char *version
);

syntra_status_t config_set_architecture(
    const char *architecture
);

syntra_status_t config_set_memory_size(
    unsigned long long memory_size
);

syntra_status_t config_set_cpu_count(
    unsigned int cpu_count
);

syntra_status_t config_set_debug_mode(
    int enabled
);

/* Information API */

syntra_status_t config_get_info(
    syntra_system_config_t *config
);

/* Validation API */

syntra_status_t config_validate(void);

#endif
