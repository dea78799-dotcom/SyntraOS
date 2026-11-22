#include <stdint.h>
#include <stddef.h>

#include "../../core/status.h"
#include "fdt.h"
#include "memory_map.h"
#include "../../../memory/memory_region.h"
#include "../../../memory/page_allocator.h"
#include "../../../process/process.h"
#include "../exception.h"
#include "../context.h"
#include "../gic.h"
#include "../mmu.h"
#include "../page_table_builder.h"
#include "../address_space.h"
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
static syntra_virtio_input_queue_t syntra_virtio_input_queue;
static syntra_input_state_t syntra_virtio_input_state;
static int syntra_virtio_input_irq_registered;
static int syntra_virtio_input_key_a_test_reported;
static int syntra_virtio_input_irq_test_reported;

static void syntra_uart_puts(const char *message);
static void syntra_uart_put_hex(uint64_t value);

static void syntra_virtio_input_irq_handler(
    syntra_interrupt_id_t interrupt_id,
    void *context)
{
    syntra_virtio_input_queue_t *queue =
        (syntra_virtio_input_queue_t *)context;
    syntra_status_t status;

    if (queue == NULL ||
        interrupt_id != syntra_virtio_mmio_device.interrupt_id)
        return;

    status = syntra_virtio_mmio_handle_input_irq(
        &syntra_virtio_mmio_device,
        (volatile void *)(uintptr_t)syntra_virtio_mmio_device.base,
        syntra_virtio_mmio_device.region_size,
        queue,
        &syntra_virtio_input_state);
    if (status != SYNTRA_STATUS_OK)
    {
        queue->irq_error_count++;
        return;
    }

    if (!syntra_virtio_input_irq_test_reported)
    {
        syntra_virtio_input_irq_test_reported = 1;
        syntra_uart_puts("VirtIO Input IRQ: HANDLED events=");
        syntra_uart_put_hex(queue->event_count);
        syntra_uart_puts(" keys=");
        syntra_uart_put_hex(queue->key_event_count);
        syntra_uart_puts("\n");
    }

    /* QEMU test marker: report once when KEY_A is observed pressed. */
    if (!syntra_virtio_input_key_a_test_reported &&
        queue->key_a_press_count != 0U)
    {
        syntra_virtio_input_key_a_test_reported = 1;
        syntra_uart_puts("VirtIO Input test: KEY_A DOWN\n");
    }
}
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
extern void syntra_arm64_user_hello(void *argument);
extern void syntra_arm64_user_write_private_data(void *argument);
extern void syntra_arm64_user_write_code_fault(void *argument);
extern void syntra_arm64_el0_thread_exit_trampoline(void);
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
        if (syntra_virtio_mmio_device.interrupt_valid)
        {
            syntra_uart_puts("FDT VirtIO-MMIO interrupt: id=");
            syntra_uart_put_hex(syntra_virtio_mmio_device.interrupt_id);
            syntra_uart_puts(" flags=");
            syntra_uart_put_hex(syntra_virtio_mmio_device.interrupt_flags);
            syntra_uart_puts("\n");
        }
        else
        {
            syntra_uart_puts("FDT VirtIO-MMIO interrupt: UNAVAILABLE\n");
        }
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

    status = syntra_page_allocator_validate();
    if (status != SYNTRA_STATUS_OK)
    {
        syntra_uart_puts("Physical page allocator initial audit failed\n");
        return status;
    }

    syntra_uart_puts("Physical page allocator: PASS audit=PASS free=");
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
    status = syntra_page_alloc_owned(
        &page, SYNTRA_PAGE_OWNER_PAGE_TABLE);
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

#define SYNTRA_PROCESS_AS_TEST_ID 42U
#define SYNTRA_PROCESS_AS_TEST_ID_2 43U
#define SYNTRA_PROCESS_AS_TEST_THREAD_ID 4U
#define SYNTRA_PROCESS_AS_TEST_THREAD_ID_2 5U

static _Alignas(16) uint8_t process_as_test_stack[4096];
static _Alignas(16) uint8_t process_as_test_stack_2[4096];
static syntra_arm64_address_space_t process_as_test_space;
static syntra_arm64_address_space_t process_as_test_space_2;
static volatile unsigned int process_as_test_result;
static volatile unsigned int process_as_test_failure;
static uint64_t process_as_test_expected_root;
static uint64_t process_as_test_expected_root_2;

static void syntra_process_as_test_worker(void *argument)
{
    uint64_t ttbr0 = 0ULL;
    syntra_thread_t *current = scheduler_get_current();
    syntra_arm64_address_space_t *thread_space;
    uint64_t expected_root = 0ULL;
    unsigned int result_bit = 0U;

    (void)argument;
    __asm__ volatile("mrs %0, ttbr0_el1" : "=r"(ttbr0));
    thread_space = current == NULL ? NULL :
        process_get_address_space_for_thread(current->thread_id);

    if (current != NULL &&
        current->thread_id == SYNTRA_PROCESS_AS_TEST_THREAD_ID)
    {
        expected_root = process_as_test_expected_root;
        result_bit = 1U;
        if (thread_space != &process_as_test_space)
            process_as_test_failure = 1U;
    }
    else if (current != NULL &&
        current->thread_id == SYNTRA_PROCESS_AS_TEST_THREAD_ID_2)
    {
        expected_root = process_as_test_expected_root_2;
        result_bit = 2U;
        if (thread_space != &process_as_test_space_2)
            process_as_test_failure = 1U;
    }
    else
    {
        process_as_test_failure = 1U;
    }

    if (result_bit == 0U ||
        (ttbr0 & SYNTRA_ARM64_MMU_PA_MASK) != expected_root)
        process_as_test_failure = 1U;
    else
        process_as_test_result |= result_bit;
}

