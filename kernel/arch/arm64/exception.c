#include <stddef.h>

#include "exception.h"
#include "../../interrupt/interrupt.h"
#include "../../scheduler/scheduler.h"

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

    info->origin = SYNTRA_ARM64_EXCEPTION_ORIGIN_UNKNOWN;
    info->exception_class =
        (syntra_arm64_exception_class_t)(
            (esr >> SYNTRA_ARM64_EXCEPTION_CLASS_SHIFT)
            & SYNTRA_ARM64_EXCEPTION_CLASS_MASK
        );
    info->vector_index = SYNTRA_ARM64_VECTOR_UNKNOWN;

    return SYNTRA_STATUS_OK;
}


syntra_status_t syntra_arm64_exception_classify_origin(
    uint64_t vector_index,
    uint64_t saved_processor_state,
    syntra_arm64_exception_origin_t *origin)
{
    uint64_t mode = saved_processor_state & 0xFULL;
    syntra_arm64_exception_origin_t decoded;

    if (origin == NULL || vector_index >= 16U)
        return SYNTRA_STATUS_INVALID_ARGUMENT;

    if (vector_index <= SYNTRA_ARM64_VECTOR_CURRENT_SP0_SERROR)
    {
        if (mode != 0x4U) return SYNTRA_STATUS_INVALID_ARGUMENT;
        decoded = SYNTRA_ARM64_EXCEPTION_ORIGIN_EL1_SP0;
    }
    else if (vector_index <= SYNTRA_ARM64_VECTOR_CURRENT_SPX_SERROR)
    {
        if (mode != 0x5U) return SYNTRA_STATUS_INVALID_ARGUMENT;
        decoded = SYNTRA_ARM64_EXCEPTION_ORIGIN_EL1_SPX;
    }
    else if (vector_index <= SYNTRA_ARM64_VECTOR_LOWER_AARCH64_SERROR)
    {
        if (mode != 0x0U) return SYNTRA_STATUS_INVALID_ARGUMENT;
        decoded = SYNTRA_ARM64_EXCEPTION_ORIGIN_EL0_AARCH64;
    }
    else
    {
        decoded = SYNTRA_ARM64_EXCEPTION_ORIGIN_LOWER_AARCH32;
    }

    *origin = decoded;
    return decoded == SYNTRA_ARM64_EXCEPTION_ORIGIN_LOWER_AARCH32
        ? SYNTRA_STATUS_NOT_IMPLEMENTED
        : SYNTRA_STATUS_OK;
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

    status = syntra_arm64_exception_classify_origin(
        context->vector_index, context->spsr_el1,
        &last_exception_info.origin);
    if (status != SYNTRA_STATUS_OK)
        return status;

    /*
     * EL0 SVC/IRQ/abort dispatch is intentionally fail-closed until the
     * syscall ABI, user fault policy, and user-thread return validation exist.
     * Do not set either assembly return marker for a lower-EL exception.
     */
    if (last_exception_info.origin ==
        SYNTRA_ARM64_EXCEPTION_ORIGIN_EL0_AARCH64)
        return SYNTRA_STATUS_NOT_IMPLEMENTED;

    if (context->vector_index == SYNTRA_ARM64_VECTOR_CURRENT_SPX_IRQ)
    {
        syntra_status_t irq_status = interrupt_handle_pending();
        if (irq_status != SYNTRA_STATUS_OK) return irq_status;

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


/* ARM64 exception-to-scheduler bridge. */
#define SYNTRA_ARM64_BOOT_THREAD_ID 1U
static int scheduler_context_switching_active;

syntra_arm64_exception_context_t *
syntra_arm64_exception_handle_and_select(
    syntra_arm64_exception_context_t *context
)
{
    syntra_status_t status;
    syntra_thread_t *current;
    syntra_thread_t *next;

    if (context == NULL) return NULL;

    status = syntra_arm64_exception_handle(context);
    if (status != SYNTRA_STATUS_OK) return NULL;

    if (context->vector_index == SYNTRA_ARM64_VECTOR_CURRENT_SPX_SYNC &&
        last_exception_info.exception_class == SYNTRA_ARM64_EXCEPTION_SVC_AARCH64 &&
        (context->esr_el1 & 0xFFFFU) ==
            SYNTRA_ARM64_SCHEDULER_START_SVC_IMMEDIATE)
    {
        syntra_thread_t *boot_thread =
            scheduler_get_thread(SYNTRA_ARM64_BOOT_THREAD_ID);

        if (scheduler_context_switching_active ||
            boot_thread == NULL ||
            boot_thread->state != THREAD_BLOCKED)
            return NULL;

        boot_thread->cpu_context = context;
        context->reserved = SYNTRA_ARM64_IRQ_RETURN_MAGIC;
        scheduler_context_switching_active = 1;

        next = scheduler_schedule_next();
        if (next == NULL || next->cpu_context == NULL)
        {
            scheduler_context_switching_active = 0;
            return NULL;
        }
        return (syntra_arm64_exception_context_t *)next->cpu_context;
    }

    if (scheduler_context_switching_active &&
        context->vector_index == SYNTRA_ARM64_VECTOR_CURRENT_SPX_IRQ)
    {
        current = scheduler_get_current();
        if (current != NULL) current->cpu_context = context;

        next = scheduler_schedule_next();
        if (next == NULL || next->cpu_context == NULL) return NULL;
        return (syntra_arm64_exception_context_t *)next->cpu_context;
    }

    return NULL;
}
