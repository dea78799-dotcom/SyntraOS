
#include "hal_backend_manager.h"

#include <assert.h>
#include <stdio.h>

int main(void)
{
    syntra_hal_backend_manager_t manager;
    syntra_hal_backend_t backend1;
    syntra_hal_backend_t backend2;

    syntra_hal_backend_t *found = NULL;

    syntra_hal_backend_event_t event;

    unsigned int count = 0U;
    unsigned int event_count = 0U;
    unsigned int i;

    /* Initialize manager */

    assert(
        hal_backend_manager_init(&manager)
        == SYNTRA_STATUS_OK
    );

    assert(manager.initialized == 1);
    assert(manager.backend_count == 0U);
    assert(manager.event_count == 0U);
    assert(manager.event_sequence == 0U);

    /* Initialize backends */

    assert(
        hal_backend_init(
            &backend1,
            1U,
            "Software Backend",
            SYNTRA_HAL_BACKEND_SOFTWARE
        ) == SYNTRA_STATUS_OK
    );

    assert(
        hal_backend_init(
            &backend2,
            2U,
            "ARM64 Backend",
            SYNTRA_HAL_BACKEND_ARM64
        ) == SYNTRA_STATUS_OK
    );

    /* Register backend 1 */

    assert(
        hal_backend_manager_register(
            &manager,
            &backend1
        ) == SYNTRA_STATUS_OK
    );

    assert(manager.backend_count == 1U);
    assert(manager.event_count == 1U);
    assert(manager.event_sequence == 1U);

    /* Check registration event */

    assert(
        hal_backend_manager_get_event(
            &manager,
            0U,
            &event
        ) == SYNTRA_STATUS_OK
    );

    assert(event.sequence == 1U);
    assert(event.backend_id == 1U);
    assert(
        event.type == SYNTRA_HAL_BACKEND_EVENT_REGISTERED
    );

    /* Register backend 2 */

    assert(
        hal_backend_manager_register(
            &manager,
            &backend2
        ) == SYNTRA_STATUS_OK
    );

    assert(manager.backend_count == 2U);
    assert(manager.event_count == 2U);
    assert(manager.event_sequence == 2U);

    /* Update backend statistics */

    assert(
        hal_backend_manager_update(&manager)
        == SYNTRA_STATUS_OK
    );

    assert(manager.available_backends == 1U);
    assert(manager.unavailable_backends == 1U);

    assert(
        hal_backend_manager_get_event_count(
            &manager,
            &event_count
        ) == SYNTRA_STATUS_OK
    );

    assert(event_count == 4U);

    /* Check availability events */

    assert(
        hal_backend_manager_get_event(
            &manager,
            2U,
            &event
        ) == SYNTRA_STATUS_OK
    );

    assert(event.backend_id == 1U);
    assert(
        event.type == SYNTRA_HAL_BACKEND_EVENT_AVAILABLE
    );

    assert(
        hal_backend_manager_get_event(
            &manager,
            3U,
            &event
        ) == SYNTRA_STATUS_OK
    );

    assert(event.backend_id == 2U);
    assert(
        event.type == SYNTRA_HAL_BACKEND_EVENT_UNAVAILABLE
    );

    /* Find backend */

    assert(
        hal_backend_manager_find(
            &manager,
            1U,
            &found
        ) == SYNTRA_STATUS_OK
    );

    assert(found == &backend1);

    /* Get backend count */

    assert(
        hal_backend_manager_get_count(
            &manager,
            &count
        ) == SYNTRA_STATUS_OK
    );

    assert(count == 2U);

    /* Duplicate registration */

    assert(
        hal_backend_manager_register(
            &manager,
            &backend1
        ) == SYNTRA_STATUS_ERROR
    );

    assert(manager.backend_count == 2U);

    /* Unregister backend 1 */

    assert(
        hal_backend_manager_unregister(
            &manager,
            1U
        ) == SYNTRA_STATUS_OK
    );

    assert(manager.backend_count == 1U);
    assert(manager.event_count == 5U);

    assert(
        hal_backend_manager_get_event(
            &manager,
            4U,
            &event
        ) == SYNTRA_STATUS_OK
    );

    assert(event.backend_id == 1U);
    assert(
        event.type == SYNTRA_HAL_BACKEND_EVENT_UNREGISTERED
    );

    /* Get event count */

    assert(
        hal_backend_manager_get_event_count(
            &manager,
            &event_count
        ) == SYNTRA_STATUS_OK
    );

    assert(event_count == 5U);

    /* Invalid event index */

    assert(
        hal_backend_manager_get_event(
            &manager,
            100U,
            &event
        ) == SYNTRA_STATUS_ERROR
    );

    /* Invalid arguments */

    assert(
        hal_backend_manager_get_event_count(
            NULL,
            &event_count
        ) == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    assert(
        hal_backend_manager_get_event_count(
            &manager,
            NULL
        ) == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    assert(
        hal_backend_manager_get_event(
            NULL,
            0U,
            &event
        ) == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    assert(
        hal_backend_manager_get_event(
            &manager,
            0U,
            NULL
        ) == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    assert(
        hal_backend_manager_clear_events(NULL)
        == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    /* Clear event history */

    assert(
        hal_backend_manager_clear_events(&manager)
        == SYNTRA_STATUS_OK
    );

    assert(manager.event_count == 0U);

    assert(
        hal_backend_manager_get_event_count(
            &manager,
            &event_count
        ) == SYNTRA_STATUS_OK
    );

    assert(event_count == 0U);

    /* Event sequence remains monotonic */

    assert(manager.event_sequence == 5U);

    /* Validate manager */

    assert(
        hal_backend_manager_validate(&manager)
        == SYNTRA_STATUS_OK
    );

    /* Fill event history beyond capacity */

    for (i = 0U;
         i < SYNTRA_HAL_BACKEND_MANAGER_MAX_EVENTS + 2U;
         ++i)
    {
        assert(
            hal_backend_manager_register(
                &manager,
                &backend1
            ) == SYNTRA_STATUS_OK
        );

        assert(
            hal_backend_manager_unregister(
                &manager,
                1U
            ) == SYNTRA_STATUS_OK
        );
    }

    assert(
        manager.event_count ==
        SYNTRA_HAL_BACKEND_MANAGER_MAX_EVENTS
    );

    assert(
        manager.event_sequence ==
        5U + 2U *
        (SYNTRA_HAL_BACKEND_MANAGER_MAX_EVENTS + 2U)
    );

    assert(
        hal_backend_manager_validate(&manager)
        == SYNTRA_STATUS_OK
    );

    /* Reset manager */

    assert(
        hal_backend_manager_reset(&manager)
        == SYNTRA_STATUS_OK
    );

    assert(manager.initialized == 0);
    assert(manager.backend_count == 0U);
    assert(manager.event_count == 0U);
    assert(manager.event_sequence == 0U);

    assert(
        hal_backend_manager_validate(&manager)
        == SYNTRA_STATUS_ERROR
    );

    /* Null checks */

    assert(
        hal_backend_manager_init(NULL)
        == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    assert(
        hal_backend_manager_reset(NULL)
        == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    assert(
        hal_backend_manager_validate(NULL)
        == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    printf("HAL Backend Manager V3 tests passed.\n");

    return 0;
}
