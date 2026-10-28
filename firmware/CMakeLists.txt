# STM32F10xxE 固件。顶层在交叉编译时 add_subdirectory 进来，链接的是顶层那份 mc_core。
enable_language(ASM)

# 芯片：DEV_ID 0x414 只定到 F101/F103 大容量型，两者寄存器上分不出。
# 2026-10-28 丝印直读「STM32F103」（板为普中 Z300），默认改为 F103xE：RAM 按 64 KB 链接。
# F101xE 是它的子集（36 MHz、48 KB RAM、无 USB/CAN/TIM1/TIM8），换到 F101 的板子上要改回。
set(MC_MCU "STM32F103xE" CACHE STRING "目标芯片，按丝印定：STM32F101xE 或 STM32F103xE")
set_property(CACHE MC_MCU PROPERTY STRINGS STM32F101xE STM32F103xE)
string(TOLOWER "${MC_MCU}" mc_mcu_lower)
if(NOT mc_mcu_lower MATCHES "^stm32f10[13]xe$")
    message(FATAL_ERROR "MC_MCU 只支持 STM32F101xE / STM32F103xE，收到 ${MC_MCU}")
endif()

set(ST_DEVICE ${CMAKE_CURRENT_SOURCE_DIR}/third_party/cmsis_device_f1)
set(ST_HAL ${CMAKE_CURRENT_SOURCE_DIR}/third_party/stm32f1xx_hal_driver)
set(CMSIS_CORE ${CMAKE_CURRENT_SOURCE_DIR}/third_party/cmsis_core)
if(NOT EXISTS ${ST_HAL}/Inc/stm32f1xx_hal.h OR NOT EXISTS ${ST_DEVICE}/Include/stm32f1xx.h)
    message(FATAL_ERROR "ST 子模块没取下来：git submodule update --init")
endif()

# 顶层的 -Werror 那一套只管自己写的代码。ST 的 HAL 与启动文件不归我们改，
# 本目录先把继承来的选项摘掉，下面只挂回给自己的目标；HAL 头文件按 SYSTEM 引入，告警不外溢。
get_directory_property(mc_strict_options COMPILE_OPTIONS)
set_directory_properties(PROPERTIES COMPILE_OPTIONS "")

# 与 src/stm32f1xx_hal_conf.h 打开的模块一一对应，开新模块时两边一起改
add_library(st_hal STATIC
    ${ST_DEVICE}/Source/Templates/system_stm32f1xx.c
    ${ST_HAL}/Src/stm32f1xx_hal.c
    ${ST_HAL}/Src/stm32f1xx_hal_cortex.c
    ${ST_HAL}/Src/stm32f1xx_hal_dma.c
    ${ST_HAL}/Src/stm32f1xx_hal_exti.c
    ${ST_HAL}/Src/stm32f1xx_hal_flash.c
    ${ST_HAL}/Src/stm32f1xx_hal_flash_ex.c
    ${ST_HAL}/Src/stm32f1xx_hal_gpio.c
    ${ST_HAL}/Src/stm32f1xx_hal_gpio_ex.c
    ${ST_HAL}/Src/stm32f1xx_hal_pwr.c
    ${ST_HAL}/Src/stm32f1xx_hal_rcc.c
    ${ST_HAL}/Src/stm32f1xx_hal_rcc_ex.c
)
target_include_directories(st_hal SYSTEM PUBLIC
    src
    ${ST_HAL}/Inc
    ${ST_DEVICE}/Include
    ${CMSIS_CORE}/Include
)
target_compile_definitions(st_hal PUBLIC ${MC_MCU} USE_HAL_DRIVER)

add_executable(mcu_calendar_fw
    src/main.c
    src/stm32f1xx_it.c
    src/crt_stubs.c
    ${ST_DEVICE}/Source/Templates/gcc/startup_${mc_mcu_lower}.s
)
target_compile_options(mcu_calendar_fw PRIVATE "$<$<COMPILE_LANGUAGE:C>:${mc_strict_options}>")
target_link_libraries(mcu_calendar_fw PRIVATE mc_core st_hal)

set(mc_ld_dir ${CMAKE_CURRENT_SOURCE_DIR}/ld)
target_link_options(mcu_calendar_fw PRIVATE
    -L${mc_ld_dir}
    -T${mc_ld_dir}/${mc_mcu_lower}.ld
    -Wl,-Map=$<TARGET_FILE_DIR:mcu_calendar_fw>/mcu_calendar_fw.map
    -Wl,--print-memory-usage
)
set_target_properties(mcu_calendar_fw PROPERTIES
    SUFFIX .elf
    LINK_DEPENDS "${mc_ld_dir}/${mc_mcu_lower}.ld;${mc_ld_dir}/stm32f1_sections.ld"
)
add_custom_command(TARGET mcu_calendar_fw POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:mcu_calendar_fw> $<TARGET_FILE_DIR:mcu_calendar_fw>/mcu_calendar_fw.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:mcu_calendar_fw> $<TARGET_FILE_DIR:mcu_calendar_fw>/mcu_calendar_fw.bin
    VERBATIM
)
