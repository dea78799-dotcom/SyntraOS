#include <stddef.h>

#include "exception.h"

/*
 * SyntraOS ARM64 Hardware Exception Decoder
 * Version: 1.0.0
 *
 * Decodes ARM64 exception information
 * supplied by the architecture layer.
 */

syntra_status_t syntra_arm64_exception_decode(
    uint64_t esr,
    uint64_t far,
    uint64_t elr,
    uint64_t spsr,
    syntra_arm64_exception_info_t *info
)
{
    if (info == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    info->syndrome = esr;
    info->fault_address = far;
    info->exception_return_address = elr;
    info->saved_processor_state = spsr;

    info->exception_class =
        (syntra_arm64_exception_class_t)(
            (esr >> SYNTRA_ARM64_EXCEPTION_CLASS_SHIFT)
            & SYNTRA_ARM64_EXCEPTION_CLASS_MASK
        );

    return SYNTRA_STATUS_OK;
}
