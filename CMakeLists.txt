
cmake_minimum_required(VERSION 3.20)

project(SyntraOS
    VERSION 0.1.0
    LANGUAGES C
)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# ==================================================
# Kernel Library
# ==================================================

add_library(syntra_kernel STATIC

    kernel/core/kernel.c
    kernel/core/log.c
    kernel/core/panic.c

    kernel/scheduler/scheduler.c
    kernel/memory/memory.c
    kernel/process/process.c
    kernel/ipc/ipc.c

    kernel/interrupt/interrupt.c
    kernel/exception/exception.c
    kernel/timer/timer.c
    kernel/clock/clock.c
    kernel/uptime/uptime.c
    kernel/config/config.c
)

target_include_directories(syntra_kernel PUBLIC

    kernel/core
    kernel/scheduler
    kernel/memory
    kernel/process
    kernel/ipc
    kernel/interrupt
    kernel/exception
    kernel/timer
    kernel/clock
    kernel/uptime
    kernel/config
)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")

    target_compile_options(syntra_kernel PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )

endif()

# ==================================================
# Testing
# ==================================================

enable_testing()

# Scheduler Test

add_executable(scheduler_test
    tests/scheduler/scheduler_test.c
)

target_link_libraries(scheduler_test PRIVATE syntra_kernel)

add_test(NAME SchedulerTest COMMAND scheduler_test)

# Memory Test

add_executable(memory_test
    tests/memory/memory_test.c
)

target_link_libraries(memory_test PRIVATE syntra_kernel)

add_test(NAME MemoryTest COMMAND memory_test)

# Process Test

add_executable(process_test
    tests/process/process_test.c
)

target_link_libraries(process_test PRIVATE syntra_kernel)

add_test(NAME ProcessTest COMMAND process_test)

# IPC Test

add_executable(ipc_test
    tests/ipc/ipc_test.c
)

target_link_libraries(ipc_test PRIVATE syntra_kernel)

add_test(NAME IPCTest COMMAND ipc_test)

# Kernel Integration Test

add_executable(kernel_test
    tests/kernel/kernel_test.c
)

target_link_libraries(kernel_test PRIVATE syntra_kernel)

add_test(NAME KernelIntegrationTest COMMAND kernel_test)

# Kernel Core Test

add_executable(core_test
    tests/core/core_test.c
)

target_link_libraries(core_test PRIVATE syntra_kernel)

add_test(NAME KernelCoreTest COMMAND core_test)

# Logging V5 Test

add_executable(log_test
    tests/core/log_test.c
)

target_link_libraries(log_test PRIVATE syntra_kernel)

add_test(NAME LoggingV5Test COMMAND log_test)

# Kernel Panic V2 Test

add_executable(panic_test
    tests/core/panic_test.c
)

target_link_libraries(panic_test PRIVATE syntra_kernel)

add_test(NAME KernelPanicV2Test COMMAND panic_test)

# Interrupt Manager V1 Test

add_executable(interrupt_test
    tests/interrupt/interrupt_test.c
)

target_link_libraries(interrupt_test PRIVATE syntra_kernel)

add_test(NAME InterruptManagerV1Test COMMAND interrupt_test)

# Exception Manager V1 Test

add_executable(exception_test
    tests/exception/exception_test.c
)

target_link_libraries(exception_test PRIVATE syntra_kernel)

add_test(NAME ExceptionManagerV1Test COMMAND exception_test)

# System Timer V1 Test

add_executable(timer_test
    tests/timer/timer_test.c
)

target_link_libraries(timer_test PRIVATE syntra_kernel)

add_test(NAME SystemTimerV1Test COMMAND timer_test)

# Clock Manager V1 Test

add_executable(clock_test
    tests/clock/clock_test.c
)

target_link_libraries(clock_test PRIVATE syntra_kernel)

add_test(NAME ClockManagerV1Test COMMAND clock_test)

# System Uptime V1 Test

add_executable(uptime_test
    tests/uptime/uptime_test.c
)

target_link_libraries(uptime_test PRIVATE syntra_kernel)

add_test(NAME SystemUptimeV1Test COMMAND uptime_test)

# System Configuration V1 Test

add_executable(config_test
    tests/config/config_test.c
)

target_link_libraries(config_test PRIVATE syntra_kernel)

add_test(NAME SystemConfigurationV1Test COMMAND config_test)
