
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

    printf("\n=================================\n");
    printf(" Total failures: %d\n", failures);
    printf("=================================\n");

    return failures == 0 ? 0 : 1;
}
