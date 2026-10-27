#!/usr/bin/env python3
"""首版上板自检的 PC 端：（可选）烧录，然后经 SWD 只读取回 g_selftest，与独立来源逐项比对。

    python firmware/scripts/selftest.py --flash        # 擦除整片 Flash、烧录、复位，再核对
    python firmware/scripts/selftest.py                # 只核对，不写板子

期望值的出处（不拿被测代码自己的输出当期望值）：
  - 1970-01-01 起的天数、星期、32 位时间戳：运行时用 Python 标准库现算；
  - 农历：.NET ChineseLunisolarCalendar（2026-10-27 在 Windows 上算出，写死在 LUNAR_EXPECTED）。

读取用 CubeProgrammer 的 HOTPLUG 模式：不复位、不停核，读的是运行中的内存。
g_selftest 的布局见 firmware/src/main.c，两边改动要同步，并对上 SELFTEST_VERSION。
"""
from __future__ import annotations

import argparse
import calendar
import datetime as dt
import os
import re
import shutil
import subprocess
import sys
import time
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]

MAGIC = 0x4D434331
VERSION = 1
DONE = 0xD0D0D0D0
COUNTDOWN_MS = 5000
HEADER_WORDS = 9
DATE_WORDS = 10
TS_WORDS = 2

# 与 main.c 的 k_dates 同序
DATES = [(2026, 10, 27), (2000, 2, 29), (2020, 5, 23), (2033, 12, 22), (1901, 2, 19), (2100, 12, 31)]
# (农历年, 月, 日, 是否闰月)，来源见文件头
LUNAR_EXPECTED = [
    (2026, 9, 18, False),
    (2000, 1, 25, False),
    (2020, 4, 1, True),   # 闰四月初一
    (2033, 11, 1, True),  # 闰十一月初一
    (1901, 1, 1, False),
    (2100, 12, 1, False),
]
# 与 main.c 的 k_local_times 同序，北京时间
LOCAL_TIMES = [
    (1970, 1, 1, 8, 0, 0),
    (2026, 10, 27, 0, 0, 0),
    (2106, 2, 7, 14, 28, 15),
    (2106, 2, 7, 14, 28, 16),
    (1970, 1, 1, 7, 59, 59),
]
WORDS = HEADER_WORDS + DATE_WORDS * len(DATES) + TS_WORDS * len(LOCAL_TIMES)

CLI_CANDIDATES = [
    r"C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe",
    "/Applications/STMicroelectronics/STM32Cube/STM32CubeProgrammer/STM32CubeProgrammer.app/Contents/MacOs/bin/STM32_Programmer_CLI",
]


def find_cli(explicit: str | None) -> str:
    for c in (explicit, os.environ.get("STM32_PROGRAMMER_CLI"), shutil.which("STM32_Programmer_CLI")):
        if c and Path(c).is_file():
            return c
    for c in CLI_CANDIDATES:
        if Path(c).is_file():
            return c
    sys.exit("找不到 STM32_Programmer_CLI：用 --cli 或环境变量 STM32_PROGRAMMER_CLI 指定")


def cache_value(build: Path, key: str) -> str:
    text = (build / "CMakeCache.txt").read_text(encoding="utf-8", errors="replace")
    m = re.search(rf"^{re.escape(key)}:[A-Z]+=(.*)$", text, re.M)
    if not m:
        sys.exit(f"{build / 'CMakeCache.txt'} 里没有 {key}，先跑 cmake --preset firmware")
    return m.group(1).strip()


def symbol_address(nm: str, elf: Path, name: str) -> int:
    out = subprocess.run([nm, str(elf)], capture_output=True, text=True, check=True).stdout
    for line in out.splitlines():
        parts = line.split()
        if len(parts) == 3 and parts[2] == name:
            return int(parts[0], 16)
    sys.exit(f"{elf} 里找不到符号 {name}")


def run_cli(cli: str, args: list[str], log: list[str]) -> str:
    cmd = [cli, *args]
    proc = subprocess.run(cmd, capture_output=True)
    out = proc.stdout.decode("utf-8", errors="replace") + proc.stderr.decode("utf-8", errors="replace")
    # 进度条是非 ASCII 方块字符，留证时去掉
    kept = "\n".join(l for l in out.splitlines() if l.isascii())
    log.append("$ STM32_Programmer_CLI " + " ".join(args) + "\n" + kept)
    if proc.returncode != 0 or "Error" in out:
        print(kept)
        sys.exit(f"CLI 失败（退出码 {proc.returncode}）")
    return out


def read_words(cli: str, addr: int, count: int, log: list[str]) -> tuple[list[int], float, float]:
    """返回（读回的字，CLI 调用起点，CLI 调用终点），时刻取 time.monotonic()。"""
    t0 = time.monotonic()
    out = run_cli(cli, ["-c", "port=SWD", "mode=HOTPLUG", "-r32", f"0x{addr:08X}", f"0x{count * 4:X}"], log)
    t1 = time.monotonic()
    words: list[int] = []
    for m in re.finditer(r"^0x[0-9A-Fa-f]{8}\s*:\s*((?:[0-9A-Fa-f]{8}\s*)+)$", out, re.M):
        words += [int(w, 16) for w in m.group(1).split()]
    if len(words) < count:
        sys.exit(f"只读回 {len(words)} 个字，要 {count} 个")
    return words[:count], t0, t1


def s32(x: int) -> int:
    return x - (1 << 32) if x & 0x80000000 else x


