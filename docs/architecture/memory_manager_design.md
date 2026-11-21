
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

- Uses a static memory pool.
- Does not manage physical RAM.
- Does not support virtual memory.
- Does not support memory protection.
- Does not support multiple allocation owners.
- Not thread-safe.
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
