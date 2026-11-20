# SyntraOS ARM64 Boot Integration

Version: 1.0.0  
Status: Design Proposal  
Target: ARM64  
Initial Platform: QEMU `virt`

## 1. Overview

This document defines the initial boot integration
design for SyntraOS on ARM64.

The first milestone targets a reproducible
development environment using QEMU ARM64 `virt`.

The implementation must preserve the existing
host-side build and test environment.

## 2. Objectives

The initial boot integration must provide:

- An ARM64 kernel entry point.
- A defined kernel memory layout.
- A linker script for the ARM64 kernel image.
- A reproducible cross-compilation environment.
- Basic early serial output for debugging.
- A QEMU boot test.
- Separate host and ARM64 build configurations.

## 3. Initial Target Platform

The first supported development platform is:

- Machine: QEMU `virt`
- Architecture: AArch64
- CPU: ARM64-compatible QEMU CPU
- Memory: Configurable QEMU RAM
- Output: Virtual serial console

Real mobile hardware is not part of V1.

Hardware-specific boot integration will be handled
in later platform-specific stages.

## 4. Boot Method

The initial implementation will investigate
QEMU's generic ELF loader.

The loader must be able to:

- Load the ARM64 ELF kernel image.
- Select the boot CPU.
- Set the initial program counter.
- Transfer execution to the kernel entry point.

The exact QEMU command and loader behavior
must be verified before implementation.

The boot design must not assume that the CPU
always starts at a specific exception level.

The kernel entry code must inspect the current
exception level before architecture initialization.

## 5. Boot Sequence

The proposed boot sequence is:

1. QEMU initializes the virtual machine.
2. QEMU loads the ARM64 kernel image.
3. The CPU enters the kernel entry point.
4. The entry code establishes the required
   execution environment.
5. The kernel prepares its initial stack.
6. The kernel enters the C initialization stage.
7. ARM64 architecture initialization is performed.
8. Early serial output reports boot progress.
9. The kernel reports its current initialization status.

Memory management, interrupt handling,
and scheduler activation will be integrated
in later milestones.

## 6. Proposed Source Layout

The proposed ARM64 boot-related structure is:

kernel/
  arch/
    arm64/
      arch.c
      arch.h
      exception.c
      exception.h
      registers.c
      registers.h
      boot/
        entry.S
        linker.ld

The existing ARM64 architecture files
must remain intact unless a specific change
is reviewed and approved.

## 7. Kernel Entry Point

The ARM64 entry point will be implemented
in `entry.S`.

Responsibilities:

- Provide the initial execution symbol.
- Establish a valid stack.
- Preserve required boot information.
- Transfer control to the C kernel entry function.

The entry code must not assume that
the stack or runtime environment is already
configured by the host build system.

## 8. Linker Script

The linker script will define:

- Kernel entry symbol.
- Kernel load address.
- Text section.
- Read-only data section.
- Initialized data section.
- Zero-initialized data section.
- Stack and alignment requirements.

Memory addresses must be validated
against the selected QEMU platform.

No arbitrary memory layout will be treated
as a verified hardware specification.

## 9. Cross-Compilation

The ARM64 target will use a dedicated
bare-metal cross-compilation toolchain.

The toolchain configuration must define:

- Target architecture.
- Compiler.
- Assembler.
- Linker.
- Target-specific compiler flags.
- Target-specific linker flags.

The ARM64 build must remain separate
from the existing host CMake presets.

## 10. Early Debug Output

V1 will provide basic serial output
for boot diagnostics.

The initial implementation will target
the QEMU virtual serial device.

The serial driver must be isolated from
the generic kernel logging interface.

A hardware-independent logging redesign
is outside the scope of this milestone.

## 11. Build Separation

The project must preserve two independent paths:

### Host Build

- Existing host-debug preset.
- Existing host-release preset.
- Existing unit tests.
- Existing CI behavior.

### ARM64 Build

- ARM64 cross-compilation.
- ARM64 assembly.
- ARM64 linker script.
- ARM64 kernel ELF image.
- QEMU boot validation.

ARM64-specific assembly must not be included
in the host static library.

## 12. Testing Strategy

The implementation will be validated in stages:

1. Verify cross-compiler availability.
2. Compile the ARM64 entry assembly.
3. Link the ARM64 ELF image.
4. Inspect the ELF architecture and entry symbol.
5. Load the image in QEMU.
6. Verify execution reaches the kernel entry point.
7. Verify early serial output.
8. Verify the existing host build and tests remain valid.

A successful host build does not prove
that the ARM64 kernel can boot.

A successful ELF link does not prove
that the kernel can execute.

Boot success must be verified through QEMU.

## 13. Compatibility Requirements

The implementation must:

- Preserve existing host functionality.
- Preserve existing unit tests.
- Avoid unnecessary changes to existing modules.
- Keep ARM64-specific code isolated.
- Avoid claiming hardware support before verification.

## 14. Out of Scope for V1

The following features are not included:

- Physical smartphone boot support.
- Real hardware device drivers.
- GIC interrupt controller initialization.
- Hardware timer initialization.
- Context switching.
- Preemptive scheduling.
- User-space process execution.
- Full memory management activation.

These features will be developed
in separate milestones.

## 15. Completion Criteria

ARM64 Boot Integration V1 is complete when:

- The ARM64 kernel ELF image builds successfully.
- The ELF entry point is verified.
- QEMU loads and executes the kernel.
- Early serial output is visible.
- Existing host builds remain functional.
- Existing host unit tests pass.
- CI reports the relevant build and test results.

## 16. Current Status

Design proposal only.

No boot assembly, linker script,
cross-compilation target, or QEMU boot
implementation is claimed to exist yet.
