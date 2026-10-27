# arm-none-eabi-gcc 交叉编译，Cortex-M3（STM32F1）。
# 编译器按 ARM_TOOLCHAIN_DIR（缓存变量或同名环境变量）→ PATH 的顺序找。各机装的位置不同，
# 这里不写死；本机路径放 CMakeUserPresets.json（不入库），写法见 firmware/README.md。
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

if(NOT ARM_TOOLCHAIN_DIR AND DEFINED ENV{ARM_TOOLCHAIN_DIR})
    set(ARM_TOOLCHAIN_DIR "$ENV{ARM_TOOLCHAIN_DIR}")
endif()
# CMake 探测编译器时会在子工程里重读本文件，不把变量带过去那边就找不到编译器
list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES ARM_TOOLCHAIN_DIR)

find_program(ARM_GCC arm-none-eabi-gcc HINTS "${ARM_TOOLCHAIN_DIR}/bin")
if(NOT ARM_GCC)
    message(FATAL_ERROR "找不到 arm-none-eabi-gcc：设 ARM_TOOLCHAIN_DIR 为工具链根目录（含 bin/），或把 bin/ 加进 PATH")
endif()

set(CMAKE_C_COMPILER "${ARM_GCC}")
set(CMAKE_ASM_COMPILER "${ARM_GCC}")
# 裸机没有宿主 libc 的启动代码，探测时链接可执行文件必失败，只编静态库
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(MC_CPU_FLAGS "-mcpu=cortex-m3 -mthumb")
set(CMAKE_C_FLAGS_INIT "${MC_CPU_FLAGS} -ffunction-sections -fdata-sections")
set(CMAKE_ASM_FLAGS_INIT "${MC_CPU_FLAGS}")
set(CMAKE_C_FLAGS_DEBUG_INIT "-Og -g3")
set(CMAKE_C_FLAGS_RELEASE_INIT "-Os")
# -nostartfiles：入口是 ST 启动文件的 Reset_Handler，用不上 newlib 的 crt0；留着它，它引用的 exit
# 会把整套 stdio 连同 libnosys 的 _read/_write 桩拖进来，链接器每个桩报一条告警。
# 以后若再看到这些告警，说明是代码自己用上了 stdio。_init/_fini 的空实现在 firmware/src/crt_stubs.c。
set(CMAKE_EXE_LINKER_FLAGS_INIT "${MC_CPU_FLAGS} -nostartfiles --specs=nano.specs --specs=nosys.specs -Wl,--gc-sections")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
