#ifndef SYNTRA_SCHEDULER_REAPER_INTERNAL_H
#define SYNTRA_SCHEDULER_REAPER_INTERNAL_H

#include "scheduler.h"

/*
 * Private scheduler/reaper boundary. Call only after the reaper has consumed
 * the exact terminal switch-away proof and reclaimed the owned kernel stack.
 */
syntra_status_t scheduler_retire_thread_after_reap(
    syntra_thread_handle_t handle, uint64_t switch_away_sequence);

#endif
