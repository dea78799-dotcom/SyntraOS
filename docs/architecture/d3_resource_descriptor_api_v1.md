# D3.2 Resource Descriptor and Trusted Allocation API Design V1

Version target: SYNTRA OS v0.1.0  
Status: Design-only; no allocator, scheduler, process coordinator, or reaper runtime code is changed.  
Parent audit: D3.1 PR #80, branch `design/d3-resource-reclamation-audit-v1`  
Design scope: kernel-owned EL1 stack allocation and registration for kernel threads.

## 1. Decision

Adopt a **kernel-controlled allocate, prepare, bind, and publish transaction**. The caller must not supply a raw stack pointer and claim that it is kernel-owned. The trusted process/thread coordinator obtains the allocation from Memory, prepares the context, associates the allocation with the exact generation-aware thread handle, and only then publishes the thread as READY.

For the first implementation:
- support kernel-owned kernel stacks from the existing contiguous Memory V1 pool only;
- keep legacy caller-supplied kernel stacks BORROWED;
- keep current user-thread kernel stacks and EL0 user stacks BORROWED;
- do not free user stacks until address-space mapping/reference teardown exists;
- do not free any resource until the D3 reaper implementation has passed its separate gates;
- do not support physical-page-backed stacks, SMP, TCB removal, or slot reuse.

This deliberately narrows the first implementation. It is safer to support one allocator/resource combination correctly than to accept descriptors for resources whose provenance cannot be proved.

## 2. Source constraints

The current Memory V1 implementation has:
- a static 4 KiB-block pool;
- allocation records containing base address, start block, block count, and active flag;
- `memory_allocate(blocks)` returning a raw pointer;
- `memory_release(address, blocks)` locating the active allocation by exact base and validating exact block count;
- no allocation generation, opaque token, owner binding, or owned-versus-borrowed distinction;
- no allocator-internal IRQ guard in the inspected implementation.

The scheduler TCB currently stores stack ranges and `cpu_context`, but no allocator descriptor. Existing process/thread creation APIs accept caller-provided stack pointers. ARM64 context frames are embedded in the kernel stack.

Therefore, merely adding a caller-populated descriptor to the existing API would not establish provenance. A stale pointer could also alias a later allocation at the same address (ABA) because Memory V1 has no allocation identity.

## 3. Allocation identity V2

Before kernel-owned stack registration is implemented, Memory needs an opaque allocation identity.

Proposed logical type:

```c
typedef struct {
    uint32_t slot;
    uint64_t generation;
} syntra_memory_allocation_handle_t;
```

The handle is valid only when:
- slot is within the allocator's fixed allocation-record table;
- generation is nonzero;
- both values exactly match the active record.

Each allocation record gains:
- a 64-bit nonzero generation;
- an allocation state: FREE, STAGED, or THREAD_OWNED;
- the exact owner thread handle when THREAD_OWNED;
- existing exact base/start-block/block-count metadata.

Generation advances whenever a record slot is allocated for a new lifetime. If generation would wrap, that record slot is permanently quarantined rather than reusing an ambiguous identity. No address-only reconstruction is allowed.

Proposed internal APIs (names are design placeholders, not implemented ABI):

```c
syntra_status_t memory_allocate_staged(
    unsigned int blocks,
    void **base_out,
    syntra_memory_allocation_handle_t *handle_out
);

syntra_status_t memory_bind_staged_allocation(
    syntra_memory_allocation_handle_t allocation,
    syntra_thread_handle_t owner
);

syntra_status_t memory_release_staged_allocation(
    syntra_memory_allocation_handle_t allocation
);

syntra_status_t memory_release_thread_allocation(
    syntra_memory_allocation_handle_t allocation,
    syntra_thread_handle_t owner
);
```

Rules:
1. STAGED allocation is available only to the trusted kernel creation coordinator; it is not yet published to a runnable thread.
2. Binding requires an exact live thread handle and transitions STAGED to THREAD_OWNED.
3. A THREAD_OWNED allocation cannot be released through legacy `memory_release(address, blocks)`.
4. Owned release requires both the exact allocation token and exact owner thread handle.
5. Release validates slot, generation, state, owner, base metadata, and block count before changing allocator state.
6. Successful release invalidates that allocation lifetime. A stale token can never release a later allocation at the same address.
7. A staged allocation can be rolled back only by its exact allocation token and only before publication.
8. Allocation and release metadata updates are serialized. The initial implementation must use the existing single-core IRQ-save/restore discipline or an equivalent allocator-owned critical section; nested calls must preserve the previous IRQ mask state.
9. Legacy raw-pointer APIs remain available for legacy non-owned allocations, but must reject records marked THREAD_OWNED.
10. No allocator function is called from IRQ context or the ARM64 post-switch completion hook.

