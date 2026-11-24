
#include "ipc.h"
#include "../arch/irq.h"

static syntra_ipc_message_t messages[SYNTRA_IPC_MAX_MESSAGES];

static unsigned int message_count = 0;
static int ipc_initialized = 0;


static syntra_status_t ipc_init_unlocked(void)
{
    if (ipc_initialized)
    {
        return SYNTRA_STATUS_ERROR;
    }

    message_count = 0;
    ipc_initialized = 1;

    return SYNTRA_STATUS_OK;
}


static syntra_status_t ipc_send_unlocked(
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


static syntra_status_t ipc_receive_unlocked(
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


static unsigned int ipc_get_message_count_unlocked(void)
{
    return message_count;
}


/*
 * The bounded message table is shared kernel state. Local IRQ masking keeps
 * enqueue/dequeue/count operations atomic against local IRQ re-entry on the
 * current single-core ARM64 target. This is not an SMP lock. Sender IDs are
 * still caller-supplied metadata and are not authenticated here.
 */
syntra_status_t ipc_init(void)
{
    syntra_irq_state_t irq = syntra_irq_save();
    syntra_status_t status = ipc_init_unlocked();
    syntra_irq_restore(irq);
    return status;
}

syntra_status_t ipc_send(
    unsigned int sender_id,
    unsigned int receiver_id,
    const void *data,
    unsigned int size)
{
    syntra_irq_state_t irq = syntra_irq_save();
    syntra_status_t status = ipc_send_unlocked(
        sender_id, receiver_id, data, size);
    syntra_irq_restore(irq);
    return status;
}

syntra_status_t ipc_receive(
    unsigned int receiver_id,
    syntra_ipc_message_t *message)
{
    syntra_irq_state_t irq = syntra_irq_save();
    syntra_status_t status = ipc_receive_unlocked(receiver_id, message);
    syntra_irq_restore(irq);
    return status;
}

unsigned int ipc_get_message_count(void)
{
    syntra_irq_state_t irq = syntra_irq_save();
    unsigned int count = ipc_get_message_count_unlocked();
    syntra_irq_restore(irq);
    return count;
}
