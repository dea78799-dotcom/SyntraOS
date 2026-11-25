# D3.5.2 — Dedicated Reaper Execution Context

Target: SYNTRA OS v0.1.0  
Status: dedicated static-stack context and dispatch guard implemented; resource reclamation remains disabled.  
Parent: D3.5.1 switch-away proof handoff (PR #91).

## Goal

Move candidate dispatch off the bootstrap/test thread and onto a separately scheduled, permanently reserved reaper thread. The reaper must prove both its exact scheduler identity and its active EL1 stack range before it may consume proof events.

## Execution-context binding

`thread_reaper_bind_execution_context(handle, stack_base, stack_size)` accepts one binding after scheduler initialization. It validates:

- the exact nonzero-generation thread handle resolves;
- the TCB is READY and has an initialized CPU context and entry;
- the supplied stack range exactly matches the TCB's EL1 stack range;
- the stack is BORROWED, permanent, and not a user thread;
- the initial context pointer is inside the registered stack range;
- the stack range is aligned, nonempty, and does not overflow.

The binding is one-time and cannot be replaced after successful registration.

## Dispatch guard

After binding, `thread_reaper_dispatch()` first checks:

1. `scheduler_get_current_handle()` matches the bound reaper handle exactly;
2. the live stack pointer is inside `[stack_base, stack_base + stack_size)`;
3. on ARM64, SP_EL1 is 16-byte aligned.

A mismatch increments the rejection counter and returns an error without peeking or acknowledging the scheduler proof FIFO. This preserves proof events for diagnosis and prevents another thread from impersonating the reaper.

Host tests exercise valid binding, descriptor mismatch, duplicate binding, successful dispatch from the bound current context, and rejection after the scheduler's current identity changes.

## ARM64/QEMU integration

The scheduler context-switch smoke test creates a third worker with its own static 8 KiB stack and initialized ARM64 exception frame. It binds that exact thread handle and stack to the reaper module. The reaper thread:

- verifies its live SP_EL1 and current handle on entry;
- is the sole candidate dispatcher while running;
- waits for two terminal worker candidates while timer IRQs provide scheduling opportunities;
- wakes the bootstrap thread only after both candidates are recorded;
- blocks instead of returning, so its permanent stack is not retired.

After the bootstrap thread resumes, QEMU validation checks that its own SP_EL1 is inside the linker-defined bootstrap stack, that the reaper's exact nonterminal switch-away proof was published, and that the reaper TCB is READY or BLOCKED. This proves the CPU has moved off the reaper stack before any later reclamation phase can consider it.

## Safety boundaries

- The reaper stack is static and BORROWED; it is never released.
- No worker or reaper stack is freed in this phase.
- No TCB is removed and no scheduler slot is reused.
- The reaper thread remains registered; its stack is permanently reserved.
- Candidate records still contain identity/proof only.
- The reaper loop is a QEMU validation runtime, not yet a production wait-queue or wakeup design.
- IRQ masking remains single-core only; no SMP guarantee is made.
- No physical hardware validation is claimed.

## Validation requirements

Host Debug/Release tests must validate execution-context binding and rejection behavior. ARM64/QEMU must validate the actual live SP_EL1 range on the reaper stack, candidate dispatch from that stack, exact switch-away proof publication, and bootstrap-stack resumption. Stack reclamation remains explicitly out of scope.
