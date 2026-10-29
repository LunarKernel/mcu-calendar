/* 应用固件：板级初始化后把 app/ 跑在主循环里。
 *
 * 能做的：72 MHz 时钟、两路串口、片内 RTC 走时、按键与蜂鸣器；屏上显示时钟主界面
 * （日期星期、大号时分秒、Unix 时间戳、数字形式的农历、RTC 与校时状态，见 ui.c）。
 * 屏上还没有设置界面，校时走调试串口（USART1，115200 8N1，经 USB2 的 CH340）：
 *   - 每秒打印一行本地时间；整点响一声（基本功能 2 的报时）；
 *   - 收到「T2026-10-29 16:45:00」一行即校时，PC 上可以 `date "+T%Y-%m-%d %H:%M:%S"` 发过来；
 *   - 收到「?」打印一次状态。
 * 按键：KEY0 短鸣一声，KEY1 打印状态，KEY_UP 发起网络校时（app_request_sync）。
 *
 * 另有 g_diag 放在 RAM 里，只插 ST-Link、不接串口时，也能经 SWD 只读取回看状态；
 * 往 g_set_unix 写一个非零的 UTC 秒数（diag.py --set-time），主循环会拿它校时，同样不必接串口。 */
#include <stdint.h>

#include "app.h"
#include "bsp.h"
#include "fw.h"
#include "port.h"
#include "stm32f1xx_hal.h"
#include "ui.h"

#include "mc/mc_date.h"
#include "mc/mc_key.h"
#include "mc/mc_text.h"

#define DIAG_MAGIC 0x4D434631u /* "MCF1" */
#define DIAG_VERSION 1u
#define KEY_POLL_MS 5u
#define KEY_BEEP_MS 40u
#define CHIME_BEEP_MS 500u
#define CMD_MAX 32u
#define LCD_ID_R61509V 0xB509u

/* 只由 32 位字组成，SWD 读回按字解析；布局改了要加 DIAG_VERSION */
typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t sysclk_hz;
    uint32_t clock_ok; /* 1 = 已切到 PLL 72 MHz */
    uint32_t lcd_id;
    uint32_t rtc_ok;
    uint32_t rtc_retained; /* 1 = 上电时 RTC 已在跑，断电期间备份域保住了 */
    uint32_t rtc_time_set;
    uint32_t bdcr_initial;
    uint32_t lse_wait_ms;
    uint32_t rtc_seconds; /* UTC 秒，主循环里刷新 */
    uint32_t tick_ms;
    uint32_t loops;
    uint32_t key_press[BSP_KEY_COUNT];
    uint32_t esp_rx_dropped;
    uint32_t sync_state;
} fw_diag_t;

volatile fw_diag_t g_diag;
/* 调试器写入的校时请求，0 表示没有。32 位对齐的单字写是原子的，主循环读到即清 */
volatile uint32_t g_set_unix;

static bool s_beeping;
static uint32_t s_beep_until;

void fw_beep(uint32_t ms)
{
    s_beep_until = HAL_GetTick() + ms;
    s_beeping = true;
    bsp_beep_set(true);
}

static void beep_poll(uint32_t now)
{
    if (s_beeping && (int32_t)(now - s_beep_until) >= 0) {
        s_beeping = false;
        bsp_beep_set(false);
    }
}

static void print_status(void)
{
    bsp_dbg_puts("SYSCLK ");
    bsp_dbg_put_u32(g_diag.sysclk_hz);
    bsp_dbg_puts(" Hz, LCD ID 0x");
    bsp_dbg_put_hex(g_diag.lcd_id, 4);
    bsp_dbg_puts(", RTC ok=");
    bsp_dbg_put_u32(g_diag.rtc_ok);
    bsp_dbg_puts(" retained=");
    bsp_dbg_put_u32(g_diag.rtc_retained);
    bsp_dbg_puts(" time_set=");
    bsp_dbg_put_u32(g_diag.rtc_time_set);
    bsp_dbg_puts(" BDCR0=0x");
    bsp_dbg_put_hex(g_diag.bdcr_initial, 8);
    bsp_dbg_puts(" LSE ");
    bsp_dbg_put_u32(g_diag.lse_wait_ms);
    bsp_dbg_puts(" ms, sync ");
    bsp_dbg_put_u32(g_diag.sync_state);
    bsp_dbg_puts("\r\n");
}

static void print_time(const mc_datetime_t *dt)
{
    char text[MC_DATETIME_TEXT_LEN + 1u];
    mc_datetime_format(dt, text);
    bsp_dbg_puts(text);
    bsp_dbg_puts(" w");
    bsp_dbg_put_u32(mc_weekday(dt->date)); /* 0 = 周日 */
    bsp_dbg_puts("\r\n");
}

static void handle_command(const char *line, size_t len)
{
    if (len == 1u && line[0] == '?') {
        print_status();
        return;
    }
    mc_datetime_t dt;
    if (len >= 1u && line[0] == 'T' && mc_datetime_parse(line + 1, len - 1u, &dt)) {
        if (port_rtc_set(&dt)) {
            g_diag.rtc_time_set = 1u;
            bsp_dbg_puts("OK set ");
            print_time(&dt);
        } else {
            bsp_dbg_puts("ERR rtc write\r\n");
        }
        return;
    }
    bsp_dbg_puts("ERR usage: T2026-10-29 16:45:00 | ?\r\n");
}