The token is an internal kernel identity, not a user-visible security capability. Public/user-mode code must never receive it.

## 4. Resource descriptor V1

The scheduler stores a fixed-size descriptor by value; it must not retain a pointer to caller-owned descriptor memory.

Proposed logical shape:

```c
typedef enum {
    SYNTRA_THREAD_RESOURCE_KERNEL_STACK = 1,
    SYNTRA_THREAD_RESOURCE_USER_STACK = 2
} syntra_thread_resource_role_t;

typedef enum {
    SYNTRA_RESOURCE_BORROWED = 0,
    SYNTRA_RESOURCE_MEMORY_POOL_OWNED = 1
} syntra_thread_resource_ownership_t;

typedef enum {
    SYNTRA_RESOURCE_STATE_BORROWED = 0,
    SYNTRA_RESOURCE_STATE_OWNED,
    SYNTRA_RESOURCE_STATE_RELEASE_PENDING,
    SYNTRA_RESOURCE_STATE_RELEASED,
    SYNTRA_RESOURCE_STATE_RETAINED
} syntra_thread_resource_state_t;

typedef struct {
    uint32_t descriptor_version;
    syntra_thread_resource_role_t role;
    syntra_thread_resource_ownership_t ownership;
    syntra_thread_resource_state_t state;
    uintptr_t base;
    size_t size_bytes;
    unsigned int allocation_units;
    syntra_memory_allocation_handle_t allocation;
} syntra_thread_resource_t;
```

The final header location and exact public/private split must be settled during implementation. The scheduler should depend on a small resource contract header, not call Memory allocator functions itself.

Descriptor invariants:
- descriptor version is recognized;
- role and ownership enums are known;
- base is nonzero and range arithmetic is overflow-safe;
- size is nonzero and meets the scheduler/architecture minimum;
- for Memory Pool ownership, allocation token is valid and exact allocation units match the allocator record;
- for BORROWED, allocation token is invalid/zero and release is forbidden;
- the descriptor is copied into the unpublished TCB reservation and immutable after publication, except controlled lifecycle state;
- exact owner thread handle is stored/validated separately and must match the allocator record;
- no descriptor may overlap another independently owned resource;
- the ARM64 context frame is an interior range of the kernel-stack descriptor, never a separate resource descriptor.

For Memory Pool V1, `size_bytes` must equal `allocation_units * SYNTRA_MEMORY_BLOCK_SIZE`, using overflow-safe multiplication. The allocator token remains the authoritative identity; the base and size are consistency metadata.

## 5. Trusted kernel-thread creation transaction

Add a new coordinator API for kernel-owned stacks. It must not replace the legacy caller-stack API.

Illustrative caller-facing API:

```c
syntra_status_t process_thread_create_kernel_owned(
    syntra_process_handle_t owner,
    unsigned int thread_id,
    unsigned int priority,
    syntra_thread_entry_t entry,
    void *argument,
    unsigned int stack_blocks,
    syntra_thread_handle_t *handle_out
);
```

The caller supplies a stack size in allocator blocks, not a stack pointer. The coordinator owns the allocation and does not expose the raw stack pointer.

Required transaction under the current single-core IRQ-save critical-section model:

1. Validate arguments and exact process handle/state.
2. Validate requested block count and calculate stack bytes with overflow checks.
3. Obtain a STAGED allocation and exact allocation token from Memory.
4. Verify stack alignment and minimum size.
5. Initialize the ARM64 context inside that allocation.
6. Ask the scheduler to reserve an unpublished TCB slot and generate the exact thread handle. The reservation is not visible to scheduling and cannot be selected.
7. Bind the staged allocation token to that exact thread handle.
8. Copy the immutable resource descriptor into the unpublished TCB reservation.
9. Publish/commit the prepared TCB as READY as the final visibility point.
10. Return only the thread handle to the caller.

Failure before step 9:
- abort the unpublished scheduler reservation;
- release the STAGED or just-bound allocation by its exact token through a trusted rollback path;
- clear the caller's output handle;
- leave no runnable TCB and no transferred resource ownership.

Success at step 9:
- ownership is transferred to the kernel/thread lifecycle;
- caller cannot free or mutate the owned stack;
- only the deferred reaper may later request owned release;
- no caller-visible stack pointer is returned.

