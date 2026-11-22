#include <stdio.h>
#include <stddef.h>
#include <stdint.h>

#include "../../kernel/process/process.h"
#include "../../kernel/scheduler/scheduler.h"

static int failures = 0;
static _Alignas(16) unsigned char process_thread_stack[2048];

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
    printf(" SYNTRAOS PROCESS MANAGER TEST V2\n");
    printf("=================================\n");

    check("Initialize scheduler", scheduler_init() == SYNTRA_STATUS_OK);
    check("Initialize process manager", process_init() == SYNTRA_STATUS_OK);
    check("Create process ID 1", process_create(1U, 5U) == SYNTRA_STATUS_OK);

    process = process_get(1U);
    check("Get process ID 1",
        process != NULL && process->process_id == 1U);
    check("Reject duplicate process ID",
        process_create(1U, 3U) == SYNTRA_STATUS_INVALID_ARGUMENT);
    check("Initial state is READY",
        process != NULL && process->state == PROCESS_READY);
    check("Reject process ID 0",
        process_create(0U, 5U) == SYNTRA_STATUS_INVALID_ARGUMENT);

    check("Create scheduler thread owned by process candidate",
        scheduler_create_thread(101U, 5U, dummy_thread, NULL,
            process_thread_stack, sizeof(process_thread_stack)) == SYNTRA_STATUS_OK);
    check("Attach existing scheduler thread",
        process_attach_thread(1U, 101U) == SYNTRA_STATUS_OK);
    check("Process records thread ownership",
        process_get_thread_count(1U) == 1U &&
        process_get_thread_id(1U, 0U) == 101U);
    check("Reject attaching same thread twice",
        process_attach_thread(1U, 101U) == SYNTRA_STATUS_INVALID_ARGUMENT);
    check("Reject nonexistent thread",
        process_attach_thread(1U, 999U) == SYNTRA_STATUS_INVALID_ARGUMENT);
    check("Reject out-of-range thread index",
        process_get_thread_id(1U, 1U) == 0U);

    check("Process state follows READY scheduler thread",
        process_refresh_state(1U) == SYNTRA_STATUS_OK &&
        process_get(1U)->state == PROCESS_READY);
    check("Cannot mark process RUNNING while its thread is READY",
        process_set_state(1U, PROCESS_RUNNING) ==
            SYNTRA_STATUS_PERMISSION_DENIED);

    check("Set scheduler thread RUNNING",
        scheduler_set_thread_state(101U, THREAD_RUNNING) == SYNTRA_STATUS_OK);
    check("Refresh process state to RUNNING",
        process_refresh_state(1U) == SYNTRA_STATUS_OK &&
        process_get(1U)->state == PROCESS_RUNNING);
    check("Process RUNNING state matches scheduler",
        process_set_state(1U, PROCESS_RUNNING) == SYNTRA_STATUS_OK);

    check("Block scheduler thread",
        scheduler_set_thread_state(101U, THREAD_BLOCKED) == SYNTRA_STATUS_OK);
    check("Refresh process state to BLOCKED",
        process_refresh_state(1U) == SYNTRA_STATUS_OK &&
        process_get(1U)->state == PROCESS_BLOCKED);
    check("Cannot terminate while thread is BLOCKED",
        process_terminate(1U) == SYNTRA_STATUS_PERMISSION_DENIED);

    check("Terminate scheduler thread first",
        scheduler_set_thread_state(101U, THREAD_TERMINATED) == SYNTRA_STATUS_OK);
    check("Refresh process state to TERMINATED",
        process_refresh_state(1U) == SYNTRA_STATUS_OK &&
        process_get(1U)->state == PROCESS_TERMINATED);
    check("Terminate process after all threads terminated",
        process_terminate(1U) == SYNTRA_STATUS_OK);
    check("Cannot attach thread to terminated process",
        process_attach_thread(1U, 101U) == SYNTRA_STATUS_PERMISSION_DENIED);

    check("Process and scheduler invariants",
        process_validate() == SYNTRA_STATUS_OK);
    check("Process count remains one", process_get_count() == 1U);

    printf("\n=================================\n");
    printf(" Total failures: %d\n", failures);
    printf("=================================\n");
    return failures == 0 ? 0 : 1;
}
