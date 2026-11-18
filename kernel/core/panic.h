
#ifndef SYNTRA_KERNEL_PANIC_H
#define SYNTRA_KERNEL_PANIC_H

#define SYNTRA_PANIC_REASON_SIZE 128

typedef enum
{
    SYNTRA_PANIC_NONE = 0,
    SYNTRA_PANIC_ACTIVE = 1
} syntra_panic_state_t;

typedef struct
{
    syntra_panic_state_t state;
    unsigned int timestamp;
    char reason[SYNTRA_PANIC_REASON_SIZE];
} syntra_panic_info_t;

void syntra_kernel_panic(const char *reason);

void syntra_kernel_panic_at(
    const char *reason,
    unsigned int timestamp
);

void syntra_kernel_panic_get_info(
    syntra_panic_info_t *info
);

void syntra_kernel_panic_reset(void);

#endif
