/* 时钟主界面（横屏 400×240）。只重画变了的部分：每秒通常只动秒位与时间戳一行。 */
#ifndef UI_H
#define UI_H

#include <stdbool.h>
#include <stdint.h>

#include "mc/mc_date.h"

typedef struct {
    mc_datetime_t local; /* 北京时间 */
    uint32_t unix_s;
    bool rtc_ok;
    bool time_set; /* false：RTC 还停在默认起点，没人校过 */
    uint8_t sync_state; /* app_sync_state_t 的值 */
} ui_clock_t;

void ui_clock_init(void); /* 清屏并画固定部分；之后第一次 ui_clock_show 会全量画 */
void ui_clock_show(const ui_clock_t *st);

#endif
