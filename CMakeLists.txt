
cmake_minimum_required(VERSION 3.20)

project(SyntraOS
    VERSION 0.1.0
    LANGUAGES C
)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# =================================
# KERNEL LIBRARY
# =================================

add_library(syntra_kernel STATIC

    kernel/core/kernel.c
    kernel/core/log.c
    kernel/core/panic.c

    kernel/scheduler/scheduler.c
    kernel/memory/memory.c
    kernel/process/process.c
    kernel/ipc/ipc.c
)

target_include_directories(syntra_kernel PUBLIC

    kernel/core
    kernel/scheduler
    kernel/memory
    kernel/process
    kernel/ipc
)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")

    target_compile_options(syntra_kernel PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )

endif()

# =================================
# TEST SYSTEM
# =================================

include(CTest)

if(BUILD_TESTING)

    enable_testing()

    # Scheduler Test

    add_executable(scheduler_test
        tests/scheduler/scheduler_test.c
    )

    target_link_libraries(scheduler_test
        PRIVATE syntra_kernel
    )

    add_test(
        NAME SchedulerTest
        COMMAND scheduler_test
    )

    # Memory Test

    add_executable(memory_test
        tests/memory/memory_test.c
    )

    target_link_libraries(memory_test
        PRIVATE syntra_kernel
    )

    add_test(
        NAME MemoryTest
        COMMAND memory_test
    )

    # Process Test

    add_executable(process_test
        tests/process/process_test.c
    )

    target_link_libraries(process_test
        PRIVATE syntra_kernel
    )

    add_test(
        NAME ProcessTest
        COMMAND process_test
    )

    # IPC Test

    add_executable(ipc_test
        tests/ipc/ipc_test.c
    )

    target_link_libraries(ipc_test
        PRIVATE syntra_kernel
    )

    add_test(
        NAME IPCTest
        COMMAND ipc_test
    )

    # Kernel Integration Test

    add_executable(kernel_test
        tests/kernel/kernel_test.c
    )

    target_link_libraries(kernel_test
        PRIVATE syntra_kernel
    )

    add_test(
        NAME KernelIntegrationTest
        COMMAND kernel_test
    )

    # Kernel Core Test

    add_executable(core_test
        tests/core/core_test.c
    )

    target_link_libraries(core_test
        PRIVATE syntra_kernel
    )

    add_test(
        NAME KernelCoreTest
        COMMAND core_test
    )

    # Logging V2 Test

    add_executable(log_test
        tests/core/log_test.c
    )

    target_link_libraries(log_test
        PRIVATE syntra_kernel
    )

    add_test(
        NAME LoggingV2Test
        COMMAND log_test
    )

endif()
