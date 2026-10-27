cmake_minimum_required(VERSION 3.21)
project(mcu_calendar C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/W4 /WX /utf-8)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Wconversion -Werror)
endif()

# 与芯片无关的核心算法库。host 测试与固件链接的是同一份，交叉编译时同样受上面的 -Werror 约束。
add_library(mc_core STATIC
    core/src/mc_date.c
    core/src/mc_lunar.c
    core/src/mc_calview.c
    core/src/mc_timers.c
    core/src/mc_sntp.c
)
target_include_directories(mc_core PUBLIC core/include)

# 交叉编译（firmware 预设）只出固件；下面的 PC 工具与测试在目标上没法跑
if(CMAKE_CROSSCOMPILING)
    add_subdirectory(firmware)
    return()
endif()

add_executable(lunar_dump tools/lunar_dump.c)
target_link_libraries(lunar_dump PRIVATE mc_core)

include(CTest)
if(BUILD_TESTING)
    foreach(name date lunar timers sntp)
        add_executable(test_${name} tests/test_${name}.c)
        target_link_libraries(test_${name} PRIVATE mc_core)
        add_test(NAME ${name} COMMAND test_${name})
    endforeach()

    # app 层：port 由测试自己实现，配置取 tests/host_config 下的假凭据
    add_executable(test_app tests/test_app.c app/app.c)
    target_include_directories(test_app PRIVATE app tests/host_config)
    target_link_libraries(test_app PRIVATE mc_core)
    add_test(NAME app COMMAND test_app)
endif()
