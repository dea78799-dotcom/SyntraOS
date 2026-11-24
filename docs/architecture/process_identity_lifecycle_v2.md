# SYNTRA OS Process Identity and Lifecycle V2 — Design

Version baseline: 0.1.0
Status: Design proposal; Phase 1 identity primitives implemented
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

V2 uses a generation-aware process handle composed of two fixed-width public fields. The C struct is not opaque; callers can inspect or construct values, but only the Process Manager can validate them:

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
2. **Process API migration:** migrate internal callers to handle-first creation and lookup APIs; inventory and migrate remaining ID-based callers. Keep compatibility wrappers only where required.
3. **Ownership contract:** define how scheduler threads belong to processes and how stacks/contexts are released. Do not implement reaping before this exists.
4. **Termination cleanup:** add an explicit termination sequence and prove that scheduler execution cannot continue for a terminated process.
5. **IPC V2:** use generation-aware endpoints, derive sender identity from trusted execution context, authorize receivers, and purge stale messages.
6. **Reaping and reuse:** enable slot reuse only after all resource and IPC references are proven released.

Each phase must have focused tests and full Debug/Release plus ARM64 QEMU CI validation before proceeding.

## 8. Phase 1 implementation status

The first identity-primitives increment is implemented alongside the V1 ID API:

- A process-table slot now carries a 32-bit generation and an occupied flag.
- The public `syntra_process_handle_t` value contains a 32-bit slot and generation.
- `process_create_with_handle()` creates a record and returns its handle atomically.
- `process_get_handle()` resolves an existing legacy ID to its current handle.
- `process_get_snapshot_by_handle()` and `process_set_state_by_handle()` validate both fields before accessing metadata.
- Legacy ID-based APIs remain available and route through the same slot table.
- Initial generation is one. Slot reuse and generation advancement are intentionally unavailable because safe reaping and resource ownership are not implemented.

This increment prevents a mismatched or zero generation from resolving to a live record, but it does not yet demonstrate stale-handle rejection after slot reuse. That test belongs to the later reaping/reuse phase and must not be simulated by exposing an unsafe production API.

## 9. Required regression tests

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

## 10. Explicit non-goals

This design does not implement address-space isolation, user credentials, capability security, file descriptors, a full process loader, SMP scheduling, or production-grade IPC. Those require separate designs and validation.

## 11. Acceptance criteria

V2 is not complete until identity, lifecycle, scheduler ownership, IPC authentication, cleanup, and reaping work together with regression coverage. A passing metadata unit test alone is insufficient.

The project version remains 0.1.0 for this design work. No release or production-readiness claim is implied.


## 12. Scheduler and resource ownership audit — 2026-11-24

This section records an inspection of the current `design/process-identity-lifecycle-v1` implementation. It is an implementation audit, not a claim that every repository call site has been exhaustively reviewed.

### 12.1 Scheduler facts

- `syntra_thread_t` currently has a thread ID, scheduling state, entry/argument, kernel stack range, optional user stack range, user-thread flag, and saved CPU-context pointer.
- It has no process-owner handle or owner-validity field. The scheduler therefore cannot currently answer which process owns a thread.
- `scheduler_create_thread()` and `scheduler_create_user_thread()` accept caller-provided stack memory. Their API contract leaves allocation ownership with the caller.
- Scheduler records are stored densely in a fixed array. There is no thread-removal/reaping API. Terminating a thread changes its state, but does not decrement the table count or release its stack/context.
- Stack ranges remain reserved by the scheduler until scheduler reset, including after a thread is marked terminated.
- The boot path creates scheduler worker threads directly. Kernel initialization initializes Scheduler and Process Manager as separate subsystems; no process/thread binding is established there.

### 12.2 Process and IPC facts

- Process Manager stores process metadata and generation-aware handles, but it does not create, enumerate, or own scheduler threads.
- A process state update is independent of scheduler state. There is currently no cross-module invariant that prevents a thread from being scheduled after its associated process is marked TERMINATED, because no association exists yet.
- IPC V1 stores caller-supplied integer sender and receiver IDs. It has no process-handle validation, trusted sender derivation, or endpoint cleanup.
- Memory V1 exposes block allocation and release operations, but its public allocation metadata does not associate an allocation with a process handle. Scheduler stack APIs currently receive externally supplied pointers rather than memory-allocation ownership records.

