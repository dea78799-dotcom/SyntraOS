
cmake_minimum_required(VERSION 3.20)

project(SyntraOS
    VERSION 0.1.0
    LANGUAGES C
)

# =================================
# C LANGUAGE CONFIGURATION
# =================================

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

# =================================
# COMPILER WARNINGS
# =================================

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")

    target_compile_options(syntra_kernel PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )

endif()

# =================================
# TESTING CONFIGURATION
# =================================

include(CTest)

if(BUILD_TESTING)

    enable_testing()

    # Scheduler Unit Test

    add_executable(scheduler_test

        tests/scheduler/scheduler_test.c

    )

    target_link_libraries(scheduler_test
        PRIVATE syntra_kernel
    )

    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")

        target_compile_options(scheduler_test PRIVATE
            -Wall
            -Wextra
            -Wpedantic
        )

    endif()

    add_test(
        NAME SchedulerTest
        COMMAND scheduler_test
    )

endif()
