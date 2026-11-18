
#include <stdio.h>

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

int main(void)
{
    int failures = 0;

    printf("\n===== SYNTRAOS SCHEDULER TEST =====\n");

    failures += test_scheduler_init();
    failures += test_add_thread();
    failures += test_get_thread();

    printf("\nFailures: %d\n", failures);

    return failures == 0 ? 0 : 1;
}
