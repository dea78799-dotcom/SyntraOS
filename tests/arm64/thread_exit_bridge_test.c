#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "../../kernel/arch/arm64/exception.h"
#include "../../kernel/interrupt/interrupt.h"
#include "../../kernel/scheduler/scheduler.h"

static unsigned int failures;
static _Alignas(16) unsigned char worker_stack[2048];
static _Alignas(16) syntra_arm64_exception_context_t worker_context;

static void check(const char *name, int condition)
{
    printf("%s: %s\n", condition ? "PASS" : "FAIL", name);
    if (!condition) ++failures;
}

static syntra_status_t fake_gic_initialize(void *context)
{
    (void)context;
    return SYNTRA_STATUS_OK;
}

static syntra_status_t fake_gic_enable(void *context, syntra_interrupt_id_t id)
{
    (void)context; (void)id;
    return SYNTRA_STATUS_OK;
}

static syntra_status_t fake_gic_disable(void *context, syntra_interrupt_id_t id)
{
    (void)context; (void)id;
    return SYNTRA_STATUS_OK;
}

static syntra_status_t fake_gic_priority(
    void *context, syntra_interrupt_id_t id, unsigned int priority)
{
    (void)context; (void)id; (void)priority;
    return SYNTRA_STATUS_OK;
}

static syntra_status_t fake_gic_acknowledge(
    void *context, syntra_interrupt_id_t *id)
{
    (void)context;
    if (id == NULL) return SYNTRA_STATUS_INVALID_ARGUMENT;
    *id = SYNTRA_INTERRUPT_SPURIOUS_ID;
    return SYNTRA_STATUS_OK;
}

static syntra_status_t fake_gic_eoi(void *context, syntra_interrupt_id_t id)
{
    (void)context; (void)id;
    return SYNTRA_STATUS_OK;
}

static void worker_entry(void *argument)
{
    (void)argument;
}

int main(void)
{
    const syntra_interrupt_controller_ops_t gic = {
        fake_gic_initialize,
        fake_gic_enable,
        fake_gic_disable,
        fake_gic_priority,
        fake_gic_acknowledge,
        fake_gic_eoi
    };
    syntra_arm64_exception_context_t start_frame;
    syntra_arm64_exception_context_t irq_frame;
    syntra_arm64_exception_context_t *selected;

    memset(&start_frame, 0, sizeof(start_frame));
    memset(&irq_frame, 0, sizeof(irq_frame));
    memset(&worker_context, 0, sizeof(worker_context));

    check("Initialize scheduler", scheduler_init() == SYNTRA_STATUS_OK);
    check("Register boot thread", scheduler_add_thread(1U, 0U) == SYNTRA_STATUS_OK);
    check("Block boot thread",
        scheduler_set_thread_state(1U, THREAD_BLOCKED) == SYNTRA_STATUS_OK);
    check("Register worker thread",
        scheduler_create_thread(2U, 0U, worker_entry, NULL,
            worker_stack, sizeof(worker_stack)) == SYNTRA_STATUS_OK);
    check("Attach fake GIC",
        interrupt_init() == SYNTRA_STATUS_OK &&
        interrupt_controller_attach(&gic, NULL) == SYNTRA_STATUS_OK &&
        interrupt_controller_init() == SYNTRA_STATUS_OK);

    start_frame.vector_index = SYNTRA_ARM64_VECTOR_CURRENT_SPX_SYNC;
    start_frame.spsr_el1 = 0x5U;
    start_frame.esr_el1 = (0x15ULL << SYNTRA_ARM64_EXCEPTION_CLASS_SHIFT) |
        SYNTRA_ARM64_SCHEDULER_START_SVC_IMMEDIATE;
    selected = syntra_arm64_exception_handle_and_select(&start_frame);
    check("Scheduler start SVC selects worker context",
        selected != NULL && selected != &start_frame);
    check("Worker becomes current",
        scheduler_get_current() != NULL &&
        scheduler_get_current()->thread_id == 2U);

    check("Terminate selected worker",
        scheduler_set_thread_state(2U, THREAD_TERMINATED) == SYNTRA_STATUS_OK);

    irq_frame.vector_index = SYNTRA_ARM64_VECTOR_CURRENT_SPX_IRQ;
    irq_frame.spsr_el1 = 0x5U;
    irq_frame.esr_el1 = (0x25ULL << SYNTRA_ARM64_EXCEPTION_CLASS_SHIFT);
    irq_frame.reserved = SYNTRA_ARM64_IRQ_RETURN_MAGIC;
    selected = syntra_arm64_exception_handle_and_select(&irq_frame);

    check("No-successor IRQ bridge returns only the current frame for halt",
        selected == &irq_frame);
    check("No-successor IRQ bridge invalidates exception-return cookie",
        irq_frame.reserved == 0U);
    check("No terminated worker remains current",
        scheduler_get_current() == NULL);
    check("Terminated worker stays terminal",
        scheduler_get_thread(2U) != NULL &&
        scheduler_get_thread(2U)->state == THREAD_TERMINATED);
    check("Scheduler invariants hold after no-successor IRQ",
        scheduler_validate() == SYNTRA_STATUS_OK);

    printf("ARM64 thread-exit bridge failures: %u\n", failures);
    return failures == 0U ? 0 : 1;
}
