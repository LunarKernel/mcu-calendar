# mcu-calendar · 万年历时钟（2026 微控制器课程设计）

两人组队，下文以 A、B 指代；分工按课程选题表。

与芯片无关的部分：纯 C11 算法库、应用层骨架、硬件边界头文件，以及在 PC 上跑的单元测试。

固件（`firmware/`，2026-10-27 起）：开发板为普中 Z300，芯片 STM32F103 大容量型（512 KB Flash；10/28 丝印直读 F103，封装推断 ZET6），原理图未得。现有的只是 CMake 交叉编译工程与一版**不碰任何引脚**的上板自检（含 HSE、LSE 起振探测）：`core/` 原样链进固件，在目标芯片上算出的日期、农历、时间戳与 PC 上的独立来源逐项一致。没有任何外设驱动，没有一项选题功能上过板。构建与烧录见 `firmware/README.md`。

## 分工与代码的对应

| 选题表条目 | 分值 | 负责 | 代码 | 状态 |
|---|---|---|---|---|
| 基本 4 · 32 位时间戳 | 5 | A | `core/src/mc_date.c` | 算法完成，host 已测 |
| 扩展 1 · 农历 | 5 | A | `core/src/mc_lunar.c` | 算法完成，全范围已核对 |
| 扩展 2 · 万年历 1900–2099 | 5 | A | `core/src/mc_calview.c` ＋ `mc_date.c` | 算法完成，host 已测；界面未做 |
| 扩展 3 · 多秒表多定时 | 5 | A | `core/src/mc_timers.c` | 状态机完成，host 已测；界面未做 |
| 扩展 5 · ESP8266 网络校时 | 5 | A | `core/src/mc_sntp.c` ＋ `app/app.c` | 状态机完成，对脚本化应答已测；**未接真模块** |
| 整体软件框架 | — | A | `app/` | 骨架与中断分层约定已定 |
| 固件工程 | — | A | `firmware/` | CMake 工程与上板自检已通过（10/27）；`app/` 尚未接进固件 |
| 基本 1/2/3/5、扩展 4、硬件 | 25 | B | 实现 `app/port.h` ＋ 各驱动 | 未开始 |

「host 已测」只说明算法与状态机逻辑正确。上板之前，没有任何一项算验收通过。

## 目录

```
core/include/mc/   公开头文件
core/src/          算法与状态机，不含任何 HAL 头文件
app/port.h         硬件边界：上层只经这里碰硬件，由驱动侧实现
app/app.c          应用层骨架：把状态机接到 port 上，全部跑在主循环
app/app_config.example.h   热点与 NTP 配置模板，复制为 app_config.h 后填写（不入库）
tests/             单元测试；test_app.c 内含一份假 port 与脚本化 ESP8266
tools/             lunar_dump.c ＋ verify_lunar.ps1：农历全范围核对
scripts/host-test.ps1      一键编译并跑测试
firmware/          STM32F10xxE 固件：CMake 工程、链接脚本、上板自检；ST 驱动库为子模块
```

## 给 B：port.h 是两人代码的唯一接缝

A 的代码只调用 `app/port.h` 里的六个函数，B 的驱动只需要实现它们：

- `port_millis()`：单调毫秒时基，`HAL_GetTick()` 即可。
- `port_rtc_get / port_rtc_set`：DS3231 读写，本地时间，I²C 失败返回 false。
- `port_esp_write`：ESP8266 串口发送，可阻塞。
- `port_esp_read_byte`：**必须非阻塞**。RX 中断或 DMA 把字节放进环形缓冲，这里每次取一个，没有就返回 -1。
- `port_on_countdown_fired(channel)`：倒计时到点回调，在主循环上下文，可在里面响蜂鸣器。

中断分层约定（写在 `app/app.h` 头部）：中断里只累加时基、收串口字节、给外部触发抓时间戳；解析、I²C、刷屏、DHT11 一律在主循环。基本功能 5「DHT11 不影响实时性」靠的就是这一条——走时和秒表只依赖时基中断，主循环被慢操作拖住也不丢时间。

扩展 4（外部触发计时）接法：EXTI 中断里记 `port_millis()` 的值，回主循环后调用 `mc_stopwatch_start/stop(&app_timers()->stopwatch[n], 抓到的时间戳)`。秒表函数都显式收 `now_ms` 就是为了这个。

## 编译与测试（Windows）

本机 PATH 里不需要有 cmake / ninja / cl，脚本借 Visual Studio 自带的那一套（需装「使用 C++ 的桌面开发」与「用于 Windows 的 C++ CMake 工具」）：

```powershell
.\scripts\host-test.ps1        # 配置、编译（/W4 /WX）、跑 7 组测试
.\tools\verify_lunar.ps1       # 农历查表 vs .NET ChineseLunisolarCalendar 逐日比对
```

固件的编译、烧录与上板自检（Windows 与 macOS 同一套命令）见 `firmware/README.md`。

2026-09-20 的结果：5/5 通过；农历比对 73 000 天（1901-02-19 至 2100-12-31）不一致 0。1900-01-01 至 1901-02-18 这 414 天超出 .NET 的支持范围，没有独立来源比对，只有逐日连续性检查与 1900-01-31＝正月初一这个基准锚点兜着。

## 已知限制与待定

- **未接真 ESP8266**。AT 固件版本不同，`AT+CIPSNTPCFG` 的时区写法有差异（旧固件填 `8`，ESP-AT 2.x 之后也接受 `800`）；拿到模块后先用串口助手手敲一遍 `mc_sntp.h` 头部列的序列，再接状态机。
- 校园网要 web 认证，ESP8266 过不去，校时用手机 2.4 GHz 热点。
- 农历名称表是 UTF-8。ST7920 一类自带 GB2312 字库的屏需要在显示层转码；屏的型号未定，定了再做。
- 秒表与倒计时用 32 位毫秒时基，单次连续运行上限约 49.7 天。
- 闹钟、番茄钟、单路秒表在选题表里归 B（基本 3）。`mc_timers` 的多路实现已经覆盖单路秒表与定时，B 可以直接用，不必另写一份；闹钟（`mc_alarm`，按 RTC 时刻触发，单次或按星期重复）与番茄钟（`mc_pomodoro`，默认 25/5 循环，段间按期限衔接不累积滞后）2026-10-28 已写成 `core/` 模块并 host 测过，尚未接进 `app/`、未上板。
- CubeIDE 的 make 构建对路径里的括号、空格与非 ASCII 字符不可靠，克隆本仓库与建固件工程都用纯 ASCII 路径。