The scheduler reservation/commit API must guarantee that an unpublished reservation cannot be selected, snapshotted as a live thread, or terminated by unrelated code. If the current scheduler cannot provide this property, implementation must first add and test a private prepare/commit primitive; do not approximate it by publishing READY and trying to roll back afterward.

## 6. Legacy compatibility matrix

| Creation path | Kernel stack | User stack | Reclamation authority |
|---|---|---|---|
| Existing scheduler legacy API | BORROWED | N/A | Caller |
| Existing process-thread kernel API | BORROWED | N/A | Caller |
| Existing process-thread user API | BORROWED | BORROWED | External/address-space owner |
| New kernel-owned coordinator API | Memory Pool owned | N/A | Kernel reaper after all proof gates |
| Future user-owned API | Deferred | Deferred | Deferred until VM ownership exists |

Legacy API semantics must not silently change. A caller-owned stack is never passed to `memory_release()`, even if the address happens to lie inside the Memory pool.

## 7. Locking and publication model

The current target is one ARM64 core. Local IRQ masking serializes this transaction against local interrupt-driven scheduler activity; it is not an SMP lock.

Implementation requirements:
- allocator metadata operations must be serialized consistently, including legacy allocation/release paths;
- nested IRQ-save/restore must restore the prior DAIF state, not blindly unmask interrupts;
- scheduler prepare/commit must use the same documented critical-section discipline;
- no partially initialized thread becomes READY;
- no memory release occurs in the creation transaction except rollback of an unpublished allocation;
- no lock-free or cross-core guarantees are claimed;
- critical-section duration must be measured because the current allocator scans a fixed table/pool.

Before SMP, allocator metadata, scheduler reservations, resource descriptors, and reaper state require a cross-core lock/memory-ordering redesign.

## 8. Reaper-facing release contract

The reaper must not call generic `memory_release(base, blocks)` for an owned thread stack.

It must submit:
- exact allocation handle (slot + generation);
- exact owning thread handle;
- resource descriptor in RELEASE_PENDING state.

Memory validates token and owner in one serialized operation. Only a successful exact match may release the blocks. Borrowed, stale, wrong-owner, wrong-generation, wrong-size, or already-released requests fail closed.

The reaper updates the descriptor's RELEASED state while protected by the same single-core critical section, before another candidate can retry. TCB removal remains a later, separately designed operation. Slot reuse remains disabled.

## 9. Required D3.2 tests before implementation approval

### Allocation identity
- zero generation and out-of-range slot rejected;
- wrong generation rejected;
- stale token rejected after address reuse;
- generation wrap quarantines the record slot;
- wrong owner thread handle rejected;
- owned allocation rejected by legacy raw-pointer release;
- staged allocation can be rolled back only once;
- borrowed allocations remain releasable only through their legacy ownership contract.

### Descriptor validation
- unknown version, role, ownership, or state rejected;
- null/zero base and size rejected;
- pointer-range overflow rejected;
- block multiplication overflow rejected;
- exact allocation units and byte size required;
- token/base/size mismatch rejected;
- context frame must be fully contained in kernel stack;
- context frame cannot be registered as a second resource;
- user and kernel stack ranges cannot overlap.

### Transaction
- invalid process or terminated process allocates nothing;
- allocation failure creates no scheduler reservation;
- context initialization failure rolls back staged allocation;
- scheduler reservation failure rolls back staged allocation;
- owner binding failure aborts reservation and allocation;
- commit failure leaves no runnable thread and rolls back safely;
- success publishes exactly one READY thread and transfers ownership once;
- caller receives handle only, not owned stack pointer;
- legacy caller-owned API behavior remains unchanged.

### Release boundary
- no release before TERMINATED plus exact D2 SWITCHED_AWAY proof;
- wrong token/owner cannot release;
- borrowed resource never reaches owned release;
- duplicate release cannot free a later allocation at the same address;
- no allocator operation runs from IRQ or post-switch assembly hook.

## 10. Deferred work and explicit non-goals

Not included in D3.2:
- implementation of allocation tokens or descriptors;
- actual owned stack allocation or creation API;
- production reaper queue or execution context;
- actual release of a thread stack;
- user-stack ownership or address-space teardown;
- physical-page contiguous-run allocator;
- process resource ownership;
- TCB removal or slot reuse;
- SMP;
- crash-recovery persistence.

D3.2 freezes the API and transaction contract only. D3.3 must define the reaper queue, dedicated execution context, exact D2 event join, retry/backpressure behavior, and retirement ordering before any implementation enables real resource reclamation.