static syntra_status_t syntra_arm64_process_address_space_scheduler_test(void)
{
    syntra_status_t status;
    syntra_thread_t *thread, *current;
    syntra_arm64_exception_context_t *initial_context = NULL;
    syntra_arm64_mmu_registers_t registers;
    uint64_t start, timeout_ticks;
    int process_1_created = 0;
    int process_2_created = 0;

    current = scheduler_get_current();
    if (current == NULL ||
        current->thread_id != SYNTRA_SCHEDULER_TEST_BOOT_ID)
        return SYNTRA_STATUS_ERROR;

    process_as_test_result = 0U;
    process_as_test_failure = 0U;

    status = syntra_arm64_address_space_create(
        &process_as_test_space, SYNTRA_PROCESS_AS_TEST_ID,
        syntra_boot_page_table_resolve, NULL);
    if (status != SYNTRA_STATUS_OK) return status;

    status = syntra_arm64_address_space_inherit_kernel_mappings(
        &process_as_test_space,
        syntra_active_page_table.root_physical_address);
    if (status != SYNTRA_STATUS_OK) goto fail_space_1;

    status = syntra_arm64_address_space_create(
        &process_as_test_space_2, SYNTRA_PROCESS_AS_TEST_ID_2,
        syntra_boot_page_table_resolve, NULL);
    if (status != SYNTRA_STATUS_OK) goto fail_space_1;

    status = syntra_arm64_address_space_inherit_kernel_mappings(
        &process_as_test_space_2,
        syntra_active_page_table.root_physical_address);
    if (status != SYNTRA_STATUS_OK) goto fail_space_2;

    process_as_test_expected_root =
        process_as_test_space.page_table.root_physical_address;
    process_as_test_expected_root_2 =
        process_as_test_space_2.page_table.root_physical_address;

    if (process_as_test_expected_root == process_as_test_expected_root_2)
    {
        status = SYNTRA_STATUS_ERROR;
        goto fail_space_2;
    }

    status = process_create(SYNTRA_PROCESS_AS_TEST_ID, 1U);
    if (status != SYNTRA_STATUS_OK) goto fail_space_2;
    process_1_created = 1;

    status = process_create(SYNTRA_PROCESS_AS_TEST_ID_2, 1U);
    if (status != SYNTRA_STATUS_OK) goto fail_processes;
    process_2_created = 1;

    status = process_attach_address_space(
        SYNTRA_PROCESS_AS_TEST_ID, &process_as_test_space);
    if (status != SYNTRA_STATUS_OK) goto fail_processes;

    status = process_attach_address_space(
        SYNTRA_PROCESS_AS_TEST_ID_2, &process_as_test_space_2);
    if (status != SYNTRA_STATUS_OK) goto fail_processes;

    status = scheduler_create_thread(
        SYNTRA_PROCESS_AS_TEST_THREAD_ID, 1U,
        syntra_process_as_test_worker, NULL,
        process_as_test_stack, sizeof(process_as_test_stack));
    if (status != SYNTRA_STATUS_OK) goto fail_processes;

    status = scheduler_create_thread(
        SYNTRA_PROCESS_AS_TEST_THREAD_ID_2, 1U,
        syntra_process_as_test_worker, NULL,
        process_as_test_stack_2, sizeof(process_as_test_stack_2));
    if (status != SYNTRA_STATUS_OK) goto fail_processes;

    thread = scheduler_get_thread(SYNTRA_PROCESS_AS_TEST_THREAD_ID);
    if (thread == NULL)
    {
        status = SYNTRA_STATUS_ERROR;
        goto fail_processes;
    }
    status = syntra_arm64_context_initialize(
        process_as_test_stack, sizeof(process_as_test_stack),
        syntra_process_as_test_worker, NULL, &initial_context);
    if (status != SYNTRA_STATUS_OK) goto fail_processes;
    thread->cpu_context = initial_context;

    thread = scheduler_get_thread(SYNTRA_PROCESS_AS_TEST_THREAD_ID_2);
    if (thread == NULL)
    {
        status = SYNTRA_STATUS_ERROR;
        goto fail_processes;
    }
    initial_context = NULL;
    status = syntra_arm64_context_initialize(
        process_as_test_stack_2, sizeof(process_as_test_stack_2),
        syntra_process_as_test_worker, NULL, &initial_context);
    if (status != SYNTRA_STATUS_OK) goto fail_processes;
    thread->cpu_context = initial_context;

    status = process_attach_thread(
        SYNTRA_PROCESS_AS_TEST_ID, SYNTRA_PROCESS_AS_TEST_THREAD_ID);
    if (status != SYNTRA_STATUS_OK) goto fail_processes;
    status = process_attach_thread(
        SYNTRA_PROCESS_AS_TEST_ID_2, SYNTRA_PROCESS_AS_TEST_THREAD_ID_2);
    if (status != SYNTRA_STATUS_OK) goto fail_processes;

    status = interrupt_enable(30U);
    if (status != SYNTRA_STATUS_OK) goto fail_processes;
    status = syntra_arm64_timer_enable();
    if (status != SYNTRA_STATUS_OK)
    {
        (void)interrupt_disable(30U);
        goto fail_processes;
    }

    start = syntra_arm64_timer_read_counter();
    timeout_ticks = syntra_arm64_timer_read_frequency() * 3U;
    __asm__ volatile("dsb sy" ::: "memory");
    __asm__ volatile("msr daifclr, #2" ::: "memory");
    __asm__ volatile("isb" ::: "memory");

    while (process_as_test_result != 3U ||
        scheduler_get_thread(SYNTRA_PROCESS_AS_TEST_THREAD_ID)->state !=
            THREAD_TERMINATED ||
        scheduler_get_thread(SYNTRA_PROCESS_AS_TEST_THREAD_ID_2)->state !=
            THREAD_TERMINATED)
    {
        if (process_as_test_failure != 0U ||
            (syntra_arm64_timer_read_counter() - start) > timeout_ticks)
        {
            status = SYNTRA_STATUS_ERROR;
            break;
        }
        __asm__ volatile("wfi" ::: "memory");
    }

    __asm__ volatile("msr daifset, #2" ::: "memory");
    __asm__ volatile("isb" ::: "memory");
    (void)syntra_arm64_timer_disable();
    (void)interrupt_disable(30U);

    if (status != SYNTRA_STATUS_OK ||
        process_as_test_result != 3U ||
        process_as_test_failure != 0U)
        goto fail_processes;

    current = scheduler_get_current();
    if (current == NULL ||
        current->thread_id != SYNTRA_SCHEDULER_TEST_BOOT_ID)
    {
        status = SYNTRA_STATUS_ERROR;
        goto fail_processes;
    }

    status = syntra_arm64_mmu_read_registers(&registers);
    if (status != SYNTRA_STATUS_OK ||
        (registers.ttbr0_el1 & SYNTRA_ARM64_MMU_PA_MASK) !=
            syntra_active_page_table.root_physical_address)
    {
        status = SYNTRA_STATUS_ERROR;
        goto fail_processes;
    }

    status = process_terminate(SYNTRA_PROCESS_AS_TEST_ID);
    if (status != SYNTRA_STATUS_OK) goto fail_processes;
    process_1_created = 0;
    status = process_terminate(SYNTRA_PROCESS_AS_TEST_ID_2);
    if (status != SYNTRA_STATUS_OK) return status;
    process_2_created = 0;

    if (process_validate() != SYNTRA_STATUS_OK)
        return SYNTRA_STATUS_ERROR;

    syntra_uart_puts(
        "Process scheduler TTBR0 switching: PASS (process A <-> process B -> kernel root)\n");
    return SYNTRA_STATUS_OK;

fail_processes:
    (void)scheduler_set_thread_state(
        SYNTRA_PROCESS_AS_TEST_THREAD_ID, THREAD_TERMINATED);
    (void)scheduler_set_thread_state(
        SYNTRA_PROCESS_AS_TEST_THREAD_ID_2, THREAD_TERMINATED);
    if (process_1_created)
    {
        (void)process_refresh_state(SYNTRA_PROCESS_AS_TEST_ID);
        (void)process_terminate(SYNTRA_PROCESS_AS_TEST_ID);
    }
    if (process_2_created)
    {
        (void)process_refresh_state(SYNTRA_PROCESS_AS_TEST_ID_2);
        (void)process_terminate(SYNTRA_PROCESS_AS_TEST_ID_2);
    }
    return status;

fail_space_2:
    if (process_as_test_space_2.initialized)
        (void)syntra_arm64_address_space_destroy(&process_as_test_space_2);
fail_space_1:
    if (process_as_test_space.initialized)
        (void)syntra_arm64_address_space_destroy(&process_as_test_space);
    return status;
}

#define SYNTRA_PROCESS_EL0_TEST_ID 44U
#define SYNTRA_PROCESS_EL0_TEST_THREAD_ID 6U

static _Alignas(4096) uint8_t process_el0_test_kernel_stack[4096];
static syntra_arm64_address_space_t process_el0_test_space;
static uintptr_t process_el0_test_a_data_physical;

