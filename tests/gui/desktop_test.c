#include "../../kernel/gui/desktop.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#define TEST_WIDTH 640U
#define TEST_HEIGHT 480U
#define TEST_STRIDE 648U

int main(void)
{
    uint32_t pixels[TEST_STRIDE * TEST_HEIGHT];
    syntra_framebuffer_t framebuffer = {
        pixels, TEST_WIDTH, TEST_HEIGHT, TEST_STRIDE
    };
    syntra_framebuffer_t invalid = {
        pixels, TEST_WIDTH, TEST_HEIGHT, TEST_WIDTH - 1U
    };

    assert(syntra_framebuffer_validate(&invalid) ==
           SYNTRA_STATUS_INVALID_ARGUMENT);
    assert(syntra_desktop_render(&framebuffer) == SYNTRA_STATUS_OK);

    /* Top bar and wallpaper are painted, with row padding left untouched. */
    assert(pixels[0] == 0xE8202D49U);
    assert(pixels[(size_t)200U * TEST_STRIDE] == 0xFF1B2D55U);
    assert(pixels[TEST_STRIDE + TEST_WIDTH] == 0U);
    assert(pixels[TEST_STRIDE - 1U] == 0U);

    /* The four app tiles and bottom dock are present. */
    assert(pixels[(size_t)180U * TEST_STRIDE + 24U] == 0xFF5578F2U);
    assert(pixels[(size_t)180U * TEST_STRIDE + 104U] == 0xFF9874F5U);
    assert(pixels[(size_t)180U * TEST_STRIDE + 184U] == 0xFF58C99AU);
    assert(pixels[(size_t)180U * TEST_STRIDE + 264U] == 0xFFFFB66EU);
    assert(pixels[(size_t)(TEST_HEIGHT - 42U) * TEST_STRIDE] == 0xE8202D49U);

    assert(syntra_framebuffer_fill_rect(
        &framebuffer, TEST_WIDTH - 2U, TEST_HEIGHT - 2U,
        UINT32_MAX, UINT32_MAX, 0xFFAABBCCU) == SYNTRA_STATUS_OK);
    assert(pixels[(size_t)(TEST_HEIGHT - 1U) * TEST_STRIDE +
                  TEST_WIDTH - 1U] == 0xFFAABBCCU);

    puts("SyntraOS GUI desktop renderer tests passed.");
    return 0;
}
