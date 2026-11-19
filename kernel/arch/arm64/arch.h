#ifndef SYNTRA_ARM64_ARCH_H
#define SYNTRA_ARM64_ARCH_H

#include "../../core/status.h"

/*
 * SyntraOS ARM64 Architecture Foundation
 * Version: 1.0.0
 *
 * Architecture interface for ARM64.
 */

#define SYNTRA_ARM64_ARCH_VERSION "1.0.0"

#define SYNTRA_ARM64_ARCH_NAME "ARM64"

typedef enum
{
    SYNTRA_ARM64_MODE_UNKNOWN = 0,
    SYNTRA_ARM64_MODE_EL1 = 1,
    SYNTRA_ARM64_MODE_EL2 = 2,
    SYNTRA_ARM64_MODE_EL3 = 3
} syntra_arm64_exception_level_t;

/*
 * Initialize ARM64 architecture support.
 *
 * This interface will be implemented in arch.c.
 */
syntra_status_t syntra_arm64_arch_init(void);

#endif