static syntra_status_t syntra_arm64_process_el0_scheduler_test(void)
{
    const uint64_t code_va = SYNTRA_ARM64_USER_VA_BASE;
    const uint64_t data_va = SYNTRA_ARM64_USER_VA_BASE + 0x1000ULL;
    const uint64_t stack_va = SYNTRA_ARM64_USER_VA_BASE + 0x2000ULL;
    const uint64_t data_sentinel = 0x53594E5452410001ULL;
    uintptr_t code_page = 0U, data_page = 0U, stack_page = 0U;
    uintptr_t data_page_physical = 0U;
    uintptr_t text_start = (uintptr_t)__user_text_start;
    uintptr_t text_size = (uintptr_t)__user_text_end - text_start;
    uintptr_t entry_address = (uintptr_t)syntra_arm64_user_write_private_data;
    uintptr_t entry_offset;
    uint64_t exit_count_before;
    uint64_t start_counter, timeout_ticks;
    syntra_status_t status;
    syntra_thread_t *thread, *current;
    syntra_arm64_exception_context_t *context = NULL;
    syntra_arm64_mmu_registers_t registers;
    syntra_arm64_mmu_translation_t data_translation;
    union
    {
        uint64_t address;
        void (*function)(void *);
    } entry;

    current = scheduler_get_current();
    if (current == NULL ||
        current->thread_id != SYNTRA_SCHEDULER_TEST_BOOT_ID ||
        text_size != SYNTRA_ARM64_MMU_PAGE_SIZE ||
        entry_address < text_start ||
        entry_address >= text_start + text_size)
        return SYNTRA_STATUS_ERROR;

    status = syntra_page_alloc_owned(&code_page, SYNTRA_PAGE_OWNER_PROCESS);
    if (status != SYNTRA_STATUS_OK) return status;
    status = syntra_page_alloc_owned(&stack_page, SYNTRA_PAGE_OWNER_PROCESS);
    if (status != SYNTRA_STATUS_OK)
    {
        (void)syntra_page_free_owned(code_page, SYNTRA_PAGE_OWNER_PROCESS);
        return status;
    }
    status = syntra_page_alloc_owned(&data_page, SYNTRA_PAGE_OWNER_PROCESS);
    if (status != SYNTRA_STATUS_OK)
    {
        (void)syntra_page_free_owned(code_page, SYNTRA_PAGE_OWNER_PROCESS);
        (void)syntra_page_free_owned(stack_page, SYNTRA_PAGE_OWNER_PROCESS);
        return status;
    }
    data_page_physical = data_page;
    *(volatile uint64_t *)(uintptr_t)data_page = data_sentinel;

    for (size_t i = 0U; i < SYNTRA_ARM64_MMU_PAGE_SIZE; ++i)
        ((uint8_t *)(uintptr_t)code_page)[i] =
            ((const uint8_t *)text_start)[i];

    entry_offset = entry_address - text_start;
    entry.address = code_va + (uint64_t)entry_offset;

    status = syntra_arm64_address_space_create(
        &process_el0_test_space, SYNTRA_PROCESS_EL0_TEST_ID,
        syntra_boot_page_table_resolve, NULL);
    if (status != SYNTRA_STATUS_OK) goto fail_pages;

    status = syntra_arm64_address_space_inherit_kernel_mappings(
        &process_el0_test_space,
        syntra_active_page_table.root_physical_address);
    if (status != SYNTRA_STATUS_OK) goto fail_space;

    status = syntra_arm64_address_space_map_user_page(
        &process_el0_test_space, code_va, (uint64_t)code_page,
        SYNTRA_ARM64_MMU_ACCESS_READ_ONLY, 1);
    if (status != SYNTRA_STATUS_OK) goto fail_space;
    code_page = 0U;

    status = syntra_arm64_address_space_map_user_page(
        &process_el0_test_space, data_va, (uint64_t)data_page,
        SYNTRA_ARM64_MMU_ACCESS_READ_WRITE, 0);
    if (status != SYNTRA_STATUS_OK) goto fail_space;
    data_page = 0U;

    status = syntra_arm64_address_space_map_user_page(
        &process_el0_test_space, stack_va, (uint64_t)stack_page,
        SYNTRA_ARM64_MMU_ACCESS_READ_WRITE, 0);
    if (status != SYNTRA_STATUS_OK) goto fail_space;
    stack_page = 0U;

    status = syntra_arm64_address_space_validate(&process_el0_test_space);
    if (status != SYNTRA_STATUS_OK) goto fail_space;

    status = syntra_arm64_address_space_translate(
        &process_el0_test_space, data_va, &data_translation);
    if (status != SYNTRA_STATUS_OK ||
        data_translation.physical_address != (uint64_t)data_page_physical ||
        data_translation.user_read == 0U ||
        data_translation.user_write == 0U ||
        data_translation.user_execute != 0U)
    {
        status = SYNTRA_STATUS_ERROR;
        goto fail_space;
    }

    status = process_create(SYNTRA_PROCESS_EL0_TEST_ID, 1U);
    if (status != SYNTRA_STATUS_OK) goto fail_space;
    status = process_attach_address_space(
        SYNTRA_PROCESS_EL0_TEST_ID, &process_el0_test_space);
    if (status != SYNTRA_STATUS_OK) goto fail_process;

    status = syntra_arm64_user_context_initialize(
        process_el0_test_kernel_stack,
        sizeof(process_el0_test_kernel_stack),
        (void *)(uintptr_t)stack_va, SYNTRA_ARM64_MMU_PAGE_SIZE,
        entry.function, syntra_arm64_el0_thread_exit_trampoline,
        (void *)(uintptr_t)data_va, &context);
    if (status != SYNTRA_STATUS_OK) goto fail_process;

    status = scheduler_create_user_thread(
        SYNTRA_PROCESS_EL0_TEST_THREAD_ID, 1U,
        entry.function, (void *)(uintptr_t)data_va,
        process_el0_test_kernel_stack,
        sizeof(process_el0_test_kernel_stack),
        (void *)(uintptr_t)stack_va, SYNTRA_ARM64_MMU_PAGE_SIZE,
        context);
    if (status != SYNTRA_STATUS_OK) goto fail_process;

    status = process_attach_thread(
        SYNTRA_PROCESS_EL0_TEST_ID, SYNTRA_PROCESS_EL0_TEST_THREAD_ID);
    if (status != SYNTRA_STATUS_OK) goto fail_process;

    thread = scheduler_get_thread(SYNTRA_PROCESS_EL0_TEST_THREAD_ID);
    if (thread == NULL) return SYNTRA_STATUS_ERROR;

    exit_count_before = syntra_arm64_exception_user_thread_exit_count();
    status = interrupt_enable(30U);
    if (status != SYNTRA_STATUS_OK) goto fail_process;
    status = syntra_arm64_timer_enable();
    if (status != SYNTRA_STATUS_OK)
    {
        (void)interrupt_disable(30U);
        goto fail_process;
    }

    start_counter = syntra_arm64_timer_read_counter();
    timeout_ticks = syntra_arm64_timer_read_frequency() * 2U;
    __asm__ volatile("dsb sy" ::: "memory");
    __asm__ volatile("msr daifclr, #2" ::: "memory");
    __asm__ volatile("isb" ::: "memory");

    while (thread->state != THREAD_TERMINATED)
    {
        if ((syntra_arm64_timer_read_counter() - start_counter) > timeout_ticks)
        {
            status = SYNTRA_STATUS_ERROR;
            break;
        }
        __asm__ volatile("wfi" ::: "memory");
    }

    __asm__ volatile("msr daifset, #2" ::: "memory");
    __asm__ volatile("isb" ::: "memory");
    (void)syntra_arm64_timer_disable();
    (void)interrupt_disable(30U);

    if (status != SYNTRA_STATUS_OK ||
        thread->state != THREAD_TERMINATED ||
        syntra_arm64_exception_user_thread_exit_count() !=
            exit_count_before + 1U)
        goto fail_process;

    current = scheduler_get_current();
    if (current == NULL ||
        current->thread_id != SYNTRA_SCHEDULER_TEST_BOOT_ID)
    {
        status = SYNTRA_STATUS_ERROR;
        goto fail_process;
    }

    status = syntra_arm64_mmu_read_registers(&registers);
    if (status != SYNTRA_STATUS_OK ||
        (registers.ttbr0_el1 & SYNTRA_ARM64_MMU_PA_MASK) !=
            syntra_active_page_table.root_physical_address)
    {
        status = SYNTRA_STATUS_ERROR;
        goto fail_process;
    }

    if (*(volatile uint64_t *)(uintptr_t)data_page_physical !=
        data_sentinel + 1ULL)
    {
        status = SYNTRA_STATUS_ERROR;
        goto fail_process;
    }

    process_el0_test_a_data_physical = data_page_physical;
    syntra_uart_puts("Process EL0 A: PASS (private RW page written from EL0)\\n");
    return SYNTRA_STATUS_OK;

fail_process:
    thread = scheduler_get_thread(SYNTRA_PROCESS_EL0_TEST_THREAD_ID);
    if (thread != NULL && thread->state != THREAD_TERMINATED)
        (void)scheduler_set_thread_state(
            SYNTRA_PROCESS_EL0_TEST_THREAD_ID, THREAD_TERMINATED);
    (void)process_refresh_state(SYNTRA_PROCESS_EL0_TEST_ID);
    (void)process_terminate(SYNTRA_PROCESS_EL0_TEST_ID);
    return status;

fail_space:
    if (process_el0_test_space.initialized)
        (void)syntra_arm64_address_space_destroy(&process_el0_test_space);
fail_pages:
    if (code_page != 0U)
        (void)syntra_page_free_owned(code_page, SYNTRA_PAGE_OWNER_PROCESS);
    if (stack_page != 0U)
        (void)syntra_page_free_owned(stack_page, SYNTRA_PAGE_OWNER_PROCESS);
    if (data_page != 0U)
        (void)syntra_page_free_owned(data_page, SYNTRA_PAGE_OWNER_PROCESS);
    return status;
}

