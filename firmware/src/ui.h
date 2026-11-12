/* 六个界面（横屏 400×240）：时钟主界面、时间设置、万年历月视图、闹钟、番茄钟、多路秒表与倒计时。
 * 时钟界面只重画变了的部分：每秒通常只动秒位与时间戳一行。 */
#ifndef UI_H
#define UI_H

#include <stdbool.h>
#include <stdint.h>

#include "mc/mc_alarm.h"
#include "mc/mc_calview.h"
#include "mc/mc_date.h"
#include "mc/mc_edit.h"
#include "mc/mc_pomodoro.h"
#include "mc/mc_timers.h"

typedef struct {
    mc_datetime_t local; /* 北京时间 */
    uint32_t unix_s;
    bool rtc_ok;
    bool time_set; /* false：RTC 还停在默认起点，没人校过 */
    uint8_t sync_state; /* app_sync_state_t 的值 */
} ui_clock_t;

void ui_clock_init(void); /* 清屏并画固定部分；之后第一次 ui_clock_show 会全量画 */
void ui_clock_show(const ui_clock_t *st);

void ui_set_init(void); /* 清屏并画标题与按键提示 */
void ui_set_show(const mc_edit_t *e); /* 画日期与时间两行，选中的字段反色 */

/* 整屏重画一个月；today 落在这个月里时那一格反色 */
void ui_cal_show(const mc_calview_t *v, mc_date_t today);

/* 闹钟：大号 HH:MM、重复方式、开关；选中的字段反色。每次整块重画，只在按键后调用 */
#define UI_ALARM_WORKDAYS 0x3Eu /* 周一到周五：mc_alarm 的星期位图，bit0 是周日 */
typedef enum { UI_ALARM_HOUR, UI_ALARM_MINUTE, UI_ALARM_MODE, UI_ALARM_ENABLE, UI_ALARM_FIELDS } ui_alarm_field_t;
void ui_alarm_init(void);
void ui_alarm_show(const mc_alarm_t *al, ui_alarm_field_t field, bool ringing);
const char *ui_alarm_mode_name(uint8_t weekdays);

/* 番茄钟与多路计时：数字在跳，主循环每 100 ms 调一次 show，内部只重画变了的字符 */
void ui_pomo_init(void);
void ui_pomo_show(const mc_pomodoro_t *p, uint32_t now_ms);
void ui_timers_init(void);
void ui_timers_show(const mc_timer_bank_t *bank, uint8_t selected, uint32_t now_ms); /* selected：0–3 秒表，4–7 倒计时 */

#endif
