# firmware · STM32F10xxE 固件

CMake ＋ arm-none-eabi-gcc，Windows 与 macOS 用同一套命令。顶层 `CMakeLists.txt` 在交叉编译时把本目录加进来，链接的是 host 测试用的同一份 `mc_core`。

## 现状（2026-10-29）

- 芯片：经 SWD 读 DBGMCU_IDCODE 得 DEV_ID 0x414（F101/F103 大容量型，512 KB Flash）；10/28 丝印直读「STM32F103」，`MC_MCU` 默认改为 `STM32F103xE`。封装后缀丝印太淡未读出，板上引出 PF/PG，推断为 ZET6（144 脚），未直读。
- 板卡：普中（PRECHIN）Z300 V2.0。10/29 得到原理图（普中资料包，不入本仓库），引脚逐条直读进 `src/board.h`。
- 时钟：HSE 起振，SYSCLK 切到 HSE（不经 PLL）；10/28 以 PC 时钟拟合测得 8.0006 MHz。LSE（32.768 kHz）起振用时约 2.9 s；LSE 频率未测。
- 两个固件目标：
  - `mcu_calendar_fw`（`src/main.c`）：应用固件。板级底座是 72 MHz 时钟、调试串口 USART1、ESP 串口 USART3、片内 RTC、按键消抖、蜂鸣器，还有读屏 ID；`app/` 跑在主循环里。屏的初始化序列还没有出处，人机界面暂时走调试串口，用法写在 `src/main.c` 文件头。10/29 上板：`diag.py` 9 项全过（72 MHz、屏 ID 0xB509、RTC 与 PC 同步、主循环在跑）；现场看到 DS0 每秒闪、KEY0 按下蜂鸣器「嘀」、背光亮（屏未初始化，白屏）。
  - `mcu_selftest_fw`（`src/selftest_main.c`）：上板自检。`core/` 在目标上算日期、农历、时间戳，`mc_countdown` 由真实 SysTick 驱动，再加 HSE、LSE 探测；结果放在 RAM 里的 `g_selftest`，PC 端经 SWD 只读取回比对。10/27 版 41 项、10/28 版（version 2）44 项实测全过。
- 这些都不是功能验收：选题表里没有一项功能因此算完成。

## 工具

| 工具 | 实测版本 | 说明 |
|---|---|---|
| Arm GNU Toolchain（arm-none-eabi） | 15.2.rel1 | developer.arm.com 官方包。macOS 也用官方包；Homebrew 的 `arm-none-eabi-gcc` formula 据称不带 newlib（未核实），避开它 |
| CMake | 4.4.4（要求 ≥ 3.21） | `pip install cmake` 或系统包 |
| Ninja | 1.13.2 | `pip install ninja` 或系统包 |
| STM32CubeProgrammer | 2.23.0 | 烧录与自检读取都走它的 CLI；ST-Link/V2 固件要先升到它认的版本（V2J23 会被拒） |
| Python | 3.13 | 只用标准库，跑 `scripts/selftest.py` |

macOS 这一列目前没人实际跑过，首次使用若与这里不符，改这份文档。

## 首次取代码

ST 驱动库是子模块，克隆后要取一次：

```
git submodule update --init
```

## 本机工具链路径

`arm-none-eabi-gcc` 在 PATH 里就不用管。不在的话，在仓库根建 `CMakeUserPresets.json`（已被 `.gitignore` 挡住，各机自己写）：

```json
{
  "version": 3,
  "configurePresets": [
    {
      "name": "firmware-local",
      "inherits": "firmware",
      "cacheVariables": { "ARM_TOOLCHAIN_DIR": "<工具链根目录，含 bin/>" }
    }
  ],
  "buildPresets": [ { "name": "firmware-local", "configurePreset": "firmware-local" } ]
}
```

也可以不建这个文件，改设同名环境变量 `ARM_TOOLCHAIN_DIR`。

## 编译、烧录、自检

```
cmake --preset firmware-local
cmake --build --preset firmware-local
python firmware/scripts/selftest.py --flash
```

