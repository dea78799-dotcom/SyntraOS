# D3.4B — Scheduler Resource Integration Contract

Status: design contract for the next runtime implementation step.
Baseline: Memory Allocation Identity V2 (D3.4A), PR #83.
Scope: single-core ARM64/QEMU development only. This document does not claim that
scheduler-owned stack allocation or deferred reclamation is implemented.

## 1. Source audit

The current scheduler stores a thread's kernel stack as a raw address and size.
Its creation helpers append a TCB directly in THREAD_READY state. Process-owned
creation adds the exact process handle, but the stack remains caller-owned.
There is no allocation token in the TCB and no scheduler prepare/publish split.

D3.4A provides generation-aware allocation handles and these transitions:

- STAGED: allocated, no owner.
- RESERVED: bound to an exact owner handle.
- THREAD_OWNED: committed to that owner.
- Legacy release rejects these non-legacy states.

The existing process/thread coordinator masks local IRQs across process validation
and scheduler registration. Nested scheduler and allocator calls preserve the
already-masked DAIF state. This is a single-core critical section, not an SMP lock.

## 2. Required TCB resource descriptor

A kernel-owned stack descriptor must retain:

- allocation token: exact allocator slot plus 64-bit generation;
- allocation owner: exact thread slot plus thread generation;
- stack base and usable stack size;
- ownership kind: OWNED or BORROWED.

The descriptor is metadata, not an authority by itself. The allocator's
THREAD_OWNED record must independently contain the same exact thread owner.
The TCB's thread handle must equal that owner. A mismatch is an invariant
failure and must fail closed.

User stacks and externally supplied stacks remain BORROWED in this phase.
Do not free them through the kernel-owned stack path.

## 3. Prepare / commit / publish transaction

The coordinator is the only caller of the private owned-thread transaction.
All steps below execute under the existing outer local-IRQ guard.

1. Validate the exact process handle and reject a terminated process.
2. Allocate the kernel stack with memory_allocate_staged(). Convert the
   requested byte size to allocator blocks using checked ceiling division.
   Reject zero, overflow, or a size that cannot satisfy stack alignment and
   minimum-size constraints.
3. Ask the scheduler to reserve an unpublished TCB. The TCB receives a stable
   thread handle and remains BLOCKED; it must not be eligible for scheduling.
   The reservation validates ID uniqueness, capacity, stack range, and
   process owner. No raw stack pointer is returned to an ordinary caller.
4. Bind the staged allocation to the exact thread handle using
   memory_bind_staged_allocation().
5. Attach the exact allocation token and OWNED descriptor to the reserved TCB.
   The scheduler validates the token/owner tuple before accepting the descriptor.
6. Commit the allocator record with memory_commit_thread_allocation().
7. Publish the reserved TCB as READY as the final visibility step. Only now may
   the thread be selected by the scheduler and its handle be returned.

The publication operation must be a bounded metadata update with all validation
performed before changing state. A failed publish must leave the TCB unpublished.

## 4. Rollback and fail-closed rules

Before allocator ownership is committed:

- If TCB reservation fails, abort the staged allocation with its exact token.
- If binding or descriptor attachment fails, abort the reserved TCB and abort
  the exact staged/reserved allocation using the matching allocator API.
- Never release by raw address or allocation slot alone.

After allocator ownership is committed but before TCB publication:

- The TCB is still unpublished and cannot have executed on the stack.
- Rollback may release only with memory_release_owned_allocation(exact token,
  exact thread owner), then abort the unpublished TCB reservation.
- If exact release or reservation rollback fails, quarantine the allocation
  and TCB metadata. Do not make the stack reusable and do not guess at recovery.

After TCB publication:

- Ordinary creation rollback is no longer permitted.
- The resource is released only by a future reaper after D2 switch-away proof
  for the exact thread handle and exact resource-owner validation.

The coordinator must not expose a stack pointer, allocator token, or mutable TCB
to user mode or ordinary drivers.

## 5. Scheduler invariants

scheduler_validate() must eventually verify:

- every OWNED descriptor has a nonzero allocation generation;
- its allocation owner equals the TCB's exact thread handle;
- the descriptor's base and size match the allocator record;
- BORROWED descriptors do not claim allocator ownership;
- unpublished reservations are not RUNNING or READY;
- a published kernel-owned thread cannot lack its required descriptor.

The scheduler must not assume that an allocation token alone proves the stack
is safe to free. Reclamation additionally requires D2 switch-away evidence.

## 6. Implementation sequence

D3.4B runtime work should be split into reviewable commits:

A. Add the TCB resource descriptor and private reserve/attach/publish/abort APIs.
B. Add host tests for owner mismatch, duplicate IDs, full scheduler capacity,
   rollback at each pre-publication boundary, and unpublished invisibility.
C. Integrate exact allocator-token validation and test allocator state agreement.
D. Run Debug and Release host tests, ARM64 cross-build, EL0/QEMU, scheduler
   switch-away, and framebuffer QEMU checks.

D3.4C will add the public kernel-owned thread-creation coordinator that obtains
its stack from the allocator. D3.5 will connect exact D2 switch-away events to
the deferred reaper. Neither stack reclamation nor TCB-slot reuse is in scope
for D3.4B.

## 7. Explicit non-goals

- No freeing of stacks on thread exit.
- No TCB removal or slot reuse.
- No user-stack ownership or address-space teardown.
- No SMP synchronization.
- No physical hardware validation.
- No production-readiness claim.
