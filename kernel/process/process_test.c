
#include <stdio.h>

#include "process.h"

int main(void)
{
    printf("=== SYNTRAOS PROCESS TEST ===\n");

    if (process_init() != SYNTRA_STATUS_OK)
    {
        printf("Process initialization failed!\n");
        return 1;
    }

    printf("Process Manager initialized.\n");

    if (process_create(1, 5) != SYNTRA_STATUS_OK)
    {
        printf("Process creation failed!\n");
        return 1;
    }

    printf("Process created successfully.\n");

    syntra_process_t *process = process_get(1);

    if (process == 0)
    {
        printf("Process not found!\n");
        return 1;
    }

    printf("Process ID: %u\n", process->process_id);
    printf("Priority: %u\n", process->priority);
    printf("State: %d\n", process->state);

    if (process_terminate(1) != SYNTRA_STATUS_OK)
    {
        printf("Process termination failed!\n");
        return 1;
    }

    printf("Process terminated.\n");

    printf("Process count: %u\n", process_get_count());

    printf("=== TEST COMPLETED ===\n");

    return 0;
}
