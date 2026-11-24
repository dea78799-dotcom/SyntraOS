# D3.4C — Owned Kernel Thread Creation Coordinator

Status: first runtime implementation, under CI validation.

## API

`process_thread_create_kernel_allocated_with_handle()` creates a process-owned kernel thread and obtains its EL1 stack from Memory V2. The caller supplies a block count; each allocator block is 4 KiB. The function returns the generation-aware thread handle only after publication succeeds.

## Transaction order

1. Validate the exact process handle and reject terminated processes.
2. Ensure Memory V2 is initialized and allocate a STAGED stack using an allocation-generation token.
3. Reserve an unpublished BLOCKED TCB in the scheduler.
4. Bind the allocation to the reserved thread's exact slot/generation owner.
5. Attach the allocation token after the scheduler independently validates RESERVED state, exact owner, address, and capacity.
6. Commit the allocation to THREAD_OWNED.
7. Publish the fully described TCB as READY; only then write the caller's output handle.

The coordinator holds the existing local IRQ guard across the complete operation. Nested manager and scheduler guards preserve the prior DAIF mask. This is single-core serialization, not an SMP lock.

## Rollback

Before publication, rollback first removes the exact unpublished reservation, then releases only the matching allocator token in its current exact state: STAGED, RESERVED with exact owner, or THREAD_OWNED with exact owner. If aborting the reservation fails, the stack is not freed. If exact allocator rollback fails, the resource is retained and the operation reports an error; it is never released by address or guessed metadata.

## Compatibility and limits

- Existing caller-provided kernel stacks remain BORROWED compatibility paths.
- Existing user-thread creation still receives caller-provided kernel stack, user stack, and CPU context.
- This API does not build architecture context frames; it follows the existing kernel-thread metadata creation contract.
- Process termination still marks threads TERMINATED but does not reclaim owned stacks.
- No reaper, stack release on exit, TCB removal, slot reuse, SMP locking, or physical-hardware validation is included.
