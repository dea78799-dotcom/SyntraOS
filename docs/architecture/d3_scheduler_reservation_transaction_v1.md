# D3.4B.2 — Unpublished Scheduler Reservation Transaction

Status: implementation slice under CI validation.
Base: D3.4B descriptor groundwork (PR #85).

## Runtime boundary

The scheduler now has a private, single-pending reservation record stored outside the published TCB array. A reservation is assigned the next append-only slot and a nonzero generation, but it is not counted as a published thread and is not visible through thread snapshots or either scheduler selection path.

The private process coordinator uses this sequence while holding the existing outer local-IRQ guard:

1. Allocate a stack as STAGED and retain its exact generation token.
2. Reserve a BLOCKED, unpublished TCB with the intended stack range and exact process handle.
3. Bind the staged allocation to the reserved thread's exact slot/generation.
4. Attach the token only after a copy-out allocator lookup confirms RESERVED state, exact owner identity, exact base address, and sufficient capacity.
5. Commit the allocator record to THREAD_OWNED.
6. Publish the TCB as READY as the final visibility operation.

The scheduler's attach and publish operations repeat allocator-state checks. This prevents a structurally plausible token from being treated as ownership proof. memory_get_allocation_by_handle() returns a copy and never exposes a mutable allocator record.

## Abort and failure handling

scheduler_abort_reserved_thread() clears only the exact unpublished handle. The caller must separately abort the matching STAGED or RESERVED allocation using the exact token and owner. This slice deliberately does not implement a public creation coordinator or automated rollback orchestration.

If allocator commit succeeds but publication fails, the caller must not guess or free by address. Recovery must use memory_release_owned_allocation() with the exact token and thread owner, and abort the exact unpublished reservation. If either exact operation fails, retain/quarantine the resource for diagnosis.

## Constraints

- One pending reservation: matches the current single-core append-only model.
- Local IRQ masking is not an SMP lock.
- Existing caller-stack APIs remain BORROWED and unchanged.
- No user-stack ownership, reaper, stack release on exit, TCB removal, or slot reuse is introduced.
- No physical hardware validation is claimed.
