cmake_minimum_required(VERSION 3.20)

project(SyntraOS_ARM64
    VERSION 0.1.0
    LANGUAGES C ASM
)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# ==================================================
# Repository paths
# ==================================================

get_filename_component(
    SYNTRAOS_ROOT
    "${CMAKE_CURRENT_LIST_DIR}/../.."
    ABSOLUTE
)

set(SYNTRAOS_ARM64_DIR
    "${SYNTRAOS_ROOT}/kernel/arch/arm64"
)

# ==================================================
# ARM64 Kernel ELF
# ==================================================

add_executable(syntraos_arm64

    "${SYNTRAOS_ARM64_DIR}/boot/entry.S"
    "${SYNTRAOS_ARM64_DIR}/boot/vectors.S"
    "${SYNTRAOS_ARM64_DIR}/boot/user_entry.S"
    "${SYNTRAOS_ARM64_DIR}/boot/user_program.S"
    "${SYNTRAOS_ARM64_DIR}/boot/boot.c"
    "${SYNTRAOS_ARM64_DIR}/boot/fdt.c"
    "${SYNTRAOS_ARM64_DIR}/boot/memory_map.c"
    "${SYNTRAOS_ROOT}/kernel/memory/memory_region.c"
    "${SYNTRAOS_ROOT}/kernel/memory/page_allocator.c"

    "${SYNTRAOS_ARM64_DIR}/arch.c"
    "${SYNTRAOS_ARM64_DIR}/exception.c"
    "${SYNTRAOS_ARM64_DIR}/gic.c"
    "${SYNTRAOS_ARM64_DIR}/registers.c"
    "${SYNTRAOS_ARM64_DIR}/mmu.c"
    "${SYNTRAOS_ARM64_DIR}/page_table_builder.c"
    "${SYNTRAOS_ARM64_DIR}/mmu_mapping_plan.c"
    "${SYNTRAOS_ARM64_DIR}/context.c"
    "${SYNTRAOS_ROOT}/kernel/scheduler/scheduler.c"
    "${SYNTRAOS_ARM64_DIR}/timer.c"

    "${SYNTRAOS_ROOT}/kernel/interrupt/interrupt.c"
    "${SYNTRAOS_ROOT}/kernel/timer/timer.c"
    "${SYNTRAOS_ROOT}/kernel/gui/framebuffer.c"
    "${SYNTRAOS_ROOT}/kernel/gui/desktop.c"
    "${SYNTRAOS_ROOT}/kernel/gui/ramfb.c"
    "${SYNTRAOS_ROOT}/kernel/gui/desktop_session.c"
    "${SYNTRAOS_ROOT}/kernel/gui/window_manager.c"
    "${SYNTRAOS_ROOT}/kernel/gui/window_renderer.c"
    "${SYNTRAOS_ROOT}/kernel/input/input.c"
)

set_target_properties(syntraos_arm64 PROPERTIES
    OUTPUT_NAME "syntraos_arm64"
    SUFFIX ".elf"
)

target_compile_definitions(syntraos_arm64 PRIVATE
    SYNTRA_ARM64_FREESTANDING_IMAGE=1
)

target_include_directories(syntraos_arm64 PRIVATE
    "${SYNTRAOS_ROOT}/kernel/core"
    "${SYNTRAOS_ROOT}/kernel/memory"
    "${SYNTRAOS_ROOT}/kernel/scheduler"
    "${SYNTRAOS_ROOT}/kernel/timer"
    "${SYNTRAOS_ROOT}/kernel/interrupt"
    "${SYNTRAOS_ROOT}/kernel/input"
    "${SYNTRAOS_ARM64_DIR}"
)

target_compile_options(syntraos_arm64 PRIVATE
    $<$<COMPILE_LANGUAGE:C>:-Wall>
    $<$<COMPILE_LANGUAGE:C>:-Wextra>
    $<$<COMPILE_LANGUAGE:C>:-Wpedantic>
    $<$<COMPILE_LANGUAGE:C>:-ffreestanding>
    $<$<COMPILE_LANGUAGE:C>:-fno-builtin>
    $<$<COMPILE_LANGUAGE:C>:-fno-stack-protector>
    $<$<COMPILE_LANGUAGE:C>:-fno-pic>
    $<$<COMPILE_LANGUAGE:C>:-fno-unwind-tables>
    $<$<COMPILE_LANGUAGE:C>:-fno-asynchronous-unwind-tables>
)

target_link_options(syntraos_arm64 PRIVATE
    -nostdlib
    -static
    "-Wl,-T,${SYNTRAOS_ARM64_DIR}/boot/linker.ld"
    -Wl,--build-id=none
    "-Wl,-Map,${CMAKE_CURRENT_BINARY_DIR}/syntraos_arm64.map"
)
