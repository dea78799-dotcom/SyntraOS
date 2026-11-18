
#include "panic.h"
#include "log.h"

void syntra_kernel_panic(const char *reason)
{
    syntra_log_write(
        SYNTRA_LOG_ERROR,
        reason
    );

    /*
     * TODO:
     * Implement architecture-specific
     * kernel panic handling.
     */
}
