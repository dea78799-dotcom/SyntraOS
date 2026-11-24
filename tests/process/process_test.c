
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

    /* Test 2: Create process through the new handle-first API. */
    syntra_process_handle_t process_handle = {0U, 0U};
    check(
        "Create process ID 1 and return handle",
        process_create_with_handle(1U, 5U, &process_handle) ==
            SYNTRA_STATUS_OK
    );
    check(
        "Reject NULL create-handle output",
        process_create_with_handle(2U, 3U, NULL) ==
            SYNTRA_STATUS_INVALID_ARGUMENT
    );

    /* Test 3: Validate the returned generation-aware process handle. */
    syntra_process_handle_t resolved_handle = {0U, 0U};
    check(
        "Resolve legacy ID to same handle",
        process_get_handle(1U, &resolved_handle) == SYNTRA_STATUS_OK &&
        resolved_handle.slot == process_handle.slot &&
        resolved_handle.generation == process_handle.generation
    );
    check(
        "Handle slot is in range and generation is nonzero",
        process_handle.slot < SYNTRA_MAX_PROCESSES &&
        process_handle.generation != 0U
    );
    syntra_process_t handle_snapshot;
    check(
        "Read process through handle",
        process_get_snapshot_by_handle(process_handle, &handle_snapshot) ==
            SYNTRA_STATUS_OK &&
        handle_snapshot.process_id == 1U
    );
    check(
        "Accept idempotent state update through valid handle",
        process_set_state_by_handle(process_handle, PROCESS_READY) ==
            SYNTRA_STATUS_OK
    );
    syntra_process_handle_t invalid_handle = process_handle;
    invalid_handle.generation = 0U;
    check(
        "Reject zero-generation handle",
        process_get_snapshot_by_handle(invalid_handle, &handle_snapshot) ==
            SYNTRA_STATUS_INVALID_ARGUMENT
    );
    invalid_handle = process_handle;
    invalid_handle.generation++;
    check(
        "Reject mismatched-generation handle",
        process_get_snapshot_by_handle(invalid_handle, &handle_snapshot) ==
            SYNTRA_STATUS_INVALID_ARGUMENT
    );
    invalid_handle = process_handle;
    invalid_handle.slot = SYNTRA_MAX_PROCESSES;
    check(
        "Reject out-of-range handle slot",
        process_get_snapshot_by_handle(invalid_handle, &handle_snapshot) ==
            SYNTRA_STATUS_INVALID_ARGUMENT
    );
    check(
        "Reject state update through invalid handle",
        process_set_state_by_handle(invalid_handle, PROCESS_RUNNING) ==
            SYNTRA_STATUS_INVALID_ARGUMENT
    );

    /* Test 4: Get process through a caller-owned snapshot. */
    syntra_process_t process_snapshot;
    syntra_process_t *process = &process_snapshot;
    check(
        "Copy process snapshot",
        process_get_snapshot(1U, &process_snapshot) == SYNTRA_STATUS_OK
    );

    check(
        "Get process ID 1",
        process != NULL &&
        process->process_id == 1
    );

    /* Test 5: Keep the legacy ID-based creation API compatible. */
    check(
        "Create process through legacy ID API",
        process_create(2U, 3U) == SYNTRA_STATUS_OK
    );
    check(
        "Read legacy-created process snapshot",
        process_get_snapshot(2U, &process_snapshot) == SYNTRA_STATUS_OK &&
        process_snapshot.process_id == 2U
    );
    check(
        "Reject duplicate process ID",
        process_create(1U, 3U) != SYNTRA_STATUS_OK
    );

    /* Test 6: Check initial state */
    check(
        "Initial state is READY",
        process != NULL &&
        process->state == PROCESS_READY
    );

    /* Test 7: Change state */
    check(
        "Change state to RUNNING",
        process_set_state_by_handle(process_handle, PROCESS_RUNNING) == SYNTRA_STATUS_OK
    );

    check(
        "Block a RUNNING process",
        process_set_state_by_handle(process_handle, PROCESS_BLOCKED) == SYNTRA_STATUS_OK
    );
    check(
        "Reject direct BLOCKED to RUNNING transition",
        process_set_state_by_handle(process_handle, PROCESS_RUNNING) ==
            SYNTRA_STATUS_INVALID_ARGUMENT
    );
    check(
        "Wake BLOCKED process to READY",
        process_set_state_by_handle(process_handle, PROCESS_READY) == SYNTRA_STATUS_OK
    );
    check(
        "Return READY process to RUNNING",
        process_set_state_by_handle(process_handle, PROCESS_RUNNING) == SYNTRA_STATUS_OK
    );

    /* Test 8: Verify state */
    check(
        "Refresh process snapshot",
        process_get_snapshot(1U, &process_snapshot) == SYNTRA_STATUS_OK
    );

    check(
        "Verify RUNNING state",
        process != NULL &&
        process->state == PROCESS_RUNNING
    );

    /* Test 9: Reject invalid process ID */
    check(
        "Reject process ID 0",
        process_create(0, 5) != SYNTRA_STATUS_OK
    );

    /* Test 10: Terminate process */
    check(
        "Terminate process",
        process_set_state_by_handle(process_handle, PROCESS_TERMINATED) == SYNTRA_STATUS_OK
    );

    /* Test 11: Verify termination */
    check(
        "Refresh terminated process snapshot",
        process_get_snapshot(1U, &process_snapshot) == SYNTRA_STATUS_OK
    );

    check(
        "Verify TERMINATED state",
        process != NULL &&
        process->state == PROCESS_TERMINATED
    );

    check(
        "Reject NULL process snapshot",
        process_get_snapshot(1U, NULL) == SYNTRA_STATUS_INVALID_ARGUMENT
    );
    process_snapshot.state = PROCESS_READY;
    check(
        "Snapshot mutation does not alter manager record",
        process_get_snapshot(1U, &process_snapshot) == SYNTRA_STATUS_OK &&
        process_snapshot.state == PROCESS_TERMINATED
    );

    check(
        "Reject resurrection of TERMINATED process",
        process_set_state(1U, PROCESS_RUNNING) ==
            SYNTRA_STATUS_INVALID_ARGUMENT
    );
    check(
        "TERMINATED state remains unchanged",
        process_get_snapshot(1U, &process_snapshot) == SYNTRA_STATUS_OK &&
        process_snapshot.state == PROCESS_TERMINATED
    );

    printf("\n=================================\n");
    printf(" Total failures: %d\n", failures);
    printf("=================================\n");

    return failures == 0 ? 0 : 1;
}
