#include <stdint.h>
#include <stddef.h>

#include "../../core/status.h"
#include "fdt.h"
#include "memory_map.h"
#include "../../../memory/memory_region.h"
#include "../../../memory/page_allocator.h"
#include "../exception.h"
#include "../context.h"
#include "../gic.h"
#include "../mmu.h"
#include "../page_table_builder.h"
#include "../mmu_mapping_plan.h"
#include "../../../gui/ramfb.h"
#include "../../../virtio/virtio_mmio.h"

/* Optional QEMU fw_cfg MMIO window, discovered from the supplied FDT. */
static uintptr_t syntra_fw_cfg_base;
static syntra_arm64_page_table_t syntra_active_page_table;
static _Alignas(4096) uint8_t syntra_el0_kernel_stack[4096];
static size_t syntra_fw_cfg_size;
static syntra_virtio_mmio_device_t syntra_virtio_mmio_device;
static int syntra_virtio_mmio_available;
#include "../timer.h"
#include "../../../timer/timer.h"
#include "../../../interrupt/interrupt.h"
#include "../../../scheduler/scheduler.h"

/*
 * SyntraOS ARM64 Boot Stage
 * Version: 1.1.0
 */

#define SYNTRA_ARM64_DTB_ADDRESS  0x40000000ULL
#define SYNTRA_ARM64_DTB_LIMIT    0x00200000ULL
#define SYNTRA_ARM64_EL1_VALUE    0x4ULL

#define SYNTRA_PL011_BASE 0x09000000ULL
#define SYNTRA_PL011_UARTDR (*(volatile uint32_t *)(uintptr_t)(SYNTRA_PL011_BASE + 0x00U))
#define SYNTRA_PL011_UARTFR (*(volatile uint32_t *)(uintptr_t)(SYNTRA_PL011_BASE + 0x18U))
#define SYNTRA_PL011_UARTIBRD (*(volatile uint32_t *)(uintptr_t)(SYNTRA_PL011_BASE + 0x24U))
#define SYNTRA_PL011_UARTFBRD (*(volatile uint32_t *)(uintptr_t)(SYNTRA_PL011_BASE + 0x28U))
#define SYNTRA_PL011_UARTLCRH (*(volatile uint32_t *)(uintptr_t)(SYNTRA_PL011_BASE + 0x2cU))
#define SYNTRA_PL011_UARTCR (*(volatile uint32_t *)(uintptr_t)(SYNTRA_PL011_BASE + 0x30U))
#define SYNTRA_PL011_UARTFR_TXFF (1U << 5)

extern char __kernel_start[];
extern char __kernel_end[];
extern char __text_start[];
extern char __text_end[];
extern char __rodata_start[];
extern char __rodata_end[];
extern char __data_start[];
extern char __data_end[];
extern char __bss_start[];
extern char __bss_end[];
extern char __stack_bottom[];
extern char __stack_top[];
extern char __user_text_start[];
extern char __user_text_end[];
extern char __user_stack_bottom[];
extern char __user_stack_top[];
extern void syntra_arm64_user_fault_halt(void);
extern void syntra_arm64_enter_user_context(syntra_arm64_exception_context_t *, uintptr_t);

#define SYNTRA_EL0_CODE_VA  0x10000000ULL
#define SYNTRA_EL0_STACK_VA 0x10001000ULL

static void syntra_uart_init(void)
{
    SYNTRA_PL011_UARTCR = 0U;
    SYNTRA_PL011_UARTIBRD = 13U;
    SYNTRA_PL011_UARTFBRD = 1U;
    SYNTRA_PL011_UARTLCRH = (3U << 5);
    SYNTRA_PL011_UARTCR = (1U << 0) | (1U << 8) | (1U << 9);
}

static void syntra_uart_putc(char character)
{
    while ((SYNTRA_PL011_UARTFR & SYNTRA_PL011_UARTFR_TXFF) != 0U)
    {
    }
    SYNTRA_PL011_UARTDR = (uint32_t)character;
}

static void syntra_uart_puts(const char *message)
{
    while (*message != '\0')
    {
        if (*message == '\n')
        {
            syntra_uart_putc('\r');
        }
        syntra_uart_putc(*message++);
    }
}



void syntra_arm64_exception_uart_puts(const char *message) { syntra_uart_puts(message); }

static void syntra_uart_put_hex(uint64_t value)
{
    static const char digits[] = "0123456789abcdef";
    int shift;

    syntra_uart_puts("0x");
    for (shift = 60; shift >= 0; shift -= 4)
    {
        syntra_uart_putc(digits[(value >> (unsigned int)shift) & 0x0fU]);
    }
}


void syntra_arm64_exception_uart_put_hex(uint64_t value) { syntra_uart_put_hex(value); }

static void syntra_boot_report_mmu_registers(const char *prefix)
{
    syntra_arm64_mmu_registers_t registers;
    syntra_status_t status = syntra_arm64_mmu_read_registers(&registers);

    if (status != SYNTRA_STATUS_OK)
    {
        syntra_uart_puts("MMU register snapshot: NOT AVAILABLE\n");
        return;
    }

    syntra_uart_puts(prefix);
    syntra_uart_puts("SCTLR_EL1=");
    syntra_uart_put_hex(registers.sctlr_el1);
    syntra_uart_puts(" MAIR_EL1=");
    syntra_uart_put_hex(registers.mair_el1);
    syntra_uart_puts(" TCR_EL1=");
    syntra_uart_put_hex(registers.tcr_el1);
    syntra_uart_puts(" TTBR0_EL1=");
    syntra_uart_put_hex(registers.ttbr0_el1);
    syntra_uart_puts("\n");
}

/*
 * Call only after the bootstrap page tables identity-map the complete live
 * kernel image, active stack, DTB, UART and all discovered GIC MMIO windows.
 * Kept separate from the current boot path until those mappings are verified.
 */
syntra_status_t syntra_arm64_boot_enable_mmu(uint64_t root_table_physical_address)
{
    syntra_arm64_mmu_registers_t registers;
    syntra_status_t status =
        syntra_arm64_mmu_prepare(root_table_physical_address);

    if (status != SYNTRA_STATUS_OK)
    {
        syntra_uart_puts("MMU prepare: FAIL\n");
        return status;
    }

    status = syntra_arm64_mmu_read_registers(&registers);
    if (status != SYNTRA_STATUS_OK)
    {
        syntra_uart_puts("MMU register readback: FAIL\n");
        return status;
    }

    syntra_uart_puts("MMU registers before SCTLR.M: ");
    syntra_uart_puts("MAIR_EL1=");
    syntra_uart_put_hex(registers.mair_el1);
    syntra_uart_puts(" TCR_EL1=");
    syntra_uart_put_hex(registers.tcr_el1);
    syntra_uart_puts(" TTBR0_EL1=");
    syntra_uart_put_hex(registers.ttbr0_el1);
    syntra_uart_puts("\n");

    status = syntra_arm64_mmu_enable();
    if (status == SYNTRA_STATUS_OK)
    {
        /* First UART marker after the enable routine's final ISB. */
        syntra_uart_puts("MMU enabled: PASS (after ISB)\n");
    }
    else
    {
        syntra_uart_puts("MMU enable: FAIL\n");
    }

    return status;
}

