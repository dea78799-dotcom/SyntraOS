
#ifndef SYNTRA_CONFIG_STORAGE_H
#define SYNTRA_CONFIG_STORAGE_H

#include "config.h"

/*
 * SyntraOS Config Storage
 * Version: 3.0.0
 */

#define SYNTRA_CONFIG_STORAGE_PATH_MAX 256U

syntra_status_t config_storage_save(
    const char *path,
    const syntra_system_config_t *config
);

syntra_status_t config_storage_load(
    const char *path,
    syntra_system_config_t *config
);

syntra_status_t config_storage_exists(
    const char *path,
    int *exists
);

#endif
