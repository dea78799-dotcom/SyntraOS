#include <stdint.h>
#include <stddef.h>

#include "../../core/status.h"

/*
 * SyntraOS ARM64 Boot Stage
 * Version: 1.0.0
 */

#define SYNTRA_ARM64_DTB_ADDRESS  0x40000000ULL
#define SYNTRA_ARM64_DTB_LIMIT    0x00200000ULL
#define SYNTRA_ARM64_DTB_MAGIC   0xD00DFEEDU
#define SYNTRA_ARM64_DTB_HEADER_SIZE 40U

#define SYNTRA_ARM64_EL1_VALUE 0x4ULL

static uint32_t read_be32(const uint8_t *data)
{
    return ((uint32_t)data[0] << 24) |
           ((uint32_t)data[1] << 16) |
           ((uint32_t)data[2] << 8)  |
           (uint32_t)data[3];
}

static int validate_dtb(const uint8_t *dtb)
{
    uint32_t total_size;

    if (dtb == NULL)
    {
        return 0;
    }

    if (read_be32(dtb) != SYNTRA_ARM64_DTB_MAGIC)
    {
        return 0;
    }

    total_size = read_be32(dtb + 4);

    if (total_size < SYNTRA_ARM64_DTB_HEADER_SIZE)
    {
        return 0;
    }

    if (total_size > SYNTRA_ARM64_DTB_LIMIT)
    {
        return 0;
    }

    return 1;
}

syntra_status_t syntra_arm64_boot_main(
    uint64_t dtb_address,
    uint64_t current_el
)
{
    const uint8_t *dtb;

    if (dtb_address != SYNTRA_ARM64_DTB_ADDRESS)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (current_el != SYNTRA_ARM64_EL1_VALUE)
    {
        return SYNTRA_STATUS_NOT_IMPLEMENTED;
    }

    dtb = (const uint8_t *)(uintptr_t)dtb_address;

    if (!validate_dtb(dtb))
    {
        return SYNTRA_STATUS_ERROR;
    }

    /*
     * DTB header validation completed.
     * Full FDT parsing and hardware initialization
     * are not implemented yet.
     */

    return SYNTRA_STATUS_OK;
}
