#ifndef SYNTRA_STACK_ALLOCATOR_H
#define SYNTRA_STACK_ALLOCATOR_H

#include <stddef.h>
#include <stdint.h>

#include "../core/status.h"
#include "page_allocator.h"

#define SYNTRA_STACK_ALLOCATOR_MAX_STACKS 32U
#define SYNTRA_STACK_MIN_SIZE ((size_t)SYNTRA_PAGE_SIZE)

typedef struct
{
    uintptr_t physical_base;
    size_t size;
    size_t page_count;
} syntra_stack_allocation_t;

/*
 * Allocate contiguous physical RAM for a stack. This allocator does not create
 * a virtual mapping or return a dereferenceable host pointer; the architecture
 * layer must map/resolve the physical range before passing it to a Thread.
 */
syntra_status_t syntra_stack_alloc(
    size_t requested_size,
    syntra_stack_allocation_t *allocation);

/* Release only an exact live allocation created by syntra_stack_alloc(). */
syntra_status_t syntra_stack_free(
    const syntra_stack_allocation_t *allocation);

/* Validate registry and page-owner accounting for stack allocations. */
syntra_status_t syntra_stack_allocator_validate(void);
size_t syntra_stack_allocator_live_count(void);

#endif
