
#include "../../kernel/config/config.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    syntra_system_config_t config;

    /* Initialization tests */

    assert(config_validate() == SYNTRA_STATUS_ERROR);
    assert(config_init() == SYNTRA_STATUS_OK);
    assert(config_init() == SYNTRA_STATUS_OK);

    /* Default configuration tests */

    assert(config_get_info(&config) == SYNTRA_STATUS_OK);

    assert(strcmp(config.os_name, "SyntraOS") == 0);
    assert(strcmp(config.version, "0.1.0") == 0);
    assert(strcmp(config.architecture, "unknown") == 0);

    assert(config.memory_size == 0ULL);
    assert(config.cpu_count == 1U);
    assert(config.debug_mode == 0);
    assert(config.initialized == 1);
    assert(config.valid == 1);

    /* Name tests */

    assert(config_set_name("SyntraOS Mobile") == SYNTRA_STATUS_OK);
    assert(config_set_name("") == SYNTRA_STATUS_INVALID_ARGUMENT);
    assert(config_set_name(NULL) == SYNTRA_STATUS_INVALID_ARGUMENT);

    /* Name boundary tests */

    char long_name[SYNTRA_CONFIG_NAME_MAX + 1U];

    memset(long_name, 'A', sizeof(long_name));
    long_name[sizeof(long_name) - 1U] = '\0';

    assert(config_set_name(long_name) == SYNTRA_STATUS_INVALID_ARGUMENT);
    assert(config_get_info(&config) == SYNTRA_STATUS_OK);
    assert(strcmp(config.os_name, "SyntraOS Mobile") == 0);

    /* Version tests */

    assert(config_set_version("1.0.0") == SYNTRA_STATUS_OK);
    assert(config_set_version("") == SYNTRA_STATUS_INVALID_ARGUMENT);

    /* Version boundary tests */

    char long_version[SYNTRA_CONFIG_VERSION_MAX + 1U];

    memset(long_version, 'B', sizeof(long_version));
    long_version[sizeof(long_version) - 1U] = '\0';

    assert(config_set_version(long_version) == SYNTRA_STATUS_INVALID_ARGUMENT);

    assert(config_get_info(&config) == SYNTRA_STATUS_OK);
    assert(strcmp(config.version, "1.0.0") == 0);

    /* Architecture tests */

    assert(config_set_architecture("ARM64") == SYNTRA_STATUS_OK);
    assert(config_set_architecture("") == SYNTRA_STATUS_INVALID_ARGUMENT);

    /* Architecture boundary tests */

    char long_architecture[SYNTRA_CONFIG_ARCH_MAX + 1U];

    memset(long_architecture, 'C', sizeof(long_architecture));
    long_architecture[sizeof(long_architecture) - 1U] = '\0';

    assert(config_set_architecture(long_architecture) == SYNTRA_STATUS_INVALID_ARGUMENT);

    assert(config_get_info(&config) == SYNTRA_STATUS_OK);
    assert(strcmp(config.architecture, "ARM64") == 0);

    /* Memory tests */

    assert(config_set_memory_size(4096ULL) == SYNTRA_STATUS_OK);
    assert(config_set_memory_size(0ULL) == SYNTRA_STATUS_INVALID_ARGUMENT);

    /* CPU tests */

    assert(config_set_cpu_count(8U) == SYNTRA_STATUS_OK);
    assert(config_set_cpu_count(0U) == SYNTRA_STATUS_INVALID_ARGUMENT);

    /* Debug mode tests */

    assert(config_set_debug_mode(1) == SYNTRA_STATUS_OK);
    assert(config_set_debug_mode(0) == SYNTRA_STATUS_OK);
    assert(config_set_debug_mode(2) == SYNTRA_STATUS_INVALID_ARGUMENT);

    /* Updated configuration tests */

    assert(config_get_info(&config) == SYNTRA_STATUS_OK);

    assert(strcmp(config.os_name, "SyntraOS Mobile") == 0);
    assert(strcmp(config.version, "1.0.0") == 0);
    assert(strcmp(config.architecture, "ARM64") == 0);

    assert(config.memory_size == 4096ULL);
    assert(config.cpu_count == 8U);
    assert(config.debug_mode == 0);
    assert(config.valid == 1);

    /* Information validation */

    assert(config_get_info(NULL) == SYNTRA_STATUS_INVALID_ARGUMENT);
    assert(config_validate() == SYNTRA_STATUS_OK);

    /* Reset tests */

    assert(config_reset() == SYNTRA_STATUS_OK);
    assert(config_get_info(&config) == SYNTRA_STATUS_OK);

    assert(strcmp(config.os_name, "SyntraOS") == 0);
    assert(strcmp(config.version, "0.1.0") == 0);
    assert(strcmp(config.architecture, "unknown") == 0);

    assert(config.memory_size == 0ULL);
    assert(config.cpu_count == 1U);
    assert(config.debug_mode == 0);
    assert(config.valid == 1);

    printf("System Configuration V1: all tests passed.\n");

    return 0;
}
