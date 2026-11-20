#include <stddef.h>

#include "exception.h"
#include "gic.h"
#include "timer.h"

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
    info->vector_index = SYNTRA_ARM64_VECTOR_UNKNOWN;

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
    last_exception_context.vector_index = context->vector_index;
    last_exception_context.reserved = context->reserved;

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

    if (context->vector_index >= 16U)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    last_exception_info.vector_index =
        (syntra_arm64_vector_index_t)context->vector_index;

    if (context->vector_index == SYNTRA_ARM64_VECTOR_CURRENT_SPX_IRQ)
    {
        uint32_t interrupt_id;
        syntra_status_t irq_status =
            syntra_arm64_gic_acknowledge(&interrupt_id);
        if (irq_status != SYNTRA_STATUS_OK) return irq_status;

        if (interrupt_id == 30U) syntra_arm64_timer_handle_irq();

        if (interrupt_id < 1020U)
        {
            irq_status = syntra_arm64_gic_end_of_interrupt(interrupt_id);
            if (irq_status != SYNTRA_STATUS_OK) return irq_status;
        }

        ((syntra_arm64_exception_context_t *)context)->reserved =
            SYNTRA_ARM64_IRQ_RETURN_MAGIC;
        last_exception_context.reserved = SYNTRA_ARM64_IRQ_RETURN_MAGIC;
    }
    else if (context->vector_index == SYNTRA_ARM64_VECTOR_CURRENT_SPX_SYNC
        && last_exception_info.exception_class == SYNTRA_ARM64_EXCEPTION_SVC_AARCH64
        && (context->esr_el1 & 0xFFFFU)
            == SYNTRA_ARM64_EXCEPTION_TEST_SVC_IMMEDIATE)
    {
        ((syntra_arm64_exception_context_t *)context)->reserved =
            SYNTRA_ARM64_EXCEPTION_RETURN_MAGIC;
        last_exception_context.reserved =
            SYNTRA_ARM64_EXCEPTION_RETURN_MAGIC;
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
