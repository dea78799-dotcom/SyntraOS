#include "hal_device_event_queue.h"

#include <assert.h>
#include <stddef.h>

static syntra_hal_device_event_t create_event(
    unsigned int sequence,
    unsigned int device_id,
    syntra_hal_device_event_type_t type
)
{
    syntra_hal_device_event_t event = {0};

    event.sequence = sequence;
    event.device_id = device_id;
    event.type = type;
    event.previous_state = SYNTRA_HAL_DEVICE_OFFLINE;
    event.current_state = SYNTRA_HAL_DEVICE_ONLINE;

    return event;
}

int main(void)
{
    syntra_hal_device_event_queue_t queue;
    syntra_hal_device_event_t input;
    syntra_hal_device_event_t output;

    unsigned int count;
    unsigned int index;

    int is_empty;
    int is_full;

    /* Initialization */

    assert(
        hal_device_event_queue_init(&queue) ==
        SYNTRA_STATUS_OK
    );

    assert(queue.initialized == 1);
    assert(queue.count == 0U);

    assert(
        hal_device_event_queue_validate(&queue) ==
        SYNTRA_STATUS_OK
    );

    /* Empty queue */

    assert(
        hal_device_event_queue_is_empty(
            &queue,
            &is_empty
        ) == SYNTRA_STATUS_OK
    );

    assert(is_empty == 1);

    assert(
        hal_device_event_queue_dequeue(
            &queue,
            &output
        ) == SYNTRA_STATUS_ERROR
    );

    /* Enqueue and dequeue */

    input = create_event(
        1U,
        100U,
        SYNTRA_HAL_DEVICE_EVENT_ADDED
    );

    assert(
        hal_device_event_queue_enqueue(
            &queue,
            &input
        ) == SYNTRA_STATUS_OK
    );

    input = create_event(
        2U,
        101U,
        SYNTRA_HAL_DEVICE_EVENT_REMOVED
    );

    assert(
        hal_device_event_queue_enqueue(
            &queue,
            &input
        ) == SYNTRA_STATUS_OK
    );

    assert(
        hal_device_event_queue_get_count(
            &queue,
            &count
        ) == SYNTRA_STATUS_OK
    );

    assert(count == 2U);

    assert(
        hal_device_event_queue_dequeue(
            &queue,
            &output
        ) == SYNTRA_STATUS_OK
    );

    assert(output.sequence == 1U);
    assert(output.device_id == 100U);

    assert(
        hal_device_event_queue_dequeue(
            &queue,
            &output
        ) == SYNTRA_STATUS_OK
    );

    assert(output.sequence == 2U);
    assert(output.device_id == 101U);

    /* Circular queue behavior */

    for (index = 0U; index < 40U; index++)
    {
        input = create_event(
            index + 3U,
            index + 200U,
            SYNTRA_HAL_DEVICE_EVENT_ADDED
        );

        assert(
            hal_device_event_queue_enqueue(
                &queue,
                &input
            ) == SYNTRA_STATUS_OK
        );
    }

    for (index = 0U; index < 30U; index++)
    {
        assert(
            hal_device_event_queue_dequeue(
                &queue,
                &output
            ) == SYNTRA_STATUS_OK
        );
    }

    for (index = 0U; index < 30U; index++)
    {
        input = create_event(
            index + 43U,
            index + 300U,
            SYNTRA_HAL_DEVICE_EVENT_STATE_CHANGED
        );

        assert(
            hal_device_event_queue_enqueue(
                &queue,
                &input
            ) == SYNTRA_STATUS_OK
        );
    }

    assert(
        hal_device_event_queue_validate(&queue) ==
        SYNTRA_STATUS_OK
    );

    /* Fill queue */

    assert(
        hal_device_event_queue_clear(&queue) ==
        SYNTRA_STATUS_OK
    );

    for (index = 0U;
         index < SYNTRA_HAL_DEVICE_EVENT_QUEUE_CAPACITY;
         index++)
    {
        input = create_event(
            index + 1U,
            index + 500U,
            SYNTRA_HAL_DEVICE_EVENT_ADDED
        );

        assert(
            hal_device_event_queue_enqueue(
                &queue,
                &input
            ) == SYNTRA_STATUS_OK
        );
    }

    assert(
        hal_device_event_queue_is_full(
            &queue,
            &is_full
        ) == SYNTRA_STATUS_OK
    );

    assert(is_full == 1);

    input = create_event(
        1000U,
        999U,
        SYNTRA_HAL_DEVICE_EVENT_RESET
    );

    assert(
        hal_device_event_queue_enqueue(
            &queue,
            &input
        ) == SYNTRA_STATUS_ERROR
    );

    assert(queue.dropped_count == 1U);

    /* Verify FIFO after full */

    for (index = 0U;
         index < SYNTRA_HAL_DEVICE_EVENT_QUEUE_CAPACITY;
         index++)
    {
        assert(
            hal_device_event_queue_dequeue(
                &queue,
                &output
            ) == SYNTRA_STATUS_OK
        );

        assert(output.sequence == index + 1U);
        assert(output.device_id == index + 500U);
    }

    /* Clear and reset */

    assert(
        hal_device_event_queue_clear(&queue) ==
        SYNTRA_STATUS_OK
    );

    assert(queue.count == 0U);

    assert(
        hal_device_event_queue_reset(&queue) ==
        SYNTRA_STATUS_OK
    );

    assert(queue.initialized == 1);
    assert(queue.count == 0U);
    assert(queue.enqueue_count == 0U);
    assert(queue.dequeue_count == 0U);
    assert(queue.dropped_count == 0U);

    /* Invalid arguments */

    assert(
        hal_device_event_queue_init(NULL) ==
        SYNTRA_STATUS_INVALID_ARGUMENT
    );

    assert(
        hal_device_event_queue_enqueue(
            NULL,
            &input
        ) == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    assert(
        hal_device_event_queue_enqueue(
            &queue,
            NULL
        ) == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    assert(
        hal_device_event_queue_dequeue(
            &queue,
            NULL
        ) == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    assert(
        hal_device_event_queue_get_count(
            &queue,
            NULL
        ) == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    assert(
        hal_device_event_queue_is_empty(
            &queue,
            NULL
        ) == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    assert(
        hal_device_event_queue_is_full(
            &queue,
            NULL
        ) == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    assert(
        hal_device_event_queue_clear(NULL) ==
        SYNTRA_STATUS_INVALID_ARGUMENT
    );

    assert(
        hal_device_event_queue_validate(NULL) ==
        SYNTRA_STATUS_INVALID_ARGUMENT
    );

    /* Invalid event type */

    input.type = (syntra_hal_device_event_type_t)99;

    assert(
        hal_device_event_queue_enqueue(
            &queue,
            &input
        ) == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    return 0;
}
