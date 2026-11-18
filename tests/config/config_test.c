
#include "../../kernel/config/config.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    syntra_system_config_t config;

    /* Initialization tests */

    assert(
        config_validate() ==
        SYNTRA_STATUS_ERROR
    );

    assert(
        config_init() ==
        SYNTRA_STATUS_OK
    );

    assert(
        config_init() ==
        SYNTRA_STATUS_OK
    );

    /* Default configuration tests */

    assert(
        config_get_info(&config) ==
        SYNTRA_STATUS_OK
    );

    assert(strcmp(config.os_name, "SyntraOS") == 0);
    assert(strcmp(config.version, "0.1.0") == 0);
    assert(strcmp(config.architecture, "unknown") == 0);

    assert(config.memory_size == 0ULL);
    assert(config.cpu_count == 1U);
    assert(config.debug_mode == 0);
    assert(config.initialized == 1);
    assert(config.valid == 1);

    /* Name tests */

    assert(
        config_set_name("SyntraOS Mobile") ==
        SYNTRA_STATUS_OK
    );

    assert(
        config_set_name("") ==
        SYNTRA_STATUS_INVALID_ARGUMENT
    );

    assert(
        config_set_name(NULL) ==
        SYNTRA_STATUS_INVALID_ARGUMENT
    );

    /* Version tests */

    assert(
        config_set_version("1.0.0") ==
        SYNTRA_STATUS_OK
    );

    assert(
        config_set_version("") ==
        SYNTRA_STATUS_INVALID_ARGUMENT
    );

    /* Architecture tests */

    assert(
        config_set_architecture("ARM64") ==
        SYNTRA_STATUS_OK
    );

    assert(
        config_set_architecture("") ==
        SYNTRA_STATUS_INVALID_ARGUMENT
    );

    /* Memory tests */

    assert(
        config_set_memory_size(4096ULL) ==
        SYNTRA_STATUS_OK
    );

    assert(
        config_set_memory_size(0ULL) ==
        SYNTRA_STATUS_INVALID_ARGUMENT
    );

    /* CPU tests */

    assert(
        config_set_cpu_count(8U) ==
        SYNTRA_STATUS_OK
    );

    assert(
        config_set_cpu_count(0U) ==
        SYNTRA_STATUS_INVALID_ARGUMENT
    );

    /* Debug mode tests */

    assert(
        config_set_debug_mode(1) ==
        SYNTRA_STATUS_OK
    );

    assert(
        config_set_debug_mode(0) ==
        SYNTRA_STATUS_OK
    );

    assert(
        config_set_debug_mode(2) ==
        SYNTRA_STATUS_INVALID_ARGUMENT
    );

    /* Updated configuration tests */

    assert(
        config_get_info(&config) ==
        SYNTRA_STATUS_OK
    );

    assert(strcmp(config.os_name, "SyntraOS Mobile") == 0);
    assert(strcmp(config.version, "1.0.0") == 0);
    assert(strcmp(config.architecture, "ARM64") == 0);

    assert(config.memory_size == 4096ULL);
    assert(config.cpu_count == 8U);
    assert(config.debug_mode == 0);
    assert(config.valid == 1);

    /* Information validation */

    assert(
        config_get_info(NULL) ==
        SYNTRA_STATUS_INVALID_ARGUMENT
    );

    assert(
        config_validate() ==
        SYNTRA_STATUS_OK
    );

    /* Reset tests */

    assert(
        config_reset() ==
        SYNTRA_STATUS_OK
    );

    assert(
        config_get_info(&config) ==
        SYNTRA_STATUS_OK
    );

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
