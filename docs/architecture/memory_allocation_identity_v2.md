# Memory Allocation Identity V2

Version target: SYNTRA OS v0.1.0  
Status: Implemented allocator primitive; not yet integrated with scheduler-owned thread creation or the deferred reaper.

## Purpose

Memory V1 identified allocations by raw base address and block count. If an address was freed and later reused, a stale pointer could refer to a different allocation lifetime. V2 adds a slot-plus-generation token and explicit ownership states so stale tokens cannot release a later allocation at the same address.

## Allocation token

`syntra_memory_allocation_handle_t` contains:
- `slot`: exact allocator-record slot;
- `generation`: nonzero 64-bit allocation lifetime.

A token resolves only when the slot is in range, active, and its current generation exactly matches. The generation is retained after release and advances on the next allocation in that slot. A slot at `UINT64_MAX` is retired instead of wrapping.

## Ownership states

| State | Meaning | Legacy raw-pointer release |
|---|---|---|
| FREE | No active allocation | Rejected |
| LEGACY | Created by `memory_allocate()` | Allowed with exact base and block count |
| STAGED | Reserved for a trusted creation transaction | Rejected |
| RESERVED | Bound to an exact owner handle, not yet published | Rejected |
| THREAD_OWNED | Ownership committed to an exact thread lifetime | Rejected |

## API sequence

1. `memory_allocate_staged(blocks, &address, &token)` reserves storage and returns a generation-aware token.
2. The trusted coordinator initializes the resource while it remains STAGED.
3. `memory_bind_staged_allocation(token, owner)` binds it to a nonzero exact owner identity and changes state to RESERVED.
4. `memory_commit_thread_allocation(token, owner)` changes RESERVED to THREAD_OWNED after the scheduler's publication transaction is ready to commit.
5. If publication fails before the thread becomes visible, `memory_abort_reserved_allocation(token, owner)` can roll back that exact RESERVED allocation.
6. `memory_release_owned_allocation(token, owner)` is the only release path for THREAD_OWNED storage. It requires both the exact token and exact owner.

STAGED allocations can be rolled back only by `memory_abort_staged_allocation(token)`.

## Compatibility and safety

- Existing `memory_allocate()` creates LEGACY allocations; existing `memory_release()` behavior remains available for those allocations.
- Legacy release returns `SYNTRA_STATUS_PERMISSION_DENIED` for STAGED, RESERVED, and THREAD_OWNED allocations.
- Owner mismatch is rejected.
- A stale generation is rejected even if the allocator reuses the same slot and base address.
- Allocation/release metadata mutations use nested-safe local IRQ save/restore. This is single-core serialization, not SMP synchronization.
- The token is an internal kernel identity and must never be exposed to user mode.
- The current PR does not create kernel-owned threads, does not free thread stacks from exit, and does not run a reaper. Scheduler integration and reaper eligibility remain separate gated work.

## Host test coverage

The Memory host test covers:
- staged allocation and nonzero generation;
- legacy-release rejection for staged and thread-owned allocations;
- exact owner binding and wrong-owner rejection;
- RESERVED-to-THREAD_OWNED transition;
- exact-token owned release and duplicate/stale-token rejection;
- same-slot/same-address reuse with a strictly newer generation;
- staged and reserved one-shot rollback;
- invalid owner rejection;
- final allocator integrity validation.
