/* 三个界面（横屏 400×240）：时钟主界面、时间设置、万年历月视图。
 * 时钟界面只重画变了的部分：每秒通常只动秒位与时间戳一行。 */
#ifndef UI_H
#define UI_H

#include <stdbool.h>
#include <stdint.h>

#include "mc/mc_calview.h"
#include "mc/mc_date.h"
#include "mc/mc_edit.h"

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

#endif
