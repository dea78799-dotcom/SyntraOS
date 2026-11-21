#ifndef SYNTRA_ARM64_GIC_H
#define SYNTRA_ARM64_GIC_H

#include <stddef.h>
#include <stdint.h>

#include "../../core/status.h"
#include "../../interrupt/interrupt.h"
#include "boot/fdt.h"

/*
 * SyntraOS ARM64 Generic Interrupt Controller interface.
 * Platform register regions must be discovered from the Device Tree.
 * This interface does not assume fixed QEMU virt MMIO addresses.
 */

typedef enum
{
    SYNTRA_ARM64_GIC_VERSION_NONE = 0,
    SYNTRA_ARM64_GIC_VERSION_2 = 2,
    SYNTRA_ARM64_GIC_VERSION_3 = 3
} syntra_arm64_gic_version_t;

typedef struct
{
    syntra_arm64_gic_version_t version;

    uintptr_t distributor_base;
    size_t distributor_size;

    /* Required by GICv2. */
    uintptr_t cpu_interface_base;
    size_t cpu_interface_size;

    /* Required by GICv3. */
    uintptr_t redistributor_base;
    size_t redistributor_size;

} syntra_arm64_gic_config_t;

typedef struct
{
    uint32_t distributor_typer;
    uint32_t distributor_iidr;
    uint64_t redistributor_typer;
} syntra_arm64_gic_probe_result_t;

typedef struct
{
    syntra_arm64_gic_config_t config;
    int configured;
    int initialized;
    int hardware_access_enabled;
} syntra_arm64_gic_info_t;

/*
 * Store and validate Device-Tree-derived register regions.
 * No MMIO register is accessed by this function.
 */
syntra_status_t syntra_arm64_gic_configure(
    const syntra_arm64_gic_config_t *config
);

/*
 * Locate a supported GIC node in the FDT and store its MMIO regions.
 * This performs no hardware register access.
 */
syntra_status_t syntra_arm64_gic_discover(const syntra_fdt_t *fdt);

/*
 * Hardware initialization remains unavailable until MMIO mapping,
 * memory attributes, and the selected GIC register layout have been
 * validated on the target platform.
 */
syntra_status_t syntra_arm64_gic_init(void);

/*
 * Read-only GICv3 register probe. This function never writes MMIO and
 * does not enable the Distributor, Redistributor, CPU interface, or IRQs.
 */
syntra_status_t syntra_arm64_gic_probe(
    syntra_arm64_gic_probe_result_t *result
);

/* Interrupt acknowledge and end-of-interrupt interface. */
syntra_status_t syntra_arm64_gic_acknowledge(
    uint32_t *interrupt_id
);

syntra_status_t syntra_arm64_gic_end_of_interrupt(
    uint32_t interrupt_id
);
/* Configure an SPI as edge-rising (DT flags 1) or level-high (flags 4). */
syntra_status_t syntra_arm64_gic_configure_interrupt(
    uint32_t interrupt_id,
    uint32_t trigger_flags
);
syntra_status_t syntra_arm64_gic_enable_interrupt(uint32_t interrupt_id);
syntra_status_t syntra_arm64_gic_disable_interrupt(uint32_t interrupt_id);
syntra_status_t syntra_arm64_gic_set_priority(
    uint32_t interrupt_id,
    uint8_t priority
);
const syntra_interrupt_controller_ops_t *
syntra_arm64_gic_controller_ops(void);

syntra_status_t syntra_arm64_gic_get_info(
    syntra_arm64_gic_info_t *info
);

#endif