static syntra_status_t syntra_boot_report_interrupt_controller(
    const syntra_fdt_t *fdt
)
{
    static const char *const compatibles[] = {
        "arm,gic-v3",
        "arm,gic-400",
        "arm,cortex-a15-gic",
        "arm,cortex-a9-gic"
    };
    syntra_fdt_node_t node;
    syntra_status_t status;
    const uint8_t *compatible_data;
    uint32_t compatible_size;
    uintptr_t base;
    size_t region_size;
    size_t index;
    uint32_t offset;

    for (index = 0U; index < sizeof(compatibles) / sizeof(compatibles[0]); ++index)
    {
        status = syntra_fdt_find_compatible(fdt, compatibles[index], &node);
        if (status == SYNTRA_STATUS_OK)
        {
            break;
        }
    }

    if (index == sizeof(compatibles) / sizeof(compatibles[0])
        || status != SYNTRA_STATUS_OK)
    {
        syntra_uart_puts("FDT interrupt-controller: NOT FOUND\n");
        return SYNTRA_STATUS_ERROR;
    }

    status = syntra_fdt_get_property(
        fdt, &node, "compatible", &compatible_data, &compatible_size);
    if (status != SYNTRA_STATUS_OK || compatible_size == 0U)
    {
        syntra_uart_puts("FDT compatible property invalid\n");
        return SYNTRA_STATUS_ERROR;
    }

    syntra_uart_puts("FDT interrupt-controller compatible: ");
    offset = 0U;
    while (offset < compatible_size)
    {
        uint32_t length = 0U;
        while (offset + length < compatible_size &&
               compatible_data[offset + length] != 0U)
        {
            ++length;
        }

        for (uint32_t char_index = 0U; char_index < length; ++char_index)
        {
            syntra_uart_putc((char)compatible_data[offset + char_index]);
        }

        offset += length + 1U;
        if (offset < compatible_size)
        {
            syntra_uart_puts(", ");
        }
    }
    syntra_uart_puts("\n");

    status = syntra_fdt_get_reg(fdt, &node, 0U, &base, &region_size);
    if (status != SYNTRA_STATUS_OK)
    {
        syntra_uart_puts("FDT interrupt-controller reg[0] invalid\n");
        return status;
    }

    syntra_uart_puts("FDT interrupt-controller MMIO reg[0]: base=");
    syntra_uart_put_hex((uint64_t)base);
    syntra_uart_puts(" size=");
    syntra_uart_put_hex((uint64_t)region_size);
    syntra_uart_puts("\n");

    status = syntra_fdt_get_reg(fdt, &node, 1U, &base, &region_size);
    if (status == SYNTRA_STATUS_OK)
    {
        syntra_uart_puts("FDT interrupt-controller MMIO reg[1]: base=");
        syntra_uart_put_hex((uint64_t)base);
        syntra_uart_puts(" size=");
        syntra_uart_put_hex((uint64_t)region_size);
        syntra_uart_puts("\n");
    }

    syntra_uart_puts(index == 0U
        ? "FDT detected controller: GICv3\n"
        : "FDT detected controller: GICv2-compatible\n");

    return SYNTRA_STATUS_OK;
}

