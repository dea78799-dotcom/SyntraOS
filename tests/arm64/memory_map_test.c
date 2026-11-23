#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "../../kernel/arch/arm64/boot/fdt.h"
#include "../../kernel/arch/arm64/boot/memory_map.h"

#define TEST_FDT_CAPACITY 4096U
#define FDT_BEGIN_NODE 1U
#define FDT_END_NODE 2U
#define FDT_PROP 3U
#define FDT_END 9U

static uint8_t blob[TEST_FDT_CAPACITY];
static size_t structure_cursor;
static uintptr_t second_memory_base = (uintptr_t)0x50000000U;
static size_t second_memory_size = (size_t)0x8000U;
static size_t extra_memory_node_count;
static int reusable_has_second_reg_tuple;
static int memory_has_second_reg_tuple;
static uintptr_t memory_second_reg_base = (uintptr_t)0x41000000U;
static size_t memory_second_reg_size = (size_t)0x8000U;

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

static void append_reg_pair(
    uintptr_t first_base,
    size_t first_size,
    uintptr_t second_base,
    size_t second_size
)
{
    uint8_t data[32];

    write_be32(data, (uint32_t)((uint64_t)first_base >> 32U));
    write_be32(data + 4U, (uint32_t)first_base);
    write_be32(data + 8U, (uint32_t)((uint64_t)first_size >> 32U));
    write_be32(data + 12U, (uint32_t)first_size);
    write_be32(data + 16U, (uint32_t)((uint64_t)second_base >> 32U));
    write_be32(data + 20U, (uint32_t)second_base);
    write_be32(data + 24U, (uint32_t)((uint64_t)second_size >> 32U));
    write_be32(data + 28U, (uint32_t)second_size);
    append_property(27U, data, sizeof(data));
}

