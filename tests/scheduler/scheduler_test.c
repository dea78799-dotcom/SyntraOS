
#include <stdio.h>
#include <stddef.h>

#include "../../kernel/scheduler/scheduler.h"

static int test_scheduler_init(void)
{
    syntra_status_t status = scheduler_init();

    if (status != SYNTRA_STATUS_OK)
    {
        printf("[FAIL] scheduler_init\n");
        return 1;
    }

    printf("[PASS] scheduler_init\n");
    return 0;
}

static int test_add_thread(void)
{
    syntra_status_t status =
        scheduler_add_thread(1, 5);

    if (status != SYNTRA_STATUS_OK)
    {
        printf("[FAIL] scheduler_add_thread\n");
        return 1;
    }

    printf("[PASS] scheduler_add_thread\n");
    return 0;
}

static int test_get_thread(void)
{
    syntra_thread_t *thread =
        scheduler_get_thread(1);

    if (thread == NULL)
    {
        printf("[FAIL] scheduler_get_thread\n");
        return 1;
    }

    if (thread->thread_id != 1)
    {
        printf("[FAIL] thread_id\n");
        return 1;
    }

    printf("[PASS] scheduler_get_thread\n");
    return 0;
}

static int test_get_next_ready(void)
{
    syntra_thread_t *thread =
        scheduler_get_next_ready();

    if (thread == NULL)
    {
        printf("[FAIL] scheduler_get_next_ready\n");
        return 1;
    }

    if (thread->thread_id != 1)
    {
        printf("[FAIL] ready_thread_id\n");
        return 1;
    }

    printf("[PASS] scheduler_get_next_ready\n");
    return 0;
}

int main(void)
{
    int failures = 0;

    printf("\n");
    printf("=================================\n");
    printf(" SYNTRAOS SCHEDULER UNIT TEST\n");
    printf("=================================\n");

    failures += test_scheduler_init();
    failures += test_add_thread();
    failures += test_get_thread();
    failures += test_get_next_ready();

    printf("\n=================================\n");
    printf(" Total failures: %d\n", failures);
    printf("=================================\n");

    return failures == 0 ? 0 : 1;
}
