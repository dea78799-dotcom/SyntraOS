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


static uint64_t syntra_arm64_return_address(void (*function)(void))
{
    union
    {
        void (*function)(void);
        uint64_t address;
    } conversion;
    conversion.function = function;
    return conversion.address;
}

/*
 * Construct an EL0t frame on the kernel stack with a distinct user stack.
 * This only prepares state; mappings and scheduler integration are separate.
 */
syntra_status_t syntra_arm64_user_context_initialize(
    void *kernel_stack, size_t kernel_stack_size,
    void *user_stack, size_t user_stack_size,
    void (*entry)(void *), void (*return_trampoline)(void),
    void *argument, syntra_arm64_exception_context_t **context
)
{
    uintptr_t kb = (uintptr_t)kernel_stack, ub = (uintptr_t)user_stack;
    uintptr_t ke, ue, kt, ut, frame_address;
    syntra_arm64_exception_context_t *frame;

    if (kernel_stack == NULL || user_stack == NULL || entry == NULL ||
        return_trampoline == NULL || context == NULL ||
        (kb & 15U) != 0U || (ub & 15U) != 0U ||
        kernel_stack_size < SYNTRA_ARM64_CONTEXT_FRAME_SIZE ||
        user_stack_size < 16U ||
        kb > UINTPTR_MAX - kernel_stack_size ||
        ub > UINTPTR_MAX - user_stack_size)
        return SYNTRA_STATUS_INVALID_ARGUMENT;

    ke = kb + kernel_stack_size;
    ue = ub + user_stack_size;
    if (kb < ue && ub < ke) return SYNTRA_STATUS_INVALID_ARGUMENT;

    kt = ke & ~(uintptr_t)15U;
    ut = ue & ~(uintptr_t)15U;
    if (kt < kb || kt - kb < SYNTRA_ARM64_CONTEXT_FRAME_SIZE || ut <= ub)
        return SYNTRA_STATUS_INVALID_ARGUMENT;

    frame_address = kt - SYNTRA_ARM64_CONTEXT_FRAME_SIZE;
    frame = (syntra_arm64_exception_context_t *)frame_address;
    for (size_t i = 0U; i < SYNTRA_ARM64_CONTEXT_FRAME_SIZE; ++i)
        ((uint8_t *)frame)[i] = 0U;

    frame->registers[0] = (uint64_t)(uintptr_t)argument;
    frame->registers[30] = syntra_arm64_return_address(return_trampoline);
    frame->sp_el0 = (uint64_t)ut;
    frame->elr_el1 = syntra_arm64_entry_address(entry);
    frame->spsr_el1 = 0x3C0U; /* EL0t, with DAIF masked on first entry. */
    frame->vector_index = SYNTRA_ARM64_VECTOR_LOWER_AARCH64_SYNC;
    frame->reserved = SYNTRA_ARM64_EXCEPTION_RETURN_MAGIC;
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