static size_t build_memory_map_fdt(
    int dynamic_reserved,
    int nonempty_ranges,
    int reserved_flags,
    int reserved_cells_flags
)
{
    static const char strings[] =
        "#address-cells\0#size-cells\0reg\0reusable\0status\0ranges\0size\0no-map\0";
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
    if (memory_has_second_reg_tuple)
    {
        append_reg_pair(
            (uintptr_t)0x40000000U, (size_t)0x10000U,
            memory_second_reg_base, memory_second_reg_size);
    }
    else
    {
        append_reg((uintptr_t)0x40000000U, (size_t)0x10000U);
    }
    append_end_node();

    /* A second enabled RAM node must be included in the discovered map. */
    append_node("memory@50000000");
    append_reg(second_memory_base, second_memory_size);
    append_end_node();

    for (index = 0U; index < extra_memory_node_count; ++index)
    {
        char memory_node_name[32];
        uintptr_t memory_base =
            (uintptr_t)0x80000000U + (uintptr_t)index * 0x10000U;

        (void)snprintf(
            memory_node_name,
            sizeof(memory_node_name),
            "memory@%08x",
            (unsigned int)memory_base);
        append_node(memory_node_name);
        append_reg(memory_base, (size_t)0x8000U);
        append_end_node();
    }

    /* Disabled memory nodes must not become allocator RAM. */
    append_node("memory@60000000");
    append_reg((uintptr_t)0x60000000U, (size_t)0x8000U);
    {
        static const uint8_t disabled_status[] = "disabled";
        append_property(40U, disabled_status, sizeof(disabled_status));
    }
    append_end_node();

    /* fail-sss memory nodes must not become allocator RAM either. */
    append_node("memory@70000000");
    append_reg((uintptr_t)0x70000000U, (size_t)0x8000U);
    {
        static const uint8_t failed_status[] = "fail-hardware";
        append_property(40U, failed_status, sizeof(failed_status));
    }
    append_end_node();

    append_node("reserved-memory");
    if ((reserved_cells_flags & 1) == 0)
    {
        append_cells_property(
            0U, (reserved_cells_flags & 4) != 0 ? 1U : 2U);
    }
    if ((reserved_cells_flags & 2) == 0)
    {
        append_cells_property(
            15U, (reserved_cells_flags & 8) != 0 ? 1U : 2U);
    }
    if (nonempty_ranges)
    {
        static const uint8_t invalid_ranges[] = {0U, 0U, 0U, 0U};
        append_property(47U, invalid_ranges, sizeof(invalid_ranges));
    }
    else
    {
        append_property(47U, NULL, 0U);
    }
    append_node("reusable@40004000");
    if (reusable_has_second_reg_tuple)
    {
        append_reg_pair(
            (uintptr_t)0x40004000U, (size_t)0x1000U,
            (uintptr_t)0x40006000U, (size_t)0x1000U);
    }
    else
    {
        append_reg((uintptr_t)0x40004000U, (size_t)0x1000U);
    }
    append_property(31U, NULL, 0U);
    append_end_node();

    /* Disabled reserved-memory children must leave the RAM range allocatable. */
    append_node("disabled@50002000");
    append_reg((uintptr_t)0x50002000U, (size_t)0x1000U);
    {
        static const uint8_t disabled_status[] = "disabled";
        append_property(40U, disabled_status, sizeof(disabled_status));
    }
    append_end_node();

    /* Nodes with status = "fail" must also be skipped. */
    append_node("failed@50003000");
    append_reg((uintptr_t)0x50003000U, (size_t)0x1000U);
    {
        static const uint8_t failed_status[] = "fail";
        append_property(40U, failed_status, sizeof(failed_status));
    }
    append_end_node();

    /* A fail-sss reserved-memory child must also be skipped. */
    append_node("failed-hardware@50004000");
    append_reg((uintptr_t)0x50004000U, (size_t)0x1000U);
    {
        static const uint8_t failed_status[] = "fail-hardware";
        append_property(40U, failed_status, sizeof(failed_status));
    }
    append_end_node();

    if (reserved_flags != 0)
    {
        append_node("special@50005000");
        append_reg((uintptr_t)0x50005000U, (size_t)0x1000U);

        if (reserved_flags == 1 || reserved_flags == 2)
        {
            append_property(59U, NULL, 0U);
        }

        if (reserved_flags == 2)
        {
            append_property(31U, NULL, 0U);
        }
        else if (reserved_flags == 3)
        {
            static const uint8_t malformed_reusable[] = {1U};
            append_property(31U, malformed_reusable,
                            sizeof(malformed_reusable));
        }

        append_end_node();
    }

    if (dynamic_reserved)
    {
        static const uint8_t dynamic_size[] = {
            0U, 0U, 0U, 0U, 0U, 0U, 0x10U, 0U
        };
        append_node("dynamic@0");
        append_property(54U, dynamic_size, sizeof(dynamic_size));
        append_end_node();
    }
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

static void test_multi_tuple_memory_node_discovery(void)
{
    syntra_fdt_t fdt;
    size_t blob_size;
    syntra_memory_region_t region;

    /* The second tuple is a separate, non-overlapping RAM extent. */
    memory_has_second_reg_tuple = 1;
    memory_second_reg_base = (uintptr_t)0x41000000U;
    memory_second_reg_size = (size_t)0x8000U;
    blob_size = build_memory_map_fdt(0, 0, 0, 0);

    assert(syntra_fdt_open(blob, blob_size, &fdt) == SYNTRA_STATUS_OK);
    assert(syntra_arm64_memory_map_discover(
        &fdt,
        (uintptr_t)0x40008000U,
        (size_t)0x1000U) == SYNTRA_STATUS_OK);

    /* One additional FDT reg tuple must produce one additional RAM region. */
    assert(syntra_memory_region_count() == 9U);
    assert(syntra_memory_region_find(
        (uintptr_t)0x41000000U, &region) == SYNTRA_STATUS_OK);
    assert(region.base == (uintptr_t)0x41000000U);
    assert(region.size == (size_t)0x8000U);
    assert(region.type == SYNTRA_MEMORY_REGION_RAM);

    /*
     * The first tuple is valid, but the second overlaps the other RAM node.
     * Discovery must reject the map and roll back the already-added tuple.
     */
    memory_second_reg_base = (uintptr_t)0x40008000U;
    memory_second_reg_size = (size_t)0x1000U;
    blob_size = build_memory_map_fdt(0, 0, 0, 0);

    assert(syntra_fdt_open(blob, blob_size, &fdt) == SYNTRA_STATUS_OK);
    assert(syntra_arm64_memory_map_discover(
        &fdt,
        (uintptr_t)0x40008000U,
        (size_t)0x1000U) == SYNTRA_STATUS_ERROR);
    assert(syntra_memory_region_count() == 0U);

    memory_has_second_reg_tuple = 0;
    memory_second_reg_base = (uintptr_t)0x41000000U;
    memory_second_reg_size = (size_t)0x8000U;
}

static void test_fdt_memory_and_reservation_discovery(void)
{
    syntra_fdt_t fdt;
    size_t blob_size = build_memory_map_fdt(0, 0, 0, 0);

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
    assert_region((uintptr_t)0x50002000U, SYNTRA_MEMORY_REGION_RAM);
    assert_region((uintptr_t)0x50003000U, SYNTRA_MEMORY_REGION_RAM);
    assert_region((uintptr_t)0x50004000U, SYNTRA_MEMORY_REGION_RAM);
    {
        syntra_memory_region_t disabled_region;
        assert(syntra_memory_region_find(
            (uintptr_t)0x60001000U, &disabled_region) != SYNTRA_STATUS_OK);
        assert(syntra_memory_region_find(
            (uintptr_t)0x70001000U, &disabled_region) != SYNTRA_STATUS_OK);
    }
}

static void test_rejects_nonempty_reserved_memory_ranges(void)
{
    syntra_fdt_t fdt;
    size_t blob_size = build_memory_map_fdt(0, 1, 0, 0);

    assert(syntra_fdt_open(blob, blob_size, &fdt) == SYNTRA_STATUS_OK);
    assert(syntra_arm64_memory_map_discover(
        &fdt,
        (uintptr_t)0x40008000U,
        (size_t)0x1000U) == SYNTRA_STATUS_ERROR);
    assert(syntra_memory_region_count() == 0U);
}

static void test_rejects_unimplemented_dynamic_reserved_memory(void)
{
    syntra_fdt_t fdt;
    size_t blob_size = build_memory_map_fdt(1, 0, 0, 0);

    assert(syntra_fdt_open(blob, blob_size, &fdt) == SYNTRA_STATUS_OK);
    assert(syntra_arm64_memory_map_discover(
        &fdt,
        (uintptr_t)0x40008000U,
        (size_t)0x1000U) == SYNTRA_STATUS_NOT_IMPLEMENTED);
    assert(syntra_memory_region_count() == 0U);
}

static void test_discovers_no_map_reservation(void)
{
    syntra_fdt_t fdt;
    size_t blob_size = build_memory_map_fdt(0, 0, 1, 0);

    assert(syntra_fdt_open(blob, blob_size, &fdt) == SYNTRA_STATUS_OK);
    assert(syntra_arm64_memory_map_discover(
        &fdt,
        (uintptr_t)0x40008000U,
        (size_t)0x1000U) == SYNTRA_STATUS_OK);
    assert_region(
        (uintptr_t)0x50005000U, SYNTRA_MEMORY_REGION_NOMAP);
}

static void test_rejects_conflicting_no_map_and_reusable(void)
{
    syntra_fdt_t fdt;
    size_t blob_size = build_memory_map_fdt(0, 0, 2, 0);

    assert(syntra_fdt_open(blob, blob_size, &fdt) == SYNTRA_STATUS_OK);
    assert(syntra_arm64_memory_map_discover(
        &fdt,
        (uintptr_t)0x40008000U,
        (size_t)0x1000U) == SYNTRA_STATUS_ERROR);
    assert(syntra_memory_region_count() == 0U);
}

static void test_rejects_nonempty_reusable_boolean(void)
{
    syntra_fdt_t fdt;
    size_t blob_size = build_memory_map_fdt(0, 0, 3, 0);

    assert(syntra_fdt_open(blob, blob_size, &fdt) == SYNTRA_STATUS_OK);
    assert(syntra_arm64_memory_map_discover(
        &fdt,
        (uintptr_t)0x40008000U,
        (size_t)0x1000U) == SYNTRA_STATUS_ERROR);
    assert(syntra_memory_region_count() == 0U);
}

static void test_rejects_reserved_memory_cell_mismatch(void)
{
    static const int invalid_cell_layouts[] = {1, 2, 4, 8};
    size_t index;

    for (index = 0U;
         index < sizeof(invalid_cell_layouts) / sizeof(invalid_cell_layouts[0]);
         ++index)
    {
        syntra_fdt_t fdt;
        size_t blob_size = build_memory_map_fdt(
            0, 0, 0, invalid_cell_layouts[index]);

        assert(syntra_fdt_open(blob, blob_size, &fdt) == SYNTRA_STATUS_OK);
        assert(syntra_arm64_memory_map_discover(
            &fdt,
            (uintptr_t)0x40008000U,
            (size_t)0x1000U) == SYNTRA_STATUS_ERROR);
        assert(syntra_memory_region_count() == 0U);
    }
}

static void test_memory_nodes_reject_overlapping_ranges(void)
{
    static const uintptr_t overlapping_bases[] = {
        (uintptr_t)0x40008000U, /* Partial overlap with the first RAM node. */
        (uintptr_t)0x40004000U, /* Fully contained in the first RAM node. */
        (uintptr_t)0x40000000U  /* Exact duplicate range. */
    };
    static const size_t overlapping_sizes[] = {
        (size_t)0x8000U,
        (size_t)0x1000U,
        (size_t)0x10000U
    };
    size_t index;

    for (index = 0U;
         index < sizeof(overlapping_bases) / sizeof(overlapping_bases[0]);
         ++index)
    {
        syntra_fdt_t fdt;
        size_t blob_size;

        second_memory_base = overlapping_bases[index];
        second_memory_size = overlapping_sizes[index];
        blob_size = build_memory_map_fdt(0, 0, 0, 0);

        assert(syntra_fdt_open(blob, blob_size, &fdt) == SYNTRA_STATUS_OK);
        assert(syntra_arm64_memory_map_discover(
            &fdt,
            (uintptr_t)0x40008000U,
            (size_t)0x1000U) == SYNTRA_STATUS_ERROR);
        assert(syntra_memory_region_count() == 0U);
    }

    /* Adjacent half-open ranges do not overlap and must both be accepted. */
    {
        syntra_fdt_t fdt;
        size_t blob_size;

        second_memory_base = (uintptr_t)0x40010000U;
        second_memory_size = (size_t)0x8000U;
        blob_size = build_memory_map_fdt(0, 0, 0, 0);

        assert(syntra_fdt_open(blob, blob_size, &fdt) == SYNTRA_STATUS_OK);
        assert(syntra_arm64_memory_map_discover(
            &fdt,
            (uintptr_t)0x40008000U,
            (size_t)0x1000U) == SYNTRA_STATUS_OK);
        assert_region(
            (uintptr_t)0x40010000U, SYNTRA_MEMORY_REGION_RAM);
    }

    second_memory_base = (uintptr_t)0x50000000U;
    second_memory_size = (size_t)0x8000U;
}

static void test_memory_map_capacity_failure_rolls_back(void)
{
    syntra_fdt_t fdt;
    size_t blob_size;

    /* Two built-in RAM nodes plus 31 additional nodes exceed the 32 slots. */
    extra_memory_node_count = 31U;
    blob_size = build_memory_map_fdt(0, 0, 0, 0);

    assert(syntra_fdt_open(blob, blob_size, &fdt) == SYNTRA_STATUS_OK);
    assert(syntra_arm64_memory_map_discover(
        &fdt,
        (uintptr_t)0x40008000U,
        (size_t)0x1000U) == SYNTRA_STATUS_OUT_OF_MEMORY);
    assert(syntra_memory_region_count() == 0U);

    extra_memory_node_count = 0U;
    second_memory_base = (uintptr_t)0x50000000U;
    second_memory_size = (size_t)0x8000U;
}

static void test_header_reservation_capacity_failure_rolls_back(void)
{
    syntra_fdt_t fdt;
    size_t blob_size;

    /*
     * 2 built-in nodes + 30 extras fill all 32 slots. The header reservation
     * splits the first RAM region into three pieces and must fail atomically.
     */
    extra_memory_node_count = 30U;
    blob_size = build_memory_map_fdt(0, 0, 0, 0);

    assert(syntra_fdt_open(blob, blob_size, &fdt) == SYNTRA_STATUS_OK);
    assert(syntra_arm64_memory_map_discover(
        &fdt,
        (uintptr_t)0x40008000U,
        (size_t)0x1000U) == SYNTRA_STATUS_OUT_OF_MEMORY);
    assert(syntra_memory_region_count() == 0U);

    extra_memory_node_count = 0U;
}

static void test_reserved_memory_capacity_failure_rolls_back(void)
{
    syntra_fdt_t fdt;
    size_t blob_size;

    /*
     * 2 built-in nodes + 27 extras leave 3 slots. The header reservation
     * consumes 2; the reusable carve then needs 2 more and must fail.
     */
    extra_memory_node_count = 27U;
    blob_size = build_memory_map_fdt(0, 0, 0, 0);

    assert(syntra_fdt_open(blob, blob_size, &fdt) == SYNTRA_STATUS_OK);
    assert(syntra_arm64_memory_map_discover(
        &fdt,
        (uintptr_t)0x40008000U,
        (size_t)0x1000U) == SYNTRA_STATUS_OUT_OF_MEMORY);
    assert(syntra_memory_region_count() == 0U);

    extra_memory_node_count = 0U;
}

static void test_multi_tuple_reserved_memory_failure_rolls_back(void)
{
    syntra_fdt_t fdt;
    size_t blob_size;

    /*
     * 2 built-in nodes + 26 extras produce 28 RAM regions.
     * The header reservation adds 2 regions (30); tuple 0 adds 2 (32).
     * Tuple 1 then cannot split its RAM region because the registry is full.
     */
    extra_memory_node_count = 26U;
    reusable_has_second_reg_tuple = 1;
    blob_size = build_memory_map_fdt(0, 0, 0, 0);

    assert(syntra_fdt_open(blob, blob_size, &fdt) == SYNTRA_STATUS_OK);
    {
        size_t reg_count;
        syntra_fdt_iterator_t iterator;
        syntra_fdt_token_t token;
        syntra_fdt_node_t reserved_child;
        int found_child = 0;

        syntra_fdt_iterator_init(&iterator);
        while (syntra_fdt_next(&fdt, &iterator, &token) == SYNTRA_STATUS_OK)
        {
            if (token.type == SYNTRA_FDT_TOKEN_BEGIN_NODE &&
                token.depth == 2U &&
                token.name != NULL &&
                token.name[0] == 'r')
            {
                reserved_child.begin_offset = token.offset;
                reserved_child.depth = token.depth;
                found_child = 1;
                break;
            }
        }
        assert(found_child);
        assert(syntra_fdt_get_reg_count(
            &fdt, &reserved_child, &reg_count) == SYNTRA_STATUS_OK);
        assert(reg_count == 2U);
    }

    assert(syntra_arm64_memory_map_discover(
        &fdt,
        (uintptr_t)0x40008000U,
        (size_t)0x1000U) == SYNTRA_STATUS_OUT_OF_MEMORY);
    assert(syntra_memory_region_count() == 0U);

    extra_memory_node_count = 0U;
    reusable_has_second_reg_tuple = 0;
}

int main(void)
{
    test_fdt_memory_and_reservation_discovery();
    test_multi_tuple_memory_node_discovery();
    test_memory_nodes_reject_overlapping_ranges();
    test_memory_map_capacity_failure_rolls_back();
    test_header_reservation_capacity_failure_rolls_back();
    test_reserved_memory_capacity_failure_rolls_back();
    test_multi_tuple_reserved_memory_failure_rolls_back();
    test_rejects_nonempty_reserved_memory_ranges();
    test_rejects_reserved_memory_cell_mismatch();
    test_rejects_unimplemented_dynamic_reserved_memory();
    test_discovers_no_map_reservation();
    test_rejects_conflicting_no_map_and_reusable();
    test_rejects_nonempty_reusable_boolean();
    puts("ARM64 FDT memory map tests passed");
    return 0;
}
