#include "ui.h"

#include <stddef.h>
#include <string.h>

#include "bsp.h"
#include "gfx.h"
#include "mc/mc_lunar.h"

#define BG GFX_BLACK

/* 大号时分秒的格子：六位数字，两两之间夹冒号 */
#define DIG_W 44u
#define DIG_H 96u
#define DIG_T 10u
#define DIG_GAP 10u
#define COLON_W 20u
#define DIG_Y 48u

#define DATE_Y 8u
#define UNIX_Y 160u
#define LUNAR_Y 186u
#define STATUS_Y 216u

static const char *const k_weekday[7] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};

/* 上次画上去的内容；0xFF 表示「没画过」，第一次全量画 */
static uint8_t s_digits[6];
static int32_t s_day = -1; /* 按 1970 起的天数记，日期变了才重画日期与农历两行 */
static uint32_t s_unix;
static bool s_unix_drawn;
static int s_status = -1;
static bool s_digits_set; /* 上次画数字时的 time_set：变了要整组重画，否则没变的那几位留着旧颜色 */

static uint16_t digit_x(uint8_t i)
{
    /* 位置：d d : d d : d d */
    const uint16_t total = (uint16_t)(6u * DIG_W + 3u * DIG_GAP + 2u * (COLON_W + 2u * DIG_GAP));
    uint16_t x = (uint16_t)((BSP_LCD_W - total) / 2u);
    for (uint8_t k = 0; k < i; ++k) {
        x = (uint16_t)(x + DIG_W + DIG_GAP);
        if (k == 1u || k == 3u) {
            x = (uint16_t)(x + COLON_W + 2u * DIG_GAP - DIG_GAP);
        }
    }
    return x;
}

static void draw_colon(uint16_t x)
{
    const uint16_t s = DIG_T;
    const uint16_t cx = (uint16_t)(x + (COLON_W - s) / 2u);
    bsp_lcd_fill(cx, (uint16_t)(DIG_Y + DIG_H / 3u - s / 2u), (uint16_t)(cx + s - 1u),
                 (uint16_t)(DIG_Y + DIG_H / 3u + s / 2u - 1u), GFX_WHITE);
    bsp_lcd_fill(cx, (uint16_t)(DIG_Y + 2u * DIG_H / 3u - s / 2u), (uint16_t)(cx + s - 1u),
                 (uint16_t)(DIG_Y + 2u * DIG_H / 3u + s / 2u - 1u), GFX_WHITE);
}

/* 把 v 写成 n 位十进制（左补零），返回写完后的指针 */
static char *put_num(char *p, uint32_t v, uint8_t n)
{
    for (uint8_t i = n; i > 0u; --i) {
        p[i - 1u] = (char)('0' + v % 10u);
        v /= 10u;
    }
    return p + n;
}

static char *put_str(char *p, const char *s)
{
    while (*s != '\0') {
        *p++ = *s++;
    }
    return p;
}

/* 整行重画：先清到行尾，避免上次更长的内容留下残影 */
static void draw_line(uint16_t y, const char *text, uint8_t scale, uint16_t fg, bool center)
{
    const uint16_t w = gfx_text_width(text, scale);
    const uint16_t x = center && w < BSP_LCD_W ? (uint16_t)((BSP_LCD_W - w) / 2u) : 8u;
    bsp_lcd_fill(0, y, (uint16_t)(BSP_LCD_W - 1u), (uint16_t)(y + 8u * scale - 1u), BG);
    (void)gfx_text(x, y, text, scale, fg, BG);
}

void ui_clock_init(void)
{
    bsp_lcd_fill(0, 0, (uint16_t)(BSP_LCD_W - 1u), (uint16_t)(BSP_LCD_H - 1u), BG);
    draw_colon((uint16_t)(digit_x(1) + DIG_W + DIG_GAP));
    draw_colon((uint16_t)(digit_x(3) + DIG_W + DIG_GAP));
    memset(s_digits, 0xFF, sizeof s_digits);
    s_day = -1;
    s_unix_drawn = false;
    s_status = -1;
}

