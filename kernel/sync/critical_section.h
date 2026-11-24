#ifndef SYNTRA_CRITICAL_SECTION_H
#define SYNTRA_CRITICAL_SECTION_H

#include <stdint.h>

/* Single-core IRQ critical sections; not an SMP lock. */
typedef uint64_t syntra_irq_state_t;

syntra_irq_state_t syntra_irq_save(void);
void syntra_irq_restore(syntra_irq_state_t saved_state);

#if defined(SYNTRA_CRITICAL_SECTION_TESTING)
void syntra_critical_section_test_set_daif(uint64_t simulated_daif);
uint64_t syntra_critical_section_test_get_daif(void);
#endif

#endif