#define SYNTRA_PROCESS_EL0_TEST_B_ID 46U
#define SYNTRA_PROCESS_EL0_TEST_B_THREAD_ID 8U

static _Alignas(4096) uint8_t process_el0_test_b_kernel_stack[4096];
static syntra_arm64_address_space_t process_el0_test_b_space;

#define SYNTRA_PROCESS_EL0_TEST_B_ID 46U
#define SYNTRA_PROCESS_EL0_TEST_B_THREAD_ID 8U

static _Alignas(4096) uint8_t process_el0_test_b_kernel_stack[4096];
static syntra_arm64_address_space_t process_el0_test_b_space;
static uintptr_t process_el0_test_a_data_physical;

static syntra_status_t syntra_arm64_process_el0_isolation_b_test(void)
{
    const uint64_t code_va = SYNTRA_ARM64_USER_VA_BASE;
    const uint64_t data_va = SYNTRA_ARM64_USER_VA_BASE + 0x1000ULL;
    const uint64_t stack_va = SYNTRA_ARM64_USER_VA_BASE + 0x2000ULL;
    const uint64_t data_sentinel = 0x53594E5452410001ULL;
    uintptr_t code_page = 0U, data_page = 0U, stack_page = 0U;
    uintptr_t data_page_physical = 0U;
    uintptr_t text_start = (uintptr_t)__user_text_start;
    uintptr_t text_size = (uintptr_t)__user_text_end - text_start;
    uintptr_t entry_address = (uintptr_t)syntra_arm64_user_write_private_data;
    uintptr_t entry_offset;
    uint64_t exit_count_before;
    uint64_t start_counter, timeout_ticks;
    syntra_status_t status;
    syntra_thread_t *thread, *current;
    syntra_arm64_exception_context_t *context = NULL;
    syntra_arm64_mmu_registers_t registers;
    syntra_arm64_mmu_translation_t data_translation;
    union
    {
        uint64_t address;
        void (*function)(void *);
    } entry;

    current = scheduler_get_current();
    if (current == NULL ||
        current->thread_id != SYNTRA_SCHEDULER_TEST_BOOT_ID ||
        text_size != SYNTRA_ARM64_MMU_PAGE_SIZE ||
        entry_address < text_start ||
        entry_address >= text_start + text_size)
        return SYNTRA_STATUS_ERROR;

    status = syntra_page_alloc_owned(&code_page, SYNTRA_PAGE_OWNER_PROCESS);
    if (status != SYNTRA_STATUS_OK) return status;
    status = syntra_page_alloc_owned(&stack_page, SYNTRA_PAGE_OWNER_PROCESS);
    if (status != SYNTRA_STATUS_OK)
    {
        (void)syntra_page_free_owned(code_page, SYNTRA_PAGE_OWNER_PROCESS);
        return status;
    }
    status = syntra_page_alloc_owned(&data_page, SYNTRA_PAGE_OWNER_PROCESS);
    if (status != SYNTRA_STATUS_OK)
    {
        (void)syntra_page_free_owned(code_page, SYNTRA_PAGE_OWNER_PROCESS);
        (void)syntra_page_free_owned(stack_page, SYNTRA_PAGE_OWNER_PROCESS);
        return status;
    }
    data_page_physical = data_page;
    *(volatile uint64_t *)(uintptr_t)data_page = data_sentinel;

    for (size_t i = 0U; i < SYNTRA_ARM64_MMU_PAGE_SIZE; ++i)
        ((uint8_t *)(uintptr_t)code_page)[i] =
            ((const uint8_t *)text_start)[i];

    entry_offset = entry_address - text_start;
    entry.address = code_va + (uint64_t)entry_offset;

    status = syntra_arm64_address_space_create(
        &process_el0_test_b_space, SYNTRA_PROCESS_EL0_TEST_B_ID,
        syntra_boot_page_table_resolve, NULL);
    if (status != SYNTRA_STATUS_OK) goto fail_pages;

    status = syntra_arm64_address_space_inherit_kernel_mappings(
        &process_el0_test_b_space,
        syntra_active_page_table.root_physical_address);
    if (status != SYNTRA_STATUS_OK) goto fail_space;

    status = syntra_arm64_address_space_map_user_page(
        &process_el0_test_b_space, code_va, (uint64_t)code_page,
        SYNTRA_ARM64_MMU_ACCESS_READ_ONLY, 1);
    if (status != SYNTRA_STATUS_OK) goto fail_space;
    code_page = 0U;

    status = syntra_arm64_address_space_map_user_page(
        &process_el0_test_b_space, data_va, (uint64_t)data_page,
        SYNTRA_ARM64_MMU_ACCESS_READ_WRITE, 0);
    if (status != SYNTRA_STATUS_OK) goto fail_space;
    data_page = 0U;

    status = syntra_arm64_address_space_map_user_page(
        &process_el0_test_b_space, stack_va, (uint64_t)stack_page,
        SYNTRA_ARM64_MMU_ACCESS_READ_WRITE, 0);
    if (status != SYNTRA_STATUS_OK) goto fail_space;
    stack_page = 0U;

    status = syntra_arm64_address_space_validate(&process_el0_test_b_space);
    if (status != SYNTRA_STATUS_OK) goto fail_space;

    status = syntra_arm64_address_space_translate(
        &process_el0_test_b_space, data_va, &data_translation);
    if (status != SYNTRA_STATUS_OK ||
        data_translation.physical_address != (uint64_t)data_page_physical ||
        data_translation.user_read == 0U ||
        data_translation.user_write == 0U ||
        data_translation.user_execute != 0U)
    {
        status = SYNTRA_STATUS_ERROR;
        goto fail_space;
    }

    status = process_create(SYNTRA_PROCESS_EL0_TEST_B_ID, 1U);
    if (status != SYNTRA_STATUS_OK) goto fail_space;
    status = process_attach_address_space(
        SYNTRA_PROCESS_EL0_TEST_B_ID, &process_el0_test_b_space);
    if (status != SYNTRA_STATUS_OK) goto fail_process;

    status = syntra_arm64_user_context_initialize(
        process_el0_test_b_kernel_stack,
        sizeof(process_el0_test_b_kernel_stack),
        (void *)(uintptr_t)stack_va, SYNTRA_ARM64_MMU_PAGE_SIZE,
        entry.function, syntra_arm64_el0_thread_exit_trampoline,
        (void *)(uintptr_t)data_va, &context);
    if (status != SYNTRA_STATUS_OK) goto fail_process;

    status = scheduler_create_user_thread(
        SYNTRA_PROCESS_EL0_TEST_B_THREAD_ID, 1U,
        entry.function, (void *)(uintptr_t)data_va,
        process_el0_test_b_kernel_stack,
        sizeof(process_el0_test_b_kernel_stack),
        (void *)(uintptr_t)stack_va, SYNTRA_ARM64_MMU_PAGE_SIZE,
        context);
    if (status != SYNTRA_STATUS_OK) goto fail_process;

    status = process_attach_thread(
        SYNTRA_PROCESS_EL0_TEST_B_ID, SYNTRA_PROCESS_EL0_TEST_B_THREAD_ID);
    if (status != SYNTRA_STATUS_OK) goto fail_process;

    thread = scheduler_get_thread(SYNTRA_PROCESS_EL0_TEST_B_THREAD_ID);
    if (thread == NULL) return SYNTRA_STATUS_ERROR;

    exit_count_before = syntra_arm64_exception_user_thread_exit_count();
    status = interrupt_enable(30U);
    if (status != SYNTRA_STATUS_OK) goto fail_process;
    status = syntra_arm64_timer_enable();
    if (status != SYNTRA_STATUS_OK)
    {
        (void)interrupt_disable(30U);
        goto fail_process;
    }

    start_counter = syntra_arm64_timer_read_counter();
    timeout_ticks = syntra_arm64_timer_read_frequency() * 2U;
    __asm__ volatile("dsb sy" ::: "memory");
    __asm__ volatile("msr daifclr, #2" ::: "memory");
    __asm__ volatile("isb" ::: "memory");

    while (thread->state != THREAD_TERMINATED)
    {
        if ((syntra_arm64_timer_read_counter() - start_counter) > timeout_ticks)
        {
            status = SYNTRA_STATUS_ERROR;
            break;
        }
        __asm__ volatile("wfi" ::: "memory");
    }

    __asm__ volatile("msr daifset, #2" ::: "memory");
    __asm__ volatile("isb" ::: "memory");
    (void)syntra_arm64_timer_disable();
    (void)interrupt_disable(30U);

    if (status != SYNTRA_STATUS_OK ||
        thread->state != THREAD_TERMINATED ||
        syntra_arm64_exception_user_thread_exit_count() !=
            exit_count_before + 1U)
        goto fail_process;

    current = scheduler_get_current();
    if (current == NULL ||
        current->thread_id != SYNTRA_SCHEDULER_TEST_BOOT_ID)
    {
        status = SYNTRA_STATUS_ERROR;
        goto fail_process;
    }

    status = syntra_arm64_mmu_read_registers(&registers);
    if (status != SYNTRA_STATUS_OK ||
        (registers.ttbr0_el1 & SYNTRA_ARM64_MMU_PA_MASK) !=
            syntra_active_page_table.root_physical_address)
    {
        status = SYNTRA_STATUS_ERROR;
        goto fail_process;
    }

    if (*(volatile uint64_t *)(uintptr_t)data_page_physical !=
        data_sentinel + 1ULL)
    {
        status = SYNTRA_STATUS_ERROR;
        goto fail_process;
    }

    if (data_page_physical == process_el0_test_a_data_physical ||
        *(volatile uint64_t *)(uintptr_t)process_el0_test_a_data_physical !=
            data_sentinel + 1ULL ||
        *(volatile uint64_t *)(uintptr_t)data_page_physical !=
            data_sentinel + 1ULL)
    {
        status = SYNTRA_STATUS_ERROR;
        goto fail_process;
    }

    status = process_terminate(SYNTRA_PROCESS_EL0_TEST_ID);
    if (status != SYNTRA_STATUS_OK) goto fail_process;
    status = process_terminate(SYNTRA_PROCESS_EL0_TEST_B_ID);
    if (status != SYNTRA_STATUS_OK) return status;
    if (process_validate() != SYNTRA_STATUS_OK)
        return SYNTRA_STATUS_ERROR;

    syntra_uart_puts("Process EL0 isolation: PASS (same VA, distinct physical pages, independent data)\\n");
    return SYNTRA_STATUS_OK;

fail_process:
    thread = scheduler_get_thread(SYNTRA_PROCESS_EL0_TEST_B_THREAD_ID);
    if (thread != NULL && thread->state != THREAD_TERMINATED)
        (void)scheduler_set_thread_state(
            SYNTRA_PROCESS_EL0_TEST_B_THREAD_ID, THREAD_TERMINATED);
    (void)process_refresh_state(SYNTRA_PROCESS_EL0_TEST_B_ID);
    (void)process_terminate(SYNTRA_PROCESS_EL0_TEST_B_ID);
    return status;

fail_space:
    if (process_el0_test_b_space.initialized)
        (void)syntra_arm64_address_space_destroy(&process_el0_test_b_space);
fail_pages:
    if (code_page != 0U)
        (void)syntra_page_free_owned(code_page, SYNTRA_PAGE_OWNER_PROCESS);
    if (stack_page != 0U)
        (void)syntra_page_free_owned(stack_page, SYNTRA_PAGE_OWNER_PROCESS);
    if (data_page != 0U)
        (void)syntra_page_free_owned(data_page, SYNTRA_PAGE_OWNER_PROCESS);
    return status;
}

