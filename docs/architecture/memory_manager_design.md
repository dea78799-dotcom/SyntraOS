
# SyntraOS Memory Manager Design

Version: 0.1.0
Status: Prototype

## 1. Overview

The SyntraOS Memory Manager is responsible
for managing memory allocation and release.

The current implementation uses a static
memory pool.

## 2. Memory Configuration

- Block size: 4096 bytes
- Maximum blocks: 1024
- Total memory pool: 4 MiB
- Allocation method: Contiguous block allocation
- Tracking method: Bitmap

## 3. Main Components

### memory_init()

Initializes the memory manager.

### memory_allocate()

Allocates a contiguous memory region.

### memory_release()

Releases a previously allocated memory region.

### memory_get_info()

Returns memory usage information.

## 4. Current Limitations

The original `memory_init()` / `memory_allocate()` pool remains a small
legacy allocator. It is not the allocator used for ARM64 process page tables.

The ARM64 memory path now includes a physical page allocator, per-process
address spaces, user page mappings, permission checks and TTBR0 switching.
These components are still prototype-quality and have important limits:

- Physical page allocation and address-space operations are single-core.
- Allocator and page-table operations are not yet SMP-safe or generally
  interrupt-safe.
- Kernel mappings are inherited from the boot address space; a complete
  user/kernel virtual address layout is still under development.
- Process isolation is validated by boot-time EL0 tests, not by a general
  application loader or a production process runtime.
- Kernel heap integration and demand paging are not implemented.
- Not ready for production use.

## 5. Future Development

- Physical memory management
- Virtual memory management
- Page tables
- Memory protection
- Kernel heap allocator
- Memory mapping
- Multi-core synchronization

## 6. Safety

The current implementation is source code
only and does not modify the host device.

## 7. Development Status

Memory Manager prototype: In progress.

## Physical Page Allocator V1

The ARM64 physical page allocator manages complete 4-KiB RAM pages in the
physical address window below 4 GiB. It uses static free/allocated bitmaps.
RAM regions crossing the 4-GiB boundary are clipped; regions wholly above
4 GiB are ignored. Initialization returns OUT_OF_MEMORY if no eligible pages
remain. Allocation search begins at a next-free cursor and wraps; freeing a
lower page moves the cursor backward. This improves common reuse paths but
does not bound worst-case scan time. It is currently a single-core,
non-thread-safe allocator; locking or interrupt-safe serialization is required
before concurrent runtime use.


## ARM64 Process Virtual Memory and Isolation Validation

The current ARM64 process path uses independent address-space roots. Each
process owns its user mappings while inheriting the required kernel mappings.
User pages are mapped with explicit read/write and execute permissions.

The QEMU boot validation now runs two real EL0 processes sequentially while
both process address spaces remain allocated:

- Both processes use the same user virtual address for their private data page.
- Each virtual address resolves to a different physical page.
- Each EL0 program increments only its own data page and exits through SVC.
- EL1 verifies that both physical pages contain their expected independent
  values before either process address space is destroyed.
- The existing EL0 write-to-code-page fault test continues to verify that a
  user write to a read-only executable mapping is trapped and the faulting
  thread is terminated.

This is a focused runtime isolation test. It does not yet prove concurrent
multi-core isolation, complete kernel/user separation, or a general-purpose
process execution environment.
