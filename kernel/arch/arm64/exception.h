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
#define SYNTRA_ARM64_EXCEPTION_RETURN_MAGIC 0x53594E5452415254ULL
#define SYNTRA_ARM64_IRQ_RETURN_MAGIC 0x4952515245545552ULL
#define SYNTRA_ARM64_EXCEPTION_TEST_SVC_IMMEDIATE 0x42U
#define SYNTRA_ARM64_SCHEDULER_START_SVC_IMMEDIATE 0x43U

/*
 * Exception frame layout shared by the ARM64 vector-entry assembly and C.
 *
 * registers[0..30] map to x0..x30 in ascending order.
 * sp_el0 stores the EL0 stack pointer at exception entry.
 * For an exception taken from EL1 using SPx, the interrupted SP_EL1
 * is the address immediately above this frame on the same stack.
 *
 * The assembly allocation is 304 bytes, preserving 16-byte stack alignment.
 * vector_index identifies the originating slot in the architectural table.
 */
typedef struct
{
    uint64_t registers[31];
    uint64_t sp_el0;
    uint64_t elr_el1;
    uint64_t spsr_el1;
    uint64_t esr_el1;
    uint64_t far_el1;
    uint64_t vector_index;
    uint64_t reserved;

} syntra_arm64_exception_context_t;

/* Stable offsets consumed by ARM64 assembly. */
#define SYNTRA_ARM64_CONTEXT_REGISTERS_OFFSET 0U
#define SYNTRA_ARM64_CONTEXT_SP_EL0_OFFSET 248U
#define SYNTRA_ARM64_CONTEXT_ELR_EL1_OFFSET 256U
#define SYNTRA_ARM64_CONTEXT_SPSR_EL1_OFFSET 264U
#define SYNTRA_ARM64_CONTEXT_ESR_EL1_OFFSET 272U
#define SYNTRA_ARM64_CONTEXT_FAR_EL1_OFFSET 280U
#define SYNTRA_ARM64_CONTEXT_VECTOR_INDEX_OFFSET 288U
#define SYNTRA_ARM64_CONTEXT_RESERVED_OFFSET 296U
#define SYNTRA_ARM64_CONTEXT_SIZE 304U

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
    offsetof(syntra_arm64_exception_context_t, vector_index) ==
        SYNTRA_ARM64_CONTEXT_VECTOR_INDEX_OFFSET,
    "ARM64 exception vector index offset mismatch"
);

_Static_assert(
    offsetof(syntra_arm64_exception_context_t, reserved) ==
        SYNTRA_ARM64_CONTEXT_RESERVED_OFFSET,
    "ARM64 exception reserved offset mismatch"
);

_Static_assert(
    sizeof(syntra_arm64_exception_context_t) ==
        SYNTRA_ARM64_CONTEXT_SIZE,
    "ARM64 exception context size mismatch"
);

/*
 * Architectural vector slot indices, grouped by origin and exception type.
 */
typedef enum
{
    SYNTRA_ARM64_VECTOR_CURRENT_SP0_SYNC = 0,
    SYNTRA_ARM64_VECTOR_CURRENT_SP0_IRQ = 1,
    SYNTRA_ARM64_VECTOR_CURRENT_SP0_FIQ = 2,
    SYNTRA_ARM64_VECTOR_CURRENT_SP0_SERROR = 3,
    SYNTRA_ARM64_VECTOR_CURRENT_SPX_SYNC = 4,
    SYNTRA_ARM64_VECTOR_CURRENT_SPX_IRQ = 5,
    SYNTRA_ARM64_VECTOR_CURRENT_SPX_FIQ = 6,
    SYNTRA_ARM64_VECTOR_CURRENT_SPX_SERROR = 7,
    SYNTRA_ARM64_VECTOR_LOWER_AARCH64_SYNC = 8,
    SYNTRA_ARM64_VECTOR_LOWER_AARCH64_IRQ = 9,
    SYNTRA_ARM64_VECTOR_LOWER_AARCH64_FIQ = 10,
    SYNTRA_ARM64_VECTOR_LOWER_AARCH64_SERROR = 11,
    SYNTRA_ARM64_VECTOR_LOWER_AARCH32_SYNC = 12,
    SYNTRA_ARM64_VECTOR_LOWER_AARCH32_IRQ = 13,
    SYNTRA_ARM64_VECTOR_LOWER_AARCH32_FIQ = 14,
    SYNTRA_ARM64_VECTOR_LOWER_AARCH32_SERROR = 15,
    SYNTRA_ARM64_VECTOR_UNKNOWN = 16
} syntra_arm64_vector_index_t;

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

typedef enum
{
    SYNTRA_ARM64_EXCEPTION_ORIGIN_UNKNOWN = 0,
    SYNTRA_ARM64_EXCEPTION_ORIGIN_EL1_SP0,
    SYNTRA_ARM64_EXCEPTION_ORIGIN_EL1_SPX,
    SYNTRA_ARM64_EXCEPTION_ORIGIN_EL0_AARCH64,
    SYNTRA_ARM64_EXCEPTION_ORIGIN_LOWER_AARCH32
} syntra_arm64_exception_origin_t;

typedef struct
{
    uint64_t syndrome;
    uint64_t fault_address;
    uint64_t exception_return_address;
    uint64_t saved_processor_state;

    syntra_arm64_exception_class_t exception_class;
    syntra_arm64_vector_index_t vector_index;
    syntra_arm64_exception_origin_t origin;

} syntra_arm64_exception_info_t;

/*
 * Validate vector provenance against the saved PSTATE.M[3:0] mode.
 * Lower AArch32 is identified but deliberately unsupported.
 */
syntra_status_t syntra_arm64_exception_classify_origin(
    uint64_t vector_index,
    uint64_t saved_processor_state,
    syntra_arm64_exception_origin_t *origin
);

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

/*
 * Capture and decode a context produced by the ARM64 vector entry.
 * The architecture bridge handles validated IRQ/SVC paths and may select
 * a different saved context for round-robin thread switching.
 */
syntra_status_t syntra_arm64_exception_handle(
    const syntra_arm64_exception_context_t *context
);

syntra_arm64_exception_context_t *
syntra_arm64_exception_handle_and_select(
    syntra_arm64_exception_context_t *context
);

/* Read-only access to the most recently captured exception snapshot. */
const syntra_arm64_exception_context_t *
syntra_arm64_exception_get_last_context(void);

const syntra_arm64_exception_info_t *
syntra_arm64_exception_get_last_info(void);

unsigned int syntra_arm64_exception_user_thread_exit_count(void);
unsigned int syntra_arm64_exception_user_thread_fault_count(void);
int syntra_arm64_exception_has_last_context(void);

#endif
