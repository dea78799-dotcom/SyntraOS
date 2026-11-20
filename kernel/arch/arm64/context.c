#include "context.h"

#include "../../scheduler/scheduler.h"

static uint64_t syntra_arm64_entry_address(void (*function)(void *))
{
    union
    {
        void (*function)(void *);
        uint64_t address;
    } conversion;

    conversion.function = function;
    return conversion.address;
}

static uint64_t syntra_arm64_exit_address(void)
{
    union
    {
        void (*function)(void);
        uint64_t address;
    } conversion;

    conversion.function = syntra_arm64_thread_exit;
    return conversion.address;
}

syntra_status_t syntra_arm64_context_initialize(
    void *stack,
    size_t stack_size,
    void (*entry)(void *),
    void *argument,
    syntra_arm64_exception_context_t **context
)
{
    uintptr_t stack_base = (uintptr_t)stack;
    uintptr_t stack_top;
    uintptr_t frame_address;
    syntra_arm64_exception_context_t *frame;

    if (stack == NULL || entry == NULL || context == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if ((stack_base & 15U) != 0U ||
        stack_size < SYNTRA_ARM64_CONTEXT_FRAME_SIZE ||
        stack_base > UINTPTR_MAX - stack_size)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    stack_top = (stack_base + stack_size) & ~(uintptr_t)15U;
    if (stack_top < stack_base ||
        stack_top - stack_base < SYNTRA_ARM64_CONTEXT_FRAME_SIZE)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    frame_address = stack_top - SYNTRA_ARM64_CONTEXT_FRAME_SIZE;
    frame = (syntra_arm64_exception_context_t *)frame_address;

    /* Clear both the architectural frame and the assembly scratch slot. */
    for (size_t i = 0U; i < SYNTRA_ARM64_CONTEXT_FRAME_SIZE; ++i)
    {
        ((uint8_t *)frame)[i] = 0U;
    }

    frame->registers[0] = (uint64_t)(uintptr_t)argument;
    frame->registers[30] = syntra_arm64_exit_address();
    frame->elr_el1 = syntra_arm64_entry_address(entry);
    frame->spsr_el1 = 0x5U; /* EL1h; DAIF unmasked on exception return. */
    frame->vector_index = SYNTRA_ARM64_VECTOR_CURRENT_SPX_IRQ;
    frame->reserved = SYNTRA_ARM64_IRQ_RETURN_MAGIC;

    *context = frame;
    return SYNTRA_STATUS_OK;
}

void syntra_arm64_thread_exit(void)
{
    (void)scheduler_terminate_current();

#if defined(__aarch64__)
    for (;;)
    {
        __asm__ volatile("wfe" ::: "memory");
    }
#endif
}
