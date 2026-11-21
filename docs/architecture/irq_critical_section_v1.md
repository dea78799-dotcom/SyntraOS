# IRQ Critical Section V1

## Purpose

Protect single-core kernel state that may also be accessed by IRQ handlers.
The primitive captures the caller's DAIF state, masks IRQ, then restores the
exact state captured by the paired save.

## API

- `syntra_irq_save()`: returns prior DAIF and masks IRQ.
- `syntra_irq_restore(token)`: restores the exact DAIF token.

Nesting is supported by restoring tokens in reverse order:

```c
syntra_irq_state_t outer = syntra_irq_save();
/* protected region */
syntra_irq_state_t inner = syntra_irq_save();
/* nested region */
syntra_irq_restore(inner);
syntra_irq_restore(outer);
```

## Guarantees and limits

- IRQ masking is local to the current CPU.
- If IRQ was already masked, restoring the token keeps it masked.
- Nesting works when each scope restores its own token in reverse order.
- This primitive does not mask FIQ, SError, or debug exceptions.
- It is not an SMP lock and does not protect against other CPUs.
- Do not block/sleep while IRQs are masked.
- The host backend is only a unit-test model; it is not hardware exclusion.

## V1 scope

This change adds the primitive and tests, but intentionally does not yet wire
it into the physical page allocator. Allocator integration is a separate
review after the page-allocator boundary change is resolved.
