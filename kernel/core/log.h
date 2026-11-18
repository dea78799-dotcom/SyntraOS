
#ifndef SYNTRA_KERNEL_LOG_H
#define SYNTRA_KERNEL_LOG_H

typedef enum
{
    SYNTRA_LOG_INFO,
    SYNTRA_LOG_WARNING,
    SYNTRA_LOG_ERROR,
    SYNTRA_LOG_DEBUG
} syntra_log_level_t;

void syntra_log_write(
    syntra_log_level_t level,
    const char *message
);

#endif
