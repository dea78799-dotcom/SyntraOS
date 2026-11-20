#include <stdio.h>
#include <stdint.h>

#include "../../kernel/arch/arm64/exception.h"

static unsigned int failures;

static void check(const char *name, int condition)
{
    if (condition) printf("[PASS] %s\n", name);
    else
    {
        printf("[FAIL] %s\n", name);
        ++failures;
    }
}

static void check_origin(
    const char *name, uint64_t vector, uint64_t pstate,
    syntra_arm64_exception_origin_t expected, syntra_status_t expected_status)
{
    syntra_arm64_exception_origin_t actual =
        SYNTRA_ARM64_EXCEPTION_ORIGIN_UNKNOWN;
    syntra_status_t status =
        syntra_arm64_exception_classify_origin(vector, pstate, &actual);
    check(name, status == expected_status &&
        (expected_status != SYNTRA_STATUS_OK || actual == expected));
}

int main(void)
{
    check_origin("Classify EL1t vector origin",
        SYNTRA_ARM64_VECTOR_CURRENT_SP0_SYNC, 0x4U,
        SYNTRA_ARM64_EXCEPTION_ORIGIN_EL1_SP0, SYNTRA_STATUS_OK);
    check_origin("Classify EL1h vector origin",
        SYNTRA_ARM64_VECTOR_CURRENT_SPX_IRQ, 0x5U,
        SYNTRA_ARM64_EXCEPTION_ORIGIN_EL1_SPX, SYNTRA_STATUS_OK);
    check_origin("Classify EL0 AArch64 vector origin",
        SYNTRA_ARM64_VECTOR_LOWER_AARCH64_SYNC, 0x0U,
        SYNTRA_ARM64_EXCEPTION_ORIGIN_EL0_AARCH64, SYNTRA_STATUS_OK);
    check_origin("Classify lower AArch32 as unsupported",
        SYNTRA_ARM64_VECTOR_LOWER_AARCH32_SYNC, 0x10U,
        SYNTRA_ARM64_EXCEPTION_ORIGIN_LOWER_AARCH32,
        SYNTRA_STATUS_NOT_IMPLEMENTED);
    check_origin("Reject EL0 vector with EL1h saved mode",
        SYNTRA_ARM64_VECTOR_LOWER_AARCH64_SYNC, 0x5U,
        SYNTRA_ARM64_EXCEPTION_ORIGIN_UNKNOWN,
        SYNTRA_STATUS_INVALID_ARGUMENT);
    check_origin("Reject EL1h vector with EL0 saved mode",
        SYNTRA_ARM64_VECTOR_CURRENT_SPX_IRQ, 0x0U,
        SYNTRA_ARM64_EXCEPTION_ORIGIN_UNKNOWN,
        SYNTRA_STATUS_INVALID_ARGUMENT);
    check_origin("Reject out-of-range vector",
        16U, 0x5U, SYNTRA_ARM64_EXCEPTION_ORIGIN_UNKNOWN,
        SYNTRA_STATUS_INVALID_ARGUMENT);
    check("Reject null origin output",
        syntra_arm64_exception_classify_origin(
            SYNTRA_ARM64_VECTOR_LOWER_AARCH64_SYNC, 0U, NULL) ==
            SYNTRA_STATUS_INVALID_ARGUMENT);

    printf("\nARM64 exception-origin failures: %u\n", failures);
    return failures == 0U ? 0 : 1;
}
