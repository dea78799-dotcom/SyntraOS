
#include "../../kernel/config/config_storage.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#define TEST_CONFIG_PATH "syntra_config_test.bin"

int main(void)
{
    syntra_system_config_t original;
    syntra_system_config_t loaded;

    int exists = 0;

    /* Initialize configuration */

    assert(config_reset() == SYNTRA_STATUS_OK);

    assert(config_get_info(&original) == SYNTRA_STATUS_OK);

    memset(&loaded, 0, sizeof(loaded));

    /* File must not exist initially */

    remove(TEST_CONFIG_PATH);

    assert(
        config_storage_exists(TEST_CONFIG_PATH, &exists)
        == SYNTRA_STATUS_OK
    );

    assert(exists == 0);

    /* Save configuration */

    assert(
        config_storage_save(TEST_CONFIG_PATH, &original)
        == SYNTRA_STATUS_OK
    );

    /* Check file existence */

    assert(
        config_storage_exists(TEST_CONFIG_PATH, &exists)
        == SYNTRA_STATUS_OK
    );

    assert(exists == 1);

    /* Load configuration */

    assert(
        config_storage_load(TEST_CONFIG_PATH, &loaded)
        == SYNTRA_STATUS_OK
    );

    /* Verify loaded data */

    assert(strcmp(loaded.os_name, original.os_name) == 0);
    assert(strcmp(loaded.version, original.version) == 0);

    assert(
        strcmp(
            loaded.architecture,
            original.architecture
        ) == 0
    );

    assert(loaded.memory_size == original.memory_size);
    assert(loaded.cpu_count == original.cpu_count);
    assert(loaded.debug_mode == original.debug_mode);
    assert(loaded.initialized == original.initialized);
    assert(loaded.valid == original.valid);

    /* Invalid arguments */

    assert(
        config_storage_save(NULL, &original)
        == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    assert(
        config_storage_save(TEST_CONFIG_PATH, NULL)
        == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    assert(
        config_storage_load(TEST_CONFIG_PATH, NULL)
        == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    assert(
        config_storage_exists(TEST_CONFIG_PATH, NULL)
        == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    /* Missing file */

    assert(remove(TEST_CONFIG_PATH) == 0);

    assert(
        config_storage_load(TEST_CONFIG_PATH, &loaded)
        == SYNTRA_STATUS_ERROR
    );

    assert(
        config_storage_exists(TEST_CONFIG_PATH, &exists)
        == SYNTRA_STATUS_OK
    );

    assert(exists == 0);

    printf("Config Storage V3: all tests passed.\n");

    return 0;
}
