#include <stdio.h>
#include <stddef.h>
#include <stdint.h>

#include "../../kernel/scheduler/scheduler.h"

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

static void dummy_thread(void *argument)
{
    (void)argument;
}

static _Alignas(16) unsigned char stack_a[2048];
static _Alignas(16) unsigned char stack_b[2048];

int main(void)
{
    printf("\n=================================\n");
    printf(" SYNTRAOS SCHEDULER V1 TEST\n");
    printf("=================================\n");

    check("Initialize scheduler",
        scheduler_init() == SYNTRA_STATUS_OK);
    check("Add metadata thread 1",
        scheduler_add_thread(1U, 5U) == SYNTRA_STATUS_OK);
    check("Reject duplicate ID",
        scheduler_add_thread(1U, 3U) == SYNTRA_STATUS_INVALID_ARGUMENT);
    check("Create thread with caller stack",
        scheduler_create_thread(2U, 99U, dummy_thread, NULL,
            stack_a, sizeof(stack_a)) == SYNTRA_STATUS_OK);
    check("Reject unaligned stack",
        scheduler_create_thread(3U, 1U, dummy_thread, NULL,
            &stack_b[1], sizeof(stack_b) - 1U)
            == SYNTRA_STATUS_INVALID_ARGUMENT);
    check("Reject undersized stack",
        scheduler_create_thread(3U, 1U, dummy_thread, NULL,
            stack_b, 512U) == SYNTRA_STATUS_INVALID_ARGUMENT);

    syntra_thread_t *thread = scheduler_get_thread(2U);
    check("Store entry and stack metadata",
        thread != NULL && thread->entry == dummy_thread &&
        thread->stack_base == (uintptr_t)stack_a &&
        thread->stack_size == sizeof(stack_a));

    thread = scheduler_schedule_next();
    check("Round-robin selects first READY thread",
        thread != NULL && thread->thread_id == 1U &&
        thread->state == THREAD_RUNNING);

    thread = scheduler_schedule_next();
    check("Round-robin selects next thread, not highest priority",
        thread != NULL && thread->thread_id == 2U &&
        thread->state == THREAD_RUNNING);

    thread = scheduler_schedule_next();
    check("Round-robin wraps to first thread",
        thread != NULL && thread->thread_id == 1U);

    check("Block current thread",
        scheduler_block_current() == SYNTRA_STATUS_OK);
    check("Blocked thread is not selected",
        scheduler_schedule_next() != NULL &&
        scheduler_get_current()->thread_id == 2U);
    check("Wake blocked thread",
        scheduler_wake_thread(1U) == SYNTRA_STATUS_OK);

    check("Terminate current thread",
        scheduler_terminate_current() == SYNTRA_STATUS_OK);
    check("Terminated thread remains terminated",
        scheduler_get_thread(2U)->state == THREAD_TERMINATED);
    check("Next READY thread is scheduled",
        scheduler_schedule_next() != NULL &&
        scheduler_get_current()->thread_id == 1U);

    check("Thread count remains consistent",
        scheduler_get_thread_count() == 2U);
    check("Scheduler invariants",
        scheduler_validate() == SYNTRA_STATUS_OK);


    {
        static _Alignas(16) unsigned char user_kernel_stack[2048];
        static _Alignas(16) unsigned char user_stack[2048];
        static _Alignas(16) unsigned char user_kernel_stack_2[2048];
        static _Alignas(16) unsigned char user_stack_2[2048];
        static _Alignas(16) unsigned char fake_context[320];
        syntra_thread_t *user_thread;

        check("Create user thread with separate stacks and prepared context",
            scheduler_create_user_thread(
                4U, 1U, dummy_thread, NULL,
                user_kernel_stack, sizeof(user_kernel_stack),
                user_stack, sizeof(user_stack),
                fake_context) == SYNTRA_STATUS_OK);

        user_thread = scheduler_get_thread(4U);
        check("Store kernel stack and user stack independently",
            user_thread != NULL &&
            user_thread->stack_base == (uintptr_t)user_kernel_stack &&
            user_thread->stack_size == sizeof(user_kernel_stack) &&
            user_thread->user_stack_base == (uintptr_t)user_stack &&
            user_thread->user_stack_size == sizeof(user_stack));

        check("Mark user thread and preserve prepared CPU context",
            user_thread != NULL && user_thread->is_user_thread == 1U &&
            user_thread->cpu_context == fake_context);

        check("Reject overlapping kernel and user stacks",
            scheduler_create_user_thread(
                5U, 1U, dummy_thread, NULL,
                user_kernel_stack, sizeof(user_kernel_stack),
                user_kernel_stack, sizeof(user_kernel_stack),
                fake_context) == SYNTRA_STATUS_INVALID_ARGUMENT);

        check("Reject user stack overlapping an existing thread stack",
            scheduler_create_user_thread(
                5U, 1U, dummy_thread, NULL,
                user_kernel_stack_2, sizeof(user_kernel_stack_2),
                stack_a, sizeof(stack_a),
                fake_context) == SYNTRA_STATUS_INVALID_ARGUMENT);

        check("Reject user thread without a prepared context",
            scheduler_create_user_thread(
                5U, 1U, dummy_thread, NULL,
                user_kernel_stack_2, sizeof(user_kernel_stack_2),
                user_stack_2, sizeof(user_stack_2),
                NULL) == SYNTRA_STATUS_INVALID_ARGUMENT);

        check("Scheduler validates user stack metadata",
            scheduler_validate() == SYNTRA_STATUS_OK);
        check("User thread remains READY until explicitly scheduled",
            user_thread != NULL && user_thread->state == THREAD_READY);
    }

    printf("\n=================================\n");
    printf(" Total failures: %d\n", failures);
    printf("=================================\n");
    return failures == 0 ? 0 : 1;
}
