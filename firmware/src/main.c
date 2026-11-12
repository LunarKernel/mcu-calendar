/* 应用固件：板级初始化后把 app/ 跑在主循环里。
 *
 * 能做的：72 MHz 时钟、两路串口、片内 RTC 走时、按键与蜂鸣器；屏上显示时钟主界面
 * （日期星期、大号时分秒、Unix 时间戳、数字形式的农历、RTC 与校时状态，见 ui.c）。
 * 三个界面，按键（短按在松开时生效，长按 0.8 s）：
 *   时钟：KEY1 短按进万年历，KEY1 长按进时间设置，KEY_UP 长按发起网络校时；
 *   设置：KEY_UP 加、KEY0 减（按住连发），KEY1 短按换字段，KEY1 长按保存并返回；30 s 不动放弃修改；
 *   万年历：KEY_UP 下一月、KEY0 上一月，按住则按年翻（连发），KEY1 短按返回。
 * 整点响一声（基本功能 2 的报时），在哪个界面都响。
 * 调试串口（USART1，115200 8N1，经 USB2 的 CH340）：
 *   - 每秒打印一行本地时间；
 *   - 收到「T2026-10-29 16:45:00」一行即校时，PC 上可以 `date "+T%Y-%m-%d %H:%M:%S"` 发过来；
 *   - 收到「?」打印一次状态。
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

#include "mc/mc_calview.h"
#include "mc/mc_date.h"
#include "mc/mc_edit.h"
#include "mc/mc_key.h"
#include "mc/mc_text.h"

#define DIAG_MAGIC 0x4D434631u /* "MCF1" */
#define DIAG_VERSION 1u
#define KEY_POLL_MS 5u
#define KEY_BEEP_MS 40u
#define KEY_REPEAT_MS 150u
#define SAVE_BEEP_MS 120u
#define SET_IDLE_TIMEOUT_MS 30000u
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

typedef enum { FW_MODE_CLOCK, FW_MODE_SET, FW_MODE_CAL } fw_mode_t;
typedef enum { KEY_ACT_CLICK, KEY_ACT_LONG, KEY_ACT_REPEAT } key_act_t;

static bool s_lcd_ok;
static fw_mode_t s_mode;
static mc_edit_t s_edit;
static mc_calview_t s_cal;
static uint32_t s_set_touch_ms; /* 设置界面最后一次按键的时刻 */
static mc_datetime_t s_now;     /* 最近一次读到的本地时间；RTC 不可用时保持全零（非法日期） */
static uint32_t s_now_unix;

static void draw_clock(void)
{
    if (s_lcd_ok && s_mode == FW_MODE_CLOCK) {
        const ui_clock_t view = {s_now, s_now_unix, g_diag.rtc_ok != 0u, g_diag.rtc_time_set != 0u,
                                 (uint8_t)app_sync_state()};
        ui_clock_show(&view);
    }
}

static void enter_mode(fw_mode_t mode, uint32_t now)
{
    s_mode = mode;
    if (!s_lcd_ok) {
        return;
    }
    switch (mode) {
    case FW_MODE_SET:
        mc_edit_begin(&s_edit, &s_now);
        s_set_touch_ms = now;
        ui_set_init();
        ui_set_show(&s_edit);
        break;
    case FW_MODE_CAL:
        mc_calview_init(&s_cal, s_now.date.year, s_now.date.month);
        ui_cal_show(&s_cal, s_now.date);
        break;
    default:
        ui_clock_init();
        draw_clock();
        break;
    }
}

static void act_clock(bsp_key_t key, key_act_t act, uint32_t now)
{
    if (key == BSP_KEY1 && act == KEY_ACT_CLICK) {
        enter_mode(FW_MODE_CAL, now);
    } else if (key == BSP_KEY1 && act == KEY_ACT_LONG) {
        fw_beep(KEY_BEEP_MS);
        enter_mode(FW_MODE_SET, now);
    } else if (key == BSP_KEY_UP && act == KEY_ACT_LONG) {
        bsp_dbg_puts("KEY_UP: sync\r\n");
        fw_beep(KEY_BEEP_MS);
        app_request_sync();
    }
}

