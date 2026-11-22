#include "../../kernel/gui/window_renderer.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#define WIDTH 640U
#define HEIGHT 480U
#define STRIDE 648U

int main(void)
{
    uint32_t pixels[STRIDE * HEIGHT] = {0};
    syntra_framebuffer_t framebuffer = {pixels, WIDTH, HEIGHT, STRIDE};
    syntra_framebuffer_t wrong_size = {pixels, WIDTH - 1U, HEIGHT, STRIDE};
    syntra_gui_window_manager_t manager;
    uint32_t back_id;
    uint32_t front_id;
    uint32_t files_id;
    uint32_t settings_id;
    uint32_t launcher_id;

    assert(syntra_gui_window_manager_init(&manager, WIDTH, HEIGHT) ==
           SYNTRA_STATUS_OK);
    assert(syntra_gui_window_open(&manager, 20U, 30U, 180U, 120U,
                                  &back_id) == SYNTRA_STATUS_OK);
    assert(syntra_gui_window_open(&manager, 60U, 50U, 180U, 120U,
                                  &front_id) == SYNTRA_STATUS_OK);

    /* The renderer overlays windows in manager z-order. */
    assert(syntra_gui_render_windows(&framebuffer, &manager) ==
           SYNTRA_STATUS_OK);

    /* Back window keeps its inactive title bar where not overlapped. */
    assert(pixels[(size_t)32U * STRIDE + 22U] == 0xFF263D66U);

    /* Front window has the active title bar and visible frame border. */
    assert(pixels[(size_t)52U * STRIDE + 62U] == 0xFF1479D4U);
    assert(pixels[(size_t)50U * STRIDE + 60U] == 0xFF5578F2U);

    /* The close control has its own visible hit-target-sized visual. */
    assert(pixels[(size_t)54U * STRIDE + 223U] == 0xFFB83D55U);

    /* Pixels outside managed windows and stride padding remain untouched. */
    assert(pixels[10U * STRIDE + 10U] == 0U);
    assert(pixels[STRIDE + WIDTH] == 0U);
    assert(pixels[STRIDE - 1U] == 0U);

    /* Empty manager is a valid no-op; mismatched dimensions are rejected. */
    assert(syntra_gui_window_manager_init(&manager, WIDTH, HEIGHT) ==
           SYNTRA_STATUS_OK);
    assert(syntra_gui_render_windows(&framebuffer, &manager) ==
           SYNTRA_STATUS_OK);
    assert(syntra_gui_window_open_app(
        &manager, 40U, 40U, 220U, 160U,
        SYNTRA_GUI_APP_FILES, &files_id) == SYNTRA_STATUS_OK);
    assert(syntra_gui_window_open_app(
        &manager, 300U, 40U, 220U, 160U,
        SYNTRA_GUI_APP_SETTINGS, &settings_id) == SYNTRA_STATUS_OK);
    assert(syntra_gui_window_open_app(
        &manager, 40U, 220U, 220U, 130U,
        SYNTRA_GUI_APP_LAUNCHER, &launcher_id) == SYNTRA_STATUS_OK);
    assert(syntra_gui_window_open_app(
        &manager, 540U, 40U, 90U, 90U,
        (syntra_gui_app_type_t)99, &front_id) == SYNTRA_STATUS_INVALID_ARGUMENT);
    assert(manager.count == 3U);
    assert(syntra_gui_render_windows(&framebuffer, &manager) == SYNTRA_STATUS_OK);
    /* App-specific surfaces render folder, settings, and launcher accents. */
    assert(pixels[(size_t)75U * STRIDE + 86U] == 0xFF3B9FB5U);
    assert(pixels[(size_t)75U * STRIDE + 313U] == 0xFF54D6E8U);
    assert(pixels[(size_t)249U * STRIDE + 49U] == 0xFF54D6E8U);

    /* Selection highlight remains continuous behind opaque text rendering. */
    manager.windows[0].app_selection = 2U; /* DOWNLOADS */
    assert(syntra_gui_render_windows(&framebuffer, &manager) == SYNTRA_STATUS_OK);
    assert(pixels[(size_t)292U * STRIDE + 106U] == 0xFF25466FU);

    /* Very narrow Files windows must not underflow list highlight widths. */
    assert(syntra_gui_window_manager_init(&manager, WIDTH, HEIGHT) ==
           SYNTRA_STATUS_OK);
    assert(syntra_gui_window_open_app(
        &manager, 20U, 20U, 50U, 80U,
        SYNTRA_GUI_APP_FILES, &files_id) == SYNTRA_STATUS_OK);
    assert(syntra_gui_render_windows(&framebuffer, &manager) == SYNTRA_STATUS_OK);

    manager.windows[0].app_type = (syntra_gui_app_type_t)99;
    assert(syntra_gui_render_windows(&framebuffer, &manager) ==
           SYNTRA_STATUS_INVALID_ARGUMENT);
    manager.windows[0].app_type = SYNTRA_GUI_APP_FILES;

    assert(syntra_gui_render_windows(&wrong_size, &manager) ==
           SYNTRA_STATUS_INVALID_ARGUMENT);
    assert(syntra_gui_render_windows(&framebuffer, NULL) ==
           SYNTRA_STATUS_INVALID_ARGUMENT);

    puts("SyntraOS GUI window renderer tests passed.");
    return 0;
}
