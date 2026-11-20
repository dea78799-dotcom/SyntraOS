#include <stdint.h>
#include <stddef.h>

#include "../../core/status.h"
#include "fdt.h"
#include "../gic.h"

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

    status = syntra_boot_report_interrupt_controller(&fdt);
    if (status != SYNTRA_STATUS_OK)
    {
        return status;
    }

    return syntra_arm64_gic_discover(&fdt);
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
    status = validate_dtb_and_discover_gic(dtb);
    if (status != SYNTRA_STATUS_OK)
    {
        syntra_uart_puts("SyntraOS boot failed: FDT/GIC discovery\n");
        return status;
    }

    syntra_uart_puts("SyntraOS boot OK\n");
    return SYNTRA_STATUS_OK;
}
