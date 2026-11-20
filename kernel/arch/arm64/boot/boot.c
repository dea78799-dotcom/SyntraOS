#include <stdint.h>
#include <stddef.h>

#include "../../core/status.h"
#include "fdt.h"

/*
 * SyntraOS ARM64 Boot Stage
 * Version: 1.1.0
 */

#define SYNTRA_ARM64_DTB_ADDRESS  0x40000000ULL
#define SYNTRA_ARM64_DTB_LIMIT    0x00200000ULL
#define SYNTRA_ARM64_EL1_VALUE    0x4ULL

static int validate_dtb(const void *dtb)
{
    syntra_fdt_t fdt;
    syntra_fdt_iterator_t iterator;
    syntra_fdt_token_t token;
    syntra_status_t status;
    int found_end = 0;

    status = syntra_fdt_open(
        dtb,
        (size_t)SYNTRA_ARM64_DTB_LIMIT,
        &fdt
    );

    if (status != SYNTRA_STATUS_OK)
    {
        return 0;
    }

    syntra_fdt_iterator_init(&iterator);

    for (;;)
    {
        status = syntra_fdt_next(&fdt, &iterator, &token);

        if (status == SYNTRA_STATUS_NOT_IMPLEMENTED)
        {
            break;
        }

        if (status != SYNTRA_STATUS_OK)
        {
            return 0;
        }

        if (token.type == SYNTRA_FDT_TOKEN_END)
        {
            found_end = 1;
            break;
        }
    }

    return found_end;
}

syntra_status_t syntra_arm64_boot_main(
    uint64_t dtb_address,
    uint64_t current_el
)
{
    const void *dtb;

    if (dtb_address != SYNTRA_ARM64_DTB_ADDRESS)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (current_el != SYNTRA_ARM64_EL1_VALUE)
    {
        return SYNTRA_STATUS_NOT_IMPLEMENTED;
    }

    dtb = (const void *)(uintptr_t)dtb_address;

    if (!validate_dtb(dtb))
    {
        return SYNTRA_STATUS_ERROR;
    }

    /*
     * The FDT header and complete structure token stream are validated.
     * Device discovery and hardware initialization remain separate stages.
     */

    return SYNTRA_STATUS_OK;
}
