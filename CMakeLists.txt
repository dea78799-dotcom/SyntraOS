
cmake_minimum_required(VERSION 3.20)

project(SyntraOS
    VERSION 0.1.0
    LANGUAGES C
)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CMAKE_C_EXTENSIONS OFF)

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
