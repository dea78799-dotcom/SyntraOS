# D3 Resource Ownership & Deferred Reaper — Source Audit V1

Version target: SYNTRA OS v0.1.0  
Status: Design/audit only. No resource ownership transfer, reaper queue, allocator release, TCB removal, or slot reuse is implemented.  
Audit base: `feat/thread-switch-away-proof-v1` (D2 runtime proof)

## 1. Purpose and decision

This audit compares the existing D1/D1.1/D2 contracts with the actual source tree before any reclamation implementation is authorized.

**Decision: keep reclamation disabled.** The current code can identify a thread lifetime and publish a switch-away event after ARM64 installs a successor stack, but it cannot yet prove that a particular stack allocation belongs to that thread or that a resource may be released through a particular allocator.

D3 must first introduce immutable, validated ownership metadata and a reaper design whose execution stack is independent of every candidate being reclaimed. A D2 event is necessary evidence, not a complete permission-to-free token.

## 2. Source facts verified on the D2 branch

### 2.1 Scheduler TCB records ranges, not allocation ownership

In `kernel/scheduler/scheduler.h`, `syntra_thread_t` contains:
- `stack_base` and `stack_size` for the EL1/kernel stack;
- `user_stack_base` and `user_stack_size` for the separate EL0 stack;
- `cpu_context`, which is an architecture context pointer;
- exact thread handle and optional owning process handle.

There is no allocator-kind field, exact allocator release-unit count, allocation token, ownership-transfer state, or per-resource release state in the TCB.

The comments on the legacy create APIs explicitly describe caller-provided stacks. The process/thread coordinator header also states that stacks and CPU contexts remain caller-owned in this phase. Process ownership is not memory ownership: `owner_process` identifies the process relationship but does not authorize freeing either stack.

### 2.2 Creation paths do not transfer allocator ownership

The supported coordinator in `kernel/process/process_thread.c` validates the process handle and process state, then delegates to the scheduler's owned-thread registration APIs. It passes stack pointers and sizes; it does not receive or validate allocator provenance and does not transfer allocation ownership.

The scheduler's user-thread creation path validates that kernel and user stack ranges do not overlap and stores both ranges. That is range validation only; it does not establish who allocated either range.

### 2.3 ARM64 context frames are embedded in the kernel stack

In `kernel/arch/arm64/context.c`, both:
- `syntra_arm64_context_initialize()`, and
- `syntra_arm64_user_context_initialize()`

place the saved exception frame at the aligned top of the supplied kernel stack, then return that interior address through `cpu_context`.

Therefore, for these constructors, the context frame is a borrowed subrange of the kernel-stack allocation. It is not a second allocation and must never receive a separate release descriptor. The user stack is a distinct range and requires independent ownership evidence.

### 2.4 General memory-pool allocator contract

In `kernel/memory/memory.h` and `kernel/memory/memory.c`:
- `memory_allocate(blocks)` allocates a contiguous run of fixed-size 4096-byte blocks from the static memory pool.
- `memory_release(address, blocks)` requires the exact allocation base and exact block count.
- Interior pointers and mismatched block counts are rejected.
- The public API does not return an opaque allocation handle or generation token.

Consequences:
- A byte size alone is not enough to call `memory_release()`.
- A descriptor must retain the exact base and exact block count obtained at allocation time.
- A descriptor must not be reconstructed from an arbitrary stack pointer and a guessed size.
- The current allocator API does not provide a durable identity against address reuse (no allocation generation/token).

### 2.5 Physical page allocator is not a stack allocator contract

In `kernel/memory/page_allocator.h` and `kernel/memory/page_allocator.c`:
- `syntra_page_alloc()` allocates one 4 KiB physical page.
- `syntra_page_free()` releases one aligned physical page previously marked allocated.
- The allocator exposes no contiguous multi-page allocation, run descriptor, or allocation-generation token.

D3 must not infer that adjacent page allocations form a contiguous stack. Physical-page-backed stack ownership is deferred until a separate contiguous-run and identity contract exists.

### 2.6 D2 switch-away evidence

D2 stores an exact `syntra_thread_handle_t` and monotonically increasing sequence in a bounded 128-entry FIFO. The ARM64 assembly invokes the completion hook only after installing a different successor `SP_EL1`. The hook publishes metadata only; it does not allocate, free, mutate TCB lifetime, or perform logging.

The event currently has no production reaper consumer. The QEMU boot test drains it as a test consumer. The event proves the outgoing kernel stack is no longer the active stack at that switch boundary; it does not prove allocator ownership, user-stack unmapping, process address-space teardown, or absence of every other saved reference.

## 3. Ownership model required for D3

D3 should define a closed, versioned resource descriptor. The descriptor is metadata, not a capability supplied by an untrusted caller.

Minimum logical fields:
- resource role: kernel stack or user stack;
- ownership class: borrowed or kernel-owned;
- allocator kind: none/borrowed or general memory pool V1;
- exact base address;
- exact byte length;
- exact allocator units (memory-pool block count);
- descriptor version and validation state;
- per-resource lifecycle: owned, release-pending, released, or retained;
- immutable association with the exact thread handle.

