
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

add_library(syntra_gui STATIC
    kernel/gui/framebuffer.c
    kernel/gui/desktop.c
    kernel/gui/ramfb.c
    kernel/gui/desktop_session.c
    kernel/input/input.c
)
target_include_directories(syntra_gui PUBLIC
    kernel/gui
    kernel/core
)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(syntra_gui PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_library(syntra_kernel STATIC

    kernel/core/kernel.c
    kernel/core/log.c
    kernel/core/panic.c

    # ARM64 Architecture Foundation
    kernel/arch/arm64/arch.c
    kernel/arch/arm64/exception.c
    kernel/arch/arm64/registers.c
    kernel/arch/arm64/mmu.c
    kernel/arch/arm64/page_table_builder.c
    kernel/arch/arm64/mmu_mapping_plan.c
    kernel/arch/arm64/context.c

    kernel/scheduler/scheduler.c
    kernel/memory/memory.c
    kernel/process/process.c
    kernel/process/process_thread.c
    kernel/ipc/ipc.c

    kernel/interrupt/interrupt.c
    kernel/exception/exception.c
    kernel/timer/timer.c
    kernel/clock/clock.c
    kernel/uptime/uptime.c

    kernel/config/config.c
    kernel/config/config_manager.c
    kernel/config/config_storage.c

    kernel/hal/hal.c
    kernel/hal/hal_device.c
    kernel/hal/hal_registry.c
    kernel/hal/hal_lifecycle.c
    kernel/hal/hal_device_manager.c
    kernel/hal/hal_resource_manager.c
    kernel/hal/hal_resource_monitor.c
    kernel/hal/hal_capability_manager.c
    kernel/hal/hal_platform.c
    kernel/hal/hal_backend.c
    kernel/hal/hal_backend_manager.c
    kernel/hal/hal_device_discovery.c
    kernel/hal/hal_device_scanner.c
    kernel/hal/hal_device_event_manager.c
    kernel/hal/hal_device_event_queue.c
    kernel/hal/hal_device_event_dispatcher.c
    kernel/hal/hal_device_event_router.c
    kernel/input/input.c
)

target_include_directories(syntra_kernel PUBLIC

    kernel/core
    kernel/arch/arm64
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
    kernel/hal
    kernel/input
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

# SyntraOS input event core test
add_executable(input_test tests/input/input_test.c)
target_link_libraries(input_test PRIVATE syntra_kernel)
add_test(NAME InputEventCoreTest COMMAND input_test)

# SyntraOS virtual desktop interaction state test
add_executable(gui_desktop_session_test
    tests/gui/desktop_session_test.c
)
target_link_libraries(gui_desktop_session_test PRIVATE syntra_gui)
add_test(NAME GUIDesktopInteractionTest COMMAND gui_desktop_session_test)

# SyntraOS GUI software framebuffer and desktop renderer test
add_executable(gui_desktop_test
    tests/gui/desktop_test.c
)
target_link_libraries(gui_desktop_test PRIVATE syntra_gui)
add_test(NAME GUIDesktopRendererTest COMMAND gui_desktop_test)


# QEMU ramfb firmware-directory and wire-format tests
add_executable(gui_ramfb_test
    tests/gui/ramfb_test.c
)
target_link_libraries(gui_ramfb_test PRIVATE syntra_gui)
add_test(NAME GUIRamfbProtocolTest COMMAND gui_ramfb_test)

# Scheduler Test
add_executable(scheduler_test
    tests/scheduler/scheduler_test.c
)
target_link_libraries(scheduler_test PRIVATE syntra_kernel)
add_test(NAME SchedulerTest COMMAND scheduler_test)

# Memory Region Manager Test
add_executable(memory_region_test
    tests/memory/memory_region_test.c
    kernel/memory/memory_region.c
)
target_include_directories(memory_region_test PRIVATE kernel/memory kernel/core)
add_test(NAME MemoryRegionManagerTest COMMAND memory_region_test)

# Physical Page Allocator V1 Test
add_executable(page_allocator_test
    tests/memory/page_allocator_test.c
    kernel/memory/page_allocator.c
    kernel/memory/memory_region.c
)
target_include_directories(page_allocator_test PRIVATE kernel/memory kernel/core)
add_test(NAME PhysicalPageAllocatorV1Test COMMAND page_allocator_test)

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

# Process-owned thread lifecycle test
add_executable(process_thread_test tests/process/process_thread_test.c)
target_link_libraries(process_thread_test PRIVATE syntra_kernel)
add_test(NAME ProcessThreadOwnershipTest COMMAND process_thread_test)

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

# Config Manager V2 Test
add_executable(config_manager_test
    tests/config/config_manager_test.c
)
target_link_libraries(config_manager_test PRIVATE syntra_kernel)
add_test(NAME ConfigManagerV2Test COMMAND config_manager_test)

# Config Storage V3 Test
add_executable(config_storage_test
    tests/config/config_storage_test.c
)
target_link_libraries(config_storage_test PRIVATE syntra_kernel)
add_test(NAME ConfigStorageV3Test COMMAND config_storage_test)

# HAL Core V1 Test
add_executable(hal_test
    tests/hal/hal_test.c
)
target_link_libraries(hal_test PRIVATE syntra_kernel)
add_test(NAME HALCoreV1Test COMMAND hal_test)

# HAL Device Interface V1 Test
add_executable(hal_device_test
    tests/hal/hal_device_test.c
)
target_link_libraries(hal_device_test PRIVATE syntra_kernel)
add_test(NAME HALDeviceInterfaceV1Test COMMAND hal_device_test)

# HAL Registry V1 Test
add_executable(hal_registry_test
    tests/hal/hal_registry_test.c
)
target_link_libraries(hal_registry_test PRIVATE syntra_kernel)
add_test(NAME HALRegistryV1Test COMMAND hal_registry_test)

# HAL Lifecycle V1 Test
add_executable(hal_lifecycle_test
    tests/hal/hal_lifecycle_test.c
)
target_link_libraries(hal_lifecycle_test PRIVATE syntra_kernel)
add_test(NAME HALLifecycleV1Test COMMAND hal_lifecycle_test)

# HAL Device Manager V5 Test
add_executable(hal_device_manager_test
    tests/hal/hal_device_manager_test.c
)
target_link_libraries(hal_device_manager_test PRIVATE syntra_kernel)
add_test(NAME HALDeviceManagerV5Test COMMAND hal_device_manager_test)

# HAL Resource Manager V1 Test
add_executable(hal_resource_manager_test
    tests/hal/hal_resource_manager_test.c
)
target_link_libraries(hal_resource_manager_test PRIVATE syntra_kernel)
add_test(NAME HALResourceManagerV1Test COMMAND hal_resource_manager_test)

# HAL Resource Monitor V1 Test
add_executable(hal_resource_monitor_test
    tests/hal/hal_resource_monitor_test.c
)
target_link_libraries(hal_resource_monitor_test PRIVATE syntra_kernel)
add_test(NAME HALResourceMonitorV1Test COMMAND hal_resource_monitor_test)

# HAL Capability Manager V1 Test
add_executable(hal_capability_manager_test
    tests/hal/hal_capability_manager_test.c
)
target_link_libraries(hal_capability_manager_test PRIVATE syntra_kernel)
add_test(NAME HALCapabilityManagerV1Test COMMAND hal_capability_manager_test)

# HAL Platform Interface V1 Test
add_executable(hal_platform_test
    tests/hal/hal_platform_test.c
)
target_link_libraries(hal_platform_test PRIVATE syntra_kernel)
add_test(NAME HALPlatformInterfaceV1Test COMMAND hal_platform_test)

# HAL Hardware Backend V1 Test
add_executable(hal_backend_test
    tests/hal/hal_backend_test.c
)
target_link_libraries(hal_backend_test PRIVATE syntra_kernel)
add_test(NAME HALHardwareBackendV1Test COMMAND hal_backend_test)

# HAL Backend Manager V4 Test
add_executable(hal_backend_manager_test
    tests/hal/hal_backend_manager_test.c
)
target_link_libraries(hal_backend_manager_test PRIVATE syntra_kernel)
add_test(NAME HALBackendManagerV4Test COMMAND hal_backend_manager_test)

# HAL Device Discovery V4 Test
add_executable(hal_device_discovery_test
    tests/hal/hal_device_discovery_test.c
)
target_link_libraries(
    hal_device_discovery_test
    PRIVATE syntra_kernel
)
add_test(
    NAME HALDeviceDiscoveryV4Test
    COMMAND hal_device_discovery_test
)

# HAL Device Scanner V5 Test
add_executable(hal_device_scanner_test
    tests/hal/hal_device_scanner_test.c
)
target_link_libraries(
    hal_device_scanner_test
    PRIVATE syntra_kernel
)
add_test(
    NAME HALDeviceScannerV5Test
    COMMAND hal_device_scanner_test
)

# HAL Device Event Manager V1 Test
add_executable(hal_device_event_manager_test
    tests/hal/hal_device_event_manager_test.c
)
target_link_libraries(
    hal_device_event_manager_test
    PRIVATE syntra_kernel
)
add_test(
    NAME HALDeviceEventManagerV1Test
    COMMAND hal_device_event_manager_test
)

# HAL Device Event Queue V1 Test
add_executable(hal_device_event_queue_test
    tests/hal/hal_device_event_queue_test.c
)
target_link_libraries(
    hal_device_event_queue_test
    PRIVATE syntra_kernel
)
add_test(
    NAME HALDeviceEventQueueV1Test
    COMMAND hal_device_event_queue_test
)

# HAL Device Event Dispatcher V1 Test
add_executable(hal_device_event_dispatcher_test
    tests/hal/hal_device_event_dispatcher_test.c
)
target_link_libraries(
    hal_device_event_dispatcher_test
    PRIVATE syntra_kernel
)
add_test(
    NAME HALDeviceEventDispatcherV1Test
    COMMAND hal_device_event_dispatcher_test
)

# HAL Device Event Router V1 Test
add_executable(hal_device_event_router_test
    tests/hal/hal_device_event_router_test.c
)
target_link_libraries(
    hal_device_event_router_test
    PRIVATE syntra_kernel
)
add_test(
    NAME HALDeviceEventRouterV1Test
    COMMAND hal_device_event_router_test
)

# HAL Device Event Router V2 Test
add_executable(hal_device_event_router_v2_test
    tests/hal/hal_device_event_router_v2_test.c
)
target_link_libraries(
    hal_device_event_router_v2_test
    PRIVATE syntra_kernel
)
add_test(
    NAME HALDeviceEventRouterV2Test
    COMMAND hal_device_event_router_v2_test
)

# HAL Device Event Router V3 Test
add_executable(hal_device_event_router_v3_test
    tests/hal/hal_device_event_router_v3_test.c
)
target_link_libraries(
    hal_device_event_router_v3_test
    PRIVATE syntra_kernel
)
add_test(
    NAME HALDeviceEventRouterV3Test
    COMMAND hal_device_event_router_v3_test
)

# HAL Device Event Router V4 Test
add_executable(hal_device_event_router_v4_test
    tests/hal/hal_device_event_router_v4_test.c
)
target_link_libraries(
    hal_device_event_router_v4_test
    PRIVATE syntra_kernel
)
add_test(
    NAME HALDeviceEventRouterV4Test
    COMMAND hal_device_event_router_v4_test
)

# ARM64 Device Tree and GIC discovery test
add_executable(fdt_gic_test
    tests/arm64/fdt_gic_test.c
    kernel/arch/arm64/boot/fdt.c
    kernel/arch/arm64/gic.c
)
target_include_directories(fdt_gic_test PRIVATE
    kernel/arch/arm64
)
add_test(NAME ARM64FDTGICDiscoveryTest COMMAND fdt_gic_test)

# ARM64 boot memory metadata tests
add_executable(arm64_boot_memory_metadata_test
    tests/arm64/boot_memory_metadata_test.c
    kernel/arch/arm64/boot/memory_map.c
    kernel/arch/arm64/boot/fdt.c
    kernel/memory/memory_region.c
)
target_include_directories(arm64_boot_memory_metadata_test PRIVATE
    kernel/arch/arm64
    kernel/arch/arm64/boot
    kernel/memory
    kernel/core
)
add_test(NAME ARM64BootMemoryMetadataTest
         COMMAND arm64_boot_memory_metadata_test)

# ARM64 initial thread context frame tests
add_executable(arm64_context_test
    tests/arm64/context_test.c
)
target_link_libraries(arm64_context_test PRIVATE syntra_kernel)
add_test(NAME ARM64ContextFrameTest COMMAND arm64_context_test)

# ARM64 exception vector-origin validation tests
add_executable(arm64_exception_origin_test
    tests/arm64/exception_origin_test.c
)
target_link_libraries(arm64_exception_origin_test PRIVATE syntra_kernel)
add_test(NAME ARM64ExceptionOriginTest COMMAND arm64_exception_origin_test)

# ARM64 Current Exception Level decoder tests
add_executable(arm64_registers_test
    tests/arm64/registers_test.c
)
target_link_libraries(arm64_registers_test PRIVATE syntra_kernel)
add_test(NAME ARM64RegisterDecoderTest COMMAND arm64_registers_test)

# ARM64 MMU mapping-plan validation tests
add_executable(arm64_mmu_mapping_plan_test
    tests/arm64/mmu_mapping_plan_test.c
    kernel/arch/arm64/mmu_mapping_plan.c
    kernel/arch/arm64/mmu.c
)
target_include_directories(arm64_mmu_mapping_plan_test PRIVATE
    kernel/arch/arm64
    kernel/arch/arm64/boot
    kernel/memory
    kernel/core
)
add_test(NAME ARM64MMUMappingPlanTest COMMAND arm64_mmu_mapping_plan_test)

# ARM64 MMU descriptor construction tests
add_executable(mmu_descriptor_test
    tests/arm64/mmu_descriptor_test.c
    kernel/arch/arm64/mmu.c
)
target_include_directories(mmu_descriptor_test PRIVATE
    kernel/arch/arm64
)
add_test(NAME ARM64MMUDescriptorTest COMMAND mmu_descriptor_test)

# ARM64 MMU descriptor, software-walker, and register interface tests
add_executable(arm64_mmu_test
    tests/arm64/mmu_test.c
    kernel/arch/arm64/mmu.c
)
target_include_directories(arm64_mmu_test PRIVATE
    kernel/arch/arm64
    kernel/core
)
add_test(NAME ARM64MMUTest COMMAND arm64_mmu_test)


# ARM64 Page Table Builder backed by the physical page allocator
add_executable(arm64_page_table_allocator_integration_test
    tests/arm64/page_table_allocator_integration_test.c
    kernel/arch/arm64/mmu.c
    kernel/arch/arm64/page_table_builder.c
    kernel/memory/page_allocator.c
    kernel/memory/memory_region.c
)
target_include_directories(arm64_page_table_allocator_integration_test PRIVATE
    kernel/arch/arm64
    kernel/memory
    kernel/core
)
add_test(NAME ARM64PageTableAllocatorIntegrationTest
    COMMAND arm64_page_table_allocator_integration_test)

# ARM64 Page Table Builder tests
add_executable(arm64_page_table_builder_test
    tests/arm64/page_table_builder_test.c
    kernel/arch/arm64/mmu.c
    kernel/arch/arm64/page_table_builder.c
)
target_include_directories(arm64_page_table_builder_test PRIVATE
    kernel/arch/arm64
    kernel/core
)
add_test(NAME ARM64PageTableBuilderTest COMMAND arm64_page_table_builder_test)

# Explicit EL1/EL0 permission audit: kernel isolation and user code execute.
add_executable(arm64_el0_permission_audit_test
    tests/arm64/el0_permission_audit_test.c
    kernel/arch/arm64/mmu.c
    kernel/arch/arm64/page_table_builder.c
)
target_include_directories(arm64_el0_permission_audit_test PRIVATE
    kernel/arch/arm64
    kernel/core
)
add_test(NAME ARM64EL0PermissionAuditTest
    COMMAND arm64_el0_permission_audit_test)