PATH 里已有编译器时，前两行的 `firmware-local` 换成 `firmware`。

产物在 `build/firmware/firmware/`：每个目标一组 `.elf`（烧录与调试用）、`.hex`、`.bin`、`.map`。

应用固件的底座核对（只插 ST-Link 即可，经 SWD 读 `g_diag`）：

```
python3 firmware/scripts/diag.py --probe openocd --flash
```

核时钟 72 MHz、屏 ID 0xB509、RTC 秒数与 PC 同步、主循环在跑。同样会擦除整片 Flash。

`selftest.py --flash` 烧的是 `mcu_selftest_fw`，会**擦除整片 Flash** 再写入、校验、复位，然后读 4 次 `g_selftest` 核对；不加 `--flash` 只读不写。`--log` 可把 CLI 原始输出存档。

改了工具链文件 `cmake/arm-none-eabi.cmake` 之后要删掉 `build/firmware` 重新配置：里面的 `*_INIT` 变量只在首次配置时生效。

## 芯片选择：F101 还是 F103

缓存变量 `MC_MCU` 决定宏定义、启动文件与链接脚本，默认 `STM32F103xE`（10/28 丝印直读）。F101xE 是 F103xE 的子集（36 MHz、48 KB RAM、无 USB/CAN/TIM1/TIM8），按 F103xE 编出的固件放到 F101 上，RAM 会越界。换到 F101 的板子上：

```
cmake --preset firmware-local -DMC_MCU=STM32F101xE
```

缓存里已有旧值时，先删掉 `build/firmware` 再配置。

## 几处不显然的决定

- **链接脚本自己写**（`ld/`），不用 ST 模板：模板是 Ac6 授权，条款不许再分发；模板的 `_estack = 0x2000BFFF` 也不是 8 字节对齐，AAPCS 要求的栈对齐在入口处就破了。`.init_array` 等几段标了 `READONLY`，否则与 `.text` 同处的 Flash 段被标成 RWX，链接器告警。
- **`-nostartfiles`**：入口是 ST 启动文件的 `Reset_Handler`，newlib 的 crt0 用不上；留着它，它引用的 `exit` 会把整套 stdio 和 libnosys 的 `_read/_write` 桩拖进来，每个桩报一条链接告警。`_init/_fini` 的空实现在 `src/crt_stubs.c`。以后再看到这类告警，说明代码自己用上了 stdio。
- **告警分两档**：顶层的 `-Wall -Wextra -Wpedantic -Wconversion -Werror` 只挂在自己写的代码上；ST 的 HAL 和启动文件不归我们改，编译时不加这套，头文件按 SYSTEM 引入。
- **HAL 模块按需开**：`src/stm32f1xx_hal_conf.h` 打开的模块要与 `CMakeLists.txt` 里 `st_hal` 的源文件一一对应，开新模块两边一起改。

## 第三方代码

| 路径 | 来源 | 版本 | 许可 |
|---|---|---|---|
| `third_party/cmsis_device_f1` | 子模块 STMicroelectronics/cmsis_device_f1 | c8e9a4a（v4.3.5 之后 1 个提交） | Apache-2.0 |
| `third_party/stm32f1xx_hal_driver` | 子模块 STMicroelectronics/stm32f1xx_hal_driver | fee494a（v1.1.10 之后 17 个提交） | BSD-3-Clause |
| `third_party/cmsis_core` | STM32CubeF1 v1.8.7 的 `Drivers/CMSIS/Include` 中 5 个头文件，原样复制 | CMSIS Core(M) 5.1 | Apache-2.0（`LICENSE.txt`） |
| `src/stm32f1xx_hal_conf.h` | HAL 仓库的 `stm32f1xx_hal_conf_template.h`，只改了模块开关 | 同 HAL | BSD-3-Clause |

两个子模块钉的是 STM32CubeF1 v1.8.7 整包实际引用的提交，不是最新标签：v1.8.7 比 HAL v1.1.10 多了 UART 空闲接收与 TIM 更新标志等修复，ESP8266 串口与 DHT11 输入捕获正要用到。
