#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "../../kernel/arch/arm64/boot/fdt.h"
#include "../../kernel/arch/arm64/gic.h"

#define TEST_BUFFER_SIZE 512U

static uint8_t blob[TEST_BUFFER_SIZE];
static size_t structure_offset;
static size_t strings_offset;
static size_t address_cells_data_offset;
static size_t reg_data_offset;

static void write_be32(uint8_t *data, uint32_t value)
{
    data[0] = (uint8_t)(value >> 24);
    data[1] = (uint8_t)(value >> 16);
    data[2] = (uint8_t)(value >> 8);
    data[3] = (uint8_t)value;
}

static void append_u32(size_t *cursor, uint32_t value)
{
    write_be32(blob + *cursor, value);
    *cursor += 4U;
}

static void append_string(size_t *cursor, const char *value)
{
    do
    {
        blob[(*cursor)++] = (uint8_t)*value;
    } while (*value++ != '\0');

    while ((*cursor & 3U) != 0U)
    {
        blob[(*cursor)++] = 0U;
    }
}

static void append_property(
    size_t *cursor,
    uint32_t name_offset,
    const uint8_t *data,
    uint32_t data_size
)
{
    uint32_t index;

    append_u32(cursor, 3U);
    append_u32(cursor, data_size);
    append_u32(cursor, name_offset);

    for (index = 0U; index < data_size; ++index)
    {
        blob[(*cursor)++] = data[index];
    }

    while ((*cursor & 3U) != 0U)
    {
        blob[(*cursor)++] = 0U;
    }
}

static void build_test_fdt(int use_gic_v2)
{
    static const uint8_t address_cells[] = {0U, 0U, 0U, 2U};
    static const uint8_t size_cells[] = {0U, 0U, 0U, 2U};
    static const uint8_t compatible_v3[] = "arm,gic-v3";
    static const uint8_t compatible_v2[] = "arm,gic-400";
    static const uint8_t reg_v3[] = {
        /* GIC distributor: 0x08000000, size 0x00010000. */
        0U, 0U, 0U, 0U, 0x08U, 0U, 0U, 0U,
        0U, 0U, 0U, 0U, 0U, 1U, 0U, 0U,
        /* GIC redistributor: 0x080A0000, size 0x00F60000. */
        0U, 0U, 0U, 0U, 0x08U, 0x0aU, 0U, 0U,
        0U, 0U, 0U, 0U, 0U, 0xf6U, 0U, 0U
    };
    static const uint8_t reg_v2[] = {
        /* GIC distributor: 0x08000000, size 0x00010000. */
        0U, 0U, 0U, 0U, 0x08U, 0U, 0U, 0U,
        0U, 0U, 0U, 0U, 0U, 1U, 0U, 0U,
        /* GIC CPU interface: 0x08010000, size 0x00001000. */
        0U, 0U, 0U, 0U, 0x08U, 1U, 0U, 0U,
        0U, 0U, 0U, 0U, 0U, 0U, 0x10U, 0U
    };
    const uint8_t *compatible = use_gic_v2
        ? compatible_v2 : compatible_v3;
    const uint8_t *reg = use_gic_v2 ? reg_v2 : reg_v3;
    uint32_t compatible_size = use_gic_v2
        ? (uint32_t)sizeof(compatible_v2)
        : (uint32_t)sizeof(compatible_v3);
    uint32_t reg_size = (uint32_t)sizeof(reg_v3);
    static const char strings[] =
        "#address-cells\0#size-cells\0compatible\0reg\0";
    size_t cursor;
    size_t strings_size = sizeof(strings) - 1U;
    uint32_t address_cells_name = 0U;
    uint32_t size_cells_name = 15U;
    uint32_t compatible_name = 27U;
    uint32_t reg_name = 38U;

    for (cursor = 0U; cursor < sizeof(blob); ++cursor)
    {
        blob[cursor] = 0U;
    }

    /* One FDT header reservation entry followed by its terminator. */
    write_be32(blob + 40U, 0U);
    write_be32(blob + 44U, 0x40000000U);
    write_be32(blob + 48U, 0U);
    write_be32(blob + 52U, 0x00001000U);

    structure_offset = 72U;
    cursor = structure_offset;

    append_u32(&cursor, 1U);
    append_string(&cursor, "");
    address_cells_data_offset = cursor + 12U;
    append_property(&cursor, address_cells_name,
                    address_cells, sizeof(address_cells));
    append_property(&cursor, size_cells_name,
                    size_cells, sizeof(size_cells));

    append_u32(&cursor, 1U);
    append_string(&cursor, "intc");
    append_property(&cursor, compatible_name,
                    compatible, compatible_size);
    reg_data_offset = cursor + 12U;
    append_property(&cursor, reg_name, reg, reg_size);
    append_u32(&cursor, 2U);
    append_u32(&cursor, 2U);
    append_u32(&cursor, 9U);

    strings_offset = (cursor + 3U) & ~(size_t)3U;
    while (cursor < strings_offset)
    {
        blob[cursor++] = 0U;
    }

    for (cursor = 0U; cursor < strings_size; ++cursor)
    {
        blob[strings_offset + cursor] = (uint8_t)strings[cursor];
    }

    write_be32(blob, SYNTRA_FDT_MAGIC);
    write_be32(blob + 4U, (uint32_t)(strings_offset + strings_size));
    write_be32(blob + 8U, (uint32_t)structure_offset);
    write_be32(blob + 12U, (uint32_t)strings_offset);
    write_be32(blob + 16U, 40U);
    write_be32(blob + 20U, 17U);
    write_be32(blob + 24U, 16U);
    write_be32(blob + 28U, 0U);
    write_be32(blob + 32U, (uint32_t)strings_size);
    write_be32(blob + 36U, (uint32_t)(strings_offset - structure_offset));
}