class Checker:
    def __init__(self) -> None:
        self.failed = 0
        self.passed = 0

    def check(self, label: str, got, want) -> None:
        ok = got == want
        self.passed += ok
        self.failed += not ok
        print(f"  [{'通过' if ok else '失败'}] {label}: 读回 {got}，期望 {want}")


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--build-dir", type=Path, default=REPO / "build" / "firmware")
    ap.add_argument("--cli")
    ap.add_argument("--flash", action="store_true", help="先擦除整片 Flash 并烧录、复位")
    ap.add_argument("--samples", type=int, default=4, help="读取次数（≥2），用来拟合时基速率")
    ap.add_argument("--interval", type=float, default=15.0, help="相邻两次读取之间的秒数")
    ap.add_argument("--log", type=Path, help="把 CLI 原始输出与核对结果存到这个文件")
    args = ap.parse_args()

    build = args.build_dir.resolve()
    elf = build / "firmware" / "mcu_calendar_fw.elf"
    if not elf.is_file():
        sys.exit(f"没有 {elf}，先 cmake --build --preset firmware")
    nm = cache_value(build, "CMAKE_NM")
    cli = find_cli(args.cli)
    addr = symbol_address(nm, elf, "g_selftest")
    log: list[str] = []
    print(f"ELF {elf}\ng_selftest @ 0x{addr:08X}，{WORDS} 字")

    if args.flash:
        print("擦除整片 Flash、烧录、校验、复位 ...")
        run_cli(cli, ["-c", "port=SWD", "-e", "all", "-w", str(elf), "-v", "-rst"], log)
        time.sleep(COUNTDOWN_MS / 1000 + 1.0)  # 等倒计时到点

    samples = []
    for i in range(max(2, args.samples)):
        if i:
            time.sleep(args.interval)
        samples.append(read_words(cli, addr, WORDS, log))
        print(f"第 {i + 1} 次读取完成")
    w1, w2 = samples[0][0], samples[-1][0]

    c = Checker()
    print("头部")
    c.check("magic", hex(w2[0]), hex(MAGIC))
    c.check("version", w2[1], VERSION)
    c.check("自检完成标志", hex(w2[3]), hex(DONE))
    c.check("SYSCLK（Hz，复位后的 HSI）", w2[2], 8_000_000)

    print("倒计时（mc_countdown，真实 SysTick 驱动）")
    c.check("已到点", w2[8], 1)
    c.check("到点时刻 - 起点（ms）", (w2[7] - w2[6]) & 0xFFFFFFFF, COUNTDOWN_MS)

    print("时基")
    # 每次 CLI 调用约 1.4 s，读取落在调用中的哪一刻不知道，两点法的区间会宽到 ±5%。
    # 改为多点拟合：分别对调用起点与终点做最小二乘，两个斜率一致，说明读取在调用内的位置稳定，估计可信。
    ticks = [s[0][4] for s in samples]
    unwrapped = [ticks[0]]
    for t in ticks[1:]:
        unwrapped.append(unwrapped[-1] + ((t - unwrapped[-1]) & 0xFFFFFFFF))

    def slope(xs: list[float]) -> float:
        mx, my = sum(xs) / len(xs), sum(unwrapped) / len(xs)
        return sum((x - mx) * (y - my) for x, y in zip(xs, unwrapped)) / sum((x - mx) ** 2 for x in xs)

    k_start = slope([s[1] * 1000 for s in samples])
    k_end = slope([s[2] * 1000 for s in samples])
    rate = (k_start + k_end) / 2
    print(f"  {len(samples)} 点拟合 tick/ms：按调用起点 {k_start:.5f}，按调用终点 {k_end:.5f}")
    c.check("起点、终点两种拟合相差 < 0.1%", abs(k_start - k_end) < 1e-3, True)
    c.check(f"tick 速率 {rate:.5f} 在 HSI 出厂精度量级 ±3% 内", abs(rate - 1) <= 0.03, True)
    c.check("主循环在跑", ((w2[5] - w1[5]) & 0xFFFFFFFF) > 0, True)

    epoch = dt.date(1970, 1, 1)
    print("日期与农历（core/ 在目标上的计算）")
    for i, ((y, m, d), lunar) in enumerate(zip(DATES, LUNAR_EXPECTED)):
        base = HEADER_WORDS + DATE_WORDS * i
        r = w2[base:base + DATE_WORDS]
        date = dt.date(y, m, d)
        tag = date.isoformat()
        c.check(f"{tag} 输入回显", (s32(r[0]), r[1], r[2]), (y, m, d))
        c.check(f"{tag} 距 1970-01-01 天数", s32(r[3]), (date - epoch).days)
        c.check(f"{tag} 星期（0=周日）", r[4], date.isoweekday() % 7)
        c.check(f"{tag} 农历", (r[5], s32(r[6]), r[7], r[8], bool(r[9])), (1, *lunar))

    print("32 位时间戳（北京时间 -> UTC 秒）")
    for i, (y, mo, d, h, mi, s) in enumerate(LOCAL_TIMES):
        base = HEADER_WORDS + DATE_WORDS * len(DATES) + TS_WORDS * i
        ok_word, ts = w2[base], w2[base + 1]
        want = calendar.timegm((dt.datetime(y, mo, d, h, mi, s) - dt.timedelta(hours=8)).timetuple())
        in_range = 0 <= want <= 0xFFFFFFFF
        tag = f"{y:04d}-{mo:02d}-{d:02d} {h:02d}:{mi:02d}:{s:02d}"
        c.check(f"{tag} 是否在范围内", bool(ok_word), in_range)
        if in_range:
            c.check(f"{tag} 时间戳", ts, want)

    summary = f"合计：通过 {c.passed}，失败 {c.failed}"
    print(summary)
    if args.log:
        args.log.write_text("\n\n".join(log) + "\n\n" + summary + "\n", encoding="utf-8")
    return 1 if c.failed else 0


if __name__ == "__main__":
    sys.exit(main())