static void show_date_rows(const mc_datetime_t *t)
{
    char buf[32];
    char *p = put_num(buf, (uint32_t)t->date.year, 4);
    *p++ = '-';
    p = put_num(p, t->date.month, 2);
    *p++ = '-';
    p = put_num(p, t->date.day, 2);
    *p++ = ' ';
    p = put_str(p, k_weekday[mc_weekday(t->date) % 7u]);
    *p = '\0';
    draw_line(DATE_Y, buf, 3, GFX_WHITE, true);

    /* 农历暂以数字显示：「LUNAR 2026 09-19」，闰月在月前加 R。汉字月日名等字库 */
    mc_lunar_t lunar;
    if (mc_lunar_from_solar(t->date, &lunar)) {
        p = put_str(buf, "LUNAR ");
        p = put_num(p, (uint32_t)lunar.year, 4);
        *p++ = ' ';
        if (lunar.is_leap) {
            *p++ = 'R';
        }
        p = put_num(p, lunar.month, 2);
        *p++ = '-';
        p = put_num(p, lunar.day, 2);
        *p = '\0';
    } else {
        p = put_str(buf, "LUNAR --");
        *p = '\0';
    }
    draw_line(LUNAR_Y, buf, 2, GFX_YELLOW, false);
}

void ui_clock_show(const ui_clock_t *st)
{
    const mc_datetime_t *t = &st->local;

    const int32_t day = mc_days_from_civil(t->date.year, t->date.month, t->date.day);
    if (day != s_day) {
        s_day = day;
        show_date_rows(t);
    }

    const uint8_t d[6] = {(uint8_t)(t->hour / 10u), (uint8_t)(t->hour % 10u),     (uint8_t)(t->minute / 10u),
                          (uint8_t)(t->minute % 10u), (uint8_t)(t->second / 10u), (uint8_t)(t->second % 10u)};
    if (st->time_set != s_digits_set) {
        s_digits_set = st->time_set;
        memset(s_digits, 0xFF, sizeof s_digits);
    }
    for (uint8_t i = 0; i < 6u; ++i) {
        if (d[i] != s_digits[i]) {
            s_digits[i] = d[i];
            gfx_seg_digit(digit_x(i), DIG_Y, DIG_W, DIG_H, DIG_T, d[i], st->time_set ? GFX_WHITE : GFX_GRAY, BG);
        }
    }

    if (!s_unix_drawn || st->unix_s != s_unix) {
        s_unix_drawn = true;
        s_unix = st->unix_s;
        char buf[24];
        char *p = put_str(buf, "UNIX ");
        p = put_num(p, st->unix_s, 10);
        *p = '\0';
        draw_line(UNIX_Y, buf, 2, GFX_CYAN, false);
    }

    /* 状态行：RTC 是否校过时 + 网络校时状态 */
    const int status = (st->rtc_ok ? 1 : 0) | (st->time_set ? 2 : 0) | (st->sync_state << 2);
    if (status != s_status) {
        s_status = status;
        static const char *const k_sync[] = {"", "SYNC...", "SYNC OK", "SYNC FAIL", "SYNC RTC ERR"};
        char buf[40];
        char *p = put_str(buf, !st->rtc_ok ? "RTC ERR" : st->time_set ? "RTC OK" : "RTC NOT SET");
        if (st->sync_state < sizeof k_sync / sizeof k_sync[0] && st->sync_state != 0u) {
            *p++ = ' ';
            *p++ = ' ';
            p = put_str(p, k_sync[st->sync_state]);
        }
        *p = '\0';
        draw_line(STATUS_Y, buf, 2, st->rtc_ok && st->time_set ? GFX_GREEN : GFX_RED, false);
    }
}

/* ---- 时间设置界面 ---- */

#define SET_SCALE 5u
#define SET_CH_W (6u * SET_SCALE)
#define SET_DATE_X ((BSP_LCD_W - 10u * SET_CH_W) / 2u)
#define SET_DATE_Y 56u
#define SET_TIME_X ((BSP_LCD_W - 8u * SET_CH_W) / 2u)
#define SET_TIME_Y 118u

void ui_set_init(void)
{
    bsp_lcd_fill(0, 0, (uint16_t)(BSP_LCD_W - 1u), (uint16_t)(BSP_LCD_H - 1u), BG);
    draw_line(8, "SET TIME", 3, GFX_YELLOW, true);
    draw_line(190, "UP +   K0 -   K1 NEXT", 2, GFX_GRAY, false);
    draw_line(214, "HOLD K1 SAVE   30S IDLE CANCEL", 2, GFX_GRAY, false);
}

/* 画一个字段（n 位数字）加它后面的分隔符；选中的字段反色 */
static uint16_t set_field(uint16_t x, uint16_t y, uint32_t v, uint8_t n, bool selected, const char *sep)
{
    char buf[5];
    *put_num(buf, v, n) = '\0';
    x = gfx_text(x, y, buf, SET_SCALE, selected ? GFX_BLACK : GFX_WHITE, selected ? GFX_YELLOW : BG);
    return gfx_text(x, y, sep, SET_SCALE, GFX_WHITE, BG);
}

