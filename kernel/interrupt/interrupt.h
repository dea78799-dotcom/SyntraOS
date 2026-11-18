
#ifndef SYNTRA_INTERRUPT_H
#define SYNTRA_INTERRUPT_H

#include "../core/status.h"

#define SYNTRA_MAX_INTERRUPT_HANDLERS 256U

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

/* Core API */

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

/* Information API */

syntra_status_t interrupt_get_info(
    syntra_interrupt_info_t *info
);

const syntra_interrupt_entry_t *interrupt_get_entry(
    syntra_interrupt_id_t interrupt_id
);

/* Validation API */

syntra_status_t interrupt_validate(void);

#endif