static syntra_status_t validate_dtb_and_discover_gic(const void *dtb)
{
    syntra_fdt_t fdt;
    syntra_fdt_iterator_t iterator;
    syntra_fdt_token_t token;
    syntra_status_t status;
    int found_end = 0;

    status = syntra_fdt_open(
        dtb,
        (size_t)SYNTRA_ARM64_DTB_LIMIT,
        &fdt
    );

    if (status != SYNTRA_STATUS_OK)
    {
        return status;
    }

    syntra_fw_cfg_base = 0U;
    syntra_fw_cfg_size = 0U;
    {
        syntra_fdt_node_t fw_cfg_node;
        uintptr_t fw_cfg_base;
        size_t fw_cfg_size;
        status = syntra_fdt_find_compatible(
            &fdt, "qemu,fw-cfg-mmio", &fw_cfg_node);
        if (status == SYNTRA_STATUS_OK)
        {
            status = syntra_fdt_get_reg(
                &fdt, &fw_cfg_node, 0U, &fw_cfg_base, &fw_cfg_size);
            if (status != SYNTRA_STATUS_OK || fw_cfg_size < 0x18U)
            {
                syntra_uart_puts("FDT fw_cfg MMIO region invalid\n");
                return SYNTRA_STATUS_ERROR;
            }
            syntra_fw_cfg_base = fw_cfg_base;
            syntra_fw_cfg_size = fw_cfg_size;
            syntra_uart_puts("FDT fw_cfg MMIO: base=");
            syntra_uart_put_hex((uint64_t)syntra_fw_cfg_base);
            syntra_uart_puts(" size=");
            syntra_uart_put_hex((uint64_t)syntra_fw_cfg_size);
            syntra_uart_puts("\n");
        }
        else
        {
            /* UART-only boot remains supported when fw_cfg is absent. */
            syntra_uart_puts("FDT fw_cfg: NOT FOUND; GUI output unavailable\n");
        }
    }

    syntra_virtio_mmio_available = 0;
    syntra_virtio_mmio_device.base = 0U;
    syntra_virtio_mmio_device.region_size = 0U;
    syntra_virtio_mmio_device.reg_index = 0U;
    syntra_virtio_mmio_device.discovered = 0;
    status = syntra_virtio_mmio_discover_first(
        &fdt, &syntra_virtio_mmio_device);
    if (status == SYNTRA_STATUS_OK)
    {
        syntra_virtio_mmio_available = 1;
        syntra_uart_puts("FDT VirtIO-MMIO region discovered: base=");
        syntra_uart_put_hex((uint64_t)syntra_virtio_mmio_device.base);
        syntra_uart_puts(" size=");
        syntra_uart_put_hex((uint64_t)syntra_virtio_mmio_device.region_size);
        syntra_uart_puts("\n");
    }
    else if (status == SYNTRA_STATUS_NOT_IMPLEMENTED)
    {
        syntra_uart_puts("FDT VirtIO-MMIO: NOT FOUND\n");
    }
    else
    {
        /* Input is optional; malformed discovery must not break UART boot. */
        syntra_uart_puts("FDT VirtIO-MMIO discovery: UNAVAILABLE\n");
    }

    syntra_fdt_iterator_init(&iterator);

    for (;;)
    {
        status = syntra_fdt_next(&fdt, &iterator, &token);

        if (status == SYNTRA_STATUS_NOT_IMPLEMENTED)
        {
            break;
        }

        if (status != SYNTRA_STATUS_OK)
        {
            return SYNTRA_STATUS_ERROR;
        }

        if (token.type == SYNTRA_FDT_TOKEN_END)
        {
            found_end = 1;
            break;
        }
    }

    if (!found_end)
    {
        return SYNTRA_STATUS_ERROR;
    }

    {
        syntra_arm64_boot_memory_metadata_t metadata;

        metadata.kernel_start = (uintptr_t)__kernel_start;
        metadata.kernel_end = (uintptr_t)__kernel_end;
        metadata.text_start = (uintptr_t)__text_start;
        metadata.text_end = (uintptr_t)__text_end;
        metadata.rodata_start = (uintptr_t)__rodata_start;
        metadata.rodata_end = (uintptr_t)__rodata_end;
        metadata.data_start = (uintptr_t)__data_start;
        metadata.data_end = (uintptr_t)__data_end;
        metadata.bss_start = (uintptr_t)__bss_start;
        metadata.bss_end = (uintptr_t)__bss_end;
        metadata.stack_bottom = (uintptr_t)__stack_bottom;
        metadata.stack_top = (uintptr_t)__stack_top;
        metadata.dtb_start = (uintptr_t)dtb;
        metadata.dtb_size = fdt.total_size;

        status = syntra_arm64_boot_memory_metadata_set(&metadata);
        if (status != SYNTRA_STATUS_OK)
        {
            syntra_uart_puts("Boot memory metadata validation failed\n");
            return status;
        }

        syntra_uart_puts("Boot memory metadata: PASS kernel=");
        syntra_uart_put_hex((uint64_t)metadata.kernel_start);
        syntra_uart_puts("-");
        syntra_uart_put_hex((uint64_t)metadata.kernel_end);
        syntra_uart_puts(" stack=");
        syntra_uart_put_hex((uint64_t)metadata.stack_bottom);
        syntra_uart_puts("-");
        syntra_uart_put_hex((uint64_t)metadata.stack_top);
        syntra_uart_puts(" dtb=");
        syntra_uart_put_hex((uint64_t)metadata.dtb_start);
        syntra_uart_puts(" size=");
        syntra_uart_put_hex((uint64_t)metadata.dtb_size);
        syntra_uart_puts("\n");
    }

    status = syntra_arm64_memory_map_discover(
        &fdt,
        (uintptr_t)dtb,
        fdt.total_size
    );
    if (status != SYNTRA_STATUS_OK)
    {
        syntra_uart_puts("FDT physical memory map discovery failed\n");
        return status;
    }

    /*
     * The linker symbols include .text, .rodata, .data, .bss and the
     * bootstrap stack. Reserve the complete loaded kernel before the
     * physical page allocator is allowed to return any RAM page.
     */
    {
        uintptr_t kernel_start = (uintptr_t)__kernel_start;
        uintptr_t kernel_end = (uintptr_t)__kernel_end;

        if (kernel_start == 0U || kernel_end <= kernel_start)
        {
            syntra_uart_puts("Kernel linker memory range invalid\n");
            return SYNTRA_STATUS_ERROR;
        }

        status = syntra_memory_region_reserve(
            kernel_start,
            (size_t)(kernel_end - kernel_start),
            SYNTRA_MEMORY_REGION_KERNEL);
        if (status != SYNTRA_STATUS_OK)
        {
            syntra_uart_puts("Kernel RAM reservation failed\n");
            return status;
        }

        syntra_uart_puts("Kernel reserved: base=");
        syntra_uart_put_hex((uint64_t)kernel_start);
        syntra_uart_puts(" end=");
        syntra_uart_put_hex((uint64_t)kernel_end);
        syntra_uart_puts("\n");
    }

    syntra_uart_puts("FDT physical memory regions: ");
    syntra_uart_put_hex((uint64_t)syntra_memory_region_count());
    syntra_uart_puts("\n");

    {
        size_t region_index;
        for (region_index = 0U;
             region_index < syntra_memory_region_count();
             ++region_index)
        {
            syntra_memory_region_t region;
            if (syntra_memory_region_get(region_index, &region) !=
                SYNTRA_STATUS_OK)
            {
                return SYNTRA_STATUS_ERROR;
            }

            syntra_uart_puts("  region base=");
            syntra_uart_put_hex((uint64_t)region.base);
            syntra_uart_puts(" size=");
            syntra_uart_put_hex((uint64_t)region.size);
            syntra_uart_puts(" type=");
            syntra_uart_put_hex((uint64_t)region.type);
            syntra_uart_puts("\n");
        }
    }

    status = syntra_page_allocator_init();
    if (status != SYNTRA_STATUS_OK)
    {
        syntra_uart_puts("Physical page allocator initialization failed\n");
        return status;
    }

    syntra_uart_puts("Physical page allocator: PASS free=");
    syntra_uart_put_hex((uint64_t)syntra_page_allocator_free_count());
    syntra_uart_puts(" total=");
    syntra_uart_put_hex((uint64_t)syntra_page_allocator_total_count());
    syntra_uart_puts("\n");

    status = syntra_boot_report_interrupt_controller(&fdt);
    if (status != SYNTRA_STATUS_OK)
    {
        return status;
    }

    return syntra_arm64_gic_discover(&fdt);
}

static void syntra_arm64_timer_irq_handler(
    syntra_interrupt_id_t interrupt_id,
    void *context
)
{
    (void)interrupt_id;
    (void)context;
    syntra_arm64_timer_handle_irq();
}