void ui_set_show(const mc_edit_t *e)
{
    const mc_datetime_t *t = &e->dt;
    uint16_t x = (uint16_t)SET_DATE_X;
    x = set_field(x, SET_DATE_Y, (uint32_t)t->date.year, 4, e->field == MC_EDIT_YEAR, "-");
    x = set_field(x, SET_DATE_Y, t->date.month, 2, e->field == MC_EDIT_MONTH, "-");
    (void)set_field(x, SET_DATE_Y, t->date.day, 2, e->field == MC_EDIT_DAY, "");
    x = (uint16_t)SET_TIME_X;
    x = set_field(x, SET_TIME_Y, t->hour, 2, e->field == MC_EDIT_HOUR, ":");
    x = set_field(x, SET_TIME_Y, t->minute, 2, e->field == MC_EDIT_MINUTE, ":");
    (void)set_field(x, SET_TIME_Y, t->second, 2, e->field == MC_EDIT_SECOND, "");
}

/* ---- 万年历月视图 ---- */

#define CAL_CELL_W 56u
#define CAL_CELL_H 31u
#define CAL_X0 4u
#define CAL_HEAD_Y 26u
#define CAL_GRID_Y 48u

void ui_cal_show(const mc_calview_t *v, mc_date_t today)
{
    static const char *const k_head[7] = {"SU", "MO", "TU", "WE", "TH", "FR", "SA"};
    bsp_lcd_fill(0, 0, (uint16_t)(BSP_LCD_W - 1u), (uint16_t)(BSP_LCD_H - 1u), BG);

    char buf[8];
    char *p = put_num(buf, (uint32_t)v->year, 4);
    *p++ = '-';
    p = put_num(p, v->month, 2);
    *p = '\0';
    uint16_t x = gfx_text(8, 4, buf, 2, GFX_YELLOW, BG);
    (void)gfx_text((uint16_t)(x + 24u), 4, "UP/K0 MONTH  HOLD YEAR", 2, GFX_GRAY, BG);

    const uint16_t text_dx = (uint16_t)((CAL_CELL_W - 2u * 12u) / 2u);
    for (uint8_t c = 0; c < 7u; ++c) {
        const uint16_t fg = (c == 0u || c == 6u) ? GFX_RED : GFX_CYAN;
        (void)gfx_text((uint16_t)(CAL_X0 + c * CAL_CELL_W + text_dx), CAL_HEAD_Y, k_head[c], 2, fg, BG);
    }

    uint8_t grid[42];
    mc_calview_fill_grid(v, grid);
    const bool this_month = today.year == v->year && today.month == v->month;
    for (uint8_t i = 0; i < 42u; ++i) {
        if (grid[i] == 0u) {
            continue;
        }
        const uint8_t c = (uint8_t)(i % 7u);
        const uint16_t cx = (uint16_t)(CAL_X0 + c * CAL_CELL_W);
        const uint16_t cy = (uint16_t)(CAL_GRID_Y + (i / 7u) * CAL_CELL_H);
        const bool is_today = this_month && grid[i] == today.day;
        uint16_t fg = (c == 0u || c == 6u) ? GFX_RED : GFX_WHITE;
        uint16_t bg = BG;
        if (is_today) { /* 今天：整格反色 */
            fg = GFX_BLACK;
            bg = GFX_CYAN;
            bsp_lcd_fill(cx, cy, (uint16_t)(cx + CAL_CELL_W - 3u), (uint16_t)(cy + CAL_CELL_H - 3u), bg);
        }
        char num[3];
        num[0] = grid[i] >= 10u ? (char)('0' + grid[i] / 10u) : ' ';
        num[1] = (char)('0' + grid[i] % 10u);
        num[2] = '\0';
        (void)gfx_text((uint16_t)(cx + text_dx), (uint16_t)(cy + 6u), num, 2, fg, bg);
    }
}

/* ---- 会跳动的文字：只重画变了的字符 ---- */

/* prev 是上次画上去的内容（cap 字节，含结尾 0）。force 时整行重画（颜色变了、刚清过屏）。
 * 调用者保证同一位置每次给的 text 等长，短了的部分不会被擦掉 */
static void live_text(char *prev, size_t cap, uint16_t x, uint16_t y, const char *text, uint8_t scale, uint16_t fg,
                      uint16_t bg, bool force)
{
    for (size_t i = 0; text[i] != '\0' && i + 1u < cap; ++i) {
        if (force || prev[i] != text[i]) {
            const char one[2] = {text[i], '\0'};
            (void)gfx_text((uint16_t)(x + i * 6u * scale), y, one, scale, fg, bg);
            prev[i] = text[i];
        }
    }
}

/* ---- 闹钟界面 ---- */