### 12.3 Phase 2 design decisions

Do not add process ownership by making Scheduler include the full Process Manager API. The shared identity value should be moved into a small dependency-neutral header (proposed: `kernel/process/process_handle.h`) and included by both Process Manager and Scheduler. This avoids importing process lifecycle operations into the scheduler interface.

The next implementation proposal should introduce additive, handle-aware scheduler creation APIs while preserving current compatibility entry points:

- A process-owned kernel-thread creation API that accepts a validated `syntra_process_handle_t`.
- A process-owned user-thread creation API with the same owner identity.
- An explicit owner-validity field in each TCB; kernel/bootstrap threads created through legacy APIs must be distinguishable from process-owned threads rather than assigned a fabricated process ID.
- A scheduler query that counts or snapshots threads for an exact process handle, comparing both slot and generation.
- Validation that rejects a process-owned thread when its handle is invalid or the process is already terminated.

The scheduler should store the owner handle as a reference, not as proof that the process is still live. Every lifecycle-sensitive operation must revalidate the handle through a coordinated kernel path.

### 12.4 Stack and context lifetime

The current scheduler receives borrowed stack pointers and has no release callback or allocator ownership token. It must not free caller-provided stacks.

Before introducing stack reclamation, define an explicit ownership model for each stack and context:

1. **Borrowed:** caller retains ownership; scheduler only reserves and uses the range.
2. **Scheduler-owned:** scheduler receives an allocation token and may release it only after the thread is terminated and no CPU context can still reference it.
3. **Process-owned:** Process Manager/resource layer owns the allocation; it cannot release it while any thread or kernel reference remains.

A thread marked TERMINATED is not yet safe to reclaim if it is the currently executing thread or if its saved context can still be selected. Reclamation must occur only after the scheduler has switched away and removed every runnable/reference path to that TCB and its stacks.

No implicit ownership may be inferred from a raw pointer. No stack may be released merely because a process state became TERMINATED.

### 12.5 Required Phase 2 tests

- Kernel/bootstrap threads without a process owner remain valid and are not mistaken for user processes.
- Process-owned thread creation rejects zero, out-of-range, stale, and terminated process handles.
- Two threads may share one process handle; a different generation must not match them.
- Scheduler owner-count/snapshot queries return only exact slot-and-generation matches.
- A process with any registered thread cannot be reaped.
- A terminated current thread is not reclaimed until execution has switched away.
- Borrowed stack memory is never released by Scheduler.
- A failed thread creation leaves no partial TCB or ownership reference.
- Existing legacy scheduler tests and ARM64 boot workers remain compatible.

### 12.6 Explicit hold point

Phase 2 is design-only at this point. Do not add owner fields, change scheduler ABI, release stacks, add thread removal, terminate processes through Scheduler, or enable process reaping until the API and synchronization design is reviewed and the required tests can be implemented together.

The project version remains 0.1.0.


## 13. Phase 2 API and synchronization proposal

This section refines the next implementation boundary. It remains a proposal; no runtime API is added by this document update.

### 13.1 Shared handle type

Move the existing `syntra_process_handle_t` definition unchanged into `kernel/process/process_handle.h`. Both `process.h` and the scheduler's internal ownership interface include that header. `process.h` remains the owner of process lifecycle APIs. The scheduler must not include `process.h` or call Process Manager functions directly.

This is a source-organization change only. The handle layout remains two `uint32_t` fields and the current v0.1.0 ABI is not advertised as stable.

### 13.2 One coordinator owns process-to-thread registration

Add a small Process/Thread coordinator (proposed files: `kernel/process/process_thread.h` and `kernel/process/process_thread.c`). New process-owned thread creation must enter through this coordinator, not through a public Scheduler API that accepts an unchecked owner value.

Proposed public operations:

