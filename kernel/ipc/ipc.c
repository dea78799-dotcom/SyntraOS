
#include "ipc.h"

static syntra_ipc_message_t messages[SYNTRA_IPC_MAX_MESSAGES];

static unsigned int message_count = 0;
static int ipc_initialized = 0;


syntra_status_t ipc_init(void)
{
    if (ipc_initialized)
    {
        return SYNTRA_STATUS_ERROR;
    }

    message_count = 0;
    ipc_initialized = 1;

    return SYNTRA_STATUS_OK;
}


syntra_status_t ipc_send(
    unsigned int sender_id,
    unsigned int receiver_id,
    const void *data,
    unsigned int size
)
{
    if (!ipc_initialized)
    {
        return SYNTRA_STATUS_ERROR;
    }

    if (sender_id == 0 ||
        receiver_id == 0 ||
        data == 0 ||
        size == 0 ||
        size > SYNTRA_IPC_MAX_MESSAGE_SIZE)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (message_count >= SYNTRA_IPC_MAX_MESSAGES)
    {
        return SYNTRA_STATUS_OUT_OF_MEMORY;
    }

    syntra_ipc_message_t *message = &messages[message_count];

    message->sender_id = sender_id;
    message->receiver_id = receiver_id;
    message->message_size = size;

    const unsigned char *source =
        (const unsigned char *)data;

    for (unsigned int i = 0; i < size; i++)
    {
        message->data[i] = source[i];
    }

    message_count++;

    return SYNTRA_STATUS_OK;
}


syntra_status_t ipc_receive(
    unsigned int receiver_id,
    syntra_ipc_message_t *message
)
{
    if (!ipc_initialized)
    {
        return SYNTRA_STATUS_ERROR;
    }

    if (receiver_id == 0 || message == 0)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    for (unsigned int i = 0; i < message_count; i++)
    {
        if (messages[i].receiver_id != receiver_id)
        {
            continue;
        }

        *message = messages[i];

        for (unsigned int j = i; j < message_count - 1; j++)
        {
            messages[j] = messages[j + 1];
        }

        message_count--;

        return SYNTRA_STATUS_OK;
    }

    return SYNTRA_STATUS_ERROR;
}


unsigned int ipc_get_message_count(void)
{
    return message_count;
}