#define BIG_SCALE 8u
#define BIG_CH_W (6u * BIG_SCALE)
#define BIG_X ((BSP_LCD_W - 5u * BIG_CH_W) / 2u)
#define BIG_Y 52u

const char *ui_alarm_mode_name(uint8_t weekdays)
{
    if (weekdays == MC_ALARM_ONCE) {
        return "ONCE ";
    }
    if (weekdays == MC_ALARM_EVERYDAY) {
        return "DAILY";
    }
    if (weekdays == UI_ALARM_WORKDAYS) {
        return "MO-FR";
    }
    return "DAYS ";
}

void ui_alarm_init(void)
{
    bsp_lcd_fill(0, 0, (uint16_t)(BSP_LCD_W - 1u), (uint16_t)(BSP_LCD_H - 1u), BG);
    draw_line(8, "ALARM", 3, GFX_YELLOW, true);
    draw_line(214, "K0 FIELD   UP CHANGE   K1 NEXT", 2, GFX_GRAY, false);
}

static uint16_t sel_text(uint16_t x, uint16_t y, const char *s, uint8_t scale, bool selected, uint16_t fg)
{
    return gfx_text(x, y, s, scale, selected ? GFX_BLACK : fg, selected ? GFX_YELLOW : BG);
}

void ui_alarm_show(const mc_alarm_t *al, ui_alarm_field_t field, bool ringing)
{
    char buf[3];
    uint16_t x = (uint16_t)BIG_X;
    *put_num(buf, al->hour, 2) = '\0';
    x = sel_text(x, BIG_Y, buf, BIG_SCALE, field == UI_ALARM_HOUR, GFX_WHITE);
    x = gfx_text(x, BIG_Y, ":", BIG_SCALE, GFX_WHITE, BG);
    *put_num(buf, al->minute, 2) = '\0';
    (void)sel_text(x, BIG_Y, buf, BIG_SCALE, field == UI_ALARM_MINUTE, GFX_WHITE);

    x = (uint16_t)BIG_X;
    x = sel_text(x, 136, ui_alarm_mode_name(al->weekdays), 3, field == UI_ALARM_MODE, GFX_CYAN);
    x = gfx_text(x, 136, "  ", 3, GFX_WHITE, BG);
    (void)sel_text(x, 136, al->enabled ? "ON " : "OFF", 3, field == UI_ALARM_ENABLE,
                   al->enabled ? GFX_GREEN : GFX_GRAY);

    draw_line(176, ringing ? "RINGING - PRESS ANY KEY" : "", 2, GFX_RED, true);
}

/* ---- 番茄钟界面 ---- */

static char s_pomo_big[6];
static char s_pomo_phase[13];
static char s_pomo_info[24];

void ui_pomo_init(void)
{
    bsp_lcd_fill(0, 0, (uint16_t)(BSP_LCD_W - 1u), (uint16_t)(BSP_LCD_H - 1u), BG);
    draw_line(8, "POMODORO", 3, GFX_YELLOW, true);
    draw_line(196, "UP START/PAUSE   K0 STOP", 2, GFX_GRAY, false);
    draw_line(218, "HOLD K0 PRESET   K1 NEXT", 2, GFX_GRAY, false);
    memset(s_pomo_big, 0, sizeof s_pomo_big);
    memset(s_pomo_phase, 0, sizeof s_pomo_phase);
    memset(s_pomo_info, 0, sizeof s_pomo_info);
}

/* 剩余毫秒 → 向上取整到秒的 MM:SS（剩 0.3 s 时显示 00:01，归零那一刻才是 00:00），分钟封顶 99 */
static void fmt_mmss_ceil(char out[6], uint32_t ms)
{
    uint32_t s = (ms + 999u) / 1000u;
    if (s > 99u * 60u + 59u) {
        s = 99u * 60u + 59u;
    }
    char *p = put_num(out, s / 60u, 2);
    *p++ = ':';
    *put_num(p, s % 60u, 2) = '\0';
}

