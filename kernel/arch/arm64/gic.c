#include <stddef.h>
#include <stdint.h>

#include "gic.h"

/*
 * SyntraOS ARM64 GIC platform interface.
 * Version: 0.1.0
 *
 * This stage validates and stores platform data only. Register access,
 * CPU-interface setup, interrupt routing, and EOI handling are deferred
 * until the FDT parser and MMIO access policy are available.
 */

static syntra_arm64_gic_config_t gic_config;
static int gic_configured;
static int gic_initialized;

static int valid_mmio_region(uintptr_t base, size_t size)
{
    if (base == (uintptr_t)0 || size < sizeof(uint32_t))
    {
        return 0;
    }

    if ((uintptr_t)size > UINTPTR_MAX - base)
    {
        return 0;
    }

    return 1;
}

syntra_status_t syntra_arm64_gic_configure(
    const syntra_arm64_gic_config_t *config
)
{
    if (config == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (gic_initialized)
    {
        return SYNTRA_STATUS_PERMISSION_DENIED;
    }

    if (!valid_mmio_region(
            config->distributor_base,
            config->distributor_size))
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (config->version == SYNTRA_ARM64_GIC_VERSION_2)
    {
        if (!valid_mmio_region(
                config->cpu_interface_base,
                config->cpu_interface_size))
        {
            return SYNTRA_STATUS_INVALID_ARGUMENT;
        }
    }
    else if (config->version == SYNTRA_ARM64_GIC_VERSION_3)
    {
        if (!valid_mmio_region(
                config->redistributor_base,
                config->redistributor_size))
        {
            return SYNTRA_STATUS_INVALID_ARGUMENT;
        }
    }
    else
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    gic_config = *config;
    gic_configured = 1;

    return SYNTRA_STATUS_OK;
}

syntra_status_t syntra_arm64_gic_init(void)
{
    if (!gic_configured)
    {
        return SYNTRA_STATUS_ERROR;
    }

    /*
     * Do not touch MMIO until the Device Tree parser has validated
     * the complete register ranges and the boot code has established
     * the required memory attributes and access policy.
     */
    return SYNTRA_STATUS_NOT_IMPLEMENTED;
}

syntra_status_t syntra_arm64_gic_acknowledge(
    uint32_t *interrupt_id
)
{
    if (interrupt_id == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (!gic_initialized)
    {
        return SYNTRA_STATUS_ERROR;
    }

    return SYNTRA_STATUS_NOT_IMPLEMENTED;
}

syntra_status_t syntra_arm64_gic_end_of_interrupt(
    uint32_t interrupt_id
)
{
    (void)interrupt_id;

    if (!gic_initialized)
    {
        return SYNTRA_STATUS_ERROR;
    }

    return SYNTRA_STATUS_NOT_IMPLEMENTED;
}

syntra_status_t syntra_arm64_gic_get_info(
    syntra_arm64_gic_info_t *info
)
{
    if (info == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    info->config = gic_config;
    info->configured = gic_configured;
    info->initialized = gic_initialized;
    info->hardware_access_enabled = 0;

    return SYNTRA_STATUS_OK;
}

static syntra_status_t gic_try_discover(
    const syntra_fdt_t *fdt,
    const char *compatible,
    syntra_arm64_gic_version_t version
)
{
    syntra_fdt_node_t node;
    syntra_arm64_gic_config_t config;
    syntra_status_t status;

    status = syntra_fdt_find_compatible(fdt, compatible, &node);
    if (status != SYNTRA_STATUS_OK)
    {
        return status;
    }

    config.version = version;
    config.cpu_interface_base = 0U;
    config.cpu_interface_size = 0U;
    config.redistributor_base = 0U;
    config.redistributor_size = 0U;

    status = syntra_fdt_get_reg(
        fdt,
        &node,
        0U,
        &config.distributor_base,
        &config.distributor_size);
    if (status != SYNTRA_STATUS_OK)
    {
        return status;
    }

    if (version == SYNTRA_ARM64_GIC_VERSION_2)
    {
        status = syntra_fdt_get_reg(
            fdt,
            &node,
            1U,
            &config.cpu_interface_base,
            &config.cpu_interface_size);
    }
    else
    {
        status = syntra_fdt_get_reg(
            fdt,
            &node,
            1U,
            &config.redistributor_base,
            &config.redistributor_size);
    }

    if (status != SYNTRA_STATUS_OK)
    {
        return status;
    }

    return syntra_arm64_gic_configure(&config);
}

syntra_status_t syntra_arm64_gic_discover(const syntra_fdt_t *fdt)
{
    syntra_status_t status;

    if (fdt == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    status = gic_try_discover(
        fdt,
        "arm,gic-v3",
        SYNTRA_ARM64_GIC_VERSION_3);
    if (status == SYNTRA_STATUS_OK)
    {
        return status;
    }

    status = gic_try_discover(
        fdt,
        "arm,gic-400",
        SYNTRA_ARM64_GIC_VERSION_2);
    if (status == SYNTRA_STATUS_OK)
    {
        return status;
    }

    status = gic_try_discover(
        fdt,
        "arm,cortex-a15-gic",
        SYNTRA_ARM64_GIC_VERSION_2);
    if (status == SYNTRA_STATUS_OK)
    {
        return status;
    }

    return gic_try_discover(
        fdt,
        "arm,cortex-a9-gic",
        SYNTRA_ARM64_GIC_VERSION_2);
}
