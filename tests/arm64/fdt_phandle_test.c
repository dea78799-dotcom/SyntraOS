#include "../../kernel/arch/arm64/boot/fdt.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

static const char test_strings[] = "phandle\0#interrupt-cells\0";

static void write_be32(uint8_t *target, uint32_t value)
{
    target[0] = (uint8_t)(value >> 24U);
    target[1] = (uint8_t)(value >> 16U);
    target[2] = (uint8_t)(value >> 8U);
    target[3] = (uint8_t)value;
}

static size_t append_u32(uint8_t *blob, size_t offset, uint32_t value)
{
    write_be32(blob + offset, value);
    return offset + 4U;
}

static size_t append_name(uint8_t *blob, size_t offset, const char *name)
{
    size_t length = 0U;
    while (name[length] != '\0')
    {
        blob[offset++] = (uint8_t)name[length++];
    }
    blob[offset++] = 0U;
    while ((offset & 3U) != 0U) blob[offset++] = 0U;
    return offset;
}

static size_t build_test_fdt(uint8_t *blob, size_t capacity)
{
    size_t cursor = 56U;
    size_t structure_size;
    size_t strings_offset;
    size_t total_size;

    assert(capacity >= 256U);

    /* FDT_BEGIN_NODE: root */
    cursor = append_u32(blob, cursor, 1U);
    cursor = append_name(blob, cursor, "");

    /* FDT_BEGIN_NODE: interrupt controller */
    cursor = append_u32(blob, cursor, 1U);
    cursor = append_name(blob, cursor, "intc");

    /* phandle = 0x8002 */
    cursor = append_u32(blob, cursor, 3U);
    cursor = append_u32(blob, cursor, 4U);
    cursor = append_u32(blob, cursor, 0U);
    cursor = append_u32(blob, cursor, 0x8002U);

    /* #interrupt-cells = 3 */
    cursor = append_u32(blob, cursor, 3U);
    cursor = append_u32(blob, cursor, 4U);
    cursor = append_u32(blob, cursor, 8U);
    cursor = append_u32(blob, cursor, 3U);

    cursor = append_u32(blob, cursor, 2U); /* child END_NODE */
    cursor = append_u32(blob, cursor, 2U); /* root END_NODE */
    cursor = append_u32(blob, cursor, 9U); /* FDT_END */

    structure_size = cursor - 56U;
    strings_offset = cursor;
    total_size = strings_offset + sizeof(test_strings) - 1U;

    for (cursor = 0U; cursor < total_size; ++cursor) blob[cursor] = 0U;

    /* Rebuild structure after clearing and fill the strings block. */
    cursor = 56U;
    cursor = append_u32(blob, cursor, 1U);
    cursor = append_name(blob, cursor, "");
    cursor = append_u32(blob, cursor, 1U);
    cursor = append_name(blob, cursor, "intc");
    cursor = append_u32(blob, cursor, 3U);
    cursor = append_u32(blob, cursor, 4U);
    cursor = append_u32(blob, cursor, 0U);
    cursor = append_u32(blob, cursor, 0x8002U);
    cursor = append_u32(blob, cursor, 3U);
    cursor = append_u32(blob, cursor, 4U);
    cursor = append_u32(blob, cursor, 8U);
    cursor = append_u32(blob, cursor, 3U);
    cursor = append_u32(blob, cursor, 2U);
    cursor = append_u32(blob, cursor, 2U);
    cursor = append_u32(blob, cursor, 9U);

    for (cursor = 0U; cursor < sizeof(test_strings) - 1U; ++cursor)
        blob[strings_offset + cursor] = (uint8_t)test_strings[cursor];

    write_be32(blob + 0U, SYNTRA_FDT_MAGIC);
    write_be32(blob + 4U, (uint32_t)total_size);
    write_be32(blob + 8U, 56U);
    write_be32(blob + 12U, (uint32_t)strings_offset);
    write_be32(blob + 16U, 40U);
    write_be32(blob + 20U, 17U);
    write_be32(blob + 24U, 16U);
    write_be32(blob + 28U, 0U);
    write_be32(blob + 32U, (uint32_t)(sizeof(test_strings) - 1U));
    write_be32(blob + 36U, (uint32_t)structure_size);

    return total_size;
}

int main(void)
{
    uint8_t blob[256];
    syntra_fdt_t fdt;
    syntra_fdt_node_t node;
    const uint8_t *data;
    uint32_t data_size;

    (void)build_test_fdt(blob, sizeof(blob));
    assert(syntra_fdt_open(blob, sizeof(blob), &fdt) == SYNTRA_STATUS_OK);
    assert(syntra_fdt_find_phandle(&fdt, 0x8002U, &node) ==
           SYNTRA_STATUS_OK);
    assert(syntra_fdt_get_property(
        &fdt, &node, "#interrupt-cells", &data, &data_size) ==
        SYNTRA_STATUS_OK);
    assert(data_size == 4U);
    assert(data[0] == 0U && data[1] == 0U &&
           data[2] == 0U && data[3] == 3U);
    assert(syntra_fdt_find_phandle(&fdt, 0x1234U, &node) !=
           SYNTRA_STATUS_OK);

    puts("SyntraOS FDT phandle lookup tests passed.");
    return 0;
}
