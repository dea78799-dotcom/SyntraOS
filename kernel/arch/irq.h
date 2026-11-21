#ifndef SYNTRA_ARCH_IRQ_H
#define SYNTRA_ARCH_IRQ_H

#include <stdint.h>

/*
 * Save the current CPU interrupt mask and mask IRQ exceptions locally.
 *
 * ARM64 uses DAIF.I. Restoring the saved DAIF value preserves the caller's
 * previous interrupt state, including nested critical sections.
 *
 * Non-ARM host builds use a no-op shim for unit tests only. A real non-ARM
 * kernel target must provide an architecture-specific implementation before
 * it can safely use interrupt-protected kernel data structures.
 */
typedef uint64_t syntra_irq_state_t;

static inline syntra_irq_state_t syntra_irq_save(void)
{
#if defined(__aarch64__)
    syntra_irq_state_t state;
    __asm__ volatile(
        "mrs %0, daif\n"
        "msr daifset, #2"
        : "=r"(state)
        :
        : "memory"
    );
    return state;
#else
    __asm__ volatile("" ::: "memory");
    return 0U;
#endif
}

static inline void syntra_irq_restore(syntra_irq_state_t state)
{
#if defined(__aarch64__)
    __asm__ volatile(
        "msr daif, %0"
        :
        : "r"(state)
        : "memory"
    );
#else
    (void)state;
    __asm__ volatile("" ::: "memory");
#endif
}

#endif
