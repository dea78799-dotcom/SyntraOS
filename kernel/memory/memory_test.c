
#include <stdio.h>

#include "memory.h"

int main(void)
{
    printf("=== SYNTRAOS MEMORY TEST ===\n");

    if (memory_init() != SYNTRA_STATUS_OK)
    {
        printf("Memory initialization failed!\n");
        return 1;
    }

    printf("Memory initialized successfully.\n");

    void *address = memory_allocate(2);

    if (address == 0)
    {
        printf("Memory allocation failed!\n");
        return 1;
    }

    printf("Allocated 2 memory blocks.\n");

    syntra_memory_info_t info;

    memory_get_info(&info);

    printf("Total blocks: %u\n", info.total_blocks);
    printf("Used blocks: %u\n", info.used_blocks);
    printf("Free blocks: %u\n", info.free_blocks);

    if (memory_release(address, 2) != SYNTRA_STATUS_OK)
    {
        printf("Memory release failed!\n");
        return 1;
    }

    printf("Memory released successfully.\n");

    memory_get_info(&info);

    printf("Used blocks after release: %u\n", info.used_blocks);
    printf("Free blocks after release: %u\n", info.free_blocks);

    printf("=== TEST COMPLETED ===\n");

    return 0;
}