static syntra_status_t syntra_arm64_generic_timer_irq_test(void)
{
    uint64_t start;
    uint64_t timeout_ticks;
    syntra_status_t status;

    status = interrupt_init();
    if (status != SYNTRA_STATUS_OK) return status;

    status = interrupt_controller_attach(
        syntra_arm64_gic_controller_ops(), NULL);
    if (status != SYNTRA_STATUS_OK) return status;

    status = interrupt_controller_init();
    if (status != SYNTRA_STATUS_OK)
    {
        syntra_uart_puts("GICv3 controller initialization failed; IRQs remain masked\n");
        return status;
    }

    status = syntra_arm64_timer_init(SYNTRA_ARM64_TIMER_DEFAULT_HZ);
    if (status != SYNTRA_STATUS_OK)
    {
        syntra_uart_puts("Generic Timer initialization failed\n");
        return status;
    }

    status = interrupt_register(
        30U, syntra_arm64_timer_irq_handler, NULL);
    if (status != SYNTRA_STATUS_OK) return status;

    status = interrupt_set_priority(30U, 0x80U);
    if (status != SYNTRA_STATUS_OK) return status;

    status = interrupt_enable(30U);
    if (status != SYNTRA_STATUS_OK) return status;

    status = syntra_arm64_timer_enable();
    if (status != SYNTRA_STATUS_OK)
    {
        (void)interrupt_disable(30U);
        syntra_uart_puts("Generic Timer enable failed\n");
        return status;
    }

    start = syntra_arm64_timer_read_counter();
    timeout_ticks = syntra_arm64_timer_read_frequency() * 2U;
    __asm__ volatile("dsb sy" ::: "memory");
    __asm__ volatile("msr daifclr, #2" ::: "memory");
    __asm__ volatile("isb" ::: "memory");

    while (timer_get_ticks() < 3ULL)
    {
        uint64_t now = syntra_arm64_timer_read_counter();
        if ((now - start) > timeout_ticks)
        {
            __asm__ volatile("msr daifset, #2" ::: "memory");
            (void)syntra_arm64_timer_disable();
            (void)interrupt_disable(30U);
            syntra_uart_puts("FAIL: Generic Timer IRQ timeout\n");
            return SYNTRA_STATUS_ERROR;
        }
    }

    __asm__ volatile("msr daifset, #2" ::: "memory");
    __asm__ volatile("isb" ::: "memory");
    status = syntra_arm64_timer_disable();
    if (status != SYNTRA_STATUS_OK)
    {
        (void)interrupt_disable(30U);
        return status;
    }

    status = interrupt_disable(30U);
    if (status != SYNTRA_STATUS_OK) return status;

    if (timer_get_ticks() < 3ULL) return SYNTRA_STATUS_ERROR;
    syntra_uart_puts("PASS: Generic Interrupt Framework dispatched ARM64 Timer IRQs\n");
    syntra_uart_puts("PASS: ARM64 Generic Timer IRQ delivered 3 ticks\n");
    return SYNTRA_STATUS_OK;
}

static syntra_status_t syntra_arm64_svc_vector_test(void)
{
    const syntra_arm64_exception_info_t *info;
    const syntra_arm64_exception_context_t *context;

    __asm__ volatile("svc #0x42" ::: "memory");

    info = syntra_arm64_exception_get_last_info();
    context = syntra_arm64_exception_get_last_context();
    if (info == NULL || context == NULL)
    {
        return SYNTRA_STATUS_ERROR;
    }

    if (info->exception_class != SYNTRA_ARM64_EXCEPTION_SVC_AARCH64
        || info->vector_index != SYNTRA_ARM64_VECTOR_CURRENT_SPX_SYNC
        || (context->esr_el1 & 0xFFFFU)
            != SYNTRA_ARM64_EXCEPTION_TEST_SVC_IMMEDIATE)
    {
        return SYNTRA_STATUS_ERROR;
    }

    syntra_uart_puts("PASS: SVC exception vector and ERET return path\n");
    return SYNTRA_STATUS_OK;
}

/* Reuse the initialized GIC and timer to validate a fresh IRQ after MMU-on. */
static syntra_status_t syntra_arm64_post_mmu_timer_irq_test(void)
{
    uint64_t start;
    uint64_t timeout_ticks;
    uint64_t initial_ticks;
    syntra_status_t status;

    initial_ticks = timer_get_ticks();
    start = syntra_arm64_timer_read_counter();
    timeout_ticks = syntra_arm64_timer_read_frequency() * 2U;

    status = interrupt_enable(30U);
    if (status != SYNTRA_STATUS_OK) return status;

    status = syntra_arm64_timer_enable();
    if (status != SYNTRA_STATUS_OK)
    {
        (void)interrupt_disable(30U);
        return status;
    }

    __asm__ volatile("dsb sy" ::: "memory");
    __asm__ volatile("msr daifclr, #2" ::: "memory");
    __asm__ volatile("isb" ::: "memory");

    while ((timer_get_ticks() - initial_ticks) < 3ULL)
    {
        uint64_t now = syntra_arm64_timer_read_counter();
        if ((now - start) > timeout_ticks)
        {
            __asm__ volatile("msr daifset, #2" ::: "memory");
            __asm__ volatile("isb" ::: "memory");
            (void)syntra_arm64_timer_disable();
            (void)interrupt_disable(30U);
            syntra_uart_puts("Post-MMU Generic Timer IRQ: FAIL timeout\n");
            return SYNTRA_STATUS_ERROR;
        }
    }

    __asm__ volatile("msr daifset, #2" ::: "memory");
    __asm__ volatile("isb" ::: "memory");

    status = syntra_arm64_timer_disable();
    if (status != SYNTRA_STATUS_OK)
    {
        (void)interrupt_disable(30U);
        return status;
    }
    status = interrupt_disable(30U);
    if (status != SYNTRA_STATUS_OK) return status;

    if ((timer_get_ticks() - initial_ticks) < 3ULL)
        return SYNTRA_STATUS_ERROR;

    syntra_uart_puts("Post-MMU Generic Timer IRQ: PASS ticks=3+\n");
    return SYNTRA_STATUS_OK;
}


#define SYNTRA_SCHEDULER_TEST_STACK_SIZE 8192U
#define SYNTRA_SCHEDULER_TEST_BOOT_ID 1U
#define SYNTRA_SCHEDULER_TEST_WORKER_A_ID 2U
#define SYNTRA_SCHEDULER_TEST_WORKER_B_ID 3U

static _Alignas(16) uint8_t scheduler_worker_a_stack[SYNTRA_SCHEDULER_TEST_STACK_SIZE];
static _Alignas(16) uint8_t scheduler_worker_b_stack[SYNTRA_SCHEDULER_TEST_STACK_SIZE];
static volatile uint64_t scheduler_test_deadline;
static volatile unsigned int scheduler_test_completed;
static volatile unsigned int scheduler_test_last_worker;
static volatile unsigned int scheduler_test_switches;
static volatile unsigned int scheduler_test_worker_slices[2];
static volatile char scheduler_test_trace[16];
static volatile unsigned int scheduler_test_trace_length;

