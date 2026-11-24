# SYNTRA OS Process Identity and Lifecycle V2 — Design

Version baseline: 0.1.0
Status: Design proposal; not implemented
Base implementation: Process Manager V1 metadata prototype and IPC V1 bounded queue

## 1. Purpose and scope

This document defines the contracts that must exist before Process Manager V2 can safely reuse process-table slots, reclaim terminated processes, or authenticate IPC endpoints.

This is a design-only milestone. It does not change the V1 process or IPC APIs, claim process isolation, or authorize process reaping.

## 2. V1 limitations confirmed

Process Manager V1 stores at most 64 records. Each record contains a caller-supplied nonzero integer ID, priority, and state. A terminated record cannot transition back to another state, but it remains in the table.

IPC V1 stores caller-supplied integer sender and receiver IDs in a fixed queue. It does not validate process existence, authenticate the sender from the executing context, authorize endpoints, or purge messages when a process terminates.

The current modules are metadata and queue prototypes. They do not yet provide a complete process/thread/resource ownership model.

## 3. Identity contract

### 3.1 Generation-aware handles

V2 should introduce an opaque process handle composed of two fixed-width fields:

- `slot`: index into the fixed process table, in the range 0 through 63.
- `generation`: monotonically advanced generation value associated with that slot.

Conceptual C representation:

```c
typedef struct {
    uint32_t slot;
    uint32_t generation;
} syntra_process_handle_t;
```

A handle is valid only when the slot is in range, the slot is occupied, and the supplied generation exactly matches the slot's current generation.

The generation value must survive slot reclamation. Reusing a slot must advance its generation before the slot becomes visible as occupied. Generation wraparound must not silently make an old handle valid again; define an explicit exhaustion policy (prefer permanently retiring that slot) before implementation.

Do not expose a raw slot index as a process identity. Do not accept a caller-selected slot as proof of identity.

### 3.2 Compatibility

Keep V1 integer-ID APIs isolated as compatibility interfaces during migration. New internal code must use generation-aware handles. Do not reinterpret existing integer IDs as packed handles.

Before changing or removing legacy APIs, inventory all callers and define a migration window. Compatibility wrappers must not bypass generation validation.

## 4. Lifecycle contract

The intended lifecycle remains:

`READY -> RUNNING -> BLOCKED -> READY`

with valid transitions also including:

- READY -> BLOCKED
- READY -> TERMINATED
- RUNNING -> READY
- RUNNING -> BLOCKED
- RUNNING -> TERMINATED
- BLOCKED -> TERMINATED

TERMINATED is final. A terminated process cannot be restarted. A new process receives a new generation-aware handle, even if it later reuses the same table slot.

### 4.1 Termination versus reaping

Termination marks the process as no longer eligible to execute. Reaping is a separate operation that releases the process-table slot and makes it reusable.

A process must not be reaped merely because its state is TERMINATED. Reaping is blocked until all owned resources and references have a defined cleanup path, including:

- Scheduler threads and saved execution contexts.
- Kernel and user stacks.
- Address-space/page-table resources, when those exist.
- IPC messages and endpoint references.
- Any future handles, file descriptors, timers, callbacks, or kernel objects.

Until ownership and cleanup APIs exist, V2 must retain terminated records rather than reclaiming them unsafely.

## 5. IPC identity and cleanup contract

The eventual IPC V2 message record should store generation-aware sender and receiver handles, not raw integer IDs.

The sender identity must be derived from trusted kernel execution context (for example, the current thread's registered process owner). A user or caller-supplied sender field is not authentication.

Before enqueue:
- Validate the receiver handle against the Process Manager.
- Derive and validate the sender from trusted execution context.
- Apply endpoint authorization policy.
- Reject terminated or stale identities.

Before delivery:
- Revalidate the receiver handle.
- Never deliver a message to a new process that happens to reuse the old slot.

When a process is terminated or reaped, define deterministic queue cleanup for messages where it is either sender or receiver. Cleanup must be atomic with respect to process lifecycle changes under a documented synchronization strategy.

## 6. Synchronization and lock ordering

The current implementation uses local IRQ masking for single-core ARM64 metadata protection. This is not SMP or FIQ synchronization.

Before cross-module lifecycle operations are implemented, document one lock/critical-section order for Process Manager, Scheduler, and IPC. Avoid calling public lock-taking APIs recursively while holding another subsystem's lock. Prefer private unlocked helpers under one explicitly owned critical section or a reviewed lock-order protocol.

Do not claim SMP safety until real cross-core synchronization and tests exist.

## 7. Required implementation phases

1. **Identity primitives:** define the handle type, slot occupancy, generation initialization, validation, and generation exhaustion behavior. Add unit tests without changing IPC.
2. **Process API migration:** add create-by-handle and lookup-by-handle APIs; inventory and migrate every internal caller. Keep compatibility wrappers only where required.
3. **Ownership contract:** define how scheduler threads belong to processes and how stacks/contexts are released. Do not implement reaping before this exists.
4. **Termination cleanup:** add an explicit termination sequence and prove that scheduler execution cannot continue for a terminated process.
5. **IPC V2:** use generation-aware endpoints, derive sender identity from trusted execution context, authorize receivers, and purge stale messages.
6. **Reaping and reuse:** enable slot reuse only after all resource and IPC references are proven released.

Each phase must have focused tests and full Debug/Release plus ARM64 QEMU CI validation before proceeding.

## 8. Required regression tests

At minimum:

- Reject out-of-range slots and zero/invalid generations.
- Reject stale handles after slot reuse.
- Ensure a reused slot receives a different generation.
- Ensure generation exhaustion never revives an old handle.
- Reject state changes through stale handles.
- Prevent reaping while scheduler/resource references remain.
- Verify terminated processes cannot be scheduled again.
- Reject IPC send from a forged sender identity.
- Reject stale or terminated receiver handles.
- Purge queued messages for terminated/reaped endpoints.
- Ensure a new process reusing a slot never receives an old process's messages.
- Exercise lifecycle and IPC operations under the documented synchronization model.

## 9. Explicit non-goals

This design does not implement address-space isolation, user credentials, capability security, file descriptors, a full process loader, SMP scheduling, or production-grade IPC. Those require separate designs and validation.

## 10. Acceptance criteria

V2 is not complete until identity, lifecycle, scheduler ownership, IPC authentication, cleanup, and reaping work together with regression coverage. A passing metadata unit test alone is insufficient.

The project version remains 0.1.0 for this design work. No release or production-readiness claim is implied.