void ui_pomo_show(const mc_pomodoro_t *p, uint32_t now_ms)
{
    static const char *const k_phase[] = {"IDLE ", "WORK ", "BREAK"};
    char text[24];

    /* 颜色随阶段变，所以阶段一变两行都整行重画 */
    const bool work = p->phase == MC_POMODORO_WORK;
    char *q = put_str(text, k_phase[p->phase <= MC_POMODORO_BREAK ? p->phase : 0]);
    q = put_str(q, p->phase != MC_POMODORO_IDLE && !p->running ? " PAUSED" : "       ");
    *q = '\0';
    const bool phase_changed = strncmp(text, s_pomo_phase, 5) != 0;
    live_text(s_pomo_phase, sizeof s_pomo_phase, (uint16_t)BIG_X, 40, text, 3, work ? GFX_RED : GFX_GREEN, BG,
              phase_changed);

    fmt_mmss_ceil(text, p->phase == MC_POMODORO_IDLE ? p->work_ms : mc_pomodoro_remaining(p, now_ms));
    live_text(s_pomo_big, sizeof s_pomo_big, (uint16_t)BIG_X, 72, text, BIG_SCALE, GFX_WHITE, BG, phase_changed);

    q = put_str(text, "DONE ");
    q = put_num(q, p->completed > 999u ? 999u : p->completed, 3);
    q = put_str(q, "   SET ");
    q = put_num(q, p->work_ms / 60000u > 99u ? 99u : p->work_ms / 60000u, 2);
    *q++ = '/';
    q = put_num(q, p->break_ms / 60000u > 99u ? 99u : p->break_ms / 60000u, 2);
    *q = '\0';
    live_text(s_pomo_info, sizeof s_pomo_info, (uint16_t)BIG_X, 150, text, 2, GFX_CYAN, BG, false);
}

/* ---- 多路秒表与倒计时界面 ---- */

#define TM_ROWS (MC_STOPWATCH_CHANNELS + MC_COUNTDOWN_CHANNELS)
#define TM_SCALE 3u
#define TM_Y0 24u
#define TM_DY 27u
#define TM_LABEL_X 8u
#define TM_VALUE_X (TM_LABEL_X + 4u * 6u * TM_SCALE)

static char s_tm_prev[TM_ROWS][14];
static int s_tm_sel = -1;

void ui_timers_init(void)
{
    bsp_lcd_fill(0, 0, (uint16_t)(BSP_LCD_W - 1u), (uint16_t)(BSP_LCD_H - 1u), BG);
    (void)gfx_text(8, 2, "K0 SEL  UP RUN/STOP  HOLD RESET", 2, GFX_GRAY, BG);
    memset(s_tm_prev, 0, sizeof s_tm_prev);
    s_tm_sel = -1;
}

static void tm_label(uint8_t row, bool selected)
{
    char label[4];
    const bool sw = row < MC_STOPWATCH_CHANNELS;
    label[0] = sw ? 'S' : 'C';
    label[1] = sw ? 'W' : 'D';
    label[2] = (char)('1' + (sw ? row : row - MC_STOPWATCH_CHANNELS));
    label[3] = '\0';
    (void)sel_text(TM_LABEL_X, (uint16_t)(TM_Y0 + row * TM_DY), label, TM_SCALE, selected, sw ? GFX_CYAN : GFX_YELLOW);
}

void ui_timers_show(const mc_timer_bank_t *bank, uint8_t selected, uint32_t now_ms)
{
    if ((int)selected != s_tm_sel) {
        for (uint8_t r = 0; r < TM_ROWS; ++r) {
            if (s_tm_sel < 0 || r == selected || (int)r == s_tm_sel) {
                tm_label(r, r == selected);
            }
        }
        s_tm_sel = selected;
    }

    for (uint8_t r = 0; r < TM_ROWS; ++r) {
        char text[14];
        char *p;
        const char *state;
        if (r < MC_STOPWATCH_CHANNELS) {
            const mc_stopwatch_t *sw = &bank->stopwatch[r];
            uint32_t tenths = mc_stopwatch_elapsed(sw, now_ms) / 100u;
            if (tenths > 99u * 600u + 599u) { /* 封顶 99:59.9 */
                tenths = 99u * 600u + 599u;
            }
            p = put_num(text, tenths / 600u, 2);
            *p++ = ':';
            p = put_num(p, tenths / 10u % 60u, 2);
            *p++ = '.';
            p = put_num(p, tenths % 10u, 1);
            state = sw->running ? "RUN " : "STOP";
        } else {
            const mc_countdown_t *cd = &bank->countdown[r - MC_STOPWATCH_CHANNELS];
            fmt_mmss_ceil(text, mc_countdown_remaining(cd, now_ms));
            p = put_str(text + 5, "  ");
            state = cd->running ? "RUN " : cd->fired ? "DONE" : cd->remaining_ms < cd->duration_ms ? "PAUS" : "STOP";
        }
        *p++ = ' ';
        p = put_str(p, state);
        *p = '\0';
        live_text(s_tm_prev[r], sizeof s_tm_prev[r], (uint16_t)TM_VALUE_X, (uint16_t)(TM_Y0 + r * TM_DY), text,
                  TM_SCALE, GFX_WHITE, BG, false);
    }
}
