# Kernel Stack Virtual-Mapping Contract

Status: Design / review milestone; no kernel stack mapping implementation yet.
Base: stack physical allocator v9 and ARM64 address-space prototype.
Scope: future kernel-thread stacks only. User stacks remain address-space-owned mappings.

## 1. Verified implementation boundary

The stack allocator reserves contiguous physical pages tagged KERNEL and tracks up to 32
live allocation descriptors. The descriptor contains physical base, rounded size, and page
count. The allocator does not zero pages, establish virtual mappings, or return a
dereferenceable pointer. The generic Scheduler accepts caller-provided stack buffers and
does not own their storage.

The ARM64 address-space prototype has a resolver for allocated page-table pages and
supports user mappings, but its user-map API requires PROCESS-owned pages and uses the
reserved EL0 VA range. That API is not suitable for KERNEL-owned stack pages.

MMU enablement and a general kernel virtual-memory manager remain separate unfinished
work. Therefore this document defines integration constraints; it does not authorize
creating or dereferencing a kernel virtual pointer from a physical address.

## 2. Required mapping properties

A kernel stack mapping must:

- Use Normal memory attributes.
- Be privileged read/write and execute-never (RW/NX).
- Be inaccessible from EL0.
- Use page-aligned physical and virtual ranges, with overflow checked before calculating
  the end or stack top.
- Be mapped only in the kernel address-space regime/root, not into a process-private
  user range.
- Have an explicit mapping record that associates the virtual range with its physical
  allocation descriptor and owner/thread lifecycle.
- Reject overlaps and stale/double-unmap descriptors.
- Never rely on identity mapping unless the boot contract explicitly verifies it.

## 3. Virtual-range reservation and guard-page policy

The future kernel VA allocator must reserve stack ranges independently of physical
allocation. A stack reservation should include a guard page where the selected VA layout
and page-table implementation can enforce it. The guard page remains unmapped and must
not be counted as part of the usable stack allocation.

The mapping layer must define whether stack growth is fixed-size (V1) or dynamic. V1
should be fixed-size: no automatic growth on a fault. A guard-page fault should be
reported as a stack-boundary violation; recovery/termination policy is outside this
allocator.

The kernel VA layout and guard-page implementation are blockers: current generic mapping
primitives do not constitute a complete kernel VA allocator or unmap service.

## 4. Creation sequence (transactional)

1. Validate requested stack size and round to pages.
2. Reserve a kernel virtual range, including any guard-page reservation.
3. Allocate physical pages with KERNEL ownership.
4. Validate the physical-to-kernel resolver for every page; never cast a PA to a pointer.
5. Create RW/NX, EL1-only Normal-memory mappings for the usable stack range.
6. Verify the mapping through the software page-table walker.
7. Resolve a usable kernel pointer/stack top only through the approved mapping resolver.
8. Initialize the architecture context with ABI-aligned stack top.
9. Register the Thread with the Scheduler only after all earlier steps succeed.

On failure, unwind in reverse order. If mapping partially succeeded, unmap all mapped
pages and release only page-table pages proven private and empty. Then free the physical
stack allocation and release the VA reservation. The Thread must not become visible to
the Scheduler before the complete stack is ready.

## 5. Reclamation sequence

Stack reclamation must be explicit and performed in a kernel-management context:

1. Confirm the Thread is TERMINATED and is not the current Thread.
2. Remove external Process/Thread references according to the established process
   reclamation order.
3. Successfully reap the Scheduler TCB; this removes the TCB's stack references.
4. Unmap the kernel virtual stack range and verify that no active execution or saved
   context references it.
5. Free the exact physical allocation descriptor through the stack allocator.
6. Release the virtual reservation and guard-page reservation.

Do not reclaim a stack from its own execution path, IRQ, or exception handler. No SMP
safety is implied; scheduler mutation, mapping operations, and reclamation must remain
serialized until locks and remote TLB shootdown are implemented.

## 6. Ownership model

- Physical Stack Allocator: owns the KERNEL-tagged physical pages and exact allocation
  descriptor until explicit free.
- Kernel VA allocator/mapping layer (future): owns virtual reservation and mapping record.
- Scheduler: owns TCB metadata only; stack storage remains external to Scheduler.
- Process Manager (when applicable): tracks thread/process associations and external IDs,
  but must not free stack pages before the Thread is safely reaped.
- ARM64 context layer: consumes a mapped kernel stack pointer; it does not allocate or
  reclaim physical storage.

Ownership transfer must be explicit; a descriptor must not be copied into multiple
owners that may independently free it.

## 7. Test gates before integration

Host tests should cover:
- VA reservation exhaustion and reuse.
- Alignment, arithmetic-overflow, and overlap rejection.
- Guard-page remains unmapped.
- Physical resolver failure at each page and rollback.
- Mapping attributes (EL1 RW, EL0 denied, NX, Normal memory).
- Mapping failure rollback without physical-page, table-page, or VA leaks.
- Thread-create failure after each staged operation.
- Repeated create/terminate/reap/unmap/free cycles.
- Reject free/unmap while current or still referenced by a TCB.
- Corrupted/stale descriptors and duplicate reclamation.
- Page allocator owner accounting and mapping registry invariants after every test.

ARM64 QEMU tests must later verify actual kernel stack use through a mapped range,
stack-boundary fault behavior, and successful context switching/reclamation. Host tests
and cross-compilation alone are not evidence of hardware mapping correctness.

## 8. Implementation order

1. Define and review kernel VA layout plus reserved stack range.
2. Add a host-testable VA reservation registry (no MMU activation).
3. Define a mapping record and mapping/resolve/unmap interfaces with explicit ownership.
4. Add page-table mapping for kernel-owned stack pages, distinct from PROCESS user APIs.
5. Add transactional stack+mapping creation and rollback tests.
6. Integrate scheduler Thread creation.
7. Add lifecycle-safe reclamation after successful TCB reaping.
8. Add QEMU tests before enabling the path by default.

Until these gates are met, stack allocator output remains a physical descriptor only and must
not be used as a CPU stack pointer.