static void syntra_scheduler_test_worker(void *argument)
{
    unsigned int worker_id = (unsigned int)(uintptr_t)argument;
    unsigned int worker_index = worker_id - SYNTRA_SCHEDULER_TEST_WORKER_A_ID;
    uint64_t observed_tick = timer_get_ticks();

    for (;;)
    {
        uint64_t current_tick;

        /*
         * Keep the tick observation and all associated trace state in one
         * IRQ-atomic section. Otherwise a timer IRQ can switch workers after
         * the tick comparison but before the trace append, making the test
         * report a false non-alternating sequence.
         */
        __asm__ volatile("msr daifset, #2" ::: "memory");
        current_tick = timer_get_ticks();
        if (current_tick >= scheduler_test_deadline)
        {
            __asm__ volatile("msr daifclr, #2" ::: "memory");
            __asm__ volatile("isb" ::: "memory");
            break;
        }

        if (current_tick != observed_tick)
        {
            observed_tick = current_tick;
            scheduler_test_worker_slices[worker_index]++;
            if (scheduler_test_last_worker != 0U &&
                scheduler_test_last_worker != worker_id)
                scheduler_test_switches++;
            scheduler_test_last_worker = worker_id;

            if (scheduler_test_trace_length <
                (unsigned int)sizeof(scheduler_test_trace))
            {
                scheduler_test_trace[scheduler_test_trace_length] =
                    worker_id == SYNTRA_SCHEDULER_TEST_WORKER_A_ID
                        ? 'A' : 'B';
                scheduler_test_trace_length++;
            }
        }
        __asm__ volatile("msr daifclr, #2" ::: "memory");
        __asm__ volatile("isb" ::: "memory");
    }

    scheduler_test_completed++;
    if (scheduler_test_completed == 2U)
        (void)scheduler_wake_thread(SYNTRA_SCHEDULER_TEST_BOOT_ID);
}

static syntra_status_t syntra_arm64_scheduler_context_switch_test(void)
{
    syntra_status_t status;
    syntra_thread_t *thread;
    syntra_arm64_exception_context_t *initial_context = NULL;

    scheduler_test_completed = 0U;
    scheduler_test_last_worker = 0U;
    scheduler_test_switches = 0U;
    scheduler_test_worker_slices[0] = 0U;
    scheduler_test_worker_slices[1] = 0U;
    scheduler_test_trace_length = 0U;
    for (unsigned int i = 0U; i < sizeof(scheduler_test_trace); ++i)
        scheduler_test_trace[i] = '\0';

    status = scheduler_init();
    if (status != SYNTRA_STATUS_OK) return status;

    status = scheduler_create_thread(
        SYNTRA_SCHEDULER_TEST_WORKER_A_ID, 0U,
        syntra_scheduler_test_worker,
        (void *)(uintptr_t)SYNTRA_SCHEDULER_TEST_WORKER_A_ID,
        scheduler_worker_a_stack, sizeof(scheduler_worker_a_stack));
    if (status != SYNTRA_STATUS_OK) return status;
    thread = scheduler_get_thread(SYNTRA_SCHEDULER_TEST_WORKER_A_ID);
    if (thread == NULL) return SYNTRA_STATUS_ERROR;
    status = syntra_arm64_context_initialize(
        scheduler_worker_a_stack, sizeof(scheduler_worker_a_stack),
        syntra_scheduler_test_worker,
        (void *)(uintptr_t)SYNTRA_SCHEDULER_TEST_WORKER_A_ID,
        &initial_context);
    if (status != SYNTRA_STATUS_OK) return status;
    thread->cpu_context = initial_context;

    status = scheduler_create_thread(
        SYNTRA_SCHEDULER_TEST_WORKER_B_ID, 0U,
        syntra_scheduler_test_worker,
        (void *)(uintptr_t)SYNTRA_SCHEDULER_TEST_WORKER_B_ID,
        scheduler_worker_b_stack, sizeof(scheduler_worker_b_stack));
    if (status != SYNTRA_STATUS_OK) return status;
    thread = scheduler_get_thread(SYNTRA_SCHEDULER_TEST_WORKER_B_ID);
    if (thread == NULL) return SYNTRA_STATUS_ERROR;
    initial_context = NULL;
    status = syntra_arm64_context_initialize(
        scheduler_worker_b_stack, sizeof(scheduler_worker_b_stack),
        syntra_scheduler_test_worker,
        (void *)(uintptr_t)SYNTRA_SCHEDULER_TEST_WORKER_B_ID,
        &initial_context);
    if (status != SYNTRA_STATUS_OK) return status;
    thread->cpu_context = initial_context;

    status = scheduler_add_thread(SYNTRA_SCHEDULER_TEST_BOOT_ID, 0U);
    if (status != SYNTRA_STATUS_OK) return status;
    status = scheduler_set_thread_state(
        SYNTRA_SCHEDULER_TEST_BOOT_ID, THREAD_BLOCKED);
    if (status != SYNTRA_STATUS_OK) return status;

    scheduler_test_deadline = timer_get_ticks() + 8ULL;
    status = interrupt_enable(30U);
    if (status != SYNTRA_STATUS_OK) return status;
    status = syntra_arm64_timer_enable();
    if (status != SYNTRA_STATUS_OK)
    {
        (void)interrupt_disable(30U);
        return status;
    }

    __asm__ volatile("dsb sy" ::: "memory");
    __asm__ volatile("msr daifclr, #2" ::: "memory");
    __asm__ volatile("isb" ::: "memory");
    __asm__ volatile("svc #0x43" ::: "memory");

    __asm__ volatile("msr daifset, #2" ::: "memory");
    __asm__ volatile("isb" ::: "memory");
    (void)syntra_arm64_timer_disable();
    (void)interrupt_disable(30U);

    if (scheduler_test_completed != 2U ||
        scheduler_test_worker_slices[0] == 0U ||
        scheduler_test_worker_slices[1] == 0U ||
        scheduler_test_switches < 2U ||
        scheduler_test_trace_length < 4U ||
        scheduler_validate() != SYNTRA_STATUS_OK)
    {
        syntra_uart_puts("FAIL: ARM64 Scheduler context switch validation\n");
        return SYNTRA_STATUS_ERROR;
    }

    for (unsigned int i = 1U; i < scheduler_test_trace_length; ++i)
    {
        if (scheduler_test_trace[i] == scheduler_test_trace[i - 1U])
        {
            syntra_uart_puts("FAIL: ARM64 Scheduler trace did not alternate\n");
            return SYNTRA_STATUS_ERROR;
        }
    }

    syntra_uart_puts("Scheduler trace: ");
    for (unsigned int i = 0U; i < scheduler_test_trace_length; ++i)
        syntra_uart_putc(scheduler_test_trace[i]);
    syntra_uart_puts("\n");
    syntra_uart_puts("PASS: ARM64 Scheduler switched between two threads\n");
    return SYNTRA_STATUS_OK;
}

