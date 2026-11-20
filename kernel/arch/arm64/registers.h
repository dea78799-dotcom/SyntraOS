#ifndef SYNTRA_ARM64_REGISTERS_H
#define SYNTRA_ARM64_REGISTERS_H

#include <stdint.h>

#include "arch.h"

/*
 * SyntraOS ARM64 System Registers
 * Version: 1.1.0
 */

/*
 * System register identifiers.
 * These identifiers are not hardware register addresses.
 */

typedef enum
{
    SYNTRA_ARM64_REG_CURRENT_EL = 0,
    SYNTRA_ARM64_REG_ESR_EL1,
    SYNTRA_ARM64_REG_FAR_EL1,
    SYNTRA_ARM64_REG_ELR_EL1,
    SYNTRA_ARM64_REG_SPSR_EL1,
    SYNTRA_ARM64_REG_VBAR_EL1,
    SYNTRA_ARM64_REG_SCTLR_EL1,
    SYNTRA_ARM64_REG_TTBR0_EL1,
    SYNTRA_ARM64_REG_TTBR1_EL1,
    SYNTRA_ARM64_REG_TCR_EL1,
    SYNTRA_ARM64_REG_MAIR_EL1

} syntra_arm64_register_id_t;

/*
 * ARM64 Exception Level values.
 */

#define SYNTRA_ARM64_EL0 0U
#define SYNTRA_ARM64_EL1 1U
#define SYNTRA_ARM64_EL2 2U
#define SYNTRA_ARM64_EL3 3U

/*
 * Current Exception Level bit definitions.
 */

#define SYNTRA_ARM64_CURRENT_EL_SHIFT 2U
#define SYNTRA_ARM64_CURRENT_EL_MASK  0x3U

/*
 * Decode CurrentEL register value.
 */

syntra_status_t syntra_arm64_register_decode_current_el(
    uint64_t current_el,
    syntra_arm64_exception_level_t *level
);

#endif
