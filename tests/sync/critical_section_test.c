#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "../../kernel/sync/critical_section.h"

#define IRQ_MASK ((uint64_t)1U << 7U)
#define FIQ_MASK ((uint64_t)1U << 6U)
#define ABORT_MASK ((uint64_t)1U << 8U)
#define DEBUG_MASK ((uint64_t)1U << 9U)

int main(void)
{
    syntra_irq_state_t outer, inner;

    syntra_critical_section_test_set_daif(0U);
    outer = syntra_irq_save();
    assert(outer == 0U);
    assert((syntra_critical_section_test_get_daif() & IRQ_MASK) != 0U);

    inner = syntra_irq_save();
    assert((inner & IRQ_MASK) != 0U);
    syntra_irq_restore(inner);
    assert((syntra_critical_section_test_get_daif() & IRQ_MASK) != 0U);
    syntra_irq_restore(outer);
    assert(syntra_critical_section_test_get_daif() == 0U);

    syntra_critical_section_test_set_daif(
        FIQ_MASK | ABORT_MASK | DEBUG_MASK | IRQ_MASK);
    outer = syntra_irq_save();
    assert(outer == (FIQ_MASK | ABORT_MASK | DEBUG_MASK | IRQ_MASK));
    syntra_irq_restore(outer);
    assert(syntra_critical_section_test_get_daif() == outer);

    syntra_critical_section_test_set_daif(FIQ_MASK | ABORT_MASK | DEBUG_MASK);
    outer = syntra_irq_save();
    assert((syntra_critical_section_test_get_daif() & IRQ_MASK) != 0U);
    assert((syntra_critical_section_test_get_daif() &
        (FIQ_MASK | ABORT_MASK | DEBUG_MASK)) ==
        (FIQ_MASK | ABORT_MASK | DEBUG_MASK));
    syntra_irq_restore(outer);
    assert(syntra_critical_section_test_get_daif() ==
        (FIQ_MASK | ABORT_MASK | DEBUG_MASK));

    puts("IRQ Critical Section V1: save/disable, nesting, and exact restore passed.");
    return 0;
}
