
#include <stdio.h>

#include "../../kernel/core/kernel.h"

int main(void)
{
    printf("\n");
    printf("=================================\n");
    printf(" SYNTRAOS KERNEL INTEGRATION TEST\n");
    printf("=================================\n");

    syntra_status_t status = syntra_kernel_init();

    if (status != SYNTRA_STATUS_OK)
    {
        printf("[FAIL] Kernel initialization\n");
        printf("Status: %d\n", status);
        return 1;
    }

    printf("[PASS] Kernel initialization\n");

    printf("\n=================================\n");
    printf(" Total failures: 0\n");
    printf("=================================\n");

    return 0;
}
