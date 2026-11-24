# SYNTRA OS

### Independent Operating System Project

SyntraOS is an early-stage operating system project exploring a modular kernel and a native software ecosystem.

## Project Information

- Project: SyntraOS
- Version: 0.1.0
- Status: Early development — virtual-machine prototype
- Kernel: Custom kernel with microkernel-oriented architecture
- Primary Languages: C, Assembly; Rust is a future possibility
- Current validation target: ARM64 in QEMU
- Long-term targets: Mobile and additional CPU architectures

## Current Development Scope

The current milestone focuses on a reliable virtual development environment before attempting physical hardware support.

Implemented foundations include:

- ARM64 kernel build and QEMU boot.
- Exception handling, interrupt controller and Generic Timer tests.
- MMU enablement and post-MMU interrupt validation in QEMU.
- Basic scheduler and context-switch test.
- Memory Region Manager and physical Page Allocator.
- Memory Manager V1 fixed-pool allocator.
- Process Manager V1 metadata prototype.
- IPC V1 bounded message-queue prototype.
- HAL and RAM framebuffer desktop validation.

## Architecture Direction

The long-term architecture includes:

1. Boot and platform initialization
2. Kernel and scheduler
3. Memory management
4. Process and thread management
5. Inter-process communication
6. Hardware abstraction and drivers
7. System services
8. Application runtime
9. Syntra UI
10. Security and isolation

Some components currently exist only as prototypes or design targets. See the architecture documentation for each component's implementation status and limitations.

## Run SyntraOS in QEMU

The ARM64 CI workflow builds and boots the kernel in QEMU and publishes test artifacts. See [the Android/Termux QEMU guide](docs/testing/qemu_android_termux.md) for running the console build in a controlled virtual environment.

## Development Safety

All current development and testing use virtual machines and emulators. SyntraOS is not currently validated for installation on physical phones. Do not flash experimental builds to a primary device.

## License

To be determined.

---

SyntraOS — Building a new operating system.
