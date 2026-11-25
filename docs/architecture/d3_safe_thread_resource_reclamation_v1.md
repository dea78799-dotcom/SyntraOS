# D3.5.3 — Proof-Gated Owned Thread Stack Reclamation

Target: SYNTRA OS v0.1.0  
Parent: D3.5.2 Dedicated Reaper Execution Context (PR #92).  
Status: first safe reclamation primitive implemented; TCB slot reuse remains disabled.

## Objective

Release an allocator-owned kernel stack only after the scheduler has published an exact post-switch-away event for the terminated thread and the dedicated reaper is executing on its own permanent stack.

## Proof sequence recorded in the TCB

Each TCB stores its latest `switch_away_sequence`. The scheduler writes this value only in `scheduler_complete_switch_away()`, after the architecture has installed a successor context. The value is copied from the exact pending switch ticket and published with the corresponding FIFO event.

A stale, zero, or mismatched sequence cannot authorize reclamation. A nonterminal event cannot authorize reclamation because the reclamation API additionally requires the TCB to be TERMINATED.

## Reclamation transaction

`thread_reaper_reclaim_candidate(index)` requires:
- the reaper subsystem is initialized;
- a dedicated reaper execution context has been bound;
- the current scheduler handle equals the exact reaper handle;
- the live SP_EL1 lies inside the registered reaper stack and is 16-byte aligned on ARM64;
- the candidate is QUEUED and contains a nonzero-generation handle, nonzero sequence, and terminal intent.

The scheduler then validates under its local IRQ guard:
- exact handle and generation resolve;
- TCB state is TERMINATED;
- target is not the current thread;
- TCB's stored switch-away sequence equals the candidate sequence;
- target is a kernel thread, not a user thread;
- stack ownership is OWNED and its allocation token is nonzero;
- Memory V2 reports THREAD_OWNED with the exact thread slot/generation owner;
- allocator address exactly matches the TCB stack base and capacity covers the stack.

Only after every check passes does the scheduler call `memory_release_owned_allocation(token, owner)`. If release succeeds, it clears the TCB's stack base/size, allocation token, embedded CPU context pointer, entry, and argument while retaining the TERMINATED TCB identity and slot.

## Failure behavior

- Invalid/stale proof, wrong owner, wrong allocation state, wrong address, or insufficient capacity cannot free memory.
- The TCB is unchanged when validation or allocator release fails.
- The reaper marks a failed candidate QUARANTINED and does not retry it automatically.
- Duplicate reclamation is rejected because the candidate is no longer QUEUED and the allocation token is stale.
- No TCB slot is removed or reused in this phase.
- BORROWED stacks and user-thread stacks are not reclaimed by this API.

## Tests

Host test `ThreadReaperSafeReclamationV1Test` covers:
- exact owned process-thread creation;
- dedicated reaper binding and current-context validation;
- terminal switch-away proof publication;
- rejection of a stale sequence without freeing memory;
- exact one-time allocation release;
- clearing all TCB references to the released stack/context;
- retained TERMINATED TCB identity;
- stale allocation-token rejection and duplicate-reclaim rejection;
- allocator and scheduler invariants after reclamation.

The ARM64/QEMU validation from PR #92 continues to prove the actual dedicated reaper stack and switch-away handoff. This phase does not claim physical hardware validation or SMP safety.

## Explicit exclusions

- No TCB removal or slot reuse.
- No freeing BORROWED or user-mode stacks.
- No separate freeing of the exception frame; it is embedded in and released with the owned kernel stack allocation.
- No SMP synchronization; IRQ masking is single-core only.
- No production reaper wait-queue policy yet.