static void test_gic_spi_route_offsets(void)
{
    size_t offset = 0U;

    /* GICD_IROUTER uses the absolute INTID, not an SPI-relative index. */
    assert(syntra_arm64_gic_spi_route_offset(32U, 0x10000U, &offset) ==
           SYNTRA_STATUS_OK);
    assert(offset == 0x6100U);

    assert(syntra_arm64_gic_spi_route_offset(48U, 0x10000U, &offset) ==
           SYNTRA_STATUS_OK);
    assert(offset == 0x6180U);

    assert(syntra_arm64_gic_spi_route_offset(1019U, 0x10000U, &offset) ==
           SYNTRA_STATUS_OK);
    assert(offset == 0x7FF8U);

    assert(syntra_arm64_gic_spi_route_offset(31U, 0x10000U, &offset) ==
           SYNTRA_STATUS_INVALID_ARGUMENT);
    assert(syntra_arm64_gic_spi_route_offset(1020U, 0x10000U, &offset) ==
           SYNTRA_STATUS_INVALID_ARGUMENT);
    assert(syntra_arm64_gic_spi_route_offset(48U, 0x6187U, &offset) ==
           SYNTRA_STATUS_INVALID_ARGUMENT);
    assert(syntra_arm64_gic_spi_route_offset(48U, 0x6188U, &offset) ==
           SYNTRA_STATUS_OK);
    assert(syntra_arm64_gic_spi_route_offset(48U, 0x10000U, NULL) ==
           SYNTRA_STATUS_INVALID_ARGUMENT);
}

