#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "framebuffer.h"
#include "uptime_panel.h"

static uint32_t pixels[640U * 480U];
static uint32_t first_frame[640U * 480U];

int main(void)
{
    syntra_framebuffer_t fb = {
        pixels, 640U, 480U, 640U
    };

    assert(syntra_framebuffer_clear(&fb, 0xFF101A35U) == SYNTRA_STATUS_OK);
    assert(syntra_gui_render_uptime_panel(&fb, 0U) == SYNTRA_STATUS_OK);
    assert(pixels[(480U - 34U) * 640U + 8U] == 0xFF54D6E8U);
    assert(pixels[(480U - 34U) * 640U + 9U] == 0xF0202D49U);
    memcpy(first_frame, pixels, sizeof(pixels));

    assert(syntra_gui_render_uptime_panel(&fb, 1U) == SYNTRA_STATUS_OK);
    assert(memcmp(first_frame, pixels, sizeof(pixels)) != 0);

    fb.width = 120U;
    assert(syntra_gui_render_uptime_panel(&fb, 1U) ==
           SYNTRA_STATUS_INVALID_ARGUMENT);

    puts("GUI uptime panel tests: PASS");
    return 0;
}
