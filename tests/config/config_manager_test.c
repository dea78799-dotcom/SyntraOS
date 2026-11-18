
#include "../../kernel/config/config_manager.h"

#include <assert.h>
#include <stdio.h>

int main(void)
{
    syntra_system_config_t config;
    syntra_system_config_t updated_config;
    syntra_system_config_t result_config;

    syntra_config_manager_t manager;

    int dirty = 0;

    /* Initialization */

    assert(config_manager_reset() == SYNTRA_STATUS_OK);
    assert(config_manager_init() == SYNTRA_STATUS_OK);

    /* Initial state */

    assert(config_manager_get_info(&manager) == SYNTRA_STATUS_OK);

    assert(manager.initialized == 1);
    assert(manager.update_count == 0UL);
    assert(manager.dirty == 0);

    /* Read initial configuration */

    assert(config_manager_get_config(&config) == SYNTRA_STATUS_OK);

    assert(config.os_name[0] != '\0');
    assert(config.version[0] != '\0');
    assert(config.architecture[0] != '\0');

    /* Dirty state */

    assert(config_manager_is_dirty(&dirty) == SYNTRA_STATUS_OK);
    assert(dirty == 0);

    /* Update configuration */

    updated_config = config;

    updated_config.debug_mode = 1;

    assert(config_manager_update(&updated_config) == SYNTRA_STATUS_OK);

    assert(config_manager_get_config(&result_config) == SYNTRA_STATUS_OK);
    assert(result_config.debug_mode == 1);

    assert(config_manager_get_info(&manager) == SYNTRA_STATUS_OK);

    assert(manager.update_count == 1UL);
    assert(manager.dirty == 1);

    /* Identical update must not increase update_count */

    assert(config_manager_update(&updated_config) == SYNTRA_STATUS_OK);

    assert(config_manager_get_info(&manager) == SYNTRA_STATUS_OK);
    assert(manager.update_count == 1UL);

    /* Invalid arguments */

    assert(
        config_manager_update(NULL) ==
        SYNTRA_STATUS_INVALID_ARGUMENT
    );

    assert(
        config_manager_get_config(NULL) ==
        SYNTRA_STATUS_INVALID_ARGUMENT
    );

    assert(
        config_manager_get_info(NULL) ==
        SYNTRA_STATUS_INVALID_ARGUMENT
    );

    assert(
        config_manager_is_dirty(NULL) ==
        SYNTRA_STATUS_INVALID_ARGUMENT
    );

    /* Mark clean */

    assert(config_manager_mark_clean() == SYNTRA_STATUS_OK);

    assert(config_manager_is_dirty(&dirty) == SYNTRA_STATUS_OK);
    assert(dirty == 0);

    /* Reset manager */

    assert(config_manager_reset() == SYNTRA_STATUS_OK);

    assert(config_manager_get_info(&manager) == SYNTRA_STATUS_OK);

    assert(manager.initialized == 1);
    assert(manager.update_count == 0UL);
    assert(manager.dirty == 0);

    printf("Config Manager V2: all tests passed.\n");

    return 0;
}
