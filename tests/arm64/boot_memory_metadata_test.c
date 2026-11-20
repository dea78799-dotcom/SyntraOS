#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "../../kernel/arch/arm64/boot/memory_map.h"

static void test_metadata_round_trip(void)
{
    const syntra_arm64_boot_memory_metadata_t expected = {
        0x40200000U, 0x40210000U,
        0x40210000U, 0x40214000U,
        0x40000000U, 0x1000U
    };
    syntra_arm64_boot_memory_metadata_t actual;

    assert(syntra_arm64_boot_memory_metadata_set(&expected) ==
           SYNTRA_STATUS_OK);
    assert(syntra_arm64_boot_memory_metadata_get(&actual) ==
           SYNTRA_STATUS_OK);
    assert(actual.kernel_start == expected.kernel_start);
    assert(actual.kernel_end == expected.kernel_end);
    assert(actual.stack_bottom == expected.stack_bottom);
    assert(actual.stack_top == expected.stack_top);
    assert(actual.dtb_start == expected.dtb_start);
    assert(actual.dtb_size == expected.dtb_size);
}

static void test_metadata_rejects_invalid_ranges(void)
{
    syntra_arm64_boot_memory_metadata_t value = {
        0x40200000U, 0x40210000U,
        0x40210000U, 0x40214000U,
        0x40000000U, 0x1000U
    };

    value.stack_top = value.stack_bottom;
    assert(syntra_arm64_boot_memory_metadata_set(&value) ==
           SYNTRA_STATUS_INVALID_ARGUMENT);

    value.stack_top = 0x40214000U;
    value.dtb_start = value.kernel_start;
    assert(syntra_arm64_boot_memory_metadata_set(&value) ==
           SYNTRA_STATUS_INVALID_ARGUMENT);

    value.dtb_start = 0x40000000U;
    value.dtb_size = 0U;
    assert(syntra_arm64_boot_memory_metadata_set(&value) ==
           SYNTRA_STATUS_INVALID_ARGUMENT);
}

int main(void)
{
    test_metadata_round_trip();
    test_metadata_rejects_invalid_ranges();
    puts("ARM64 boot memory metadata tests passed");
    return 0;
}
