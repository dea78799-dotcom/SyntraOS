# GUI v2 — Launcher focus feedback

GUI v2 adds a session-aware rendering entry point:

- `syntra_desktop_render()` remains the stable static desktop renderer.
- `syntra_desktop_render_session()` renders the same desktop, then draws a 2-pixel white outline around the focused launcher tile.
- Focus state is owned by `syntra_desktop_session_t`; keyboard navigation continues to be processed by the transport-independent session layer.
- Both 640x480 and compact 320x240 layouts use the matching launcher geometry.

## Scope and limitations

This change provides visible focus feedback when a caller supplies a valid desktop session. It does not add a QEMU keyboard, mouse, or touch transport. The current Termux/AVNC display test therefore remains a display/boot test; it does not prove live pointer or touch input.

## Validation

The host renderer test checks the initial focus outline, focus movement to the second tile, restoration of the first tile's base color, and preservation of framebuffer stride padding. CI remains the authoritative build and test result.
