/* 首版上板自检。
 *
 * 板卡原理图未得，这一版不碰任何 GPIO 与外设引脚（引脚一个都不猜），只用复位后的 HSI 8 MHz
 * 与 SysTick；结果写进 g_selftest，由 SWD 只读取回、在 PC 上与独立来源比对
 * （firmware/scripts/selftest.py，期望值的出处写在那边）。要验的是三件事：
 *   1. core/ 在 Cortex-M3 上算出的日期、农历、时间戳与 PC 上的独立实现一致；
 *   2. SysTick 毫秒时基按真实时间走；
 *   3. mc_countdown 拿真实时基驱动，到点时刻准确。
 *
 * g_selftest 只由 32 位字组成，SWD 读回后按字解析，不受结构体填充影响。
 * 布局改了要同步改 selftest.py，并把 SELFTEST_VERSION 加一。 */
#include <stdint.h>

#include "stm32f1xx_hal.h"

#include "mc/mc_date.h"
#include "mc/mc_lunar.h"
#include "mc/mc_timers.h"

#define SELFTEST_MAGIC 0x4D434331u /* "MCC1" */
#define SELFTEST_VERSION 1u
#define SELFTEST_DONE 0xD0D0D0D0u
#define SELFTEST_COUNTDOWN_MS 5000u
#define SELFTEST_DATES 6u
#define SELFTEST_TIMESTAMPS 5u

typedef struct {
    int32_t year; /* 输入回显，防止 PC 端期望值与这里的顺序错位 */
    int32_t month;
    int32_t day;
    int32_t days_from_epoch;
    uint32_t weekday;
    uint32_t lunar_ok;
    int32_t lunar_year;
    uint32_t lunar_month;
    uint32_t lunar_day;
    uint32_t lunar_leap;
} selftest_date_t;

typedef struct {
    uint32_t ok;
    uint32_t ts;
} selftest_ts_t;

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t sysclk_hz; /* 按 RCC 寄存器反算，不是 SystemCoreClock 变量 */
    uint32_t done;
    uint32_t tick_ms;
    uint32_t loops;
    uint32_t countdown_start_ms;
    uint32_t countdown_fired_ms;
    uint32_t countdown_fired;
    selftest_date_t date[SELFTEST_DATES];
    selftest_ts_t ts[SELFTEST_TIMESTAMPS];
} selftest_t;

volatile selftest_t g_selftest;

/* 两个闰月（2020 闰四月、2033 闰十一月）、闰日、农历表两端 */
static const mc_date_t k_dates[SELFTEST_DATES] = {
    {2026, 10, 27}, {2000, 2, 29}, {2020, 5, 23}, {2033, 12, 22}, {1901, 2, 19}, {2100, 12, 31},
};

/* 北京时间；32 位无符号时间戳的两端各取一个刚好在内、一个刚好越界的点 */
static const mc_datetime_t k_local_times[SELFTEST_TIMESTAMPS] = {
    {.date = {1970, 1, 1}, .hour = 8, .minute = 0, .second = 0},
    {.date = {2026, 10, 27}, .hour = 0, .minute = 0, .second = 0},
    {.date = {2106, 2, 7}, .hour = 14, .minute = 28, .second = 15},
    {.date = {2106, 2, 7}, .hour = 14, .minute = 28, .second = 16},
    {.date = {1970, 1, 1}, .hour = 7, .minute = 59, .second = 59},
};

static void run_core_checks(void)
{
    for (uint32_t i = 0; i < SELFTEST_DATES; ++i) {
        const mc_date_t d = k_dates[i];
        mc_lunar_t lunar = {0};
        const bool ok = mc_lunar_from_solar(d, &lunar);

        g_selftest.date[i].year = d.year;
        g_selftest.date[i].month = d.month;
        g_selftest.date[i].day = d.day;
        g_selftest.date[i].days_from_epoch = mc_days_from_civil(d.year, d.month, d.day);
        g_selftest.date[i].weekday = mc_weekday(d);
        g_selftest.date[i].lunar_ok = ok;
        g_selftest.date[i].lunar_year = lunar.year;
        g_selftest.date[i].lunar_month = lunar.month;
        g_selftest.date[i].lunar_day = lunar.day;
        g_selftest.date[i].lunar_leap = lunar.is_leap;
    }

    for (uint32_t i = 0; i < SELFTEST_TIMESTAMPS; ++i) {
        uint32_t ts = 0;
        g_selftest.ts[i].ok = mc_unix_from_local(&k_local_times[i], MC_TZ_BEIJING_S, &ts);
        g_selftest.ts[i].ts = ts;
    }
}

int main(void)
{
    HAL_Init(); /* SysTick 1 ms；不配 PLL，时钟保持复位后的 HSI */

    g_selftest.magic = SELFTEST_MAGIC;
    g_selftest.version = SELFTEST_VERSION;
    g_selftest.sysclk_hz = HAL_RCC_GetSysClockFreq();
    run_core_checks();

    mc_countdown_t countdown;
    mc_countdown_set(&countdown, SELFTEST_COUNTDOWN_MS);
    const uint32_t start = HAL_GetTick();
    mc_countdown_start(&countdown, start);
    g_selftest.countdown_start_ms = start;
    g_selftest.done = SELFTEST_DONE;

    for (;;) {
        const uint32_t now = HAL_GetTick();
        if (mc_countdown_poll(&countdown, now)) {
            g_selftest.countdown_fired_ms = now;
            g_selftest.countdown_fired = 1u;
        }
        g_selftest.tick_ms = now;
        g_selftest.loops++;
    }
}
