
#include <stdio.h>
#include <stddef.h>

#include "../../kernel/process/process.h"

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
    printf(" SYNTRAOS PROCESS MANAGER TEST V1\n");
    printf("=================================\n");

    /* Test 1: Initialize */
    check(
        "process_init",
        process_init() == SYNTRA_STATUS_OK
    );

    /* Test 2: Create process */
    check(
        "Create process ID 1",
        process_create(1, 5) == SYNTRA_STATUS_OK
    );

    /* Test 3: Get process */
    syntra_process_t *process = process_get(1);

    check(
        "Get process ID 1",
        process != NULL &&
        process->process_id == 1
    );

    /* Test 4: Reject duplicate ID */
    check(
        "Reject duplicate process ID",
        process_create(1, 3) != SYNTRA_STATUS_OK
    );

    /* Test 5: Check initial state */
    check(
        "Initial state is READY",
        process != NULL &&
        process->state == PROCESS_READY
    );

    /* Test 6: Change state */
    check(
        "Change state to RUNNING",
        process_set_state(1, PROCESS_RUNNING) == SYNTRA_STATUS_OK
    );

    /* Test 7: Verify state */
    process = process_get(1);

    check(
        "Verify RUNNING state",
        process != NULL &&
        process->state == PROCESS_RUNNING
    );

    /* Test 8: Reject invalid process ID */
    check(
        "Reject process ID 0",
        process_create(0, 5) != SYNTRA_STATUS_OK
    );

    /* Test 9: Terminate process */
    check(
        "Terminate process",
        process_terminate(1) == SYNTRA_STATUS_OK
    );

    /* Test 10: Verify termination */
    process = process_get(1);

    check(
        "Verify TERMINATED state",
        process != NULL &&
        process->state == PROCESS_TERMINATED
    );

    printf("\n=================================\n");
    printf(" Total failures: %d\n", failures);
    printf("=================================\n");

    return failures == 0 ? 0 : 1;
}
