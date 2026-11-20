#include <stddef.h>

#include "exception.h"

/*
 * SyntraOS ARM64 Hardware Exception Handler
 * Version: 1.1.0
 *
 * Captures the architecture-provided exception frame for diagnostics.
 * This is not yet a recoverable exception or IRQ dispatcher.
 */

static syntra_arm64_exception_context_t last_exception_context;
static syntra_arm64_exception_info_t last_exception_info;
static int last_exception_valid;

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

syntra_status_t syntra_arm64_exception_handle(
    const syntra_arm64_exception_context_t *context
)
{
    if (context == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    last_exception_valid = 0;

    for (unsigned int i = 0U; i < 31U; i++)
    {
        last_exception_context.registers[i] =
            context->registers[i];
    }

    last_exception_context.sp_el0 = context->sp_el0;
    last_exception_context.elr_el1 = context->elr_el1;
    last_exception_context.spsr_el1 = context->spsr_el1;
    last_exception_context.esr_el1 = context->esr_el1;
    last_exception_context.far_el1 = context->far_el1;

    syntra_status_t status = syntra_arm64_exception_decode(
        context->esr_el1,
        context->far_el1,
        context->elr_el1,
        context->spsr_el1,
        &last_exception_info
    );

    if (status != SYNTRA_STATUS_OK)
    {
        return status;
    }

    last_exception_valid = 1;

    return SYNTRA_STATUS_OK;
}

const syntra_arm64_exception_context_t *
syntra_arm64_exception_get_last_context(void)
{
    if (!last_exception_valid)
    {
        return NULL;
    }

    return &last_exception_context;
}

const syntra_arm64_exception_info_t *
syntra_arm64_exception_get_last_info(void)
{
    if (!last_exception_valid)
    {
        return NULL;
    }

    return &last_exception_info;
}

int syntra_arm64_exception_has_last_context(void)
{
    return last_exception_valid;
}
