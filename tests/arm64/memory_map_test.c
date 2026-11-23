#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "../../kernel/arch/arm64/boot/fdt.h"
#include "../../kernel/arch/arm64/boot/memory_map.h"

#define TEST_FDT_CAPACITY 1024U
#define FDT_BEGIN_NODE 1U
#define FDT_END_NODE 2U
#define FDT_PROP 3U
#define FDT_END 9U

static uint8_t blob[TEST_FDT_CAPACITY];
static size_t structure_cursor;

static void write_be32(uint8_t *data, uint32_t value)
{
    data[0] = (uint8_t)(value >> 24U);
    data[1] = (uint8_t)(value >> 16U);
    data[2] = (uint8_t)(value >> 8U);
    data[3] = (uint8_t)value;
}

static void append_u32(uint32_t value)
{
    write_be32(blob + structure_cursor, value);
    structure_cursor += 4U;
}

static void append_node(const char *name)
{
    append_u32(FDT_BEGIN_NODE);
    do
    {
        blob[structure_cursor++] = (uint8_t)*name;
    } while (*name++ != '\0');

    while ((structure_cursor & 3U) != 0U)
    {
        blob[structure_cursor++] = 0U;
    }
}

static void append_end_node(void)
{
    append_u32(FDT_END_NODE);
}

static void append_property(
    uint32_t name_offset,
    const uint8_t *data,
    uint32_t size
)
{
    uint32_t index;

    append_u32(FDT_PROP);
    append_u32(size);
    append_u32(name_offset);

    for (index = 0U; index < size; ++index)
    {
        blob[structure_cursor++] = data[index];
    }

    while ((structure_cursor & 3U) != 0U)
    {
        blob[structure_cursor++] = 0U;
    }
}

static void append_cells_property(uint32_t name_offset, uint32_t value)
{
    uint8_t data[4];
    write_be32(data, value);
    append_property(name_offset, data, sizeof(data));
}

static void append_reg(uintptr_t base, size_t size)
{
    uint8_t data[16];

    write_be32(data, (uint32_t)((uint64_t)base >> 32U));
    write_be32(data + 4U, (uint32_t)base);
    write_be32(data + 8U, (uint32_t)((uint64_t)size >> 32U));
    write_be32(data + 12U, (uint32_t)size);
    append_property(27U, data, sizeof(data));
}

static size_t build_memory_map_fdt(void)
{
    static const char strings[] =
        "#address-cells\0#size-cells\0reg\0reusable\0status\0";
    const size_t reserve_offset = 40U;
    const size_t structure_offset = 72U;
    size_t strings_offset;
    size_t total_size;
    size_t index;

    for (index = 0U; index < sizeof(blob); ++index)
    {
        blob[index] = 0U;
    }

    /* Header reservation map: reserve [0x40002000, 0x40003000). */
    write_be32(blob + reserve_offset, 0U);
    write_be32(blob + reserve_offset + 4U, 0x40002000U);
    write_be32(blob + reserve_offset + 8U, 0U);
    write_be32(blob + reserve_offset + 12U, 0x1000U);

    structure_cursor = structure_offset;
    append_node("");
    append_cells_property(0U, 2U);
    append_cells_property(15U, 2U);

    append_node("memory@40000000");
    append_reg((uintptr_t)0x40000000U, (size_t)0x10000U);
    append_end_node();

    /* A second enabled RAM node must be included in the discovered map. */
    append_node("memory@50000000");
    append_reg((uintptr_t)0x50000000U, (size_t)0x8000U);
    append_end_node();

    /* Disabled memory nodes must not become allocator RAM. */
    append_node("memory@60000000");
    append_reg((uintptr_t)0x60000000U, (size_t)0x8000U);
    {
        static const uint8_t disabled_status[] = "disabled";
        append_property(40U, disabled_status, sizeof(disabled_status));
    }
    append_end_node();

    append_node("reserved-memory");
    append_cells_property(0U, 2U);
    append_cells_property(15U, 2U);
    append_node("reusable@40004000");
    append_reg((uintptr_t)0x40004000U, (size_t)0x1000U);
    append_property(31U, NULL, 0U);
    append_end_node();
    append_end_node();

    append_end_node();
    append_u32(FDT_END);

    strings_offset = (structure_cursor + 3U) & ~(size_t)3U;
    while (structure_cursor < strings_offset)
    {
        blob[structure_cursor++] = 0U;
    }

    for (index = 0U; index < sizeof(strings) - 1U; ++index)
    {
        blob[strings_offset + index] = (uint8_t)strings[index];
    }

    total_size = strings_offset + sizeof(strings) - 1U;
    write_be32(blob, SYNTRA_FDT_MAGIC);
    write_be32(blob + 4U, (uint32_t)total_size);
    write_be32(blob + 8U, (uint32_t)structure_offset);
    write_be32(blob + 12U, (uint32_t)strings_offset);
    write_be32(blob + 16U, (uint32_t)reserve_offset);
    write_be32(blob + 20U, 17U);
    write_be32(blob + 24U, 16U);
    write_be32(blob + 28U, 0U);
    write_be32(blob + 32U, (uint32_t)(sizeof(strings) - 1U));
    write_be32(blob + 36U, (uint32_t)(strings_offset - structure_offset));

    return total_size;
}

static void assert_region(
    uintptr_t address,
    syntra_memory_region_type_t expected_type
)
{
    syntra_memory_region_t region;

    assert(syntra_memory_region_find(address, &region) == SYNTRA_STATUS_OK);
    assert(region.type == expected_type);
}

static void test_fdt_memory_and_reservation_discovery(void)
{
    syntra_fdt_t fdt;
    size_t blob_size = build_memory_map_fdt();

    assert(syntra_fdt_open(blob, blob_size, &fdt) == SYNTRA_STATUS_OK);
    assert(syntra_arm64_memory_map_discover(
        &fdt,
        (uintptr_t)0x40008000U,
        (size_t)0x1000U) == SYNTRA_STATUS_OK);

    assert(syntra_memory_region_count() == 8U);
    assert_region((uintptr_t)0x40001000U, SYNTRA_MEMORY_REGION_RAM);
    assert_region((uintptr_t)0x40002000U, SYNTRA_MEMORY_REGION_RESERVED);
    assert_region((uintptr_t)0x40003000U, SYNTRA_MEMORY_REGION_RAM);
    assert_region((uintptr_t)0x40004000U, SYNTRA_MEMORY_REGION_RECLAIMABLE);
    assert_region((uintptr_t)0x40005000U, SYNTRA_MEMORY_REGION_RAM);
    assert_region((uintptr_t)0x40008000U, SYNTRA_MEMORY_REGION_DTB);
    assert_region((uintptr_t)0x40009000U, SYNTRA_MEMORY_REGION_RAM);
    assert_region((uintptr_t)0x50001000U, SYNTRA_MEMORY_REGION_RAM);
    {
        syntra_memory_region_t disabled_region;
        assert(syntra_memory_region_find(
            (uintptr_t)0x60001000U, &disabled_region) != SYNTRA_STATUS_OK);
    }
}

int main(void)
{
    test_fdt_memory_and_reservation_discovery();
    puts("ARM64 FDT memory map tests passed");
    return 0;
}
