# SYNTRA OS Architecture

Version: 0.1.0
Status: Early development — ARM64 virtual-machine prototype

## 1. Overview

SYNTRA OS is an independent operating-system project exploring a modular
kernel with a microkernel-oriented architecture. The current implementation
is an early prototype, not a production-ready mobile operating system.

## 2. Current implementation scope

The current automated validation target is ARM64 running in QEMU. Implemented
and tested foundations include:

- Freestanding ARM64 kernel build and ELF validation.
- QEMU boot and early platform discovery.
- Exception handling, GIC and Generic Timer interrupt paths.
- MMU validation in the emulated environment.
- V1 round-robin scheduler and ARM64 context-switch test path.
- Memory Region Manager and physical Page Allocator V1.
- Memory Manager V1 fixed-pool allocator.
- Process Manager V1 metadata table.
- IPC V1 bounded message-queue prototype.
- HAL and RAM framebuffer desktop validation.

The exact scope and limitations of each subsystem are documented in
`docs/architecture/`.

## 3. Architecture direction

The long-term design is organized around these responsibilities:

1. Boot and platform initialization.
2. Kernel core, exceptions, interrupts, and scheduler.
3. Memory management.
4. Process and thread management.
5. Inter-process communication.
6. Hardware abstraction and device drivers.
7. System services.
8. Application runtime and native applications.
9. SYNTRA UI and desktop/mobile shell.
10. Security, capabilities, and isolation.

Some of these components are prototypes or design targets rather than
completed subsystems. In particular, current Process Manager records do not
create isolated address spaces, and current IPC sender IDs are not authenticated
identities.

## 4. CPU architectures and hardware

ARM64 in QEMU is the current validation target. x86_64, RISC-V, and physical
mobile-device support are long-term goals, not currently supported or
validated platforms. Android version alone does not guarantee hardware
compatibility.

## 5. Application ecosystem

Native applications and a possible Android-application compatibility layer
are long-term goals. Android compatibility is not implemented by the current
prototype and requires a separate runtime and framework.

## 6. Development and safety

Development and automated testing use controlled virtual environments.
SyntraOS has not been validated for installation on physical phones and must
not be treated as a daily-driver operating system. Do not flash experimental
images to a primary device.