static syntra_status_t syntra_boot_page_table_allocate(
    void *context, uint64_t *physical_address)
{
    uintptr_t page;
    syntra_status_t status;
    (void)context;
    if (physical_address == NULL) return SYNTRA_STATUS_INVALID_ARGUMENT;
    status = syntra_page_alloc(&page);
    if (status == SYNTRA_STATUS_OK) *physical_address = (uint64_t)page;
    return status;
}

static uint64_t *syntra_boot_page_table_resolve(
    uint64_t physical_address, void *context)
{
    (void)context;
    if (physical_address > (uint64_t)UINTPTR_MAX ||
        (physical_address & (SYNTRA_ARM64_MMU_PAGE_SIZE - 1ULL)) != 0ULL)
        return NULL;
    return (uint64_t *)(uintptr_t)physical_address;
}

static const uint64_t *syntra_boot_page_table_walk_resolve(
    uint64_t physical_address, void *context)
{
    return syntra_boot_page_table_resolve(physical_address, context);
}

/* Build and audit the identity mappings, but leave SCTLR_EL1.M disabled. */
static syntra_status_t syntra_arm64_boot_mmu_mapping_audit(void)
{
    syntra_arm64_boot_memory_metadata_t metadata;
    syntra_arm64_gic_info_t gic_info;
    syntra_arm64_mmu_mapping_plan_t plan;
    syntra_arm64_page_table_t page_table;
    syntra_memory_region_t regions[SYNTRA_MEMORY_REGION_MAX];
    size_t region_count = syntra_memory_region_count();
    size_t index;
    syntra_status_t status;

    if (region_count == 0U || region_count > SYNTRA_MEMORY_REGION_MAX)
        return SYNTRA_STATUS_ERROR;

    status = syntra_arm64_boot_memory_metadata_get(&metadata);
    if (status != SYNTRA_STATUS_OK) return status;
    status = syntra_arm64_gic_get_info(&gic_info);
    if (status != SYNTRA_STATUS_OK || !gic_info.configured)
        return SYNTRA_STATUS_ERROR;

    for (index = 0U; index < region_count; ++index)
    {
        status = syntra_memory_region_get(index, &regions[index]);
        if (status != SYNTRA_STATUS_OK) return status;
    }

    status = syntra_arm64_mmu_mapping_plan_build_with_virtio(
        &metadata, regions, region_count, &gic_info.config,
        SYNTRA_PL011_BASE, 0x1000U,
        syntra_fw_cfg_base, syntra_fw_cfg_size,
        syntra_virtio_mmio_available
            ? syntra_virtio_mmio_device.base : 0U,
        syntra_virtio_mmio_available
            ? syntra_virtio_mmio_device.region_size : 0U,
        &plan);
    if (status != SYNTRA_STATUS_OK)
    {
        syntra_uart_puts("MMU mapping plan: FAIL\n");
        return status;
    }

    status = syntra_arm64_page_table_init(
        &page_table, syntra_boot_page_table_allocate,
        syntra_boot_page_table_resolve, NULL);
    if (status != SYNTRA_STATUS_OK) return status;

    for (index = 0U; index < plan.count; ++index)
    {
        const syntra_arm64_mmu_mapping_t *mapping = &plan.mappings[index];
        status = syntra_arm64_page_table_map(
            &page_table, mapping->virtual_address,
            mapping->physical_address, mapping->size,
            mapping->memory_type, mapping->access, mapping->executable);
        if (status != SYNTRA_STATUS_OK)
        {
            syntra_uart_puts("MMU page-table map failed index=");
            syntra_uart_put_hex((uint64_t)index);
            syntra_uart_puts("\n");
            return status;
        }
    }

    for (index = 0U; index < plan.count; ++index)
    {
        const syntra_arm64_mmu_mapping_t *mapping = &plan.mappings[index];
        status = syntra_arm64_page_table_verify(
            &page_table, mapping->virtual_address,
            mapping->physical_address, mapping->size,
            mapping->memory_type, mapping->access, mapping->executable);
        if (status != SYNTRA_STATUS_OK)
        {
            syntra_uart_puts("MMU software-walker verification failed index=");
            syntra_uart_put_hex((uint64_t)index);
            syntra_uart_puts("\n");
            return status;
        }
    }

    syntra_uart_puts("MMU identity mapping audit: PASS entries=");
    syntra_uart_put_hex((uint64_t)plan.count);
    if (syntra_virtio_mmio_available)
    {
        syntra_uart_puts(" VirtIO-MMIO=Device/RW/XN");
    }
    syntra_uart_puts(" root=");
    syntra_uart_put_hex(page_table.root_physical_address);
    syntra_uart_puts("\n");

    /* Configure translation registers only after the complete table audit. */
    status = syntra_arm64_mmu_prepare(page_table.root_physical_address);
    if (status != SYNTRA_STATUS_OK)
    {
        syntra_uart_puts("MMU prepare failed; translation remains disabled\n");
        return status;
    }

    status = syntra_arm64_mmu_enable();
    if (status != SYNTRA_STATUS_OK)
    {
        syntra_uart_puts("MMU enable failed\n");
        return status;
    }

    {
        syntra_arm64_mmu_registers_t registers;
        status = syntra_arm64_mmu_read_registers(&registers);
        if (status != SYNTRA_STATUS_OK ||
            (registers.sctlr_el1 & 1ULL) == 0ULL ||
            (registers.ttbr0_el1 & SYNTRA_ARM64_MMU_PA_MASK) !=
                page_table.root_physical_address)
        {
            syntra_uart_puts("MMU activation register verification: FAIL\n");
            return SYNTRA_STATUS_ERROR;
        }
    }

    syntra_active_page_table = page_table;

    /* Re-walk every planned range after enabling translation. */
    for (index = 0U; index < plan.count; ++index)
    {
        const syntra_arm64_mmu_mapping_t *mapping = &plan.mappings[index];
        status = syntra_arm64_page_table_verify(
            &page_table, mapping->virtual_address,
            mapping->physical_address, mapping->size,
            mapping->memory_type, mapping->access, mapping->executable);
        if (status != SYNTRA_STATUS_OK)
        {
            syntra_uart_puts("Post-MMU software-walker verification failed index=");
            syntra_uart_put_hex((uint64_t)index);
            syntra_uart_puts("\n");
            return status;
        }
    }

    syntra_uart_puts("MMU activation: PASS SCTLR_EL1.M=1 TTBR0_EL1=");
    syntra_uart_put_hex(page_table.root_physical_address);
    syntra_uart_puts("\n");
    syntra_uart_puts("MMU post-activation mapping verification: PASS\n");

    /* Exercise the synchronous exception vector with translation enabled. */
    status = syntra_arm64_svc_vector_test();
    if (status != SYNTRA_STATUS_OK)
    {
        syntra_uart_puts("Post-MMU SVC/ERET validation: FAIL\n");
        return status;
    }
    syntra_uart_puts("Post-MMU SVC/ERET validation: PASS\n");

    status = syntra_arm64_post_mmu_timer_irq_test();
    if (status != SYNTRA_STATUS_OK)
    {
        syntra_uart_puts("Post-MMU Timer IRQ validation: FAIL\n");
        return status;
    }
    return SYNTRA_STATUS_OK;
}