/* 调试口按行收指令；超长的行整行丢弃，不截半截去执行 */
static void poll_commands(void)
{
    static char line[CMD_MAX];
    static size_t len;
    static bool overflow;
    int c;
    while ((c = bsp_dbg_read_byte()) >= 0) {
        if (c == '\r' || c == '\n') {
            if (len > 0u && !overflow) {
                handle_command(line, len);
            }
            len = 0;
            overflow = false;
        } else if (len < CMD_MAX) {
            line[len++] = (char)c;
        } else {
            overflow = true;
        }
    }
}

static void on_key(bsp_key_t key, mc_key_event_t ev)
{
    if (ev != MC_KEY_EV_PRESS) {
        return;
    }
    g_diag.key_press[key]++;
    switch (key) {
    case BSP_KEY0:
        fw_beep(KEY_BEEP_MS);
        bsp_dbg_puts("KEY0\r\n");
        break;
    case BSP_KEY1:
        bsp_dbg_puts("KEY1 ");
        print_status();
        break;
    case BSP_KEY_UP:
        bsp_dbg_puts("KEY_UP: sync\r\n");
        app_request_sync();
        break;
    default:
        break;
    }
}

int main(void)
{
    HAL_Init();
    g_diag.magic = DIAG_MAGIC;
    g_diag.version = DIAG_VERSION;
    g_diag.clock_ok = bsp_clock_init();
    g_diag.sysclk_hz = HAL_RCC_GetSysClockFreq();

    bsp_gpio_init();
    bsp_uart_init();
    bsp_dbg_puts("\r\nmcu-calendar fw\r\n");

    bsp_lcd_init();
    g_diag.lcd_id = bsp_lcd_read_id();
    /* ID 不对说明接线或屏型号不符，不往一个认不出的控制器里写初始化序列 */
    const bool lcd_ok = g_diag.lcd_id == LCD_ID_R61509V;
    if (lcd_ok) {
        bsp_lcd_panel_init();
        ui_clock_init();
    }

    /* RTC 从没设过时间时的起点：2026-01-01 00:00:00 北京时间。看到这个日期就知道还没校过时 */
    static const mc_datetime_t k_default = {{2026, 1, 1}, 0, 0, 0};
    uint32_t default_s = 0;
    (void)mc_unix_from_local(&k_default, MC_TZ_BEIJING_S, &default_s);
    bsp_rtc_status_t rtc;
    bsp_rtc_init(default_s, &rtc);
    g_diag.rtc_ok = rtc.ok;
    g_diag.rtc_retained = rtc.retained;
    g_diag.rtc_time_set = rtc.time_set;
    g_diag.bdcr_initial = rtc.bdcr_initial;
    g_diag.lse_wait_ms = rtc.lse_wait_ms;
    print_status();

    app_init();

    mc_key_t keys[BSP_KEY_COUNT];
    for (uint8_t i = 0; i < BSP_KEY_COUNT; ++i) {
        mc_key_init(&keys[i]);
    }
    uint32_t last_key_poll = HAL_GetTick();
    uint32_t last_second = 0;
    bool have_second = false;

    for (;;) {
        const uint32_t now = HAL_GetTick();
        app_poll();
        poll_commands();
        if (g_set_unix != 0u) {
            const uint32_t req = g_set_unix;
            g_set_unix = 0;
            if (bsp_rtc_write(req)) {
                g_diag.rtc_time_set = 1u;
                bsp_dbg_puts("OK set by debugger\r\n");
            }
        }
        beep_poll(now);

        if (now - last_key_poll >= KEY_POLL_MS) {
            last_key_poll = now;
            for (uint8_t i = 0; i < BSP_KEY_COUNT; ++i) {
                const bsp_key_t key = (bsp_key_t)i;
                on_key(key, mc_key_poll(&keys[i], bsp_key_pressed(key), now));
            }
        }

        uint32_t s;
        if (g_diag.rtc_ok && bsp_rtc_read(&s) && (!have_second || s != last_second)) {
            have_second = true;
            last_second = s;
            g_diag.rtc_seconds = s;
            mc_datetime_t dt;
            mc_local_from_unix(s, MC_TZ_BEIJING_S, &dt);
            print_time(&dt);
            bsp_led_toggle(0); /* DS0 每秒翻转：走时的心跳 */
            if (dt.minute == 0u && dt.second == 0u) {
                fw_beep(CHIME_BEEP_MS);
            }
            if (lcd_ok) {
                const ui_clock_t view = {dt, s, g_diag.rtc_ok != 0u, g_diag.rtc_time_set != 0u,
                                         (uint8_t)app_sync_state()};
                ui_clock_show(&view);
            }
        }

        const app_sync_state_t sync = app_sync_state();
        if (g_diag.sync_state != (uint32_t)sync) {
            g_diag.sync_state = (uint32_t)sync;
            bsp_dbg_puts("sync state ");
            bsp_dbg_put_u32((uint32_t)sync);
            bsp_dbg_puts("\r\n");
        }
        bsp_led_set(1, sync == APP_SYNC_RUNNING);
        g_diag.esp_rx_dropped = bsp_esp_rx_dropped();
        g_diag.tick_ms = now;
        g_diag.loops++;
    }
}
