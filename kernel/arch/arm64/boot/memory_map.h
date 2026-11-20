#ifndef SYNTRA_ARM64_BOOT_MEMORY_MAP_H
#define SYNTRA_ARM64_BOOT_MEMORY_MAP_H

#include <stddef.h>
#include <stdint.h>

#include "fdt.h"
#include "../../../memory/memory_region.h"

/*
 * Build the physical memory-region map from FDT memory nodes,
 * /reserved-memory children, the FDT reservation map, and the DTB itself.
 */
syntra_status_t syntra_arm64_memory_map_discover(
    const syntra_fdt_t *fdt,
    uintptr_t dtb_address,
    size_t dtb_size
);

#endif
