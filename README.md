
# SYNTRAOS

### Universal Mobile Operating System

SyntraOS is an independent mobile operating system
project designed for multiple hardware architectures.

## Project Information

- Project: SyntraOS
- Version: 0.1.0
- Status: Early Development
- Kernel: Custom Microkernel
- Primary Language: C, Rust, Assembly
- Target: Mobile Devices
- Architectures: ARM64, x86_64, RISC-V

## Main Goals

- Build an independent operating system.
- Develop a custom Microkernel.
- Support multiple CPU architectures.
- Create a native application ecosystem.
- Develop Android application compatibility.
- Build a custom mobile user interface.
- Support multiple mobile hardware platforms.

## Architecture

SyntraOS consists of:

1. Bootloader
2. Microkernel
3. Memory Manager
4. Process Manager
5. IPC System
6. Hardware Drivers
7. System Services
8. Application Runtime
9. Syntra UI
10. Security System

## Development Principles

- Modular source code.
- Hardware abstraction.
- Memory safety.
- Security-first development.
- Continuous testing.
- Long-term development.

## Project Structure

The source code is organized into
independent modules.

See ARCHITECTURE.md for technical details.

## Run SyntraOS in QEMU

The ARM64 CI workflow validates the kernel in QEMU and publishes the tested ELF as a downloadable workflow artifact. See [the Android/Termux QEMU guide](docs/testing/qemu_android_termux.md) for running the console build on an Android device.

## Safety

All initial development and testing will use
safe development environments and virtual machines.

No experimental firmware will be installed
on the developer's primary phone.

## License

To be determined.

---

SyntraOS - Building a new mobile OS.
# SyntraOS
