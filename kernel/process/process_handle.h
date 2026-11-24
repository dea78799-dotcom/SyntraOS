#ifndef SYNTRA_PROCESS_HANDLE_H
#define SYNTRA_PROCESS_HANDLE_H

#include <stdint.h>

#define SYNTRA_MAX_PROCESSES 64U

/*
 * Stable value identity for one occupied process-table slot.
 * A handle is valid only when both slot and generation match the manager.
 */
typedef struct
{
    uint32_t slot;
    uint32_t generation;
} syntra_process_handle_t;

#endif