#define SYNTRA_PROCESS_EL0_FAULT_ID 45U
#define SYNTRA_PROCESS_EL0_FAULT_THREAD_ID 7U

static _Alignas(4096) uint8_t process_el0_fault_kernel_stack[4096];
static syntra_arm64_address_space_t process_el0_fault_space;

static syntra_status_t syntra_arm64_process_el0_write_fault_test(void)
{
    /* Use a distinct VA range: terminated scheduler slots retain stack metadata. */
    const uint64_t code_va = SYNTRA_ARM64_USER_VA_BASE + 0x100000ULL;
    const uint64_t stack_va = code_va + 0x2000ULL;
    uintptr_t code_page = 0U, stack_page = 0U;
    uintptr_t text_start = (uintptr_t)__user_text_start;
    uintptr_t text_size = (uintptr_t)__user_text_end - text_start;
    uintptr_t entry_address = (uintptr_t)syntra_arm64_user_write_code_fault;
    uintptr_t entry_offset;
    uint64_t fault_count_before, start_counter, timeout_ticks;
    syntra_status_t status;
    syntra_thread_t *thread, *current;
    syntra_arm64_exception_context_t *context = NULL;
    syntra_arm64_mmu_registers_t registers;
    union
    {
        uint64_t address;
        void (*function)(void *);
    } entry;

    current = scheduler_get_current();
    if (current == NULL ||
        current->thread_id != SYNTRA_SCHEDULER_TEST_BOOT_ID ||
        text_size != SYNTRA_ARM64_MMU_PAGE_SIZE ||
        entry_address < text_start ||
        entry_address >= text_start + text_size)
        return SYNTRA_STATUS_ERROR;

    status = syntra_page_alloc_owned(&code_page, SYNTRA_PAGE_OWNER_PROCESS);
    if (status != SYNTRA_STATUS_OK) return status;
    status = syntra_page_alloc_owned(&stack_page, SYNTRA_PAGE_OWNER_PROCESS);
    if (status != SYNTRA_STATUS_OK)
    {
        (void)syntra_page_free_owned(code_page, SYNTRA_PAGE_OWNER_PROCESS);
        return status;
    }

    for (size_t i = 0U; i < SYNTRA_ARM64_MMU_PAGE_SIZE; ++i)
        ((uint8_t *)(uintptr_t)code_page)[i] =
            ((const uint8_t *)text_start)[i];

    entry_offset = entry_address - text_start;
    entry.address = code_va + (uint64_t)entry_offset;

    status = syntra_arm64_address_space_create(
        &process_el0_fault_space, SYNTRA_PROCESS_EL0_FAULT_ID,
        syntra_boot_page_table_resolve, NULL);
    if (status != SYNTRA_STATUS_OK) goto fail_pages;

    status = syntra_arm64_address_space_inherit_kernel_mappings(
        &process_el0_fault_space,
        syntra_active_page_table.root_physical_address);
    if (status != SYNTRA_STATUS_OK) goto fail_space;

    status = syntra_arm64_address_space_map_user_page(
        &process_el0_fault_space, code_va, (uint64_t)code_page,
        SYNTRA_ARM64_MMU_ACCESS_READ_ONLY, 1);
    if (status != SYNTRA_STATUS_OK) goto fail_space;
    code_page = 0U;

    status = syntra_arm64_address_space_map_user_page(
        &process_el0_fault_space, stack_va, (uint64_t)stack_page,
        SYNTRA_ARM64_MMU_ACCESS_READ_WRITE, 0);
    if (status != SYNTRA_STATUS_OK) goto fail_space;
    stack_page = 0U;

    status = syntra_arm64_address_space_validate(&process_el0_fault_space);
    if (status != SYNTRA_STATUS_OK) goto fail_space;

    status = process_create(SYNTRA_PROCESS_EL0_FAULT_ID, 1U);
    if (status != SYNTRA_STATUS_OK) goto fail_space;
    status = process_attach_address_space(
        SYNTRA_PROCESS_EL0_FAULT_ID, &process_el0_fault_space);
    if (status != SYNTRA_STATUS_OK) goto fail_process;

    status = syntra_arm64_user_context_initialize(
        process_el0_fault_kernel_stack,
        sizeof(process_el0_fault_kernel_stack),
        (void *)(uintptr_t)stack_va, SYNTRA_ARM64_MMU_PAGE_SIZE,
        entry.function, syntra_arm64_el0_thread_exit_trampoline,
        NULL, &context);
    if (status != SYNTRA_STATUS_OK) goto fail_process;

    status = scheduler_create_user_thread(
        SYNTRA_PROCESS_EL0_FAULT_THREAD_ID, 1U,
        entry.function, NULL,
        process_el0_fault_kernel_stack,
        sizeof(process_el0_fault_kernel_stack),
        (void *)(uintptr_t)stack_va, SYNTRA_ARM64_MMU_PAGE_SIZE,
        context);
    if (status != SYNTRA_STATUS_OK) goto fail_process;

    status = process_attach_thread(
        SYNTRA_PROCESS_EL0_FAULT_ID, SYNTRA_PROCESS_EL0_FAULT_THREAD_ID);
    if (status != SYNTRA_STATUS_OK) goto fail_process;

    thread = scheduler_get_thread(SYNTRA_PROCESS_EL0_FAULT_THREAD_ID);
    if (thread == NULL) return SYNTRA_STATUS_ERROR;

    fault_count_before = syntra_arm64_exception_user_thread_fault_count();
    status = interrupt_enable(30U);
    if (status != SYNTRA_STATUS_OK) goto fail_process;
    status = syntra_arm64_timer_enable();
    if (status != SYNTRA_STATUS_OK)
    {
        (void)interrupt_disable(30U);
        goto fail_process;
    }

    start_counter = syntra_arm64_timer_read_counter();
    timeout_ticks = syntra_arm64_timer_read_frequency() * 2U;
    __asm__ volatile("dsb sy" ::: "memory");
    __asm__ volatile("msr daifclr, #2" ::: "memory");
    __asm__ volatile("isb" ::: "memory");

    while (thread->state != THREAD_TERMINATED)
    {
        if ((syntra_arm64_timer_read_counter() - start_counter) > timeout_ticks)
        {
            status = SYNTRA_STATUS_ERROR;
            break;
        }
        __asm__ volatile("wfi" ::: "memory");
    }

    __asm__ volatile("msr daifset, #2" ::: "memory");
    __asm__ volatile("isb" ::: "memory");
    (void)syntra_arm64_timer_disable();
    (void)interrupt_disable(30U);

    if (status != SYNTRA_STATUS_OK ||
        thread->state != THREAD_TERMINATED ||
        syntra_arm64_exception_user_thread_fault_count() !=
            fault_count_before + 1U)
        goto fail_process;

    /* Timer IRQs may overwrite the generic exception snapshot; use the dedicated EL0 fault record. */
    {
        const syntra_arm64_exception_info_t *fault =
            syntra_arm64_exception_get_last_user_thread_fault();
        if (fault == NULL ||
            fault->exception_class != SYNTRA_ARM64_EXCEPTION_DATA_ABORT_LOWER ||
            fault->fault_address < code_va ||
            fault->fault_address >= code_va + SYNTRA_ARM64_MMU_PAGE_SIZE ||
            (fault->syndrome & (1ULL << 6U)) == 0ULL ||
            (fault->syndrome & 0x3fULL) != 0x0fULL)
        {
            status = SYNTRA_STATUS_ERROR;
            goto fail_process;
        }
        syntra_uart_puts("EL0 fault syndrome validation: PASS FAR=");
        syntra_uart_put_hex(fault->fault_address);
        syntra_uart_puts(" ESR=");
        syntra_uart_put_hex(fault->syndrome);
        syntra_uart_puts(" (Data Abort, user write, code-page range)\\n");
    }

    current = scheduler_get_current();
    if (current == NULL ||
        current->thread_id != SYNTRA_SCHEDULER_TEST_BOOT_ID)
    {
        status = SYNTRA_STATUS_ERROR;
        goto fail_process;
    }

    status = syntra_arm64_mmu_read_registers(&registers);
    if (status != SYNTRA_STATUS_OK ||
        (registers.ttbr0_el1 & SYNTRA_ARM64_MMU_PA_MASK) !=
            syntra_active_page_table.root_physical_address)
    {
        status = SYNTRA_STATUS_ERROR;
        goto fail_process;
    }

    status = process_terminate(SYNTRA_PROCESS_EL0_FAULT_ID);
    if (status != SYNTRA_STATUS_OK) return status;
    if (process_validate() != SYNTRA_STATUS_OK)
        return SYNTRA_STATUS_ERROR;

    syntra_uart_puts(
        "Process EL0 write fault: PASS (faulting process terminated, kernel resumed)\\n");
    return SYNTRA_STATUS_OK;

fail_process:
    thread = scheduler_get_thread(SYNTRA_PROCESS_EL0_FAULT_THREAD_ID);
    if (thread != NULL && thread->state != THREAD_TERMINATED)
        (void)scheduler_set_thread_state(
            SYNTRA_PROCESS_EL0_FAULT_THREAD_ID, THREAD_TERMINATED);
    (void)process_refresh_state(SYNTRA_PROCESS_EL0_FAULT_ID);
    (void)process_terminate(SYNTRA_PROCESS_EL0_FAULT_ID);
    return status;

fail_space:
    if (process_el0_fault_space.initialized)
        (void)syntra_arm64_address_space_destroy(&process_el0_fault_space);
fail_pages:
    if (code_page != 0U)
        (void)syntra_page_free_owned(code_page, SYNTRA_PAGE_OWNER_PROCESS);
    if (stack_page != 0U)
        (void)syntra_page_free_owned(stack_page, SYNTRA_PAGE_OWNER_PROCESS);
    return status;
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

    status = syntra_arm64_address_space_register_kernel_root(
        page_table.root_physical_address);
    if (status != SYNTRA_STATUS_OK)
    {
        syntra_uart_puts("Active kernel root registration: FAIL\\n");
        return status;
    }

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

    /*
     * Verify inherited kernel mappings, activate a process root, prove that
     * an active root cannot be destroyed, then switch safely back to kernel.
     */
    {
        static syntra_arm64_address_space_t audit_space;
        syntra_arm64_mmu_translation_t inherited_translation;
        syntra_arm64_mmu_registers_t switched_registers;
        const uint64_t kernel_root =
            syntra_active_page_table.root_physical_address;
        const syntra_arm64_mmu_mapping_t *kernel_mapping = &plan.mappings[0];

        status = syntra_arm64_address_space_create(
            &audit_space, 0xFFFF0001U,
            syntra_boot_page_table_resolve, NULL);
        if (status != SYNTRA_STATUS_OK)
            return status;

        status = syntra_arm64_address_space_inherit_kernel_mappings(
            &audit_space, kernel_root);
        if (status == SYNTRA_STATUS_OK)
            status = syntra_arm64_address_space_validate(&audit_space);
        if (status == SYNTRA_STATUS_OK)
            status = syntra_arm64_address_space_translate(
                &audit_space, kernel_mapping->virtual_address,
                &inherited_translation);
        if (status == SYNTRA_STATUS_OK &&
            (inherited_translation.physical_address !=
                kernel_mapping->physical_address ||
             inherited_translation.user_read != 0U))
            status = SYNTRA_STATUS_ERROR;

        if (status == SYNTRA_STATUS_OK)
            status = syntra_arm64_address_space_activate(&audit_space);
        if (status == SYNTRA_STATUS_OK)
            status = syntra_arm64_mmu_read_registers(&switched_registers);
        if (status == SYNTRA_STATUS_OK &&
            (switched_registers.ttbr0_el1 & SYNTRA_ARM64_MMU_PA_MASK) !=
                audit_space.page_table.root_physical_address)
            status = SYNTRA_STATUS_ERROR;
        if (status == SYNTRA_STATUS_OK &&
            syntra_arm64_address_space_destroy(&audit_space) !=
                SYNTRA_STATUS_PERMISSION_DENIED)
            status = SYNTRA_STATUS_ERROR;

        if (status == SYNTRA_STATUS_OK)
            status = syntra_arm64_address_space_activate_kernel_root();
        if (status == SYNTRA_STATUS_OK)
            status = syntra_arm64_mmu_read_registers(&switched_registers);
        if (status == SYNTRA_STATUS_OK &&
            (switched_registers.ttbr0_el1 & SYNTRA_ARM64_MMU_PA_MASK) !=
                kernel_root)
            status = SYNTRA_STATUS_ERROR;

        if (status != SYNTRA_STATUS_OK)
        {
            (void)syntra_arm64_address_space_activate_kernel_root();
            if (audit_space.initialized)
                (void)syntra_arm64_address_space_destroy(&audit_space);
            syntra_uart_puts("Kernel address-space switch audit: FAIL\\n");
            return status;
        }

        status = syntra_arm64_address_space_destroy(&audit_space);
        if (status != SYNTRA_STATUS_OK)
            return status;
        syntra_uart_puts(
            "Kernel address-space TTBR0 switch audit: PASS (inactive root)\\n");
    }

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
        syntra_uart_puts("Post-MMU Timer IRQ validation: FAIL\\n");
        return status;
    }

    status = syntra_arm64_process_address_space_scheduler_test();
    if (status != SYNTRA_STATUS_OK)
    {
        syntra_uart_puts("Process scheduler address-space switch: FAIL\\n");
        return status;
    }

    status = syntra_arm64_process_el0_scheduler_test();
    if (status != SYNTRA_STATUS_OK)
    {
        syntra_uart_puts("Process EL0 A execution: FAIL\\n");
        return status;
    }

    status = syntra_arm64_process_el0_isolation_b_test();
    if (status != SYNTRA_STATUS_OK)
    {
        syntra_uart_puts("Process EL0 B isolation execution: FAIL\\n");
        return status;
    }

    status = syntra_arm64_process_el0_write_fault_test();
    if (status != SYNTRA_STATUS_OK)
    {
        syntra_uart_puts("Process EL0 write-fault isolation: FAIL\\n");
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

#if defined(SYNTRA_QEMU_INPUT_DIAGNOSTIC)
static void syntra_virtio_input_qemu_diagnostic_wait(void)
{
    uint64_t start;
    uint64_t frequency;
    uint32_t spin;
    volatile uint32_t *registers;
    volatile uint8_t *queue_memory;
    uint16_t used_index;
    uint32_t interrupt_status;

    if (syntra_virtio_input_queue.ready != 1 ||
        syntra_virtio_input_queue.mapped_memory == NULL)
        return;

    syntra_uart_puts("VirtIO Input diagnostic: BEGIN\n");
    frequency = syntra_arm64_timer_read_frequency();
    start = syntra_arm64_timer_read_counter();
    for (spin = 0U; spin < 20000000U; ++spin)
    {
        if (frequency != 0U &&
            (syntra_arm64_timer_read_counter() - start) >=
                (frequency / 4U))
            break;
    }

    registers = (volatile uint32_t *)(uintptr_t)
        syntra_virtio_mmio_device.base;
    interrupt_status = registers[
        SYNTRA_VIRTIO_MMIO_INTERRUPT_STATUS / sizeof(uint32_t)];
    queue_memory = (volatile uint8_t *)
        syntra_virtio_input_queue.mapped_memory;
    used_index = (uint16_t)((uint16_t)queue_memory[
        syntra_virtio_input_queue.used_ring_offset + 2U] |
        ((uint16_t)queue_memory[
        syntra_virtio_input_queue.used_ring_offset + 3U] << 8U));

    syntra_uart_puts("VirtIO Input diagnostic: irq_status=");
    syntra_uart_put_hex(interrupt_status);
    syntra_uart_puts(" used_index=");
    syntra_uart_put_hex(used_index);
    syntra_uart_puts(" irq_events=");
    syntra_uart_put_hex(syntra_virtio_input_queue.event_count);
    syntra_uart_puts(" key_a=");
    syntra_uart_put_hex(syntra_virtio_input_queue.key_a_press_count);
    syntra_uart_puts(" irq_errors=");
    syntra_uart_put_hex(syntra_virtio_input_queue.irq_error_count);
    syntra_uart_puts("\n");
}
#endif

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

    status = process_init();
    if (status != SYNTRA_STATUS_OK)
    {
        syntra_uart_puts("SyntraOS boot failed: Process Manager initialization\\n");
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
            syntra_uart_puts("\n");

            status = syntra_virtio_mmio_decode_identity(
                &snapshot, &identity);
            if (status == SYNTRA_STATUS_OK)
            {
                if (identity.function == SYNTRA_VIRTIO_MMIO_FUNCTION_INPUT)
                {
                    syntra_uart_puts("VirtIO-MMIO function: INPUT\n");
                    status = syntra_virtio_mmio_prepare_input_mapped(
                        &syntra_virtio_mmio_device,
                        (volatile void *)(uintptr_t)syntra_virtio_mmio_device.base,
                        syntra_virtio_mmio_device.region_size,
                        &identity);
                    if (status == SYNTRA_STATUS_OK)
                    {
                        syntra_uart_puts("VirtIO Input transport: DRIVER FEATURES NEGOTIATED\n");
                        if (syntra_virtio_mmio_device.interrupt_valid)
                        {
                            status = syntra_arm64_gic_configure_interrupt(
                                syntra_virtio_mmio_device.interrupt_id,
                                syntra_virtio_mmio_device.interrupt_flags);
                            if (status == SYNTRA_STATUS_OK)
                            {
                                uintptr_t queue_physical_base = 0U;
                                syntra_uart_puts("VirtIO Input GIC trigger: CONFIGURED\n");
                                status = syntra_page_alloc_contiguous_owned(
                                    2U, &queue_physical_base,
                                    SYNTRA_PAGE_OWNER_DRIVER);
                                if (status == SYNTRA_STATUS_OK)
                                {
                                    status = syntra_virtio_mmio_setup_input_event_queue_mapped(
                                        &syntra_virtio_mmio_device,
                                        (volatile void *)(uintptr_t)syntra_virtio_mmio_device.base,
                                        syntra_virtio_mmio_device.region_size,
                                        &identity,
                                        (void *)(uintptr_t)queue_physical_base,
                                        queue_physical_base,
                                        2U * SYNTRA_PAGE_SIZE,
                                        &syntra_virtio_input_queue);
                                    if (status == SYNTRA_STATUS_OK)
                                    {
                                        syntra_uart_puts("VirtIO Input event queue: CONFIGURED\n");
                                        status = syntra_input_init(
                                            &syntra_virtio_input_state,
                                            640U, 480U);
                                        if (status == SYNTRA_STATUS_OK)
                                            status = interrupt_register(
                                                syntra_virtio_mmio_device.interrupt_id,
                                                syntra_virtio_input_irq_handler,
                                                &syntra_virtio_input_queue);
                                        if (status == SYNTRA_STATUS_OK)
                                        {
                                            syntra_virtio_input_irq_registered = 1;
                                            status = interrupt_set_priority(
                                                syntra_virtio_mmio_device.interrupt_id,
                                                0xA0U);
                                        }
                                        if (status == SYNTRA_STATUS_OK)
                                            status = interrupt_enable(
                                                syntra_virtio_mmio_device.interrupt_id);
                                        if (status == SYNTRA_STATUS_OK)
                                            status = syntra_virtio_mmio_start_input_mapped(
                                                &syntra_virtio_mmio_device,
                                                (volatile void *)(uintptr_t)syntra_virtio_mmio_device.base,
                                                syntra_virtio_mmio_device.region_size,
                                                &identity,
                                                &syntra_virtio_input_queue);
                                        if (status == SYNTRA_STATUS_OK)
                                        {
                                            syntra_uart_puts("VirtIO Input IRQ: REGISTERED\n");
                                            syntra_uart_puts("VirtIO Input queue: STARTED\n");
                                        }
                                        else
                                        {
                                            (void)interrupt_disable(
                                                syntra_virtio_mmio_device.interrupt_id);
                                            if (syntra_virtio_input_irq_registered)
                                            {
                                                (void)interrupt_unregister(
                                                    syntra_virtio_mmio_device.interrupt_id);
                                                syntra_virtio_input_irq_registered = 0;
                                            }
                                            syntra_uart_puts("VirtIO Input IRQ/queue: UNAVAILABLE\n");
                                        }
                                    }
                                    else
                                    {
                                        (void)syntra_page_free_owned(
                                            queue_physical_base,
                                            SYNTRA_PAGE_OWNER_DRIVER);
                                        (void)syntra_page_free_owned(
                                            queue_physical_base + SYNTRA_PAGE_SIZE,
                                            SYNTRA_PAGE_OWNER_DRIVER);
                                        syntra_uart_puts("VirtIO Input event queue: UNAVAILABLE\n");
                                    }
                                }
                                else
                                {
                                    syntra_uart_puts("VirtIO Input event queue: ALLOCATION FAILED\n");
                                }
                            }
                            else
                            {
                                syntra_uart_puts("VirtIO Input GIC trigger: UNSUPPORTED\n");
                            }
                        }
                    }
                    else
                    {
                        syntra_uart_puts("VirtIO Input transport: PREPARATION UNAVAILABLE\n");
                    }
                }
                else if (identity.function == SYNTRA_VIRTIO_MMIO_FUNCTION_OTHER)
                    syntra_uart_puts("VirtIO-MMIO function: OTHER\n");
                else
                    syntra_uart_puts("VirtIO-MMIO function: EMPTY SLOT\n");
            }
            else
            {
                syntra_uart_puts("VirtIO-MMIO identity decode: UNAVAILABLE\n");
            }
        }
        else
        {
            /* Optional input transport probe must not prevent the OS booting. */
            syntra_uart_puts("VirtIO-MMIO mapped identity read: UNAVAILABLE\n");
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
            syntra_uart_puts("SyntraOS GUI v5: COMPOSED DESKTOP + LAUNCHER + FILES + SETTINGS\n");
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

    /*
     * Timer and EL0 validation tests leave DAIF.I masked. Enable IRQ delivery
     * for the running kernel now that device handlers are registered.
     */
#if defined(__aarch64__)
    __asm__ volatile("dsb sy" ::: "memory");
    __asm__ volatile("msr daifclr, #2" ::: "memory");
    __asm__ volatile("isb" ::: "memory");
#endif

    syntra_uart_puts("SyntraOS boot OK\n");
#if defined(SYNTRA_QEMU_INPUT_DIAGNOSTIC)
    syntra_virtio_input_qemu_diagnostic_wait();
#endif
    return SYNTRA_STATUS_OK;
}

/*
 * Permanent normal-context idle loop. IRQ handlers only enqueue input; this
 * pump drains the FIFO and redraws the framebuffer after events are delivered.
 * WFI allows the CPU to sleep until the next interrupt.
 */
void syntra_arm64_boot_idle_loop(void)
{
    for (;;)
    {
        if (syntra_fw_cfg_base != 0U &&
            syntra_virtio_input_queue.ready == 1)
        {
            (void)syntra_ramfb_process_input_events(
                &syntra_virtio_input_queue);
        }
#if defined(__aarch64__)
        __asm__ volatile("dsb sy" ::: "memory");
        __asm__ volatile("wfi" ::: "memory");
#else
        __asm__ volatile("" ::: "memory");
#endif
    }
}
