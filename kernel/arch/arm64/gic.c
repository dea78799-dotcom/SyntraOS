#include <stddef.h>
#include <stdint.h>

#include "gic.h"

/*
 * SyntraOS ARM64 GIC platform interface.
 * Version: 0.1.0
 *
 * FDT discovery validates and stores platform register regions.
 * Register access, CPU-interface setup, interrupt routing, and EOI
 * handling remain deferred until MMIO access policy is available.
 */

static syntra_arm64_gic_config_t gic_config;
static int gic_configured;
static int gic_initialized;

#define GICD_CTLR 0x000U
#define GICD_TYPER 0x004U
#define GICD_CTLR_RWP (1U << 31)
#define GICD_CTLR_DS (1U << 6)
#define GICD_CTLR_ENABLE_G1 (1U << 1)
#define GICR_FRAME_SIZE 0x20000U
#define GICR_TYPER 0x0008U
#define GICR_WAKER 0x0014U
#define GICR_WAKER_PROCESSOR_SLEEP (1U << 1)
#define GICR_WAKER_CHILDREN_ASLEEP (1U << 2)
#define GICR_SGI_BASE 0x10000U
/* Offsets below are relative to the SGI frame base. */
#define GICR_IGROUPR0 0x00080U
#define GICR_ISENABLER0 0x00100U
#define GICR_IPRIORITYR0 0x00400U
#define GICR_PPI_TIMER_ID 30U
#define GICR_PPI_TIMER_MASK (1U << GICR_PPI_TIMER_ID)
#define GIC_WAIT_LIMIT 1000000U

static inline void gic_dsb(void)
{
#if defined(__aarch64__)
    __asm__ volatile("dsb sy" ::: "memory");
#else
    __asm__ volatile("" ::: "memory");
#endif
}

static inline void gic_isb(void)
{
#if defined(__aarch64__)
    __asm__ volatile("isb" ::: "memory");
#else
    __asm__ volatile("" ::: "memory");
#endif
}

static int gic_wait_register_clear(volatile uint32_t *reg, uint32_t mask)
{
    uint32_t count;
    for (count = 0U; count < GIC_WAIT_LIMIT; ++count)
    {
        if ((*reg & mask) == 0U) return 1;
    }
    return 0;
}

static uint64_t gic_read_mpidr(void)
{
    uint64_t value = 0U;
#if defined(__aarch64__)
    __asm__ volatile("mrs %0, mpidr_el1" : "=r"(value));
#endif
    return value;
}

static uint64_t gic_mpidr_affinity(uint64_t mpidr)
{
    return (((mpidr >> 32U) & 0xFFU) << 24U) |
           (((mpidr >> 16U) & 0xFFU) << 16U) |
           (((mpidr >> 8U) & 0xFFU) << 8U) |
           (mpidr & 0xFFU);
}

static syntra_status_t gic_find_local_redistributor(uintptr_t *base)
{
    uintptr_t offset;
    uint64_t affinity;

    if (base == NULL || gic_config.redistributor_size < GICR_FRAME_SIZE)
        return SYNTRA_STATUS_INVALID_ARGUMENT;

    affinity = gic_mpidr_affinity(gic_read_mpidr());
    for (offset = 0U;
         offset <= gic_config.redistributor_size - GICR_FRAME_SIZE;
         offset += GICR_FRAME_SIZE)
    {
        volatile uint64_t *typer =
            (volatile uint64_t *)(gic_config.redistributor_base +
                                  offset + GICR_TYPER);
        uint64_t value = *typer;
        if (((value >> 32U) & 0xFFFFFFFFU) == affinity)
        {
            *base = gic_config.redistributor_base + offset;
            return SYNTRA_STATUS_OK;
        }
        if (((value >> 4U) & 1U) != 0U) break;
    }
    return SYNTRA_STATUS_ERROR;
}

static syntra_status_t gic_init_cpu_interface(void)
{
#if defined(__aarch64__)
    uint64_t value;
    __asm__ volatile("mrs %0, ICC_SRE_EL1" : "=r"(value));
    value |= 1U;
    __asm__ volatile("msr ICC_SRE_EL1, %0" :: "r"(value) : "memory");
    gic_isb();
    __asm__ volatile("mrs %0, ICC_SRE_EL1" : "=r"(value));
    if ((value & 1U) == 0U) return SYNTRA_STATUS_ERROR;

    value = 0xFFU;
    __asm__ volatile("msr ICC_PMR_EL1, %0" :: "r"(value) : "memory");
    value = 0U;
    __asm__ volatile("msr ICC_BPR1_EL1, %0" :: "r"(value) : "memory");
    value = 1U;
    __asm__ volatile("msr ICC_IGRPEN1_EL1, %0" :: "r"(value) : "memory");
    gic_isb();
    return SYNTRA_STATUS_OK;
#else
    return SYNTRA_STATUS_NOT_IMPLEMENTED;
#endif
}

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

