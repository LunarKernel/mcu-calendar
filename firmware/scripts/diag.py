#!/usr/bin/env python3
"""应用固件（mcu_calendar_fw）的板级底座核对：（可选）烧录，然后经 SWD 只读取回 g_diag 两次比对。

    python3 firmware/scripts/diag.py --probe openocd --flash   # 擦除整片 Flash、烧录、复位，再核对
    python3 firmware/scripts/diag.py --probe openocd           # 只读，不写板子
    python3 firmware/scripts/diag.py --probe openocd --set-time  # 把 PC 当前时间写进板上 RTC（只写 RAM 里一个字）

只插 ST-Link、不接调试串口也能用。核的是「底座通了」，不是任何一项选题功能的验收：
  - 时钟：clock_ok＝1，SYSCLK＝72 MHz（HSE 8 MHz × 9）；
  - 屏：控制器 ID 读出 0xB509（R61509V 的器件代码），说明 FSMC 片选、RS、数据线按原理图接对了；
  - RTC：ok＝1，两次读取之间 RTC 秒数的增量与 PC 时钟对得上（±1 s）；
  - 主循环在跑：tick 与 loops 都在涨。
g_diag 的布局见 firmware/src/main.c，两边改动要同步，并对上 DIAG_VERSION。
"""
from __future__ import annotations

import argparse
import datetime as dt
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from selftest import (REPO, Checker, cache_value, find_cli, find_openocd, flash,  # noqa: E402
                      read_words, run_cli, run_openocd, symbol_address)

MAGIC = 0x4D434631
VERSION = 1
FIELDS = ["magic", "version", "sysclk_hz", "clock_ok", "lcd_id", "rtc_ok", "rtc_retained", "rtc_time_set",
          "bdcr_initial", "lse_wait_ms", "rtc_seconds", "tick_ms", "loops",
          "key_up", "key0", "key1", "esp_rx_dropped", "sync_state"]
LCD_ID_R61509V = 0xB509
LSE_TIMEOUT_MS = 5000  # 固件里 RTC 从头配时 LSE 最多等这么久


def read_diag(probe: str, tool: str, addr: int, log: list[str]) -> tuple[dict[str, int], float]:
    words, t0, t1 = read_words(probe, tool, addr, len(FIELDS), log)
    return dict(zip(FIELDS, words)), (t0 + t1) / 2


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--build-dir", type=Path, default=REPO / "build" / "firmware")
    ap.add_argument("--probe", choices=["cubeprog", "openocd"], default="cubeprog")
    ap.add_argument("--cli", help="STM32_Programmer_CLI 路径（--probe cubeprog）")
    ap.add_argument("--openocd", help="openocd 路径（--probe openocd）")
    ap.add_argument("--flash", action="store_true", help="先擦除整片 Flash 并烧录、复位")
    ap.add_argument("--set-time", action="store_true",
                    help="经 SWD 往 g_set_unix 写 PC 当前 UTC 秒数，固件主循环据此校时（不写 Flash）")
    ap.add_argument("--interval", type=float, default=10.0, help="两次读取之间的秒数")
    ap.add_argument("--log", type=Path, help="把 CLI 原始输出与核对结果存到这个文件")
    args = ap.parse_args()

    build = args.build_dir.resolve()
    elf = build / "firmware" / "mcu_calendar_fw.elf"
    if not elf.is_file():
        sys.exit(f"没有 {elf}，先 cmake --build --preset firmware")
    nm = cache_value(build, "CMAKE_NM")
    tool = find_openocd(args.openocd) if args.probe == "openocd" else find_cli(args.cli)
    addr = symbol_address(nm, elf, "g_diag")
    log: list[str] = []
    print(f"ELF {elf}\ng_diag @ 0x{addr:08X}，{len(FIELDS)} 字")

    if args.flash:
        print("擦除整片 Flash、烧录、校验、复位 ...")
        flash(args.probe, tool, elf, log)
        time.sleep(LSE_TIMEOUT_MS / 1000 + 1.0)

    if args.set_time:
        set_addr = symbol_address(nm, elf, "g_set_unix")
        now = int(time.time()) + 1  # 写进去时已过去小半秒，取下一整秒更接近
        if args.probe == "openocd":
            run_openocd(tool, [f"mww 0x{set_addr:08X} 0x{now:08X}"], log)
        else:
            run_cli(tool, ["-c", "port=SWD", "mode=HOTPLUG", "-w32", f"0x{set_addr:08X}", f"0x{now:08X}"], log)
        print(f"已写 g_set_unix = {now}")
        time.sleep(1.0)

    a, ta = read_diag(args.probe, tool, addr, log)
    time.sleep(args.interval)
    b, tb = read_diag(args.probe, tool, addr, log)

    print("读回（第二次）：")
    for k in FIELDS:
        print(f"  {k:15s} 0x{b[k]:08X}  {b[k]}")
    utc = dt.datetime.fromtimestamp(b["rtc_seconds"], dt.timezone.utc)
    print(f"  RTC 本地时间 {(utc + dt.timedelta(hours=8)).strftime('%Y-%m-%d %H:%M:%S')}（北京时间）")

    c = Checker()
    c.check("magic", hex(b["magic"]), hex(MAGIC))
    c.check("version", b["version"], VERSION)
    c.check("clock_ok", b["clock_ok"], 1)
    c.check("SYSCLK", b["sysclk_hz"], 72_000_000)
    c.check("LCD ID", hex(b["lcd_id"]), hex(LCD_ID_R61509V))
    c.check("rtc_ok", b["rtc_ok"], 1)
    pc_dt = tb - ta
    rtc_dt = b["rtc_seconds"] - a["rtc_seconds"]
    c.check(f"RTC 增量 ≈ PC {pc_dt:.1f} s（±1）", abs(rtc_dt - pc_dt) <= 1.0, True)
    tick_dt = (b["tick_ms"] - a["tick_ms"]) & 0xFFFFFFFF
    c.check(f"tick 增量 {tick_dt} ms ≈ PC {pc_dt * 1000:.0f} ms（±2%）", abs(tick_dt - pc_dt * 1000) <= pc_dt * 20, True)
    c.check("主循环在跑", b["loops"] != a["loops"], True)
    if args.set_time:
        drift = b["rtc_seconds"] - time.time()
        c.check(f"RTC 与 PC 时间差 {drift:+.1f} s（±2）", abs(drift) <= 2.0, True)
        c.check("rtc_time_set", b["rtc_time_set"], 1)
    print(f"合计：{c.passed} 项通过，{c.failed} 项失败")

    if args.log:
        args.log.write_text("\n\n".join(log) + "\n", encoding="utf-8")
    return 1 if c.failed else 0


if __name__ == "__main__":
    sys.exit(main())
