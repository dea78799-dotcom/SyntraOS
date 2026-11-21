#include "critical_section.h"

#define SYNTRA_DAIF_IRQ_MASK ((uint64_t)1U << 7U)

#if defined(__aarch64__)

syntra_irq_state_t syntra_irq_save(void)
{
    syntra_irq_state_t saved_state;
    __asm__ volatile(
        "mrs %0, daif\n"
        "msr daifset, #2\n"
        "isb\n"
        : "=r"(saved_state) : : "memory"
    );
    return saved_state;
}

void syntra_irq_restore(syntra_irq_state_t saved_state)
{
    __asm__ volatile(
        "msr daif, %0\n"
        "isb\n"
        : : "r"(saved_state) : "memory"
    );
}

#else

/* Host-side simulation only; no actual hardware IRQ masking is provided. */
static uint64_t simulated_daif;

syntra_irq_state_t syntra_irq_save(void)
{
    syntra_irq_state_t saved_state = simulated_daif;
    simulated_daif |= SYNTRA_DAIF_IRQ_MASK;
    return saved_state;
}

void syntra_irq_restore(syntra_irq_state_t saved_state)
{
    simulated_daif = saved_state;
}

#if defined(SYNTRA_CRITICAL_SECTION_TESTING)
void syntra_critical_section_test_set_daif(uint64_t simulated_state)
{
    simulated_daif = simulated_state;
}

uint64_t syntra_critical_section_test_get_daif(void)
{
    return simulated_daif;
}
#endif
#endif
