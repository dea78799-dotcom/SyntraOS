# SyntraOS Virtual Desktop Interaction Foundation

Status: GUI runtime and keyboard event pump integrated; live QEMU pointer/touch remains incomplete
Target: QEMU ARM64 `virt`
Framebuffer: 640x480 XRGB8888 via QEMU ramfb

## Input core

`kernel/input/input.c` converts normalized Linux/VirtIO-style input events
into pointer coordinates, button state, keyboard state, wheel movement, and
frame boundaries. It supports relative pointer motion and absolute axes when
an absolute range has been configured.

## Desktop and window interaction

The GUI runtime in `kernel/gui/gui_runtime.c` consumes normalized input events.
The window interaction layer supports:

- Hit-testing windows from front to back and raising the selected window.
- Dragging a window by its title bar while preserving the pointer offset.
- Clamping dragged windows to the virtual screen.
- Closing a window through its close control.
- Launcher tile activation for Files and Settings.
- Basic selection of visible Files entries and Settings categories.

The current Files and Settings surfaces are UI state only. They are not backed
by a persistent filesystem, settings service, or general application manager.

## QEMU VirtIO-MMIO input transport

The ARM64 boot path in `kernel/arch/arm64/boot/boot.c` discovers the first
VirtIO-MMIO device reported by the QEMU-generated FDT. When that device reports
VirtIO Input, the kernel prepares event queue 0, registers its IRQ handler,
and starts the queue.

The IRQ handler acknowledges the device interrupt, validates and copies
normalized input events into a bounded software FIFO. The normal-context
consumer in `kernel/gui/ramfb.c` drains that FIFO, passes events to the GUI
runtime, and redraws the framebuffer after a batch. GUI rendering is not done
inside the IRQ handler.

After a successful boot, `kernel/arch/arm64/boot/entry.S` calls
`syntra_arm64_boot_idle_loop()`. That loop invokes
`syntra_ramfb_process_input_events()` whenever the VirtIO input queue is
ready, then waits for the next interrupt. Therefore the IRQ-to-FIFO-to-GUI
runtime pump is connected in the current ARM64 boot path; it is not merely a
host-test-only function.

## Current limitations

- The current ARM64 transport setup handles one discovered VirtIO Input
  device. It does not yet enumerate separate keyboard and pointer devices.
- The verified QEMU diagnostic path is keyboard-oriented. A live pointer or
  tablet event delivery test is not implemented.
- GUI window drag and close behavior is covered by host-level normalized-event
  tests, but that does not prove live QEMU pointer input works.
- The QEMU command used for the Android screenshot test included `-device
  ramfb` but no VirtIO keyboard/tablet device. Its log reported an empty
  VirtIO-MMIO slot, so that run verified framebuffer output, not input delivery.
- A RAM framebuffer is an output device, not a display window or touch device.
  A `screendump` file is a still image and cannot be interacted with.
- The installed Android/Termux QEMU build must provide a graphical display
  backend (for example VNC, SDL, or GTK) and a compatible viewer before a live
  desktop can be shown on the phone. If `qemu-system-aarch64 -display help`
  does not list a usable backend, kernel changes alone cannot make that QEMU
  build display a live GUI.
- This is QEMU virtual-machine support, not physical Android hardware
  integration.

## Next engineering steps

1. Add explicit QEMU device enumeration so keyboard and pointer/tablet devices
   can be discovered independently instead of assuming the first VirtIO-MMIO
   slot is the input device.
2. Implement and test VirtIO pointer/tablet configuration, including absolute
   axis ranges and touch/button events.
3. Add QEMU integration tests that inject pointer movement, press, release,
   drag, and close events and verify the resulting GUI state/framebuffer.
4. Provide a documented Android display path that matches the actual QEMU
   backends available in the user's Termux package. Keep headless screenshot
   capture as a separate fallback, not as an interactive mode.
