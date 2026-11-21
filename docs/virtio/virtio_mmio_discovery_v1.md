# SyntraOS VirtIO-MMIO Discovery v1

Status: Device Tree discovery foundation
Target: QEMU ARM64 `virt`

## Implemented

- Locate the first Device Tree node compatible with `virtio,mmio`.
- Read its first `reg` tuple through the existing validated FDT parser.
- Validate the resulting base address and region size.
- Keep the result as metadata; no device register is read or written.

## Explicit safety boundary

This stage does not:
- map the MMIO region or change page-table memory attributes;
- negotiate VirtIO feature bits;
- reset or configure a device;
- allocate or publish a virtqueue;
- enable or acknowledge an interrupt;
- consume keyboard or pointer events.

The kernel's current boot path does not yet provide a verified device-memory mapping contract for this new transport. Accessing a physical address as a normal C pointer before that work is complete could fault or use incorrect memory attributes. Therefore discovery and register access are intentionally separate stages.

## Next stages

1. Add a bounded MMIO mapping contract and verify the mapping on QEMU ARM64.
2. Read the VirtIO-MMIO identification/version registers without changing device state.
3. Negotiate the VirtIO Input device and configure a bounded event queue.
4. Decode input events into `syntra_input_event_t`.
5. Route events to the desktop/window interaction loop and test using QEMU's virtual input devices.

The discovery implementation is original SyntraOS C code and uses the project's existing FDT parser. The VirtIO transport is based on the public VirtIO specification; no vendor GUI source or third-party GUI toolkit is imported.
