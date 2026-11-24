
#include <stdio.h>
#include <stddef.h>

#include "../../kernel/memory/memory.h"

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
    printf(" SYNTRAOS MEMORY MANAGER TEST V1\n");
    printf("=================================\n");

    /* Test 1: Initialize memory */
    check(
        "memory_init",
        memory_init() == SYNTRA_STATUS_OK
    );

    /* Test 2: Get memory information */
    syntra_memory_info_t info;
    memory_get_info(&info);

    check(
        "Memory information",
        info.total_blocks == SYNTRA_MAX_MEMORY_BLOCKS &&
        info.used_blocks == 0 &&
        info.free_blocks == SYNTRA_MAX_MEMORY_BLOCKS
    );

    /* Test 3: Allocate one block */
    void *block = memory_allocate(1);
    void *staged_address = NULL;
    void *stale_address = NULL;
    syntra_memory_allocation_handle_t staged_handle = {UINT32_MAX, 0U};
    syntra_memory_allocation_handle_t stale_handle = {UINT32_MAX, 0U};
    syntra_memory_owner_handle_t owner = {3U, 7U};
    syntra_memory_owner_handle_t wrong_owner = {4U, 7U};

    check(
        "Allocate one block",
        block != NULL
    );

    /* Test 4: Check allocated block count */
    memory_get_info(&info);

    check(
        "Used blocks equals 1",
        info.used_blocks == 1
    );

    /* Test 5: Release allocated block */
    check(
        "Release one block",
        memory_release(block, 1) == SYNTRA_STATUS_OK
    );

    /* Test 6: Check free memory */
    memory_get_info(&info);

    check(
        "All blocks are free",
        info.used_blocks == 0 &&
        info.free_blocks == SYNTRA_MAX_MEMORY_BLOCKS
    );

    /* Test 7: Reject zero allocation */
    check(
        "Reject zero allocation",
        memory_allocate(0) == NULL
    );

    /* Test 8: Reject oversized allocation */
    check(
        "Reject oversized allocation",
        memory_allocate(SYNTRA_MAX_MEMORY_BLOCKS + 1) == NULL
    );

    /* Test 9: Staged allocation identity and ownership */
    check(
        "Allocate staged memory with generation token",
        memory_allocate_staged(2U, &staged_address, &staged_handle) ==
            SYNTRA_STATUS_OK &&
        staged_address != NULL &&
        staged_handle.generation != 0U
    );

    check(
        "Legacy release rejects staged allocation",
        memory_release(staged_address, 2U) ==
            SYNTRA_STATUS_PERMISSION_DENIED
    );

    check(
        "Bind staged allocation to exact owner",
        memory_bind_staged_allocation(staged_handle, owner) ==
            SYNTRA_STATUS_OK
    );

    check(
        "Reject wrong owner during allocation commit",
        memory_commit_thread_allocation(staged_handle, wrong_owner) ==
            SYNTRA_STATUS_PERMISSION_DENIED
    );

    check(
        "Commit allocation ownership",
        memory_commit_thread_allocation(staged_handle, owner) ==
            SYNTRA_STATUS_OK
    );

    check(
        "Legacy release rejects thread-owned allocation",
        memory_release(staged_address, 2U) ==
            SYNTRA_STATUS_PERMISSION_DENIED
    );

    check(
        "Reject owned release by wrong owner",
        memory_release_owned_allocation(staged_handle, wrong_owner) ==
            SYNTRA_STATUS_PERMISSION_DENIED
    );

    check(
        "Release owned allocation by exact token and owner",
        memory_release_owned_allocation(staged_handle, owner) ==
            SYNTRA_STATUS_OK
    );

    check(
        "Reject duplicate release with stale token",
        memory_release_owned_allocation(staged_handle, owner) ==
            SYNTRA_STATUS_INVALID_ARGUMENT
    );

    /* Test 10: Address reuse must not revive a stale token */
    stale_handle = staged_handle;
    stale_address = staged_address;
    staged_address = NULL;
    check(
        "Allocate second staged lifetime",
        memory_allocate_staged(1U, &staged_address, &staged_handle) ==
            SYNTRA_STATUS_OK &&
        staged_address != NULL
    );

    check(
        "Reused allocation slot has a newer generation",
        staged_handle.slot == stale_handle.slot &&
        staged_address == stale_address &&
        staged_handle.generation > stale_handle.generation
    );

    check(
        "Bind and commit second allocation",
        memory_bind_staged_allocation(staged_handle, owner) ==
            SYNTRA_STATUS_OK &&
        memory_commit_thread_allocation(staged_handle, owner) ==
            SYNTRA_STATUS_OK
    );

    check(
        "Old generation cannot release reused allocation",
        memory_release_owned_allocation(stale_handle, owner) ==
            SYNTRA_STATUS_INVALID_ARGUMENT &&
        memory_get_allocation_count() == 1U
    );

    check(
        "Release current allocation generation",
        memory_release_owned_allocation(staged_handle, owner) ==
            SYNTRA_STATUS_OK
    );

    /* Test 11: Rollback paths are exact and one-shot */
    check(
        "Allocate staged region for rollback",
        memory_allocate_staged(1U, &staged_address, &staged_handle) ==
            SYNTRA_STATUS_OK
    );

    check(
        "Abort staged allocation",
        memory_abort_staged_allocation(staged_handle) ==
            SYNTRA_STATUS_OK
    );

    check(
        "Reject duplicate staged rollback",
        memory_abort_staged_allocation(staged_handle) ==
            SYNTRA_STATUS_INVALID_ARGUMENT
    );

    check(
        "Allocate staged region for reserved rollback",
        memory_allocate_staged(1U, &staged_address, &staged_handle) ==
            SYNTRA_STATUS_OK
    );

    check(
        "Bind reserved allocation",
        memory_bind_staged_allocation(staged_handle, owner) ==
            SYNTRA_STATUS_OK
    );

    check(
        "Reject reserved rollback by wrong owner",
        memory_abort_reserved_allocation(staged_handle, wrong_owner) ==
            SYNTRA_STATUS_PERMISSION_DENIED
    );

    check(
        "Abort exact unpublished owner reservation",
        memory_abort_reserved_allocation(staged_handle, owner) ==
            SYNTRA_STATUS_OK
    );

    check(
        "Allocate staged region for invalid-owner test",
        memory_allocate_staged(1U, &staged_address, &staged_handle) ==
            SYNTRA_STATUS_OK
    );

    check(
        "Reject owner with zero generation",
        memory_bind_staged_allocation(
            staged_handle,
            (syntra_memory_owner_handle_t){0U, 0U}
        ) == SYNTRA_STATUS_INVALID_ARGUMENT
    );

    check(
        "Invalid owner leaves staged allocation rollback-safe",
        memory_abort_staged_allocation(staged_handle) ==
            SYNTRA_STATUS_OK
    );

    check(
        "Final allocator integrity after ownership tests",
        memory_validate() == SYNTRA_STATUS_OK
    );

    printf("\n=================================\n");
    printf(" Total failures: %d\n", failures);
    printf("=================================\n");

    return failures == 0 ? 0 : 1;
}
