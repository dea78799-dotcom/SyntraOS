# SyntraOS VirtIO-MMIO Discovery and Identity v2

Status: Device Tree discovery plus pure register-snapshot decoding
Target: QEMU ARM64 `virt`

## Implemented

- Locate the first Device Tree node compatible with `virtio,mmio`.
- Read its first `reg` tuple through the existing validated FDT parser.
- Validate the resulting base address and region size.
- Define the public VirtIO-MMIO identification register offsets and expected magic value.
- Decode a copied register snapshot, validate transport versions 1 and 2, and classify device ID 18 as VirtIO Input.
- Host tests cover metadata bounds, valid Input identification, other device IDs, invalid magic, unsupported versions, and null arguments.

## Explicit safety boundary

The identity decoder accepts ordinary copied values. It never dereferences the physical device address and never reads or writes MMIO.

This stage does not:
- add the discovered region to the active ARM64 Device-memory page-table plan;
- read identification registers from hardware;
- reset or configure a device;
- negotiate VirtIO feature bits;
- allocate or publish a virtqueue;
- enable or acknowledge an interrupt;
- consume keyboard or pointer events.

The kernel's current boot mapping plan does not yet include this discovered VirtIO region. Directly casting its physical address to a C pointer would therefore be unsafe. Mapping and post-activation verification must come before a hardware register reader.

## Next stages

1. Extend the checked MMU mapping plan with the discovered VirtIO region as Device, RW, and execute-never memory.
2. Verify that mapping before adding a bounded read-only identification-register accessor.
3. Negotiate the VirtIO Input device and configure a bounded event queue.
4. Decode input events into `syntra_input_event_t`.
5. Route events to the desktop/window interaction loop and test using QEMU's virtual input devices.

The implementation is original SyntraOS C code. It uses the project's FDT and MMU components and public VirtIO transport definitions; no vendor GUI source or third-party GUI toolkit is imported.
