
#include <stdio.h>
#include <stddef.h>

#include "../../kernel/scheduler/scheduler.h"

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
    printf(" SYNTRAOS SCHEDULER TEST V2\n");
    printf("=================================\n");

    /* Test 1: Initialize scheduler */
    check(
        "scheduler_init",
        scheduler_init() == SYNTRA_STATUS_OK
    );

    /* Test 2: Add first thread */
    check(
        "Add thread ID 1",
        scheduler_add_thread(1, 5) == SYNTRA_STATUS_OK
    );

    /* Test 3: Find existing thread */
    syntra_thread_t *thread = scheduler_get_thread(1);

    check(
        "Get thread ID 1",
        thread != NULL && thread->thread_id == 1
    );

    /* Test 4: Reject duplicate thread ID */
    check(
        "Reject duplicate thread ID",
        scheduler_add_thread(1, 3) != SYNTRA_STATUS_OK
    );

    /* Test 5: Add second thread */
    check(
        "Add thread ID 2",
        scheduler_add_thread(2, 10) == SYNTRA_STATUS_OK
    );

    /* Test 6: Highest priority thread */
    thread = scheduler_get_next_ready();

    check(
        "Select highest priority thread",
        thread != NULL && thread->thread_id == 2
    );

    /* Test 7: Thread count */
    check(
        "Thread count equals 2",
        scheduler_get_thread_count() == 2
    );

    /* Test 8: Reject thread ID 0 */
    check(
        "Reject thread ID 0",
        scheduler_add_thread(0, 5) != SYNTRA_STATUS_OK
    );

    printf("\n=================================\n");
    printf(" Total failures: %d\n", failures);
    printf("=================================\n");

    return failures == 0 ? 0 : 1;
}
