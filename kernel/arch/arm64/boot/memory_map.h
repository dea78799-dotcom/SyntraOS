#ifndef SYNTRA_ARM64_BOOT_MEMORY_MAP_H
#define SYNTRA_ARM64_BOOT_MEMORY_MAP_H

#include <stddef.h>
#include <stdint.h>

#include "fdt.h"
#include "../../../memory/memory_region.h"

/*
 * Runtime boot memory metadata. The DTB address is supplied by the boot
 * protocol in x0 and therefore must not be represented by a fixed linker
 * symbol. Kernel and stack bounds come from linker-exported symbols.
 */
typedef struct
{
    uintptr_t kernel_start;
    uintptr_t kernel_end;
    uintptr_t text_start;
    uintptr_t text_end;
    uintptr_t rodata_start;
    uintptr_t rodata_end;
    uintptr_t data_start;
    uintptr_t data_end;
    uintptr_t bss_start;
    uintptr_t bss_end;
    uintptr_t stack_bottom;
    uintptr_t stack_top;
    uintptr_t dtb_start;
    size_t dtb_size;
} syntra_arm64_boot_memory_metadata_t;

/* Validate and publish the immutable memory ranges discovered during boot. */
syntra_status_t syntra_arm64_boot_memory_metadata_set(
    const syntra_arm64_boot_memory_metadata_t *metadata
);

/* Read the validated boot memory ranges for later MMU mapping. */
syntra_status_t syntra_arm64_boot_memory_metadata_get(
    syntra_arm64_boot_memory_metadata_t *metadata
);

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
