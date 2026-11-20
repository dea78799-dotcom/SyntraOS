#ifndef SYNTRA_INTERRUPT_H
#define SYNTRA_INTERRUPT_H

#include "../core/status.h"

#define SYNTRA_MAX_INTERRUPT_HANDLERS 1020U
#define SYNTRA_INTERRUPT_SPURIOUS_ID 1023U

typedef unsigned int syntra_interrupt_id_t;

typedef void (*syntra_interrupt_handler_t)(
    syntra_interrupt_id_t interrupt_id,
    void *context
);

typedef struct
{
    syntra_interrupt_handler_t handler;
    void *context;
    unsigned int call_count;
    int registered;

} syntra_interrupt_entry_t;

typedef struct
{
    unsigned int total_handlers;
    unsigned int registered_handlers;
    unsigned int dispatched_count;
    unsigned int failed_dispatches;
    int initialized;
    int valid;

} syntra_interrupt_info_t;

/*
 * Hardware-controller contract. The architecture driver supplies these
 * operations; the generic manager owns registration and dispatch policy.
 */
typedef struct
{
    syntra_status_t (*initialize)(void *context);
    syntra_status_t (*enable)(void *context, syntra_interrupt_id_t id);
    syntra_status_t (*disable)(void *context, syntra_interrupt_id_t id);
    syntra_status_t (*set_priority)(
        void *context,
        syntra_interrupt_id_t id,
        unsigned int priority
    );
    syntra_status_t (*acknowledge)(
        void *context,
        syntra_interrupt_id_t *id
    );
    syntra_status_t (*end_of_interrupt)(
        void *context,
        syntra_interrupt_id_t id
    );

} syntra_interrupt_controller_ops_t;

/* Core registration and dispatch API. */
syntra_status_t interrupt_init(void);
syntra_status_t interrupt_register(
    syntra_interrupt_id_t interrupt_id,
    syntra_interrupt_handler_t handler,
    void *context
);
syntra_status_t interrupt_unregister(
    syntra_interrupt_id_t interrupt_id
);
syntra_status_t interrupt_dispatch(
    syntra_interrupt_id_t interrupt_id
);

/* Hardware-controller integration API. */
syntra_status_t interrupt_controller_attach(
    const syntra_interrupt_controller_ops_t *ops,
    void *context
);
syntra_status_t interrupt_controller_init(void);
syntra_status_t interrupt_enable(syntra_interrupt_id_t interrupt_id);
syntra_status_t interrupt_disable(syntra_interrupt_id_t interrupt_id);
syntra_status_t interrupt_set_priority(
    syntra_interrupt_id_t interrupt_id,
    unsigned int priority
);
syntra_status_t interrupt_handle_pending(void);

/* Information and validation API. */
syntra_status_t interrupt_get_info(syntra_interrupt_info_t *info);
const syntra_interrupt_entry_t *interrupt_get_entry(
    syntra_interrupt_id_t interrupt_id
);
syntra_status_t interrupt_validate(void);

#endif
