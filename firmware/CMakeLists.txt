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

set(mc_ld_dir ${CMAKE_CURRENT_SOURCE_DIR}/ld)

# 两个固件共用的编译、链接与产物设置
function(mc_add_firmware target)
    add_executable(${target} ${ARGN}
        src/stm32f1xx_it.c
        src/crt_stubs.c
        ${ST_DEVICE}/Source/Templates/gcc/startup_${mc_mcu_lower}.s
    )
    target_compile_options(${target} PRIVATE "$<$<COMPILE_LANGUAGE:C>:${mc_strict_options}>")
    target_link_libraries(${target} PRIVATE mc_core st_hal)
    target_link_options(${target} PRIVATE
        -L${mc_ld_dir}
        -T${mc_ld_dir}/${mc_mcu_lower}.ld
        -Wl,-Map=$<TARGET_FILE_DIR:${target}>/${target}.map
        -Wl,--print-memory-usage
    )
    set_target_properties(${target} PROPERTIES
        SUFFIX .elf
        LINK_DEPENDS "${mc_ld_dir}/${mc_mcu_lower}.ld;${mc_ld_dir}/stm32f1_sections.ld"
    )
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:${target}> $<TARGET_FILE_DIR:${target}>/${target}.hex
        COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${target}> $<TARGET_FILE_DIR:${target}>/${target}.bin
        VERBATIM
    )
endfunction()

# 上板自检：不碰引脚，只验 core/ 与时钟；selftest.py 读的是它
mc_add_firmware(mcu_selftest_fw src/selftest_main.c)

# 应用固件：板级驱动 + port.h 实现 + app/
mc_add_firmware(mcu_calendar_fw
    src/main.c
    src/bsp_clock.c
    src/bsp_gpio.c
    src/bsp_uart.c
    src/bsp_rtc.c
    src/bsp_lcd.c
    src/gfx.c
    src/ui.c
    src/port_impl.c
    ${PROJECT_SOURCE_DIR}/app/app.c
)
# app_config.h 含热点密码、不入库。本机没建就退回 host 测试那份假凭据：能编能跑，只是网络校时连不上
if(EXISTS ${PROJECT_SOURCE_DIR}/app/app_config.h)
    set(mc_app_config_dir ${PROJECT_SOURCE_DIR}/app)
else()
    set(mc_app_config_dir ${PROJECT_SOURCE_DIR}/tests/host_config)
    message(WARNING "没有 app/app_config.h，固件用 tests/host_config 的假凭据编译；网络校时前先从 app_config.example.h 复制一份填好")
endif()
target_include_directories(mcu_calendar_fw PRIVATE ${PROJECT_SOURCE_DIR}/app ${mc_app_config_dir})
