#include <stdio.h>
#include <stddef.h>
#include <stdint.h>

#include "../../kernel/process/process.h"
#include "../../kernel/scheduler/scheduler.h"

static int failures = 0;
static _Alignas(16) unsigned char process_thread_stack_a[2048];
static _Alignas(16) unsigned char process_thread_stack_b[2048];

static void dummy_thread(void *argument)
{
    (void)argument;
}

static void check(const char *name, int condition)
{
    if (condition) printf("[PASS] %s\n", name);
    else { printf("[FAIL] %s\n", name); ++failures; }
}

int main(void)
{
    syntra_process_t *process;

    printf("\n=================================\n");
    printf(" SYNTRAOS PROCESS MANAGER TEST V3\n");
    printf("=================================\n");

    check("Initialize scheduler", scheduler_init() == SYNTRA_STATUS_OK);
    check("Initialize process manager", process_init() == SYNTRA_STATUS_OK);
    check("Create process ID 1", process_create(1U, 5U) == SYNTRA_STATUS_OK);
    check("Create process ID 2", process_create(2U, 4U) == SYNTRA_STATUS_OK);

    process = process_get(1U);
    check("Get process ID 1", process != NULL && process->process_id == 1U);
    check("Reject duplicate process ID",
        process_create(1U, 3U) == SYNTRA_STATUS_INVALID_ARGUMENT);
    check("Initial state is READY",
        process != NULL && process->state == PROCESS_READY);
    check("Reject process ID 0",
        process_create(0U, 5U) == SYNTRA_STATUS_INVALID_ARGUMENT);

    check("Create first scheduler thread",
        scheduler_create_thread(101U, 5U, dummy_thread, NULL,
            process_thread_stack_a, sizeof(process_thread_stack_a)) == SYNTRA_STATUS_OK);
    check("Create second scheduler thread",
        scheduler_create_thread(102U, 4U, dummy_thread, NULL,
            process_thread_stack_b, sizeof(process_thread_stack_b)) == SYNTRA_STATUS_OK);
    check("Attach first thread to process 1",
        process_attach_thread(1U, 101U) == SYNTRA_STATUS_OK);
    check("Attach second thread to process 1",
        process_attach_thread(1U, 102U) == SYNTRA_STATUS_OK);
    check("Process records both thread IDs",
        process_get_thread_count(1U) == 2U &&
        process_get_thread_id(1U, 0U) == 101U &&
        process_get_thread_id(1U, 1U) == 102U);
    check("Reject duplicate thread attachment",
        process_attach_thread(1U, 101U) == SYNTRA_STATUS_INVALID_ARGUMENT);
    check("Reject ownership by a second process",
        process_attach_thread(2U, 101U) == SYNTRA_STATUS_INVALID_ARGUMENT);
    check("Reject nonexistent thread",
        process_attach_thread(1U, 999U) == SYNTRA_STATUS_INVALID_ARGUMENT);
    check("Reject out-of-range thread index",
        process_get_thread_id(1U, 2U) == 0U);

    check("Process remains READY when all threads are READY",
        process_refresh_state(1U) == SYNTRA_STATUS_OK &&
        process_get(1U)->state == PROCESS_READY);
    check("Cannot mark process RUNNING while threads are READY",
        process_set_state(1U, PROCESS_RUNNING) ==
            SYNTRA_STATUS_PERMISSION_DENIED);

    check("Block second thread",
        scheduler_set_thread_state(102U, THREAD_BLOCKED) == SYNTRA_STATUS_OK);
    check("READY takes precedence over BLOCKED",
        process_refresh_state(1U) == SYNTRA_STATUS_OK &&
        process_get(1U)->state == PROCESS_READY);
    check("Run first thread",
        scheduler_set_thread_state(101U, THREAD_RUNNING) == SYNTRA_STATUS_OK);
    check("RUNNING takes precedence over BLOCKED",
        process_refresh_state(1U) == SYNTRA_STATUS_OK &&
        process_get(1U)->state == PROCESS_RUNNING);
    check("Matching RUNNING state is accepted",
        process_set_state(1U, PROCESS_RUNNING) == SYNTRA_STATUS_OK);

    check("Block first thread",
        scheduler_set_thread_state(101U, THREAD_BLOCKED) == SYNTRA_STATUS_OK);
    check("Process becomes BLOCKED when all threads are BLOCKED",
        process_refresh_state(1U) == SYNTRA_STATUS_OK &&
        process_get(1U)->state == PROCESS_BLOCKED);
    check("Cannot terminate while threads are BLOCKED",
        process_terminate(1U) == SYNTRA_STATUS_PERMISSION_DENIED);

    check("Terminate second thread",
        scheduler_set_thread_state(102U, THREAD_TERMINATED) == SYNTRA_STATUS_OK);
    check("Process remains BLOCKED while one thread is BLOCKED",
        process_refresh_state(1U) == SYNTRA_STATUS_OK &&
        process_get(1U)->state == PROCESS_BLOCKED);
    check("Terminate first thread",
        scheduler_set_thread_state(101U, THREAD_TERMINATED) == SYNTRA_STATUS_OK);
    check("Process becomes TERMINATED after every thread exits",
        process_refresh_state(1U) == SYNTRA_STATUS_OK &&
        process_get(1U)->state == PROCESS_TERMINATED);
    check("Terminate process after all threads terminate",
        process_terminate(1U) == SYNTRA_STATUS_OK);
    check("Cannot attach thread to terminated process",
        process_attach_thread(1U, 101U) == SYNTRA_STATUS_PERMISSION_DENIED);

    check("Process and scheduler invariants",
        process_validate() == SYNTRA_STATUS_OK);
    check("Process count remains two", process_get_count() == 2U);

    printf("\n=================================\n");
    printf(" Total failures: %d\n", failures);
    printf("=================================\n");
    return failures == 0 ? 0 : 1;
}
