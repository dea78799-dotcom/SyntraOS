#include "../../kernel/gui/desktop.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#define TEST_WIDTH 640U
#define TEST_HEIGHT 480U
#define TEST_STRIDE 648U
#define COMPACT_WIDTH 320U
#define COMPACT_HEIGHT 240U

int main(void)
{
    uint32_t pixels[TEST_STRIDE * TEST_HEIGHT];
    uint32_t compact_pixels[COMPACT_WIDTH * COMPACT_HEIGHT];
    syntra_framebuffer_t framebuffer = {
        pixels, TEST_WIDTH, TEST_HEIGHT, TEST_STRIDE
    };
    syntra_framebuffer_t compact = {
        compact_pixels, COMPACT_WIDTH, COMPACT_HEIGHT, COMPACT_WIDTH
    };
    syntra_framebuffer_t invalid = {
        pixels, TEST_WIDTH, TEST_HEIGHT, TEST_WIDTH - 1U
    };

    assert(syntra_framebuffer_validate(&invalid) ==
           SYNTRA_STATUS_INVALID_ARGUMENT);
    assert(syntra_desktop_render(&framebuffer) == SYNTRA_STATUS_OK);

    /* Desktop shell: wallpaper, status bar, welcome card and dock. */
    assert(pixels[0] == 0xE8202D49U);
    assert(pixels[(size_t)200U * TEST_STRIDE] == 0xFF1B2D55U);
    assert(pixels[(size_t)60U * TEST_STRIDE + 30U] == 0xE8202D49U);
    assert(pixels[(size_t)180U * TEST_STRIDE + 364U] == 0xE8202D49U);
    assert(pixels[(size_t)180U * TEST_STRIDE + 360U] == 0xFF54D6E8U);
    assert(pixels[(size_t)304U * TEST_STRIDE + 378U] == 0xFF58C99AU);
    assert(pixels[(size_t)8U * TEST_STRIDE + 14U] == 0xFF54D6E8U);

    /* Four launcher tiles retain their stable layout and color identities. */
    assert(pixels[(size_t)180U * TEST_STRIDE + 24U] == 0xFF5578F2U);
    assert(pixels[(size_t)180U * TEST_STRIDE + 104U] == 0xFF9874F5U);
    assert(pixels[(size_t)180U * TEST_STRIDE + 184U] == 0xFF58C99AU);
    assert(pixels[(size_t)180U * TEST_STRIDE + 264U] == 0xFFFFB66EU);
    assert(pixels[(size_t)(TEST_HEIGHT - 42U) * TEST_STRIDE] == 0xE8202D49U);

    /* Stride padding must remain untouched by all desktop drawing calls. */
    assert(pixels[TEST_STRIDE + TEST_WIDTH] == 0U);
    assert(pixels[TEST_STRIDE - 1U] == 0U);

    assert(syntra_framebuffer_fill_rect(
        &framebuffer, TEST_WIDTH - 2U, TEST_HEIGHT - 2U,
        UINT32_MAX, UINT32_MAX, 0xFFAABBCCU) == SYNTRA_STATUS_OK);
    assert(pixels[(size_t)(TEST_HEIGHT - 1U) * TEST_STRIDE +
                  TEST_WIDTH - 1U] == 0xFFAABBCCU);

    /* Compact layout is rendered for smaller displays without overflow. */
    assert(syntra_desktop_render(&compact) == SYNTRA_STATUS_OK);
    assert(compact_pixels[0] == 0xE8202D49U);
    assert(compact_pixels[(size_t)40U * COMPACT_WIDTH + 20U] ==
           0xE8202D49U);
    assert(compact_pixels[(size_t)112U * COMPACT_WIDTH + 12U] ==
           0xFF5578F2U);
    assert(compact_pixels[(size_t)(COMPACT_HEIGHT - 26U) * COMPACT_WIDTH] ==
           0xE8202D49U);

    puts("SyntraOS adaptive GUI desktop renderer tests passed.");
    return 0;
}
