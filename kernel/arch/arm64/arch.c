#include "arch.h"

/*
 * SyntraOS ARM64 Architecture Foundation
 * Version: 1.0.0
 *
 * Initial ARM64 architecture implementation.
 *
 * Hardware initialization will be added
 * during the ARM64 boot integration stage.
 */

syntra_status_t syntra_arm64_arch_init(void)
{
    /*
     * CPU register initialization is not implemented yet.
     * Do not report successful hardware initialization.
     */
    return SYNTRA_STATUS_NOT_IMPLEMENTED;
}
