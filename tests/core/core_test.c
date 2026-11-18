
#include <stdio.h>

#include "../../kernel/core/kernel_config.h"
#include "../../kernel/core/status.h"
#include "../../kernel/core/log.h"
#include "../../kernel/core/panic.h"

static int failures = 0;

static void check(const char *name, int condition)
{
    if (condition)
    {
        printf("[PASS] %s\n", name);
    }
    else
    {
        printf("[FAIL] %s\n", name);
        failures++;
    }
}

int main(void)
{
    printf("\n");
    printf("=================================\n");
    printf(" SYNTRAOS KERNEL CORE TEST V1\n");
    printf("=================================\n");

    /* Test 1: OS name */
    check(
        "OS name",
        sizeof(SYNTRA_OS_NAME) > 1
    );

    /* Test 2: OS version */
    check(
        "OS version",
        sizeof(SYNTRA_OS_VERSION) > 1
    );

    /* Test 3: Kernel type */
    check(
        "Kernel type",
        sizeof(SYNTRA_KERNEL_TYPE) > 1
    );

    /* Test 4: Debug configuration */
    check(
        "Debug configuration",
        SYNTRA_DEBUG == 1
    );

    /* Test 5: Architecture identifiers */
    check(
        "Architecture identifiers",
        SYNTRA_ARCH_ARM64 != SYNTRA_ARCH_X86_64 &&
        SYNTRA_ARCH_X86_64 != SYNTRA_ARCH_RISCV64 &&
        SYNTRA_ARCH_ARM64 != SYNTRA_ARCH_RISCV64
    );

    /* Test 6: Status codes */
    check(
        "Status OK",
        SYNTRA_STATUS_OK == 0
    );

    check(
        "Status ERROR",
        SYNTRA_STATUS_ERROR == -1
    );

    /* Test 7: Logging interface */
    syntra_log_write(
        SYNTRA_LOG_INFO,
        "SyntraOS Core Test"
    );

    check(
        "Logging interface callable",
        1
    );

    printf("\n=================================\n");
    printf(" Total failures: %d\n", failures);
    printf("=================================\n");

    return failures == 0 ? 0 : 1;
}
