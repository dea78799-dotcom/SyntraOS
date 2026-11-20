#ifndef SYNTRA_ARM64_CONTEXT_H
#define SYNTRA_ARM64_CONTEXT_H

#include <stddef.h>
#include <stdint.h>

#include "../../core/status.h"
#include "exception.h"

/*
 * An exception entry allocates 16 bytes for the original x0 scratch slot
 * and 304 bytes for syntra_arm64_exception_context_t.
 */
#define SYNTRA_ARM64_CONTEXT_SCRATCH_SIZE 16U
#define SYNTRA_ARM64_CONTEXT_FRAME_SIZE \
    (SYNTRA_ARM64_CONTEXT_SIZE + SYNTRA_ARM64_CONTEXT_SCRATCH_SIZE)

/*
 * Build the initial EL1h exception frame for a new kernel thread.
 * The stack is caller-owned and must be 16-byte aligned.
 *
 * The returned frame pointer is intended for the exception-return path.
 * This function does not switch the CPU to the new thread.
 */
syntra_status_t syntra_arm64_context_initialize(
    void *stack,
    size_t stack_size,
    void (*entry)(void *),
    void *argument,
    syntra_arm64_exception_context_t **context
);

/*
 * Prepare an EL0 exception-return frame on a dedicated kernel stack.
 * User entry, stack, and return trampoline must be mapped with EL0
 * permissions before this frame is selected. This does not enter EL0.
 */
syntra_status_t syntra_arm64_user_context_initialize(
    void *kernel_stack, size_t kernel_stack_size,
    void *user_stack, size_t user_stack_size,
    void (*entry)(void *), void (*return_trampoline)(void),
    void *argument, syntra_arm64_exception_context_t **context
);

/* Entry used as x30 when a kernel thread function returns. */

void syntra_arm64_thread_exit(void);

#endif