static void test_fdt_and_gic_discovery(void)
{
    syntra_fdt_t fdt;
    syntra_fdt_node_t node;
    syntra_arm64_gic_info_t info;
    uintptr_t base;
    size_t size;
    size_t reg_count;
    size_t reservation_count;
    uint64_t reservation_address;
    uint64_t reservation_size;

    build_test_fdt(0);

    assert(syntra_fdt_open(blob, sizeof(blob), &fdt) == SYNTRA_STATUS_OK);
    assert(syntra_fdt_get_reservation_count(
        &fdt, &reservation_count) == SYNTRA_STATUS_OK);
    assert(reservation_count == 1U);
    assert(syntra_fdt_get_reservation(
        &fdt, 0U, &reservation_address, &reservation_size) == SYNTRA_STATUS_OK);
    assert(reservation_address == 0x40000000ULL);
    assert(reservation_size == 0x1000ULL);
    assert(syntra_fdt_get_reservation(
        &fdt, 1U, &reservation_address, &reservation_size) == SYNTRA_STATUS_ERROR);
    assert(syntra_fdt_find_compatible(
        &fdt, "arm,gic-v3", &node) == SYNTRA_STATUS_OK);
    assert(syntra_fdt_get_reg_count(
        &fdt, &node, &reg_count) == SYNTRA_STATUS_OK);
    assert(reg_count == 2U);

    assert(syntra_fdt_get_reg(
        &fdt, &node, 0U, &base, &size) == SYNTRA_STATUS_OK);
    assert(base == (uintptr_t)0x08000000U);
    assert(size == (size_t)0x00010000U);

    assert(syntra_fdt_get_reg(
        &fdt, &node, 1U, &base, &size) == SYNTRA_STATUS_OK);
    assert(base == (uintptr_t)0x080A0000U);
    assert(size == (size_t)0x00F60000U);

    assert(syntra_arm64_gic_discover(&fdt) == SYNTRA_STATUS_OK);
    assert(syntra_arm64_gic_get_info(&info) == SYNTRA_STATUS_OK);
    assert(info.configured == 1);
    assert(info.initialized == 0);
    assert(info.hardware_access_enabled == 0);
    assert(info.config.version == SYNTRA_ARM64_GIC_VERSION_3);
    assert(info.config.distributor_base == (uintptr_t)0x08000000U);
    assert(info.config.redistributor_base == (uintptr_t)0x080A0000U);
    assert(syntra_arm64_gic_init() == SYNTRA_STATUS_NOT_IMPLEMENTED);
    assert(syntra_arm64_gic_get_info(&info) == SYNTRA_STATUS_OK);
    assert(info.initialized == 0);
    assert(info.hardware_access_enabled == 0);
}


static void test_gic_v2_discovery(void)
{
    syntra_fdt_t fdt;
    syntra_arm64_gic_info_t info;

    build_test_fdt(1);
    assert(syntra_fdt_open(blob, sizeof(blob), &fdt) == SYNTRA_STATUS_OK);
    assert(syntra_arm64_gic_discover(&fdt) == SYNTRA_STATUS_OK);
    assert(syntra_arm64_gic_get_info(&info) == SYNTRA_STATUS_OK);
    assert(info.configured == 1);
    assert(info.initialized == 0);
    assert(info.hardware_access_enabled == 0);
    assert(info.config.version == SYNTRA_ARM64_GIC_VERSION_2);
    assert(info.config.distributor_base == (uintptr_t)0x08000000U);
    assert(info.config.cpu_interface_base == (uintptr_t)0x08010000U);
    assert(info.config.cpu_interface_size == (size_t)0x1000U);
    assert(syntra_arm64_gic_init() == SYNTRA_STATUS_NOT_IMPLEMENTED);
    assert(syntra_arm64_gic_get_info(&info) == SYNTRA_STATUS_OK);
    assert(info.initialized == 0);
    assert(info.hardware_access_enabled == 0);
}

static void test_rejects_invalid_reg_index_and_truncation(void)
{
    syntra_fdt_t fdt;
    syntra_fdt_node_t node;
    uintptr_t base;
    size_t size;
    size_t reg_count;

    build_test_fdt(0);
    assert(syntra_fdt_open(blob, sizeof(blob), &fdt) == SYNTRA_STATUS_OK);
    assert(syntra_fdt_find_compatible(
        &fdt, "arm,gic-v3", &node) == SYNTRA_STATUS_OK);

    assert(syntra_fdt_get_reg(
        &fdt, &node, 2U, &base, &size) == SYNTRA_STATUS_ERROR);
    assert(syntra_fdt_get_reg_count(
        &fdt, &node, &reg_count) == SYNTRA_STATUS_OK);
    assert(reg_count == 2U);
    assert(syntra_fdt_open(blob, 20U, &fdt) == SYNTRA_STATUS_INVALID_ARGUMENT);
}