```c
syntra_status_t process_thread_create_kernel(
    syntra_process_handle_t owner,
    unsigned int thread_id,
    unsigned int priority,
    syntra_thread_entry_t entry,
    void *argument,
    void *kernel_stack,
    size_t kernel_stack_size
);

syntra_status_t process_thread_create_user(
    syntra_process_handle_t owner,
    unsigned int thread_id,
    unsigned int priority,
    syntra_thread_entry_t entry,
    void *argument,
    void *kernel_stack,
    size_t kernel_stack_size,
    void *user_stack,
    size_t user_stack_size,
    void *cpu_context
);
```

The coordinator performs the following sequence:

1. Reject structurally invalid handles (zero generation or out-of-range slot).
2. Enter one outer local-IRQ critical section.
3. Resolve the exact handle through Process Manager and copy a process snapshot.
4. Reject a process whose state is TERMINATED.
5. Call a scheduler-internal insertion routine that records the exact owner handle and performs all existing thread, stack-alignment, stack-overlap, capacity, and context checks.
6. Restore the original IRQ state and return the scheduler result.

The existing IRQ helper saves DAIF and restores the caller's prior state, so nested calls preserve an already-masked state on the current single-core ARM64 target. The outer critical section prevents a process-state change between handle validation and TCB insertion on that target. This is not an SMP solution; do not carry this assumption into a multicore build.

The scheduler-internal insertion entry points must not be exposed as general public APIs. Existing `scheduler_create_thread()` and `scheduler_create_user_thread()` remain compatibility/bootstrap paths and create unowned threads.

### 13.3 TCB ownership representation

Add to `syntra_thread_t`:

- `syntra_process_handle_t owner_process`.
- `uint8_t has_process_owner`.

For every TCB, `has_process_owner == 0` means a kernel/bootstrap or legacy thread with no process association; its owner fields must be zeroed. `has_process_owner == 1` means the handle is a reference to the exact process identity. Never infer ownership from `thread_id`, stack address, or a legacy integer process ID.

The scheduler's owner query must compare both handle fields. A slot-only comparison is invalid. The first increment should provide a count/snapshot query only; it must not delete threads or reclaim resources.

### 13.4 Creation failure atomicity

A failed process-owned create must leave all of the following unchanged:

- `thread_count` and scheduler slot contents.
- Existing current-thread and round-robin selection state.
- Every existing stack reservation.
- Process ownership counts (which should be derived from TCBs, not maintained as a second mutable counter in Phase 2).

Perform all validation possible before committing the new TCB. For user threads, validate both stack ranges against each other and all registered kernel/user stack ranges before incrementing the scheduler count. Once the TCB is committed, initialize every owner and context field before making it eligible for scheduling.

### 13.5 Termination remains separate

Phase 2 does not make `process_set_state_by_handle(..., PROCESS_TERMINATED)` automatically terminate or remove threads. Until Phase 3/4 defines a coordinated stop protocol, process termination with owned threads must remain prohibited at the coordinator boundary or be explicitly documented as metadata-only and unsafe for execution. Prefer rejecting the transition in a future coordinated API rather than pretending the Scheduler has stopped the process.

No owner-count result by itself authorizes reaping: the count does not yet include IPC, open objects, callbacks, or other future references.

### 13.6 Phase 2 implementation acceptance

- Shared handle header preserves the exact struct layout and compiles in host and ARM64 builds.
- All legacy/bootstrap thread constructors initialize owner fields to the unowned zero state.
- Process-owned kernel and user creation route through the coordinator.
- Invalid, stale, out-of-range, zero-generation, and terminated handles are rejected.
- Two threads can register against one process handle; a handle with the same slot but a different generation matches neither.
- Owner snapshots/counts exclude unowned threads and compare slot plus generation.
- Failed create operations leave scheduler validation, count, current slot, round-robin index, and stack reservations unchanged.
- Existing scheduler and ARM64 QEMU boot tests continue to pass.
- Debug and Release unit tests plus ARM64 ELF, EL0, QEMU boot, and framebuffer CI all pass.

### 13.7 Explicit non-goals for Phase 2

No scheduler thread removal, stack deallocation, context deallocation, process termination cascade, IPC identity migration, process reaping, slot reuse, SMP locking, or production-readiness claim.