Rules:
1. BORROWED resources are never passed to an allocator release function.
2. MEMORY_POOL resources require a non-null exact base, nonzero exact block count, overflow-safe size calculation, and size equality with the allocator's block size.
3. The allocator kind and resource role must be known enum values; unknown values fail closed.
4. The kernel-stack range must contain the entire saved context frame range.
5. The context frame is a reference into the kernel stack, never a separately releasable resource for the current ARM64 constructors.
6. A user-stack resource must be disjoint from the kernel stack and requires address-space unmap/reference-release proof before freeing.
7. Ownership transfer must be explicit and atomic with successful thread registration. Failure leaves ownership with the caller.
8. Legacy creation APIs remain BORROWED and retain their existing semantics.
9. Descriptors become immutable after TCB registration; release state may advance only through controlled reaper transitions.
10. Do not accept a caller-created descriptor as proof that the allocator actually returned that allocation. The preferred future API should allocate internally or validate an allocator-issued token. Since Memory V1 has no token, the first safe implementation must keep the allocator and registration transaction inside a trusted kernel coordinator.

## 4. Reaper architecture requirements

The reaper must run from a dedicated, permanently reserved kernel execution context and stack. It must never execute on a stack that it may release.

Initial target constraints:
- single-core ARM64 only;
- normal kernel execution only; no allocator calls from IRQ or the post-switch assembly hook;
- reaper stack/context are static or otherwise permanently reserved and excluded from ordinary thread resources;
- if the reaper is not scheduled, resources remain allocated;
- no inline reclamation as a memory-pressure fallback;
- no SMP or lock-free guarantees.

The D2 completion FIFO and a future reap-candidate queue have different responsibilities. Do not silently reinterpret the D2 event as a complete resource record. A future reaper candidate must join:
- exact thread handle;
- exact D2 switch-away sequence;
- immutable resource descriptors;
- per-resource release progress;
- enqueue/deduplication state.

The candidate must be resolved against the live TCB and exact D2 event before any release. A stale handle, duplicate event, sequence mismatch, missing descriptor, or queue overflow must leave all unresolved memory allocated.

## 5. Required lifecycle and transaction

Keep these facts separate:
- TERMINATED: scheduler will not run the thread again.
- SWITCHED_AWAY: ARM64 installed a different successor stack and published the exact D2 event.
- REAPABLE: the reaper independently validates the handle, event, ownership descriptors, and all architecture/address-space references.
- REAPED: every owned resource is released exactly once or explicitly retained by its owner, and only then may the TCB be removed from active lookup.

Required transaction:
1. Inspect a candidate without discarding retry metadata.
2. Resolve the exact generation-aware thread handle.
3. Confirm TERMINATED and matching SWITCHED_AWAY sequence.
4. Confirm the candidate is not current and no active/saved context still references its kernel stack.
5. Confirm the reaper is executing on its reserved stack.
6. Validate all resource descriptors and ownership provenance.
7. Resolve user-stack mapping/reference ownership before user-stack release.
8. Release one eligible resource through its owning allocator.
9. Mark only that successful resource released before another pass.
10. On allocator failure, retain the unreleased descriptor and retry state; never repeat successful releases.
11. Remove the TCB from active lookup only after every required resource is released or explicitly retained.
12. Keep slot reuse disabled until a separately reviewed generation-increment and stale-handle design exists.

No rollback may restore a pointer to memory that was already successfully freed.

## 6. D3 implementation stages

- **D3.1 — Source audit and contract freeze (this PR):** document allocator facts, stack/context layout, ownership gaps, D2 proof limits, and non-goals.
- **D3.2 — Resource descriptor/API design:** specify the trusted allocation-and-registration transaction, immutable descriptors, validation, and borrowed compatibility.
- **D3.3 — Reaper design:** specify dedicated execution context, queue capacity/backpressure, exact event matching, partial-release retry, and TCB retirement ordering.
- **D3.4 — Host tests before runtime enablement:** descriptor validation, overlap/overflow rejection, borrowed-resource non-release, stale handles, duplicate proof, partial failure, and exactly-once release using a test allocator.
- **D3.5 — ARM64/QEMU integration:** only after D3.2–D3.4 pass, test the reaper on its reserved stack and verify that no candidate stack is freed while active.

This PR is D3.1 only. It does not implement any of D3.2–D3.5.

## 7. Acceptance gates before any real memory release

- Ownership provenance originates from a trusted allocator/coordinator path, not a forged raw pointer.
- Legacy caller-owned stacks remain borrowed.
- Context frames cannot be double-counted.
- Kernel and user stacks are tracked independently.
- D2 event handle and sequence are matched exactly.
- Reaper stack is demonstrably independent of the candidate.
- User stack is not freed until unmap/reference ownership is complete.
- Borrowed resources never reach allocator release.
- Allocator failure and partial success preserve idempotent retry state.
- Duplicate or stale candidates cannot cause double release.
- TCB removal occurs only after resource resolution.
- All host tests and ARM64/QEMU checks pass.
- No claim of SMP safety or physical-hardware validation.

## 8. Explicit non-goals

This audit does not implement or authorize:
- allocator ownership transfer;
- memory-pool or physical-page release from thread exit;
- a production reaper or reaper queue;
- user address-space teardown;
- TCB removal or thread-slot reuse;
- process reaping;
- contiguous physical-page allocation;
- SMP synchronization;
- production-ready memory-pressure handling.

SYNTRA OS remains a single-core QEMU prototype under v0.1.0.
