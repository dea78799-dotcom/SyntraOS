
#ifndef SYNTRA_IPC_H
#define SYNTRA_IPC_H

#include "../core/status.h"

#define SYNTRA_IPC_MAX_MESSAGES 64
#define SYNTRA_IPC_MAX_MESSAGE_SIZE 256

typedef struct
{
    unsigned int sender_id;
    unsigned int receiver_id;
    unsigned int message_size;

    unsigned char data[SYNTRA_IPC_MAX_MESSAGE_SIZE];

} syntra_ipc_message_t;

/* Initialize IPC */
syntra_status_t ipc_init(void);

/* Send a message */
syntra_status_t ipc_send(
    unsigned int sender_id,
    unsigned int receiver_id,
    const void *data,
    unsigned int size
);

/* Receive a message */
syntra_status_t ipc_receive(
    unsigned int receiver_id,
    syntra_ipc_message_t *message
);

/* Get pending message count */
unsigned int ipc_get_message_count(void);

#endif