static syntra_status_t syntra_arm64_run_el0_hello_test(void)
{
    syntra_status_t status;
    syntra_arm64_mmu_translation_t code, stack;
    syntra_arm64_exception_context_t *context = NULL;
    uintptr_t text_pa = (uintptr_t)__user_text_start;
    uintptr_t stack_pa = (uintptr_t)__user_stack_bottom;
    uintptr_t text_size = (uintptr_t)__user_text_end - text_pa;
    uintptr_t fault_pa = (uintptr_t)syntra_arm64_user_fault_halt;
    uintptr_t fault_va;

    if (text_size != SYNTRA_ARM64_MMU_PAGE_SIZE ||
        (text_pa & (SYNTRA_ARM64_MMU_PAGE_SIZE - 1U)) != 0U ||
        (stack_pa & (SYNTRA_ARM64_MMU_PAGE_SIZE - 1U)) != 0U ||
        fault_pa < text_pa || fault_pa >= text_pa + text_size)
        return SYNTRA_STATUS_ERROR;

    status = syntra_arm64_page_table_map_user(
        &syntra_active_page_table, SYNTRA_EL0_CODE_VA, text_pa,
        SYNTRA_ARM64_MMU_PAGE_SIZE, SYNTRA_ARM64_MMU_ACCESS_READ_ONLY, 1);
    if (status != SYNTRA_STATUS_OK) return status;

    status = syntra_arm64_page_table_map_user(
        &syntra_active_page_table, SYNTRA_EL0_STACK_VA, stack_pa,
        SYNTRA_ARM64_MMU_PAGE_SIZE, SYNTRA_ARM64_MMU_ACCESS_READ_WRITE, 0);
    if (status != SYNTRA_STATUS_OK) return status;

    status = syntra_arm64_page_table_verify_user(
        &syntra_active_page_table, SYNTRA_EL0_CODE_VA, text_pa,
        SYNTRA_ARM64_MMU_PAGE_SIZE, SYNTRA_ARM64_MMU_ACCESS_READ_ONLY, 1);
    if (status != SYNTRA_STATUS_OK) return status;
    status = syntra_arm64_page_table_verify_user(
        &syntra_active_page_table, SYNTRA_EL0_STACK_VA, stack_pa,
        SYNTRA_ARM64_MMU_PAGE_SIZE, SYNTRA_ARM64_MMU_ACCESS_READ_WRITE, 0);
    if (status != SYNTRA_STATUS_OK) return status;

    if (syntra_arm64_mmu_walk(syntra_active_page_table.root_physical_address,
            SYNTRA_EL0_CODE_VA, syntra_boot_page_table_walk_resolve, NULL, &code)
            != SYNTRA_STATUS_OK ||
        !code.user_read || code.user_write || !code.user_execute ||
        syntra_arm64_mmu_walk(syntra_active_page_table.root_physical_address,
            SYNTRA_EL0_STACK_VA, syntra_boot_page_table_walk_resolve, NULL, &stack)
            != SYNTRA_STATUS_OK ||
        !stack.user_read || !stack.user_write || stack.user_execute)
        return SYNTRA_STATUS_PERMISSION_DENIED;

    fault_va = SYNTRA_EL0_CODE_VA + (fault_pa - text_pa);
    __asm__ volatile("dsb ishst" ::: "memory");
    __asm__ volatile("tlbi vmalle1" ::: "memory");
    __asm__ volatile("dsb ish" ::: "memory");
    __asm__ volatile("isb" ::: "memory");

    syntra_uart_puts("EL0 code descriptor: ");
    syntra_uart_put_hex(code.page_descriptor);
    syntra_uart_puts(" AF=");
    syntra_uart_put_hex((code.page_descriptor >> 10U) & 1U);
    syntra_uart_puts(" AP=");
    syntra_uart_put_hex((code.page_descriptor >> 6U) & 3U);
    syntra_uart_puts(" SH=");
    syntra_uart_put_hex((code.page_descriptor >> 8U) & 3U);
    syntra_uart_puts(" UXN=");
    syntra_uart_put_hex((code.page_descriptor >> 54U) & 1U);
    syntra_uart_puts("\nEL0 stack descriptor: ");
    syntra_uart_put_hex(stack.page_descriptor);
    syntra_uart_puts(" AF=");
    syntra_uart_put_hex((stack.page_descriptor >> 10U) & 1U);
    syntra_uart_puts(" AP=");
    syntra_uart_put_hex((stack.page_descriptor >> 6U) & 3U);
    syntra_uart_puts(" SH=");
    syntra_uart_put_hex((stack.page_descriptor >> 8U) & 3U);
    syntra_uart_puts(" UXN=");
    syntra_uart_put_hex((stack.page_descriptor >> 54U) & 1U);
    syntra_uart_puts("\n");
    if (((code.page_descriptor >> 10U) & 1U) != 1U ||
        ((code.page_descriptor >> 6U) & 3U) != 3U ||
        ((code.page_descriptor >> 8U) & 3U) != 3U ||
        ((code.page_descriptor >> 54U) & 1U) != 0U ||
        ((stack.page_descriptor >> 10U) & 1U) != 1U ||
        ((stack.page_descriptor >> 6U) & 3U) != 1U ||
        ((stack.page_descriptor >> 8U) & 3U) != 3U ||
        ((stack.page_descriptor >> 54U) & 1U) != 1U)
    {
        syntra_uart_puts("EL0 descriptor attributes: FAIL\n");
        return SYNTRA_STATUS_PERMISSION_DENIED;
    }
    syntra_uart_puts("EL0 descriptor attributes: PASS\n");
    syntra_uart_puts("EL0 page permissions: code=RO+UX stack=RW+UXN PASS\n");
    status = syntra_arm64_user_context_initialize(
        syntra_el0_kernel_stack, sizeof(syntra_el0_kernel_stack),
        (void *)(uintptr_t)SYNTRA_EL0_STACK_VA,
        SYNTRA_ARM64_MMU_PAGE_SIZE,
        (void (*)(void *))(uintptr_t)SYNTRA_EL0_CODE_VA,
        (void (*)(void))fault_va, NULL, &context);
    if (status != SYNTRA_STATUS_OK) return status;

    if (context->spsr_el1 != 0x3C0ULL ||
        (context->spsr_el1 & 0xFULL) != 0ULL ||
        context->elr_el1 != SYNTRA_EL0_CODE_VA ||
        context->sp_el0 != SYNTRA_EL0_STACK_VA + SYNTRA_ARM64_MMU_PAGE_SIZE)
        return SYNTRA_STATUS_ERROR;

    syntra_uart_puts("EL0 context: SPSR_EL1=0x3c0 ELR_EL1=0x10000000 SP_EL0=0x10002000 PASS\n");
    syntra_uart_puts("EL0 transition: ERET\n");
    syntra_arm64_enter_user_context(context,
        (uintptr_t)(syntra_el0_kernel_stack + sizeof(syntra_el0_kernel_stack)));
    syntra_uart_puts("EL0 test returned to EL1: PASS\n");
    return SYNTRA_STATUS_OK;
}

