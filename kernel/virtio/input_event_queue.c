#include "virtio_mmio.h"
#include <stddef.h>
#include <stdint.h>

syntra_status_t syntra_virtio_mmio_pop_input_event(
    syntra_virtio_input_queue_t *queue,
    syntra_input_event_t *event,
    int *available)
{
    uint32_t tail;
    uint32_t head;

    if (queue == NULL || event == NULL || available == NULL ||
        queue->ready != 1)
        return SYNTRA_STATUS_INVALID_ARGUMENT;

    *available = 0;
    tail = queue->software_event_tail;
#if defined(__aarch64__)
    __asm__ volatile("dmb oshld" ::: "memory");
#else
    __asm__ volatile("" ::: "memory");
#endif
    head = queue->software_event_head;
    if (tail == head) return SYNTRA_STATUS_OK;

    *event = queue->software_events[
        tail % SYNTRA_VIRTIO_INPUT_SOFTWARE_EVENT_QUEUE_SIZE];
#if defined(__aarch64__)
    __asm__ volatile("dmb oshst" ::: "memory");
#else
    __asm__ volatile("" ::: "memory");
#endif
    queue->software_event_tail = tail + 1U;
    *available = 1;
    return SYNTRA_STATUS_OK;
}