static void test_rejects_invalid_cell_properties(void)
{
    syntra_fdt_t fdt;
    syntra_fdt_node_t node;
    uintptr_t base;
    size_t size;
    size_t reg_count;

    build_test_fdt(0);
    write_be32(blob + address_cells_data_offset, 3U);
    assert(syntra_fdt_open(blob, sizeof(blob), &fdt) == SYNTRA_STATUS_OK);
    assert(syntra_fdt_find_compatible(
        &fdt, "arm,gic-v3", &node) == SYNTRA_STATUS_OK);
    assert(syntra_fdt_get_reg(
        &fdt, &node, 0U, &base, &size) ==
        SYNTRA_STATUS_NOT_IMPLEMENTED);

    build_test_fdt(0);
    write_be32(blob + address_cells_data_offset - 8U, 3U);
    assert(syntra_fdt_open(blob, sizeof(blob), &fdt) == SYNTRA_STATUS_OK);
    assert(syntra_fdt_find_compatible(
        &fdt, "arm,gic-v3", &node) == SYNTRA_STATUS_OK);
    assert(syntra_fdt_get_reg(
        &fdt, &node, 0U, &base, &size) == SYNTRA_STATUS_ERROR);
    assert(syntra_fdt_get_reg_count(
        &fdt, &node, &reg_count) == SYNTRA_STATUS_ERROR);
}

static void test_rejects_malformed_reg_tuple(void)
{
    syntra_fdt_t fdt;
    syntra_fdt_node_t node;
    uintptr_t base;
    size_t size;
    size_t reg_count;

    build_test_fdt(0);
    write_be32(blob + reg_data_offset - 8U, 31U);
    assert(syntra_fdt_open(blob, sizeof(blob), &fdt) == SYNTRA_STATUS_OK);
    assert(syntra_fdt_find_compatible(
        &fdt, "arm,gic-v3", &node) == SYNTRA_STATUS_OK);
    assert(syntra_fdt_get_reg(
        &fdt, &node, 0U, &base, &size) == SYNTRA_STATUS_ERROR);
    assert(syntra_fdt_get_reg_count(
        &fdt, &node, &reg_count) == SYNTRA_STATUS_ERROR);
}

static void test_rejects_reservation_overflow(void)
{
    syntra_fdt_t fdt;

    build_test_fdt(0);
    write_be32(blob + 40U, 0xFFFFFFFFU);
    write_be32(blob + 44U, 0xFFFFF000U);
    write_be32(blob + 48U, 0U);
    write_be32(blob + 52U, 0x00002000U);
    assert(syntra_fdt_open(blob, sizeof(blob), &fdt) == SYNTRA_STATUS_ERROR);
}

static void test_rejects_bad_magic(void)
{
    syntra_fdt_t fdt;

    blob[0] = 0U;
    assert(syntra_fdt_open(blob, sizeof(blob), &fdt) == SYNTRA_STATUS_ERROR);
}

static void test_rejects_overlapping_blocks(void)
{
    syntra_fdt_t fdt;

    build_test_fdt(0);
    write_be32(blob + 12U, (uint32_t)structure_offset);
    assert(syntra_fdt_open(blob, sizeof(blob), &fdt) == SYNTRA_STATUS_ERROR);
}

int main(void)
{
    test_gic_spi_route_offsets();
    test_fdt_and_gic_discovery();
    test_gic_v2_discovery();
    test_rejects_invalid_reg_index_and_truncation();
    test_rejects_invalid_cell_properties();
    test_rejects_malformed_reg_tuple();
    test_rejects_reservation_overflow();
    test_rejects_bad_magic();
    test_rejects_overlapping_blocks();
    puts("FDT/GIC discovery tests passed");
    return 0;
}
