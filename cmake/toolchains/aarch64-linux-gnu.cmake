# SyntraOS ARM64 Cross-Compilation Toolchain
# Version: 1.0.0

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_VERSION 1)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

# GNU AArch64 cross-compilers
set(CMAKE_C_COMPILER aarch64-linux-gnu-gcc)
set(CMAKE_ASM_COMPILER aarch64-linux-gnu-gcc)

# GNU binary utilities
set(CMAKE_AR aarch64-linux-gnu-ar)
set(CMAKE_RANLIB aarch64-linux-gnu-ranlib)
set(CMAKE_OBJCOPY aarch64-linux-gnu-objcopy)
set(CMAKE_SIZE aarch64-linux-gnu-size)

# Bare-metal compilation settings
set(CMAKE_C_FLAGS_INIT
    "-march=armv8-a -ffreestanding -fno-builtin -fno-stack-protector -fno-pic -mgeneral-regs-only"
)

set(CMAKE_ASM_FLAGS_INIT
    "-march=armv8-a"
)

# Avoid executable linking during compiler detection.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
