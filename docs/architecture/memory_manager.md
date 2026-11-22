# SyntraOS Memory Manager

Version: 0.1.0
Status: Implemented foundation; virtual-memory isolation in progress
Target currently validated by CI: ARM64 QEMU `virt` and host tests

## 1. Scope

The memory subsystem currently contains three distinct layers:

1. A fixed kernel allocation pool in `kernel/memory/memory.c`.
2. A physical memory-region registry and physical page allocator.
3. ARM64 translation descriptors, page-table construction, mapping-plan validation,
   and a boot-time MMU activation path.

These layers must not be treated as interchangeable. In particular, the fixed
kernel pool is not the physical page allocator.

## 2. Implemented foundation

### 2.1 Fixed kernel allocation pool

- 4 KiB allocation blocks.
- 1024 blocks (4 MiB total pool).
- Allocation/release tracking, diagnostics, snapshots, and validation APIs.
- This pool is backed by a static array; it does not represent arbitrary physical RAM.

### 2.2 Physical memory map

- FDT discovery of enabled root-level RAM nodes.
- Discovery of static `/reserved-memory` children.
- Application of FDT header reservation-map entries.
- Reservation of the DTB and complete linker-defined kernel image before page
  allocation begins.
- A bounded region registry rejects overlapping added regions and supports carving
  reserved ranges out of RAM.

Dynamic `/reserved-memory` nodes that specify `size` rather than `reg` are not
allocated by the current discovery path.

### 2.3 Physical page allocator V1

- 4 KiB pages managed with static free/allocated bitmaps and a summary bitmap.
- Single-page and physically contiguous page allocation.
- Allocation is limited to physical addresses below 4 GiB; RAM crossing that
  boundary is clipped and RAM entirely above it is ignored.
- Local IRQ masking protects allocator metadata against same-core interrupt
  re-entry on the current single-core target.
- One-byte owner tags distinguish KERNEL, PAGE_TABLE, PROCESS, and DRIVER pages.
- Owned free operations reject mismatched owners; the full allocator audit checks
  bitmap, summary, accounting, and owner-tag consistency.

Limitations: owner tags are not reference counts and do not permit shared-page
lifetime management. This is not an SMP lock, and the allocator does not yet provide
a general-purpose kernel heap or per-process allocation accounting.

### 2.4 ARM64 MMU and page tables

- 4 KiB granule descriptor helpers and a software page-table walker.
- L1/L2/L3 table construction with 1 GiB / 2 MiB block and 4 KiB page mappings
  where supported by the builder.
- Mapping-plan construction for the kernel image, DTB, UART, GIC, and discovered
  optional MMIO devices.
- Permission policy separates executable read-only kernel text from read-only
  non-executable rodata and writable non-executable data/stack; MMIO is Device
  memory and non-executable.
- The ARM64 QEMU boot path builds and verifies identity mappings, prepares the
  translation registers, enables the MMU, checks register state, and re-walks
  planned mappings after activation.
- The V1 address-space object owns an independent L1 root, creates subordinate
  page-table pages with PAGE_TABLE ownership, maps exclusively owned PROCESS
  pages into EL0, translates addresses through its own root, audits permissions,
  and releases its page-table and mapped-process pages on destruction.
- Host integration tests create two independent roots, map the same virtual
  address to different physical pages, verify distinct translations and
  read-only/read-write permissions, then validate complete page reclamation.

A successful host descriptor test or ELF build alone is not proof of hardware
translation. The QEMU boot test is the evidence for the configured emulated target;
it is not evidence of physical-device support.

## 3. Current limitations

- The physical allocator is capped at 4 GiB and is designed for the current
  single-core boot target.
- Page-table pages are allocated from the physical page allocator, but there is
  no general page-table reclamation or mapping teardown API.
- The current boot mapping is primarily identity mapping; a complete kernel
  higher-half layout and dynamic virtual address allocator are not implemented.
- Address-space objects exist, but they are not yet attached to process objects
  or activated by a context switch through TTBR0_EL1.
- Individual page unmapping, shared-page reference counting, copy-on-write,
  demand paging, swapping, and memory-mapped file support are not implemented.
- Address-space destruction requires the caller to ensure the root is inactive;
  V1 has no SMP coordination or TLB shootdown mechanism.
- Full SMP-safe allocator synchronization and per-page reference tracking are
  not implemented.
- x86_64 and RISC-V memory backends are not implemented.

## 4. Next implementation priorities

1. Add stronger validation for memory-region maps and page allocator invariants,
   including overflow, reservation boundaries, duplicate/overlapping ranges,
   allocation/free accounting, and failure rollback.
2. Define physical-page ownership and lifecycle APIs before sharing pages between
   page tables, processes, or drivers.
3. Extend the V1 address-space manager with individual unmap and shared-page
   reference-counting semantics.
4. Attach one address-space object to each process and switch TTBR0_EL1 only after
   context-switch and inactive-root lifetime rules are implemented.
5. Add SMP-safe synchronization and TLB shootdown before multi-core execution.
6. Expand QEMU tests to execute in separate EL0 address spaces and verify
   permission faults, not only software-walker isolation.

## 5. Verification requirements

Every memory change must pass:

- Host Debug and Release builds and unit tests.
- ARM64 cross-compilation and ELF architecture checks.
- QEMU boot tests for the configured ARM64 platform.
- Focused tests for allocator invariants and mapping permissions.

Do not mark a milestone complete from compilation alone. Record separately whether
host tests, ARM64 build, QEMU boot, and hardware testing have actually run.

## 6. Version policy

This document describes the v0.1.0 foundation. The project remains v0.1.0
(Early Development) until the required kernel, memory, scheduler, process, IPC,
GUI, and integration-test exit criteria are met.
