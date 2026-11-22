#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "../../kernel/arch/arm64/kernel_va_registry.h"

#ifdef NDEBUG
#undef NDEBUG
#endif

static void test_validation_and_guard_layout(void)
{
    syntra_arm64_kernel_va_reservation_t result = {0xA5U};
    syntra_arm64_kernel_va_reservation_t first;
    syntra_arm64_kernel_va_reservation_t second;

    assert(syntra_arm64_kernel_va_registry_init() == SYNTRA_STATUS_OK);
    assert(syntra_arm64_kernel_va_reserve(0U, 0U, 1U, &result) ==
        SYNTRA_STATUS_INVALID_ARGUMENT);
    assert(result.allocation_id == 0xA5U);
    assert(syntra_arm64_kernel_va_reserve(4096U, 0U, 0U, &result) ==
        SYNTRA_STATUS_INVALID_ARGUMENT);
    assert(syntra_arm64_kernel_va_reserve(UINT64_MAX, 0U, 1U, &result) ==
        SYNTRA_STATUS_INVALID_ARGUMENT);

    assert(syntra_arm64_kernel_va_reserve(
        5000U, 1U, 1U, &first) == SYNTRA_STATUS_OK);
    assert(first.reservation_base == SYNTRA_ARM64_KERNEL_VA_BASE);
    assert(first.reservation_size == 12288U);
    assert(first.usable_base == SYNTRA_ARM64_KERNEL_VA_BASE + 4096U);
    assert(first.usable_size == 8192U);
    assert(first.requested_size == 5000U);
    assert(first.guard_pages == 1U);

    assert(syntra_arm64_kernel_va_reserve(
        4096U, 0U, 2U, &second) == SYNTRA_STATUS_OK);
    assert(second.reservation_base == first.reservation_base +
        first.reservation_size);
    assert(second.usable_base == second.reservation_base);
    assert(syntra_arm64_kernel_va_registry_validate() == SYNTRA_STATUS_OK);
    assert(syntra_arm64_kernel_va_registry_count() == 2U);

    assert(syntra_arm64_kernel_va_release(&first) == SYNTRA_STATUS_OK);
    assert(syntra_arm64_kernel_va_release(&first) ==
        SYNTRA_STATUS_INVALID_ARGUMENT);
    assert(syntra_arm64_kernel_va_registry_validate() == SYNTRA_STATUS_OK);

    /* Released first-fit space is reused; the ID remains unique. */
    {
        syntra_arm64_kernel_va_reservation_t reused;
        assert(syntra_arm64_kernel_va_reserve(
            4096U, 0U, 3U, &reused) == SYNTRA_STATUS_OK);
        assert(reused.reservation_base == first.reservation_base);
        assert(reused.allocation_id != first.allocation_id);
        assert(syntra_arm64_kernel_va_release(&reused) == SYNTRA_STATUS_OK);
    }
    assert(syntra_arm64_kernel_va_release(&second) == SYNTRA_STATUS_OK);
    assert(syntra_arm64_kernel_va_registry_count() == 0U);
}

static void test_descriptor_integrity(void)
{
    syntra_arm64_kernel_va_reservation_t reservation;
    syntra_arm64_kernel_va_reservation_t altered;

    assert(syntra_arm64_kernel_va_reserve(
        8192U, 2U, 4U, &reservation) == SYNTRA_STATUS_OK);
    altered = reservation;
    altered.usable_size += 4096U;
    assert(syntra_arm64_kernel_va_release(&altered) ==
        SYNTRA_STATUS_INVALID_ARGUMENT);
    assert(syntra_arm64_kernel_va_registry_count() == 1U);
    assert(syntra_arm64_kernel_va_registry_validate() == SYNTRA_STATUS_OK);
    assert(syntra_arm64_kernel_va_release(&reservation) == SYNTRA_STATUS_OK);
}

static void test_arena_exhaustion(void)
{
    syntra_arm64_kernel_va_reservation_t whole;
    syntra_arm64_kernel_va_reservation_t untouched = {0x5AU};

    assert(syntra_arm64_kernel_va_reserve(
        SYNTRA_ARM64_KERNEL_VA_SIZE, 0U, 5U, &whole) == SYNTRA_STATUS_OK);
    assert(whole.reservation_base == SYNTRA_ARM64_KERNEL_VA_BASE);
    assert(whole.reservation_size == SYNTRA_ARM64_KERNEL_VA_SIZE);
    assert(whole.reservation_base + whole.reservation_size ==
        SYNTRA_ARM64_KERNEL_VA_LIMIT);
    assert(syntra_arm64_kernel_va_reserve(
        4096U, 0U, 6U, &untouched) == SYNTRA_STATUS_OUT_OF_MEMORY);
    assert(untouched.allocation_id == 0x5AU);
    assert(syntra_arm64_kernel_va_registry_validate() == SYNTRA_STATUS_OK);
    assert(syntra_arm64_kernel_va_release(&whole) == SYNTRA_STATUS_OK);
}

static void test_registry_capacity(void)
{
    syntra_arm64_kernel_va_reservation_t allocations[
        SYNTRA_ARM64_KERNEL_VA_REGISTRY_CAPACITY];
    syntra_arm64_kernel_va_reservation_t extra = {0xCCU};
    size_t index;

    for (index = 0U; index < SYNTRA_ARM64_KERNEL_VA_REGISTRY_CAPACITY;
         ++index)
    {
        assert(syntra_arm64_kernel_va_reserve(
            4096U, 0U, 7U, &allocations[index]) == SYNTRA_STATUS_OK);
    }

    assert(syntra_arm64_kernel_va_registry_count() ==
        SYNTRA_ARM64_KERNEL_VA_REGISTRY_CAPACITY);
    assert(syntra_arm64_kernel_va_reserve(
        4096U, 0U, 8U, &extra) == SYNTRA_STATUS_OUT_OF_MEMORY);
    assert(extra.allocation_id == 0xCCU);
    assert(syntra_arm64_kernel_va_registry_validate() == SYNTRA_STATUS_OK);

    for (index = 0U; index < SYNTRA_ARM64_KERNEL_VA_REGISTRY_CAPACITY;
         ++index)
    {
        assert(syntra_arm64_kernel_va_release(
            &allocations[index]) == SYNTRA_STATUS_OK);
    }
    assert(syntra_arm64_kernel_va_registry_count() == 0U);
    assert(syntra_arm64_kernel_va_registry_validate() == SYNTRA_STATUS_OK);
}

static void test_slot_snapshot(void)
{
    syntra_arm64_kernel_va_reservation_t reservation;
    syntra_arm64_kernel_va_reservation_t snapshot;

    assert(syntra_arm64_kernel_va_reserve(
        4096U, 0U, 9U, &reservation) == SYNTRA_STATUS_OK);
    assert(syntra_arm64_kernel_va_registry_get(
        0U, &snapshot) == SYNTRA_STATUS_OK);
    assert(snapshot.allocation_id == reservation.allocation_id);
    assert(syntra_arm64_kernel_va_registry_get(
        SYNTRA_ARM64_KERNEL_VA_REGISTRY_CAPACITY, &snapshot) ==
        SYNTRA_STATUS_INVALID_ARGUMENT);
    assert(syntra_arm64_kernel_va_release(&reservation) == SYNTRA_STATUS_OK);
}

int main(void)
{
    test_validation_and_guard_layout();
    test_descriptor_integrity();
    test_arena_exhaustion();
    test_registry_capacity();
    test_slot_snapshot();
    puts("ARM64 kernel VA reservation registry tests passed");
    return 0;
}
