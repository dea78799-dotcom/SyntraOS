#ifndef SYNTRA_ARM64_EXCEPTION_H
#define SYNTRA_ARM64_EXCEPTION_H

#include <stddef.h>
#include <stdint.h>

#include "../../core/status.h"

/*
 * SyntraOS ARM64 Hardware Exception Interface
 * Version: 1.1.0
 */

#define SYNTRA_ARM64_EXCEPTION_CLASS_SHIFT 26U
#define SYNTRA_ARM64_EXCEPTION_CLASS_MASK 0x3FU

/*
 * Exception frame layout shared by future vector-entry assembly and C.
 *
 * registers[0..30] map to x0..x30 in ascending order.
 * sp_el0 stores the EL0 stack pointer at exception entry.
 * For an exception taken from EL1 using SPx, the interrupted SP_EL1
 * is the address immediately above this frame on the same stack.
 *
 * The frame size is a multiple of 16 bytes, preserving AArch64 stack
 * alignment when allocated from an already aligned exception stack.
 */
typedef struct
{
    uint64_t registers[31];
    uint64_t sp_el0;
    uint64_t elr_el1;
    uint64_t spsr_el1;
    uint64_t esr_el1;
    uint64_t far_el1;

} syntra_arm64_exception_context_t;

/* Stable offsets consumed by ARM64 assembly. */
#define SYNTRA_ARM64_CONTEXT_REGISTERS_OFFSET 0U
#define SYNTRA_ARM64_CONTEXT_SP_EL0_OFFSET 248U
#define SYNTRA_ARM64_CONTEXT_ELR_EL1_OFFSET 256U
#define SYNTRA_ARM64_CONTEXT_SPSR_EL1_OFFSET 264U
#define SYNTRA_ARM64_CONTEXT_ESR_EL1_OFFSET 272U
#define SYNTRA_ARM64_CONTEXT_FAR_EL1_OFFSET 280U
#define SYNTRA_ARM64_CONTEXT_SIZE 288U

_Static_assert(
    offsetof(syntra_arm64_exception_context_t, registers) ==
        SYNTRA_ARM64_CONTEXT_REGISTERS_OFFSET,
    "ARM64 exception register offset mismatch"
);

_Static_assert(
    offsetof(syntra_arm64_exception_context_t, sp_el0) ==
        SYNTRA_ARM64_CONTEXT_SP_EL0_OFFSET,
    "ARM64 exception SP_EL0 offset mismatch"
);

_Static_assert(
    offsetof(syntra_arm64_exception_context_t, elr_el1) ==
        SYNTRA_ARM64_CONTEXT_ELR_EL1_OFFSET,
    "ARM64 exception ELR_EL1 offset mismatch"
);

_Static_assert(
    offsetof(syntra_arm64_exception_context_t, spsr_el1) ==
        SYNTRA_ARM64_CONTEXT_SPSR_EL1_OFFSET,
    "ARM64 exception SPSR_EL1 offset mismatch"
);

_Static_assert(
    offsetof(syntra_arm64_exception_context_t, esr_el1) ==
        SYNTRA_ARM64_CONTEXT_ESR_EL1_OFFSET,
    "ARM64 exception ESR_EL1 offset mismatch"
);

_Static_assert(
    offsetof(syntra_arm64_exception_context_t, far_el1) ==
        SYNTRA_ARM64_CONTEXT_FAR_EL1_OFFSET,
    "ARM64 exception FAR_EL1 offset mismatch"
);

_Static_assert(
    sizeof(syntra_arm64_exception_context_t) ==
        SYNTRA_ARM64_CONTEXT_SIZE,
    "ARM64 exception context size mismatch"
);

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
