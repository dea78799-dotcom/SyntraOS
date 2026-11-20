#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../kernel/gui/ramfb.h"

static uint32_t read_be32_test(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24U) | ((uint32_t)p[1] << 16U) |
           ((uint32_t)p[2] << 8U) | (uint32_t)p[3];
}

static void write_be32_test(uint8_t *p, uint32_t value)
{
    p[0] = (uint8_t)(value >> 24U);
    p[1] = (uint8_t)(value >> 16U);
    p[2] = (uint8_t)(value >> 8U);
    p[3] = (uint8_t)value;
}

static void test_find_file(void)
{
    uint8_t directory[4U + 64U * 2U] = {0};
    uint16_t selector = 0U;
    uint32_t file_size = 0U;

    write_be32_test(directory, 2U);
    write_be32_test(directory + 4U, 12U);
    directory[8U] = 0x00U;
    directory[9U] = 0x20U;
    memcpy(directory + 12U, "etc/other", sizeof("etc/other"));
    write_be32_test(directory + 68U, SYNTRA_RAMFB_CONFIG_SIZE);
    directory[72U] = 0x00U;
    directory[73U] = 0x21U;
    memcpy(directory + 76U, "etc/ramfb", sizeof("etc/ramfb"));

    assert(syntra_ramfb_find_file(
        directory, sizeof(directory), "etc/ramfb",
        &selector, &file_size) == SYNTRA_STATUS_OK);
    assert(selector == 0x21U);
    assert(file_size == SYNTRA_RAMFB_CONFIG_SIZE);
    assert(syntra_ramfb_find_file(
        directory, sizeof(directory), "etc/missing",
        &selector, &file_size) == SYNTRA_STATUS_NOT_IMPLEMENTED);
    assert(syntra_ramfb_find_file(
        directory, 4U, "etc/ramfb",
        &selector, &file_size) == SYNTRA_STATUS_ERROR);
}

static void test_encode_config(void)
{
    uint8_t config[SYNTRA_RAMFB_CONFIG_SIZE] = {0};

    assert(syntra_ramfb_encode_config(
        config, 0x40210000ULL, 640U, 480U, 2560U) == SYNTRA_STATUS_OK);
    assert(config[0] == 0x00U && config[1] == 0x00U &&
           config[2] == 0x00U && config[3] == 0x00U &&
           config[4] == 0x40U && config[5] == 0x21U &&
           config[6] == 0x00U && config[7] == 0x00U);
    assert(read_be32_test(config + 8U) == SYNTRA_RAMFB_FOURCC_XRGB8888);
    assert(read_be32_test(config + 12U) == 0U);
    assert(read_be32_test(config + 16U) == 640U);
    assert(read_be32_test(config + 20U) == 480U);
    assert(read_be32_test(config + 24U) == 2560U);

    assert(syntra_ramfb_encode_config(
        config, 0U, 640U, 480U, 2560U) ==
        SYNTRA_STATUS_INVALID_ARGUMENT);
    assert(syntra_ramfb_encode_config(
        config, 0x1000U, 640U, 480U, 100U) ==
        SYNTRA_STATUS_INVALID_ARGUMENT);
}

int main(void)
{
    test_find_file();
    test_encode_config();
    puts("PASS: QEMU ramfb directory and configuration tests");
    return 0;
}
