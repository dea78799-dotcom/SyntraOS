# SyntraOS Changelog

## 0.1.0 — Foundation prototype

Release stage: Early development / virtual-machine prototype

### Implemented foundations

- Initial kernel and modular source tree.
- ARM64 freestanding kernel build and ELF validation.
- QEMU ARM64 boot path and early hardware discovery.
- Exception handling, GIC/Generic Timer interrupt path and MMU validation in QEMU.
- Basic scheduler with round-robin selection and ARM64 context-switch testing.
- Memory Region Manager and physical Page Allocator V1.
- Memory Manager V1 fixed-pool allocator with allocation tracking and diagnostics.
- Process Manager V1 metadata table.
- IPC V1 bounded message-queue prototype.
- HAL, RAM framebuffer and basic graphical desktop validation in QEMU.
- Debug/Release builds and automated unit/integration tests.

### Validation scope

The supported development and test environment for this stage is a virtual machine using QEMU ARM64. CI validation does not establish compatibility with physical phones, production hardware, or daily-driver use.

### Known limitations

- Process records do not yet provide independent address spaces or full resource lifecycle management.
- IPC does not yet authenticate process identities or enforce endpoint permissions.
- Memory Manager V1 is not thread-safe.
- Hardware support is limited to the emulated devices and paths covered by CI.
- The project is not a production-ready operating system.

### Version policy

The current project version remains 0.1.0. Bug fixes and prototype improvements do not automatically change the version number.
