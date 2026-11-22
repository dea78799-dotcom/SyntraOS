#include "gui_runtime.h"

#include <stddef.h>
#include "desktop.h"
#include "window_renderer.h"

syntra_status_t syntra_gui_runtime_init(
    syntra_gui_runtime_t *runtime,
    uint32_t width,
    uint32_t height)
{
    syntra_status_t status;

    if (runtime == NULL || width < 320U || height < 240U)
        return SYNTRA_STATUS_INVALID_ARGUMENT;

    runtime->initialized = 0;
    runtime->redraw_count = 0U;

    status = syntra_input_init(&runtime->input, width, height);
    if (status != SYNTRA_STATUS_OK) return status;

    if (width >= 640U && height >= 480U)
    {
        status = syntra_desktop_gui_v5_init_windows(
            &runtime->windows, width, height);
    }
    else
    {
        status = syntra_gui_window_manager_init(
            &runtime->windows, width, height);
    }
    if (status != SYNTRA_STATUS_OK) return status;

    status = syntra_gui_window_interaction_init(&runtime->interaction);
    if (status != SYNTRA_STATUS_OK) return status;

    runtime->initialized = 1;
    return SYNTRA_STATUS_OK;
}

syntra_status_t syntra_gui_runtime_process_event(
    syntra_gui_runtime_t *runtime,
    const syntra_input_event_t *event)
{
    syntra_status_t status;

    if (runtime == NULL || runtime->initialized != 1 || event == NULL)
        return SYNTRA_STATUS_INVALID_ARGUMENT;

    status = syntra_input_process(&runtime->input, event);
    if (status != SYNTRA_STATUS_OK) return status;

    status = syntra_gui_window_interaction_process(
        &runtime->interaction, &runtime->windows, &runtime->input);
    if (status != SYNTRA_STATUS_OK) return status;

    return SYNTRA_STATUS_OK;
}

syntra_status_t syntra_gui_runtime_render(
    syntra_gui_runtime_t *runtime,
    syntra_framebuffer_t *framebuffer)
{
    syntra_status_t status;

    if (runtime == NULL || runtime->initialized != 1 ||
        framebuffer == NULL ||
        framebuffer->width != runtime->input.width ||
        framebuffer->height != runtime->input.height)
        return SYNTRA_STATUS_INVALID_ARGUMENT;

    status = syntra_desktop_render(framebuffer);
    if (status != SYNTRA_STATUS_OK) return status;

    if (runtime->windows.count != 0U)
    {
        status = syntra_gui_render_windows(framebuffer, &runtime->windows);
        if (status != SYNTRA_STATUS_OK) return status;
    }

    if (runtime->redraw_count < UINT32_MAX)
        runtime->redraw_count++;
    return SYNTRA_STATUS_OK;
}
