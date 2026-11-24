# D3.4C — Owned Kernel Thread Creation Coordinator

Status: implementation candidate under CI validation.
Base: D3.4B.2 unpublished scheduler reservation transaction (PR #86).

## Public API

`process_thread_create_kernel_owned_with_handle()` creates a process-owned kernel thread without accepting a caller-provided stack. It validates the exact process generation, rounds the requested stack capacity up to 4 KiB Memory V2 blocks, and returns the exact thread handle only after publication succeeds.

## Transaction

1. Validate arguments and resolve the exact process handle; terminated processes are rejected.
2. Allocate the kernel stack as STAGED and retain the generation-aware allocation token.
3. Reserve an unpublished BLOCKED scheduler TCB.
4. Bind the allocation to the reserved thread's exact slot/generation.
5. Attach the allocation token after scheduler-side validation of RESERVED state, exact owner, base address, and capacity.
6. Commit the allocation to THREAD_OWNED.
7. Publish the TCB as READY as the final visibility point.

The coordinator holds the existing outer local-IRQ guard across the sequence. Nested guards preserve the already-masked interrupt state. This is a single-core critical section, not an SMP lock.

## Rollback

- Before reservation: abort the exact STAGED token.
- After reservation but before allocator commit: first abort the exact unpublished TCB, then abort the exact STAGED or RESERVED token.
- After allocator commit but before publication: first abort the exact unpublished TCB, then release the exact THREAD_OWNED token with the exact thread owner.
- If exact TCB abort fails, do not free the allocation; quarantine it for diagnosis. If exact allocator cleanup fails, return an error and retain the uncertain resource.
- The output thread handle is written only after successful publication.

## Compatibility and exclusions

- Existing caller-stack kernel and user-thread APIs remain BORROWED and unchanged.
- This API only owns the EL1 kernel stack. User stacks and architecture CPU contexts remain outside this transaction.
- Terminating a thread does not release its stack yet. Deferred reaper integration, safe stack reclamation, TCB removal, and slot reuse remain future work.
- No physical hardware testing is claimed; validation is host CI and ARM64 QEMU.
