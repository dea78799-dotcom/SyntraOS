
#include "config_storage.h"

#include <stdio.h>
#include <string.h>
#include <stdint.h>

#define CONFIG_STORAGE_MAGIC 0x53594E54U
#define CONFIG_STORAGE_VERSION 1U

typedef struct
{
    uint32_t magic;
    uint32_t version;
    uint32_t checksum;
    syntra_system_config_t config;
} config_storage_record_t;

static uint32_t calculate_checksum(
    const unsigned char *data,
    size_t size
)
{
    uint32_t hash = 2166136261U;

    for (size_t i = 0U; i < size; i++)
    {
        hash ^= data[i];
        hash *= 16777619U;
    }

    return hash;
}

static int config_is_valid(
    const syntra_system_config_t *config
)
{
    if (config == NULL)
    {
        return 0;
    }

    if (config->initialized != 1 ||
        config->valid != 1 ||
        config->cpu_count == 0U ||
        config->os_name[0] == '\0' ||
        config->version[0] == '\0' ||
        config->architecture[0] == '\0' ||
        (config->debug_mode != 0 &&
         config->debug_mode != 1))
    {
        return 0;
    }

    if (memchr(config->os_name, '\0',
               sizeof(config->os_name)) == NULL ||
        memchr(config->version, '\0',
               sizeof(config->version)) == NULL ||
        memchr(config->architecture, '\0',
               sizeof(config->architecture)) == NULL)
    {
        return 0;
    }

    return 1;
}

syntra_status_t config_storage_save(
    const char *path,
    const syntra_system_config_t *config
)
{
    if (path == NULL || path[0] == '\0' ||
        !config_is_valid(config))
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    config_storage_record_t record = {0};

    record.magic = CONFIG_STORAGE_MAGIC;
    record.version = CONFIG_STORAGE_VERSION;
    record.config = *config;

    record.checksum = calculate_checksum(
        (const unsigned char *)&record.config,
        sizeof(record.config)
    );

    FILE *file = fopen(path, "wb");

    if (file == NULL)
    {
        return SYNTRA_STATUS_ERROR;
    }

    size_t written = fwrite(
        &record,
        sizeof(record),
        1U,
        file
    );

    int close_result = fclose(file);

    if (written != 1U || close_result != 0)
    {
        return SYNTRA_STATUS_ERROR;
    }

    return SYNTRA_STATUS_OK;
}

syntra_status_t config_storage_load(
    const char *path,
    syntra_system_config_t *config
)
{
    if (path == NULL || path[0] == '\0' ||
        config == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    FILE *file = fopen(path, "rb");

    if (file == NULL)
    {
        return SYNTRA_STATUS_ERROR;
    }

    config_storage_record_t record = {0};

    size_t read_count = fread(
        &record,
        sizeof(record),
        1U,
        file
    );

    int extra_data = fgetc(file);
    int close_result = fclose(file);

    if (read_count != 1U ||
        extra_data != EOF ||
        close_result != 0)
    {
        return SYNTRA_STATUS_ERROR;
    }

    if (record.magic != CONFIG_STORAGE_MAGIC ||
        record.version != CONFIG_STORAGE_VERSION)
    {
        return SYNTRA_STATUS_ERROR;
    }

    uint32_t expected_checksum = calculate_checksum(
        (const unsigned char *)&record.config,
        sizeof(record.config)
    );

    if (record.checksum != expected_checksum ||
        !config_is_valid(&record.config))
    {
        return SYNTRA_STATUS_ERROR;
    }

    *config = record.config;

    return SYNTRA_STATUS_OK;
}

syntra_status_t config_storage_exists(
    const char *path,
    int *exists
)
{
    if (path == NULL || path[0] == '\0' ||
        exists == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    FILE *file = fopen(path, "rb");

    if (file == NULL)
    {
        *exists = 0;
        return SYNTRA_STATUS_OK;
    }

    *exists = 1;

    if (fclose(file) != 0)
    {
        return SYNTRA_STATUS_ERROR;
    }

    return SYNTRA_STATUS_OK;
}
