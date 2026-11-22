# SyntraOS Virtual Desktop Interaction Foundation

Status: QEMU VirtIO keyboard transport prototype
Target: QEMU ARM64 `virt`
Minimum framebuffer: 320x240

## Input core

`kernel/input/input.c` converts normalized Linux/VirtIO-style input events
into pointer coordinates, button state, keyboard state, wheel movement, and
frame boundaries. It is transport-independent.

## Desktop interaction layer

`kernel/gui/desktop_session.c` consumes normalized events and provides the
desktop interaction policy:

- Arrow keys move focus between the four launcher tiles.
- Enter activates the focused launcher tile.
- A left-button press activates a launcher tile under the virtual pointer.
- Hit testing supports the 640x480 desktop and compact layout.

The activation result is currently recorded as desktop session state. It does
not launch applications because an application manager is not implemented.

## QEMU VirtIO-MMIO keyboard transport

The ARM64 boot diagnostic now performs a limited keyboard transport test:

1. Discover a VirtIO-MMIO device through the QEMU-generated FDT.
2. Negotiate the `EV_SYN` and `EV_KEY` event-type feature bits.
3. Configure event queue 0 with writable event buffers.
4. Register and enable the device interrupt.
5. Start the queue and inject `KEY_A` from QMP in CI.
6. Confirm the key press was received through the VirtIO event queue and IRQ
   path.

This test verifies the transport and key-event delivery only. The boot
diagnostic records the event; the VirtIO software event FIFO is not yet wired
into the GUI runtime or desktop session.

## Current boundary

- The implemented QEMU path is keyboard-only; a pointer device and pointer-event
  delivery test are not implemented.
- Host tests inject normalized events directly into GUI/runtime code.
- The QEMU framebuffer is visually rendered, but the boot diagnostic does not
  yet route live keyboard events to GUI windows or launch applications.
- This is QEMU virtual-machine support, not physical-device, Android, or mobile
  hardware integration.
