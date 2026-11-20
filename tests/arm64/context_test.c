#include <stdio.h>
#include <stdint.h>
#include <stddef.h>

#include "../../kernel/arch/arm64/context.h"

static int failures;

static void check(const char *name, int condition)
{
    if (condition)
    {
        printf("[PASS] %s\n", name);
    }
    else
    {
        printf("[FAIL] %s\n", name);
        ++failures;
    }
}

static void test_entry(void *argument)
{
    (void)argument;
}

static _Alignas(16) unsigned char test_stack[4096];
static int test_argument;

int main(void)
{
    syntra_arm64_exception_context_t *context = NULL;

    check("Initialize initial ARM64 thread frame",
        syntra_arm64_context_initialize(
            test_stack, sizeof(test_stack), test_entry,
            &test_argument, &context) == SYNTRA_STATUS_OK);

    check("Frame pointer is 16-byte aligned",
        context != NULL &&
        (((uintptr_t)context & 15U) == 0U));

    check("Entry PC is initialized",
        context != NULL && context->elr_el1 != 0U);

    check("Argument is passed in x0",
        context != NULL &&
        context->registers[0] == (uint64_t)(uintptr_t)&test_argument);

    check("Return trampoline is initialized",
        context != NULL && context->registers[30] != 0U);

    check("Initial frame uses EL1h state",
        context != NULL && context->spsr_el1 == 0x5U);

    check("Initial frame carries valid exception-return marker",
        context != NULL &&
        context->reserved == SYNTRA_ARM64_IRQ_RETURN_MAGIC);

    check("Initial frame uses current-SPx IRQ return slot",
        context != NULL &&
        context->vector_index == SYNTRA_ARM64_VECTOR_CURRENT_SPX_IRQ);

    check("Reject unaligned stack",
        syntra_arm64_context_initialize(
            &test_stack[1], sizeof(test_stack) - 1U, test_entry,
            NULL, &context) == SYNTRA_STATUS_INVALID_ARGUMENT);

    check("Reject undersized stack",
        syntra_arm64_context_initialize(
            test_stack, SYNTRA_ARM64_CONTEXT_FRAME_SIZE - 1U,
            test_entry, NULL, &context)
            == SYNTRA_STATUS_INVALID_ARGUMENT);

    check("Reject null entry",
        syntra_arm64_context_initialize(
            test_stack, sizeof(test_stack), NULL,
            NULL, &context) == SYNTRA_STATUS_INVALID_ARGUMENT);

    {
        static _Alignas(16) unsigned char user_stack[2048];
        static _Alignas(16) unsigned char kernel_stack[2048];
        syntra_arm64_exception_context_t *user_context = NULL;

        check("Prepare separate EL0 user and EL1 kernel stacks",
            syntra_arm64_user_context_initialize(
                kernel_stack, sizeof(kernel_stack),
                user_stack, sizeof(user_stack),
                test_entry, syntra_arm64_thread_exit,
                &test_argument, &user_context) == SYNTRA_STATUS_OK);
        check("EL0 frame is stored on the kernel stack",
            user_context != NULL &&
            (uintptr_t)user_context >= (uintptr_t)kernel_stack &&
            (uintptr_t)user_context <
                (uintptr_t)kernel_stack + sizeof(kernel_stack));
        check("EL0 stack pointer is aligned and uses user stack",
            user_context != NULL &&
            (user_context->sp_el0 & 15U) == 0U &&
            user_context->sp_el0 ==
                (uint64_t)(uintptr_t)(user_stack + sizeof(user_stack)));
        check("Initial EL0 return state masks DAIF",
            user_context != NULL && user_context->spsr_el1 == 0x3C0U);
        check("EL0 entry and return trampoline are populated",
            user_context != NULL &&
            user_context->elr_el1 == (uint64_t)(uintptr_t)test_entry &&
            user_context->registers[30] != 0U);
        check("EL0 frame has the validated exception-return marker",
            user_context != NULL &&
            user_context->reserved == SYNTRA_ARM64_EXCEPTION_RETURN_MAGIC);
        check("Reject overlapping user and kernel stacks",
            syntra_arm64_user_context_initialize(
                kernel_stack, sizeof(kernel_stack),
                kernel_stack, sizeof(kernel_stack),
                test_entry, syntra_arm64_thread_exit,
                NULL, &user_context) == SYNTRA_STATUS_INVALID_ARGUMENT);
    }

    printf("\nARM64 context frame failures: %d\n", failures);
    return failures == 0 ? 0 : 1;
}
