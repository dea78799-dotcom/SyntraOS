
#include <stdio.h>
#include <string.h>

#include "ipc.h"

int main(void)
{
    printf("=== SYNTRAOS IPC TEST ===\n");

    if (ipc_init() != SYNTRA_STATUS_OK)
    {
        printf("IPC initialization failed!\n");
        return 1;
    }

    printf("IPC initialized successfully.\n");

    const char message[] = "Hello SyntraOS!";

    if (ipc_send(
        1,
        2,
        message,
        sizeof(message)
    ) != SYNTRA_STATUS_OK)
    {
        printf("Message sending failed!\n");
        return 1;
    }

    printf("Message sent successfully.\n");

    printf("Pending messages: %u\n",
           ipc_get_message_count());

    syntra_ipc_message_t received;

    if (ipc_receive(2, &received) != SYNTRA_STATUS_OK)
    {
        printf("Message receiving failed!\n");
        return 1;
    }

    printf("Sender ID: %u\n", received.sender_id);
    printf("Receiver ID: %u\n", received.receiver_id);

    printf("Message: %s\n", received.data);

    printf("Pending messages: %u\n",
           ipc_get_message_count());

    printf("=== TEST COMPLETED ===\n");

    return 0;
}
