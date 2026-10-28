/* 定时闹钟。对应选题表基本功能 3 的「定时闹钟」。
 *
 * 按 RTC 的墙钟时刻触发，不用毫秒时基：闹钟要跟着「几点几分」走，
 * 校时、断电走时都由 RTC 承担，这里只拿当前本地时间做比较。
 * 判定是「当前时分等于设定时分，星期在掩码内，且当天还没响过」，
 * 所以主循环被慢操作拖住几秒不会漏响，同一分钟内反复轮询也只响一次。
 * 限制：主循环若被拖住整整一分钟，该次闹钟会漏掉；时间往回拨到闹钟之前，当天不再重响。
 * 本模块不可重入，只在主循环里调用。 */
#ifndef MC_ALARM_H
#define MC_ALARM_H

#include <stdbool.h>
#include <stdint.h>

#include "mc/mc_date.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MC_ALARM_ONCE 0x00u     /* 单次：响过一次后自动关闭 */
#define MC_ALARM_EVERYDAY 0x7Fu /* bit n = 星期 n，0=周日 … 6=周六 */

typedef struct {
    uint8_t hour;
    uint8_t minute;
    uint8_t weekdays; /* MC_ALARM_ONCE 或星期位图 */
    bool enabled;
    bool fired_once;       /* fired_day 是否有效 */
    int32_t fired_day;     /* 上次响的那天，mc_days_from_civil 口径 */
} mc_alarm_t;

void mc_alarm_init(mc_alarm_t *al); /* 关闭 */
/* 时分或星期位图非法返回 false，闹钟保持原状；成功则开启并清除「已响」记录 */
bool mc_alarm_set(mc_alarm_t *al, uint8_t hour, uint8_t minute, uint8_t weekdays);
void mc_alarm_disable(mc_alarm_t *al);
/* now 为当前本地时间；本次调用恰好该响返回 true（同一天只返回一次）。now 非法返回 false */
bool mc_alarm_poll(mc_alarm_t *al, const mc_datetime_t *now);

#ifdef __cplusplus
}
#endif

#endif
