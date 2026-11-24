# Thread Identity V1 — Generation-aware Thread Handles

Version target: SYNTRA OS v0.1.0  
Status: D1 implementation in progress; append-only slots, exact-handle APIs, and no resource reclamation.  
Parent: Deferred Thread Reaper V1

## 1. Objective

Introduce a stable value identity for one scheduler-thread lifetime. A numeric thread_id is currently a lookup label, not a safe lifetime identity: future TCB-slot reuse could make an old cleanup record refer to a different thread.

The canonical identity is a pair:

`{ slot, generation }`

Both fields must match. A matching slot with a different generation is stale.

## 2. Handle representation

Implemented public type in `kernel/scheduler/scheduler.h`:

```c
typedef struct
{
    uint32_t slot;
    uint32_t generation;
} syntra_thread_handle_t;
```

Use `uint32_t` from `<stdint.h>`; do not use platform-dependent `unsigned long` for the serialized identity. Generation zero is always invalid. Slot must be less than `SYNTRA_MAX_THREADS`.

Handle equality must compare `slot` and `generation` explicitly. Do not compare the whole struct with `memcmp`: C structure layout may contain padding bytes that are not part of the logical identity.

## 3. Slot and generation lifecycle

The scheduler has a fixed 32-entry TCB array and currently appends entries without removal or reuse.

Phase D1 must preserve this behavior:
- Assign the scheduler slot at successful TCB insertion.
- Assign a nonzero generation to that slot.
- Return the handle to the caller only after insertion commits.
- Never reuse a slot in D1.
- Never increment a generation merely because a thread terminates.
- Keep the generation associated with the slot even after termination.

A later reuse phase must increment the slot generation before publishing the new occupant. If increment would wrap to zero, permanently retire that slot rather than permitting an ABA identity collision.

The generation counter must not be reset by ordinary scheduler reinitialization if stale handles can survive that reset. Until persistent boot-epoch identity exists, scheduler_init must be treated as a global lifecycle reset that invalidates every outstanding handle and is allowed only before handles escape to other subsystems. This restriction must be documented and tested.

## 4. API design

D1 implemented operations while retaining numeric-ID compatibility APIs:

- `scheduler_create_thread_with_handle` creates a runnable thread and returns its committed handle;
- `scheduler_get_thread_snapshot_by_handle` resolves an exact handle to a copy-out snapshot;
- `scheduler_set_thread_state_by_handle` updates state only after exact-handle validation;
- `scheduler_thread_handle_equal` compares both fields explicitly.

The handle-first APIs do not return a mutable TCB pointer. No current-thread-handle accessor or owner-process convenience accessor is included in D1. Existing public APIs returning mutable TCB pointers remain compatibility-only and must not be used by the reaper or new lifecycle code.

Handle resolution validates:
1. non-null output pointer where required;
2. slot range;
3. nonzero generation;
4. slot is occupied;
5. stored generation exactly equals requested generation.

Invalid or stale handles must fail without mutating scheduler state or output fields.

## 5. Relationship to process identity

Thread identity and process ownership are separate:
- thread handle identifies one scheduler-thread lifetime;
- process handle identifies one process-table lifetime;
- a TCB may carry both identities;
- matching process owner does not imply matching thread identity.

Process termination continues to use the exact process handle and cascades to its owned threads. Reaper records must use the exact thread handle, not numeric thread_id or raw TCB pointer.

## 6. Reaper integration boundary

Every deferred cleanup record must contain the exact thread handle. Before inspecting resource descriptors or releasing anything, the reaper resolves that handle and verifies:
- the TCB still represents the same generation;
- state is TERMINATED;
- architecture switch-away proof matches this thread lifetime;
- the thread is not current.

No thread-slot reuse is permitted until the handle API, generation checks, switch-away proof, and retry-safe resource descriptors are all implemented and tested.

## 7. Failure and overflow policy

- Zero generation: reject.
- Out-of-range slot: reject.
- Unoccupied slot: reject.
- Generation mismatch: stale handle; reject without mutation.
- Generation overflow on future reuse: retire the slot permanently.
- Duplicate numeric thread_id: preserve current rejection behavior.
- Scheduler reset while handles are externally retained: unsupported until a boot-epoch component is designed.

No fallback to numeric thread_id is allowed in lifecycle or reclamation code.

## 8. Required tests for implementation

- Handle returned for each successful insertion identifies the expected slot.
- Generation is nonzero.
- Exact handle resolves to the expected snapshot.
- Wrong slot is rejected.
- Zero generation is rejected.
- Out-of-range slot is rejected.
- Stale generation is rejected without state mutation.
- Invalid-handle state update does not change any TCB.
- Termination does not silently change the handle.
- Different process owners do not affect thread-handle equality.
- Struct padding does not participate in equality.
- Slot generation overflow retires a slot in the future reuse implementation.
- Reaper rejects stale handles before touching resource descriptors.
- Scheduler reset behavior is explicitly enforced.

## 9. Non-goals

D1 implements thread-handle metadata and handle-first create/snapshot/state APIs. It does not implement slot reuse, TCB removal, generation increment on reuse, reaper queue, stack freeing, allocator integration, process-slot reuse, SMP synchronization, or production readiness. SYNTRA OS remains a single-core QEMU prototype at v0.1.0.
