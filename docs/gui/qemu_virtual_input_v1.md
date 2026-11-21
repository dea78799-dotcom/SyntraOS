# SyntraOS Virtual Desktop Interaction Foundation

Status: Foundation implementation
Target: QEMU ARM64 `virt`
Minimum framebuffer: 320x240

## Scope

This work is for the QEMU virtual machine only. It does not add support for
physical keyboards, mice, touchscreens, mobile phones, or other real hardware.

## Existing input core

`kernel/input/input.c` converts normalized Linux/VirtIO-style input events
into pointer coordinates, button state, keyboard state, wheel movement, and
frame boundaries. It is transport-independent.

## Desktop interaction layer

`kernel/gui/desktop_session.c` consumes those normalized events and provides
the first desktop interaction policy:

- Arrow keys move focus between the four launcher tiles.
- Enter activates the focused launcher tile.
- A left-button press activates a launcher tile under the virtual pointer.
- A held mouse button does not repeatedly activate the same tile.
- Hit testing follows both the 640x480 desktop layout and compact layout.

The activation result is currently recorded as desktop session state. It does
not launch applications because an application manager is not implemented.

## Current boundary

Host tests inject normalized events directly into the desktop session. This
validates the GUI interaction policy without pretending that QEMU's virtual
keyboard or mouse is already connected to the guest.

The next transport milestone is a QEMU-only VirtIO input path:

1. Add a VirtIO-MMIO transport discovered from the QEMU-generated FDT.
2. Negotiate the VirtIO input device and configure its event virtqueue.
3. Decode keyboard and pointer events into `syntra_input_event_t`.
4. Deliver events to the desktop session from the kernel's event loop.
5. Run QEMU with virtual keyboard and pointer devices and verify events in CI.

Until those steps are complete, the QEMU desktop remains visually rendered but
does not receive live keyboard or mouse input from the VM frontend.
