
#ifndef SYNTRA_EXCEPTION_H
#define SYNTRA_EXCEPTION_H

#include "../core/status.h"

#define SYNTRA_MAX_EXCEPTION_HANDLERS 64U

typedef unsigned int syntra_exception_id_t;

typedef void (*syntra_exception_handler_t)(
    syntra_exception_id_t exception_id,
    void *context
);

typedef struct
{
    syntra_exception_handler_t handler;
    void *context;
    unsigned int call_count;
    int registered;

} syntra_exception_entry_t;

typedef struct
{
    unsigned int total_handlers;
    unsigned int registered_handlers;
    unsigned int handled_count;
    unsigned int failed_dispatches;
    int initialized;
    int valid;

} syntra_exception_info_t;

/* Core API */

syntra_status_t exception_init(void);

syntra_status_t exception_register(
    syntra_exception_id_t exception_id,
    syntra_exception_handler_t handler,
    void *context
);

syntra_status_t exception_unregister(
    syntra_exception_id_t exception_id
);

syntra_status_t exception_dispatch(
    syntra_exception_id_t exception_id
);

/* Information API */

syntra_status_t exception_get_info(
    syntra_exception_info_t *info
);

const syntra_exception_entry_t *exception_get_entry(
    syntra_exception_id_t exception_id
);

/* Validation API */

syntra_status_t exception_validate(void);

#endif
