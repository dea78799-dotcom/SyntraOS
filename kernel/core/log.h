
#ifndef SYNTRA_KERNEL_LOG_H
#define SYNTRA_KERNEL_LOG_H

typedef enum
{
    SYNTRA_LOG_INFO = 0,
    SYNTRA_LOG_WARNING = 1,
    SYNTRA_LOG_ERROR = 2,
    SYNTRA_LOG_DEBUG = 3
} syntra_log_level_t;

typedef struct
{
    unsigned int info_count;
    unsigned int warning_count;
    unsigned int error_count;
    unsigned int debug_count;
    unsigned int total_count;
} syntra_log_info_t;

void syntra_log_write(
    syntra_log_level_t level,
    const char *message
);

void syntra_log_get_info(
    syntra_log_info_t *info
);

void syntra_log_reset(void);

#endif
