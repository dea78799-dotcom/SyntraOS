#include <stdio.h>
#include <stdint.h>

#include "../../kernel/arch/arm64/registers.h"

static unsigned int failures;

static void check(const char *name, int condition)
{
    if (condition)
    {
        printf("[PASS] %s\n", name);
    }
    else
    {
        printf("[FAIL] %s\n", name);
        ++failures;
    }
}

static void check_level(
    const char *name,
    uint64_t current_el,
    syntra_arm64_exception_level_t expected
)
{
    syntra_arm64_exception_level_t actual = SYNTRA_ARM64_MODE_UNKNOWN;
    syntra_status_t status =
        syntra_arm64_register_decode_current_el(current_el, &actual);

    check(name, status == SYNTRA_STATUS_OK && actual == expected);
}

int main(void)
{
    check_level("Decode EL0 explicitly",
        (uint64_t)SYNTRA_ARM64_EL0 << SYNTRA_ARM64_CURRENT_EL_SHIFT,
        SYNTRA_ARM64_MODE_EL0);
    check_level("Decode EL1",
        (uint64_t)SYNTRA_ARM64_EL1 << SYNTRA_ARM64_CURRENT_EL_SHIFT,
        SYNTRA_ARM64_MODE_EL1);
    check_level("Decode EL2",
        (uint64_t)SYNTRA_ARM64_EL2 << SYNTRA_ARM64_CURRENT_EL_SHIFT,
        SYNTRA_ARM64_MODE_EL2);
    check_level("Decode EL3",
        (uint64_t)SYNTRA_ARM64_EL3 << SYNTRA_ARM64_CURRENT_EL_SHIFT,
        SYNTRA_ARM64_MODE_EL3);
    check_level("Ignore unrelated CurrentEL bits",
        0xFFFFFFFFFFFFFFF0ULL |
            ((uint64_t)SYNTRA_ARM64_EL0 << SYNTRA_ARM64_CURRENT_EL_SHIFT),
        SYNTRA_ARM64_MODE_EL0);

    check("Reject null output pointer",
        syntra_arm64_register_decode_current_el(0U, NULL) ==
            SYNTRA_STATUS_INVALID_ARGUMENT);

    printf("\nARM64 register decoder failures: %u\n", failures);
    return failures == 0U ? 0 : 1;
}
