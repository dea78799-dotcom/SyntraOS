#ifndef SYNTRA_ARM64_EXCEPTION_H
#define SYNTRA_ARM64_EXCEPTION_H

#include <stdint.h>

#include "../../core/status.h"

/*
 * SyntraOS ARM64 Hardware Exception Interface
 * Version: 1.0.0
 */

#define SYNTRA_ARM64_EXCEPTION_CLASS_SHIFT 26U
#define SYNTRA_ARM64_EXCEPTION_CLASS_MASK 0x3FU

typedef enum
{
    SYNTRA_ARM64_EXCEPTION_UNKNOWN = 0x00,
    SYNTRA_ARM64_EXCEPTION_WFI_WFE = 0x01,
    SYNTRA_ARM64_EXCEPTION_SYSTEM_REGISTER = 0x18,

    SYNTRA_ARM64_EXCEPTION_INSTRUCTION_ABORT_LOWER = 0x20,
    SYNTRA_ARM64_EXCEPTION_INSTRUCTION_ABORT_CURRENT = 0x21,
    SYNTRA_ARM64_EXCEPTION_PC_ALIGNMENT = 0x22,

    SYNTRA_ARM64_EXCEPTION_DATA_ABORT_LOWER = 0x24,
    SYNTRA_ARM64_EXCEPTION_DATA_ABORT_CURRENT = 0x25,
    SYNTRA_ARM64_EXCEPTION_SP_ALIGNMENT = 0x26,

    SYNTRA_ARM64_EXCEPTION_SVC_AARCH64 = 0x15

} syntra_arm64_exception_class_t;

typedef struct
{
    uint64_t syndrome;
    uint64_t fault_address;
    uint64_t exception_return_address;
    uint64_t saved_processor_state;

    syntra_arm64_exception_class_t exception_class;

} syntra_arm64_exception_info_t;

/*
 * Decode ARM64 Exception Syndrome Register (ESR).
 */
syntra_status_t syntra_arm64_exception_decode(
    uint64_t esr,
    uint64_t far,
    uint64_t elr,
    uint64_t spsr,
    syntra_arm64_exception_info_t *info
);

#endif
