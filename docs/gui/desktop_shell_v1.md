# SyntraOS GUI Desktop Shell v1

## Purpose

The desktop shell is the first visual layer rendered by the kernel into a caller-owned framebuffer. Version 1 focuses on a consistent visual identity and safe rendering at two display classes. It is a static preview, not yet a complete graphical session.

## Visual system

- Deep navy wallpaper with a layered blue band.
- Dark translucent-style panels represented by the existing opaque XRGB8888 framebuffer colors.
- Cyan accent for system identity and active visual markers.
- White primary labels and muted blue-gray secondary labels.
- Blue, purple, green and orange launcher tiles provide distinct visual categories.

## Desktop layout

The 640x480 QEMU profile uses:
1. A top navigation/status bar with SyntraOS identity, ready indicator and ARM64 target.
2. A welcome panel with the SyntraOS name and an ARM64 preview marker.
3. Four launcher tiles for Applications, Files, Games and Settings.
4. A system overview panel that reports the preview architecture, framebuffer size and preview mode.
5. A bottom dock identifying the desktop preview.

## Compact layout

Framebuffers smaller than 640 pixels wide or 480 pixels high use a compact layout:
- Reduced top bar and welcome panel.
- Four evenly spaced launcher tiles.
- Reduced bottom dock.
- No desktop-only overview card, preventing overlap on small displays.

The renderer currently rejects framebuffers smaller than 320x240.

## Current capabilities and boundaries

Implemented:
- Software framebuffer rendering.
- 640x480 desktop layout used by the QEMU ramfb integration.
- Compact static layout for smaller framebuffer dimensions.
- Clipping through the existing framebuffer rectangle primitive.
- Host tests for desktop layout, compact layout and padded row stride.

Not implemented:
- Mouse, keyboard or touch event routing.
- Window manager, movable/resizable windows or focus management.
- Real application launching, settings persistence or desktop notifications.
- Hardware display drivers beyond the existing QEMU ramfb path.

The labels and status indicators describe the preview renderer only. They must not be interpreted as evidence that the corresponding application services or input devices are operational.

## Validation

The host GUI tests verify stable color anchors, launcher placement, the overview panel, compact rendering and row-padding safety. The ARM64 CI job boots the kernel in QEMU, initializes ramfb, captures a screenshot and checks representative desktop pixels.
