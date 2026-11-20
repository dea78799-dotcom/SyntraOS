#include <stddef.h>

#include "registers.h"

/*
 * SyntraOS ARM64 Register Decoder
 * Version: 1.1.0
 *
 * Decodes values supplied by the architecture layer.
 * This implementation does not access CPU system registers.
 */

syntra_status_t syntra_arm64_register_decode_current_el(
    uint64_t current_el,
    syntra_arm64_exception_level_t *level
)
{
    uint64_t decoded_level;

    if (level == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    decoded_level =
        (current_el >> SYNTRA_ARM64_CURRENT_EL_SHIFT)
        & SYNTRA_ARM64_CURRENT_EL_MASK;

    switch (decoded_level)
    {
        case SYNTRA_ARM64_EL0:
            *level = SYNTRA_ARM64_MODE_EL0;
            return SYNTRA_STATUS_OK;

        case SYNTRA_ARM64_EL1:
            *level = SYNTRA_ARM64_MODE_EL1;
            return SYNTRA_STATUS_OK;

        case SYNTRA_ARM64_EL2:
            *level = SYNTRA_ARM64_MODE_EL2;
            return SYNTRA_STATUS_OK;

        case SYNTRA_ARM64_EL3:
            *level = SYNTRA_ARM64_MODE_EL3;
            return SYNTRA_STATUS_OK;

        default:
            return SYNTRA_STATUS_ERROR;
    }
}
