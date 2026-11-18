
#include <stdio.h>
#include <string.h>

#include "../../kernel/ipc/ipc.h"

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
    printf(" SYNTRAOS IPC TEST V1\n");
    printf("=================================\n");

    /* Test 1: Initialize IPC */
    check(
        "ipc_init",
        ipc_init() == SYNTRA_STATUS_OK
    );

    /* Test 2: Send message */
    const char message[] = "Hello SyntraOS";

    check(
        "Send message",
        ipc_send(1, 2, message, sizeof(message)) == SYNTRA_STATUS_OK
    );

    /* Test 3: Check message count */
    check(
        "Message count equals 1",
        ipc_get_message_count() == 1
    );

    /* Test 4: Receive message */
    syntra_ipc_message_t received;

    check(
        "Receive message",
        ipc_receive(2, &received) == SYNTRA_STATUS_OK
    );

    /* Test 5: Check sender */
    check(
        "Verify sender ID",
        received.sender_id == 1
    );

    /* Test 6: Check receiver */
    check(
        "Verify receiver ID",
        received.receiver_id == 2
    );

    /* Test 7: Check message content */
    check(
        "Verify message content",
        received.message_size == sizeof(message) &&
        memcmp(received.data, message, sizeof(message)) == 0
    );

    /* Test 8: Check empty queue */
    check(
        "Message queue is empty",
        ipc_get_message_count() == 0
    );

    /* Test 9: Reject invalid message */
    check(
        "Reject zero-size message",
        ipc_send(1, 2, message, 0) != SYNTRA_STATUS_OK
    );

    printf("\n=================================\n");
    printf(" Total failures: %d\n", failures);
    printf("=================================\n");

    return failures == 0 ? 0 : 1;
}
