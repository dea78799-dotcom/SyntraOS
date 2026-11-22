#include "uptime_panel.h"

#include <stddef.h>

#define UPTIME_PANEL_BG 0xF0202D49U
#define UPTIME_PANEL_EDGE 0xFF54D6E8U
#define UPTIME_PANEL_TEXT 0xFFF4F7FFU
#define UPTIME_PANEL_MUTED 0xFFB7C5E0U

static void two_digits(char *out, uint32_t value)
{
    out[0] = (char)('0' + ((value / 10U) % 10U));
    out[1] = (char)('0' + (value % 10U));
}

syntra_status_t syntra_gui_render_uptime_panel(
    syntra_framebuffer_t *framebuffer,
    uint64_t elapsed_seconds)
{
    char label[17] = "UPTIME 00:00:00";
    uint64_t hours_total;
    uint32_t x = 8U;
    uint32_t y;
    syntra_status_t status;

    status = syntra_framebuffer_validate(framebuffer);
    if (status != SYNTRA_STATUS_OK) return status;
    if (framebuffer->width < 160U || framebuffer->height < 64U)
        return SYNTRA_STATUS_INVALID_ARGUMENT;

    hours_total = elapsed_seconds / 3600ULL;
    if (hours_total > 99ULL) hours_total = 99ULL;
    two_digits(&label[7], (uint32_t)hours_total);
    two_digits(&label[10], (uint32_t)((elapsed_seconds / 60ULL) % 60ULL));
    two_digits(&label[13], (uint32_t)(elapsed_seconds % 60ULL));

    y = framebuffer->height - 34U;
    status = syntra_framebuffer_fill_rect(
        framebuffer, x, y, 112U, 24U, UPTIME_PANEL_BG);
    if (status != SYNTRA_STATUS_OK) return status;
    status = syntra_framebuffer_fill_rect(
        framebuffer, x, y, 2U, 24U, UPTIME_PANEL_EDGE);
    if (status != SYNTRA_STATUS_OK) return status;
    return syntra_framebuffer_draw_text(
        framebuffer, x + 7U, y + 9U, label,
        UPTIME_PANEL_TEXT, UPTIME_PANEL_BG, 1U);
}
