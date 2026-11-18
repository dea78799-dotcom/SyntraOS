
#include "../../kernel/core/kernel_config.h"
#include "../../kernel/core/status.h"
#include "../../kernel/core/log.h"

#include <assert.h>
#include <stdio.h>

int main(void)
{
    assert(SYNTRA_OS_NAME != NULL);
    assert(SYNTRA_OS_VERSION != NULL);
    assert(SYNTRA_KERNEL_TYPE != NULL);

    assert(SYNTRA_DEBUG == 1);

    assert(SYNTRA_ARCH_ARM64 == 1);
    assert(SYNTRA_ARCH_X86_64 == 2);
    assert(SYNTRA_ARCH_RISCV64 == 3);

    assert(SYNTRA_STATUS_OK == 0);
    assert(SYNTRA_STATUS_ERROR == -1);
    assert(SYNTRA_STATUS_INVALID_ARGUMENT == -2);

    syntra_log_write(
        SYNTRA_LOG_INFO,
        "Core test"
    );

    printf("Kernel Core Test: PASSED\n");

    return 0;
}