static void act_set(bsp_key_t key, key_act_t act, uint32_t now)
{
    s_set_touch_ms = now;
    if (key == BSP_KEY1) {
        if (act == KEY_ACT_CLICK) {
            mc_edit_next_field(&s_edit);
            ui_set_show(&s_edit);
        } else if (act == KEY_ACT_LONG) {
            /* mc_edit 保证 dt 恒为合法日期时间，这里失败只可能是写 RTC 失败 */
            const bool ok = port_rtc_set(&s_edit.dt);
            bsp_dbg_puts(ok ? "OK set by keys\r\n" : "ERR rtc write\r\n");
            fw_beep(ok ? SAVE_BEEP_MS : CHIME_BEEP_MS);
            if (ok) {
                /* 界面立刻用新时间，不等下一次读 RTC */
                s_now = s_edit.dt;
                (void)mc_unix_from_local(&s_now, MC_TZ_BEIJING_S, &s_now_unix);
                g_diag.rtc_time_set = 1u;
            }
            enter_mode(FW_MODE_CLOCK, now);
        }
        return;
    }
    /* KEY_UP 加、KEY0 减；短按一步，按住后长按与连发各一步 */
    mc_edit_step(&s_edit, key == BSP_KEY_UP ? 1 : -1);
    ui_set_show(&s_edit);
}

static void act_cal(bsp_key_t key, key_act_t act, uint32_t now)
{
    if (key == BSP_KEY1) {
        if (act == KEY_ACT_CLICK) {
            enter_mode(FW_MODE_CLOCK, now);
        }
        return;
    }
    const int32_t dir = key == BSP_KEY_UP ? 1 : -1;
    const bool moved =
        act == KEY_ACT_CLICK ? mc_calview_step_month(&s_cal, dir) : mc_calview_step_year(&s_cal, dir);
    if (moved && s_lcd_ok) { /* 到 1900-01 / 2099-12 即停，没动就不重画 */
        ui_cal_show(&s_cal, s_now.date);
    }
}

static void on_key_action(bsp_key_t key, key_act_t act, uint32_t now)
{
    switch (s_mode) {
    case FW_MODE_SET:
        act_set(key, act, now);
        break;
    case FW_MODE_CAL:
        act_cal(key, act, now);
        break;
    default:
        act_clock(key, act, now);
        break;
    }
}

/* 把 mc_key 的原始事件换成界面用的三种动作。短按在松开时才算：按下那一刻还分不出是短按还是长按的开头 */
static void poll_keys(mc_key_t keys[BSP_KEY_COUNT], uint32_t repeat_ms[BSP_KEY_COUNT], uint32_t now)
{
    for (uint8_t i = 0; i < BSP_KEY_COUNT; ++i) {
        const bsp_key_t key = (bsp_key_t)i;
        switch (mc_key_poll(&keys[i], bsp_key_pressed(key), now)) {
        case MC_KEY_EV_PRESS:
            g_diag.key_press[i]++;
            break;
        case MC_KEY_EV_RELEASE:
            if (!keys[i].long_sent) {
                on_key_action(key, KEY_ACT_CLICK, now);
            }
            break;
        case MC_KEY_EV_LONG:
            repeat_ms[i] = now;
            on_key_action(key, KEY_ACT_LONG, now);
            break;
        default:
            if (keys[i].stable && keys[i].long_sent && now - repeat_ms[i] >= KEY_REPEAT_MS) {
                repeat_ms[i] = now;
                on_key_action(key, KEY_ACT_REPEAT, now);
            }
            break;
        }
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
    s_lcd_ok = g_diag.lcd_id == LCD_ID_R61509V;
    if (s_lcd_ok) {
        bsp_lcd_panel_init();
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
    enter_mode(FW_MODE_CLOCK, HAL_GetTick()); /* RTC 状态已知之后再画：RTC 起不来时也有一行红色状态可看 */

    mc_key_t keys[BSP_KEY_COUNT];
    for (uint8_t i = 0; i < BSP_KEY_COUNT; ++i) {
        mc_key_init(&keys[i]);
    }
    uint32_t repeat_ms[BSP_KEY_COUNT] = {0};
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
            poll_keys(keys, repeat_ms, now);
        }
        if (s_mode == FW_MODE_SET && now - s_set_touch_ms >= SET_IDLE_TIMEOUT_MS) {
            enter_mode(FW_MODE_CLOCK, now); /* 放弃修改 */
        }

        uint32_t s;
        if (g_diag.rtc_ok && bsp_rtc_read(&s) && (!have_second || s != last_second)) {
            have_second = true;
            last_second = s;
            g_diag.rtc_seconds = s;
            /* 标记从备份寄存器读：网络校时经 app/ 写的 RTC，这里同样认 */
            g_diag.rtc_time_set = bsp_rtc_time_set();
            mc_local_from_unix(s, MC_TZ_BEIJING_S, &s_now);
            s_now_unix = s;
            print_time(&s_now);
            bsp_led_toggle(0); /* DS0 每秒翻转：走时的心跳 */
            if (s_now.minute == 0u && s_now.second == 0u) {
                fw_beep(CHIME_BEEP_MS);
            }
            draw_clock();
        }

        const app_sync_state_t sync = app_sync_state();
        if (g_diag.sync_state != (uint32_t)sync) {
            g_diag.sync_state = (uint32_t)sync;
            draw_clock(); /* 状态行跟着变，不等到下一秒 */
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
