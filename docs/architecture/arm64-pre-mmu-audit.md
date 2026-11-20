# ARM64 Pre-MMU Repository Audit

Audit baseline: `main` at `32753527baa4f5933f0c8f830844567e62d725ec`
Scope: ARM64 MMU helpers, linker and entry path, FDT interfaces, memory allocator,
process metadata, scheduler context contract, ARM64 CMake target, and CI/QEMU smoke test.

Status: focused foundation audit complete; full repository audit remains open.
No hardware translation registers are changed by this audit.

## 1. Verified working foundation

- The latest recorded CI run for this baseline is run `44692629411` (#426).
  Both host build/test and ARM64 cross-build/QEMU jobs completed successfully.
- The QEMU smoke test verifies timer IRQ delivery and a real two-worker scheduler
  context-switch trace. The observed trace was `ABABABAB`.
- The boot path runs at EL1 on the configured QEMU virt target and installs the
  exception vector table before entering the C boot stage.
- The GIC backend and timer path are integrated with the interrupt framework.
  The QEMU test proves the current interrupt path works in the existing boot
  address regime. It does not prove that the same path works after MMU activation.
- The scheduler owns thread metadata and accepts caller-owned stacks. ARM64
  exception-frame construction and timer-driven switching are already exercised.

## 2. Current MMU implementation boundary

`kernel/arch/arm64/mmu.c` and `mmu.h` currently provide descriptor construction,
4 KiB / 39-bit VA / 48-bit PA constants, VA index extraction, and mapping-range
validation. The host test covers these helpers.

They do not yet provide:
- a page-table page allocator or table hierarchy builder;
- a mapping database, conflict detection, unmap, or address translation API;
- MAIR_EL1, TCR_EL1, TTBR0_EL1, SCTLR_EL1 programming;
- TLB maintenance or a controlled enable/disable transition;
- EL0-accessible page descriptors or per-region user permissions.

The descriptor helper explicitly denies user access. The existing tests validate
descriptor bit patterns, not hardware translation.

## 3. Physical memory and boot blockers

- `kernel/memory/memory.c` is a fixed 4 KiB block pool backed by a static C array.
  It is not a physical-page allocator and does not establish physical addresses
  for page-table pages.
- The ARM64 CMake executable does not currently include the general memory module.
  The host memory allocator therefore cannot be assumed available to the ARM64
  bootstrap.
- The FDT API exposes compatible-node lookup and device `reg` tuples. It does not
  currently expose a validated RAM `device_type = "memory"` map or
  `/reserved-memory` reservations for the MMU bootstrap.
- The linker places the image at `0x40200000`, reserves the boot stack, and exports
  kernel boundaries. It does not reserve a page-table arena or define section
  boundaries suitable for distinct text/rodata/data page permissions.
- The entry path checks EL1 and installs the stack/vector table, but does not inspect
  SCTLR_EL1.M or establish a documented inherited-translation contract.
- The DTB pointer is currently fixed at `0x40000000` for the QEMU virt loader
  configuration. The transition must preserve the DTB and every active code,
  stack, vector, and data address until the new translation regime is live.
- UART and GIC accesses currently use physical-looking addresses directly. When
  translation is enabled, required MMIO ranges must remain accessible with Device
  attributes before any such access.

## 4. Process and userspace boundary

`kernel/process/process.c` currently manages process IDs, priority metadata, and
basic states only. A process has no address-space root, user entry point, user
stack, kernel stack, or scheduler-thread association.

No syscall dispatcher or stable syscall ABI is present in the audited source tree.
The current SVC tests are EL1/kernel exception-path tests; they are not evidence
of an EL0 application or userspace syscall path.

EL0 must not be entered until user mappings, separate user/kernel stack handling,
lower-EL exception entry, and validated syscall argument handling are implemented.

## 5. Required implementation sequence

1. Extend FDT parsing to return validated RAM and reserved-memory ranges, including
   parent cell widths and overflow-safe range checks.
2. Define a linker-reserved, page-aligned bootstrap page-table arena and export
   section boundaries. Confirm it does not overlap the kernel, stack, DTB, or
   reserved regions reported by FDT.
3. Build host-testable page-table primitives and a bounded table-page allocator.
   Define a single explicit initial identity-map plan for kernel code/data, stack,
   vectors, DTB, page tables, UART, GIC Distributor, and GIC Redistributor.
4. Review the exact ARMv8-A MAIR/TCR/TTBR/SCTLR values for the selected 4 KiB
   granule and Cortex-A57/QEMU configuration. Add permission tests, including
   privileged-only kernel pages and non-executable Device mappings.
5. Add a controlled MMU transition with required barriers and TLB invalidation.
   Keep interrupts masked during transition. QEMU must demonstrate boot, UART,
   timer IRQ, GIC, and scheduler trace after translation is active.
6. Only after MMU validation, add process address spaces and EL0 context entry.
7. Define and test the SVC syscall ABI together with EL0: write, yield, and exit;
   validate user pointers and preserve scheduler context across EL0/EL1 transitions.
8. Begin GUI only after the memory, userspace, and display-device foundations are
   ready and the repository-wide audit is closed.

## 6. Audit cautions

- Do not treat host descriptor tests or a successful ARM64 ELF link as proof that
  translation is active.
- Do not enable the MMU using guessed RAM sizes, guessed register values, or an
  unverified page-table backing address.
- Do not repurpose the existing static block pool as physical page memory without
  an explicit physical-address and reservation contract.
- Do not enable EL0 by merely changing SPSR_EL1; user mappings, exception routing,
  stacks, and syscall validation are required together.
- Keep the working GIC/timer/scheduler path intact while adding MMU support.

## 7. Remaining audit scope

This is a focused pre-MMU audit, not a claim that every repository file has been
reviewed. Remaining review includes all HAL implementations, remaining tools and
scripts, driver stubs, all architecture documentation, and cross-module API usage.
