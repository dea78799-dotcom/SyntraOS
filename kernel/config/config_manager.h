
#ifndef SYNTRA_CONFIG_MANAGER_H
#define SYNTRA_CONFIG_MANAGER_H

#include "config.h"

typedef struct
{
    unsigned long update_count;
    int dirty;
    int initialized;
} syntra_config_manager_t;

/* Manager lifecycle */

syntra_status_t config_manager_init(void);

syntra_status_t config_manager_reset(void);

/* Configuration management */

syntra_status_t config_manager_update(
    const syntra_system_config_t *config
);

syntra_status_t config_manager_get_config(
    syntra_system_config_t *config
);

syntra_status_t config_manager_get_info(
    syntra_config_manager_t *manager
);

/* Change tracking */

syntra_status_t config_manager_mark_clean(void);

syntra_status_t config_manager_is_dirty(
    int *dirty
);

#endif