syntra_status_t syntra_arm64_boot_main(
    uint64_t dtb_address,
    uint64_t current_el
)
{
    const void *dtb;
    syntra_status_t status;

    if (dtb_address != SYNTRA_ARM64_DTB_ADDRESS)
    {
        return SYNTRA_STATUS_INVALID_ARGUMENT;
    }

    if (current_el != SYNTRA_ARM64_EL1_VALUE)
    {
        return SYNTRA_STATUS_NOT_IMPLEMENTED;
    }

    dtb = (const void *)(uintptr_t)dtb_address;


    syntra_uart_init();
    syntra_boot_report_mmu_registers("MMU registers at boot: ");
    status = validate_dtb_and_discover_gic(dtb);
    if (status != SYNTRA_STATUS_OK)
    {
        syntra_uart_puts("SyntraOS boot failed: FDT/GIC discovery\n");
        return status;
    }

    {
        syntra_arm64_gic_probe_result_t probe;

        status = syntra_arm64_gic_probe(&probe);
        if (status != SYNTRA_STATUS_OK)
        {
            syntra_uart_puts("SyntraOS boot failed: GICv3 read-only probe\n");
            return status;
        }

        syntra_uart_puts("GICv3 read-only probe: PASS\n");
        syntra_uart_puts("GICD_TYPER=");
        syntra_uart_put_hex((uint64_t)probe.distributor_typer);
        syntra_uart_puts(" GICD_IIDR=");
        syntra_uart_put_hex((uint64_t)probe.distributor_iidr);
        syntra_uart_puts("\nGICR_TYPER=");
        syntra_uart_put_hex(probe.redistributor_typer);
        syntra_uart_puts("\nGIC control registers unchanged; IRQs remain disabled\n");
    }

    status = syntra_arm64_svc_vector_test();
    if (status != SYNTRA_STATUS_OK)
    {
        syntra_uart_puts("SyntraOS boot failed: SVC/ERET test\n");
        return status;
    }

    status = syntra_arm64_generic_timer_irq_test();
    if (status != SYNTRA_STATUS_OK)
    {
        syntra_uart_puts("SyntraOS boot failed: GICv3/Generic Timer IRQ test\n");
        return status;
    }

    status = syntra_arm64_scheduler_context_switch_test();
    if (status != SYNTRA_STATUS_OK)
    {
        syntra_uart_puts("SyntraOS boot failed: ARM64 Scheduler context switch test\n");
        return status;
    }

    status = syntra_arm64_boot_mmu_mapping_audit();
    if (status != SYNTRA_STATUS_OK)
    {
        syntra_uart_puts("SyntraOS boot failed: MMU mapping audit\n");
        return status;
    }

    /*
     * The MMU audit has verified the identity-mapped VirtIO window as
     * Device/RW/XN. Only now perform bounded, read-only identification reads.
     */
    if (syntra_virtio_mmio_available)
    {
        syntra_virtio_mmio_register_snapshot_t snapshot;
        syntra_virtio_mmio_identity_t identity;

        status = syntra_virtio_mmio_read_identity_mapped(
            &syntra_virtio_mmio_device,
            (const volatile void *)(uintptr_t)syntra_virtio_mmio_device.base,
            syntra_virtio_mmio_device.region_size,
            &snapshot);
        if (status == SYNTRA_STATUS_OK)
        {
            syntra_uart_puts("VirtIO-MMIO ID registers: magic=");
            syntra_uart_put_hex(snapshot.magic_value);
            syntra_uart_puts(" version=");
            syntra_uart_put_hex(snapshot.version);
            syntra_uart_puts(" device=");
            syntra_uart_put_hex(snapshot.device_id);
            syntra_uart_puts(" vendor=");
            syntra_uart_put_hex(snapshot.vendor_id);
            syntra_uart_puts("\\n");

            status = syntra_virtio_mmio_decode_identity(
                &snapshot, &identity);
            if (status == SYNTRA_STATUS_OK)
            {
                if (identity.function == SYNTRA_VIRTIO_MMIO_FUNCTION_INPUT)
                    syntra_uart_puts("VirtIO-MMIO function: INPUT\\n");
                else if (identity.function == SYNTRA_VIRTIO_MMIO_FUNCTION_OTHER)
                    syntra_uart_puts("VirtIO-MMIO function: OTHER\\n");
                else
                    syntra_uart_puts("VirtIO-MMIO function: EMPTY SLOT\\n");
            }
            else
            {
                syntra_uart_puts("VirtIO-MMIO identity decode: UNAVAILABLE\\n");
            }
        }
        else
        {
            /* Optional input transport probe must not prevent the OS booting. */
            syntra_uart_puts("VirtIO-MMIO mapped identity read: UNAVAILABLE\\n");
        }
    }

    status = syntra_arm64_run_el0_hello_test();
    if (status != SYNTRA_STATUS_OK)
    {
        syntra_uart_puts("SyntraOS boot failed: first EL0 process\n");
        return status;
    }

    if (syntra_fw_cfg_base != 0U)
    {
        status = syntra_ramfb_init(syntra_fw_cfg_base, syntra_fw_cfg_size);
        if (status == SYNTRA_STATUS_OK)
        {
            syntra_uart_puts("QEMU ramfb: READY 640x480 XRGB8888\n");
        }
        else if (status == SYNTRA_STATUS_NOT_IMPLEMENTED)
        {
            syntra_uart_puts("QEMU ramfb: NOT PRESENT; UART boot retained\n");
        }
        else
        {
            /* Optional display failure must never invalidate the working kernel. */
            syntra_uart_puts("QEMU ramfb: UNAVAILABLE; UART boot retained\n");
        }
    }

    syntra_uart_puts("SyntraOS boot OK\n");
    return SYNTRA_STATUS_OK;
}
