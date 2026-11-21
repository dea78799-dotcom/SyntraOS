# SyntraOS GUI App Surfaces V1

## Scope

The window renderer supports four typed window surfaces:

- `SYNTRA_GUI_APP_GENERIC`: the existing generic window body.
- `SYNTRA_GUI_APP_FILES`: a visual Files prototype with sidebar and folder rows.
- `SYNTRA_GUI_APP_SETTINGS`: a visual Settings prototype with Display, Input, System, and About rows.
- `SYNTRA_GUI_APP_LAUNCHER`: a visual launcher grid.

Create a typed window with:

```c
uint32_t window_id;
syntra_gui_window_open_app(
    &manager, x, y, width, height,
    SYNTRA_GUI_APP_FILES, &window_id);
```

The original `syntra_gui_window_open()` remains source-compatible and opens a generic window.

## Rendering

`syntra_gui_render_windows()` draws each window back-to-front, its title bar, close affordance, and the typed app surface. Surface drawing is framebuffer-only and uses the existing 5x7 built-in font and fill-rectangle primitive. App types are validated when opening a window; unknown values are rejected.

## GUI v5 composition preview

On the 640x480 QEMU framebuffer, the desktop renderer composes the Files, Settings, and Launcher prototype windows over the desktop before ramfb is configured. A boot marker reports successful composition. Compact 320x240 rendering intentionally stays with the uncluttered base desktop. This is a visual integration milestone, not live window interaction.

## Current limitations

These are prototype visual surfaces, not filesystem or settings implementations. The surfaces have no app actions, file I/O, settings persistence, launcher-to-window dispatch, or event-loop integration yet. QEMU live input and CI validation are still pending for this PR.


## Reusable GUI v5 composition initializer

`syntra_desktop_gui_v5_init_windows(manager, width, height)` creates the fixed Files, Settings, and Launcher composition in a caller-owned manager. This allows integration tests or a future input loop to operate on the same window records instead of recreating a private manager. It currently requires dimensions of at least 640x480 and returns an initialized manager with three windows.

A new host integration test exercises the composition itself: it verifies typed windows and initial z-order, drags Launcher using title-bar pointer input, then closes Files and verifies the window is removed. This is host-level interaction logic only; the live VirtIO IRQ path still does not feed this manager, and no QEMU test is claimed for this integration.