#if defined(__aarch64__)
    volatile uint32_t *dist;
    volatile uint32_t *waker;
    volatile uint32_t *sgi;
    uintptr_t redist_base;
    uint32_t typer;
    uint32_t control;
    syntra_status_t status;

    if (!gic_configured) return SYNTRA_STATUS_ERROR;
    if (gic_initialized) return SYNTRA_STATUS_OK;
    if (gic_config.version != SYNTRA_ARM64_GIC_VERSION_3 ||
        gic_config.distributor_size < 0x1000U)
        return SYNTRA_STATUS_NOT_IMPLEMENTED;

    dist = (volatile uint32_t *)gic_config.distributor_base;
    typer = dist[GICD_TYPER / sizeof(uint32_t)];
    if ((((typer & 0x1FU) + 1U) * 32U) <= GICR_PPI_TIMER_ID)
        return SYNTRA_STATUS_ERROR;

    /* QEMU is run with secure=off: require single-security-state mode. */
    control = dist[GICD_CTLR / sizeof(uint32_t)];
    if ((control & GICD_CTLR_DS) == 0U)
        return SYNTRA_STATUS_NOT_IMPLEMENTED;

    status = gic_find_local_redistributor(&redist_base);
    if (status != SYNTRA_STATUS_OK) return status;

    waker = (volatile uint32_t *)(redist_base + GICR_WAKER);
    *waker &= ~GICR_WAKER_PROCESSOR_SLEEP;
    gic_dsb();
    if (!gic_wait_register_clear(waker, GICR_WAKER_CHILDREN_ASLEEP))
        return SYNTRA_STATUS_ERROR;

    sgi = (volatile uint32_t *)(redist_base + GICR_SGI_BASE);
    sgi[GICR_IGROUPR0 / sizeof(uint32_t)] |= GICR_PPI_TIMER_MASK;
    {
        volatile uint8_t *priority =
            (volatile uint8_t *)(redist_base + GICR_SGI_BASE +
                                 GICR_IPRIORITYR0);
        priority[GICR_PPI_TIMER_ID] = 0x80U;
    }
    sgi[GICR_ISENABLER0 / sizeof(uint32_t)] |= GICR_PPI_TIMER_MASK;
    gic_dsb();

    status = gic_init_cpu_interface();
    if (status != SYNTRA_STATUS_OK) return status;

    control |= GICD_CTLR_ENABLE_G1;
    dist[GICD_CTLR / sizeof(uint32_t)] = control;
    gic_dsb();
    if (!gic_wait_register_clear(
            &dist[GICD_CTLR / sizeof(uint32_t)], GICD_CTLR_RWP))
        return SYNTRA_STATUS_ERROR;

    gic_initialized = 1;
    return SYNTRA_STATUS_OK;
#else
    return SYNTRA_STATUS_NOT_IMPLEMENTED;
#endif
}

/*
 * Read only architected identification registers after validating the
 * Device-Tree-provided MMIO windows. No GIC control register is written.
 */
syntra_status_t syntra_arm64_gic_probe(
    syntra_arm64_gic_probe_result_t *result
)
{
    volatile const uint32_t *distributor;
    volatile const uint64_t *redistributor;
    uintptr_t distributor_base;
    uintptr_t redistributor_base;

    if (result == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (!gic_configured ||
        gic_config.version != SYNTRA_ARM64_GIC_VERSION_3)
    {
        return SYNTRA_STATUS_NOT_IMPLEMENTED;
    }

    /* GICD_TYPER at 0x004, GICD_IIDR at 0x008. */
    if (gic_config.distributor_size < 0x00CU ||
        gic_config.redistributor_size < 0x010U)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    distributor_base = gic_config.distributor_base;
    redistributor_base = gic_config.redistributor_base;

    if ((distributor_base & 0x3U) != 0U ||
        (redistributor_base & 0x7U) != 0U)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    distributor = (volatile const uint32_t *)distributor_base;
    redistributor = (volatile const uint64_t *)redistributor_base;

    /* Use the architectural barrier on ARM64; keep host unit-test builds portable. */
#if defined(__aarch64__)
    __asm__ volatile("dmb sy" ::: "memory");
#else
    __asm__ volatile("" ::: "memory");
#endif
    result->distributor_typer = distributor[0x004U / sizeof(uint32_t)];
    result->distributor_iidr = distributor[0x008U / sizeof(uint32_t)];
    result->redistributor_typer =
        redistributor[0x008U / sizeof(uint64_t)];
#if defined(__aarch64__)
    __asm__ volatile("dmb sy" ::: "memory");
#else
    __asm__ volatile("" ::: "memory");
#endif

    return SYNTRA_STATUS_OK;
}

syntra_status_t syntra_arm64_gic_acknowledge(
    uint32_t *interrupt_id
)
{
    if (interrupt_id == NULL)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (!gic_initialized) return SYNTRA_STATUS_ERROR;
#if defined(__aarch64__)
    {
        uint64_t iar;
        __asm__ volatile("mrs %0, ICC_IAR1_EL1" : "=r"(iar));
        *interrupt_id = (uint32_t)(iar & 0xFFFFFFU);
        return SYNTRA_STATUS_OK;
    }
#else
    return SYNTRA_STATUS_NOT_IMPLEMENTED;
#endif
}

syntra_status_t syntra_arm64_gic_end_of_interrupt(
    uint32_t interrupt_id
)
{
    if (!gic_initialized) return SYNTRA_STATUS_ERROR;
    if (interrupt_id >= 1020U) return SYNTRA_STATUS_INVALID_ARGUMENT;
#if defined(__aarch64__)
    {
        uint64_t value = interrupt_id;
        __asm__ volatile("msr ICC_EOIR1_EL1, %0" :: "r"(value) : "memory");
        gic_isb();
        return SYNTRA_STATUS_OK;
    }
#else
    return SYNTRA_STATUS_NOT_IMPLEMENTED;
#endif
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
