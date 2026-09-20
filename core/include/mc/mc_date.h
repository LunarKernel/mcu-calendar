/* 公历日期、星期、32 位 Unix 时间戳。纯算法，不依赖任何 HAL。
 * 对应选题表：基本功能 4（时间戳）、扩展功能 2（万年历）的日期底座。 */
#ifndef MC_DATE_H
#define MC_DATE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int16_t year;
    uint8_t month; /* 1..12 */
    uint8_t day;   /* 1..31 */
} mc_date_t;

typedef struct {
    mc_date_t date;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
} mc_datetime_t;

/* 默认时区：北京时间，单位秒 */
#define MC_TZ_BEIJING_S (8 * 3600)

bool mc_is_leap_year(int32_t year);
uint8_t mc_days_in_month(int32_t year, uint8_t month); /* month 非法返回 0 */
bool mc_date_valid(mc_date_t d);
bool mc_datetime_valid(const mc_datetime_t *dt);

/* 1970-01-01 起的天数（可为负）。格里历外推，任意年份有效。 */
int32_t mc_days_from_civil(int32_t year, uint8_t month, uint8_t day);
mc_date_t mc_civil_from_days(int32_t days);

/* 0=周日 … 6=周六 */
uint8_t mc_weekday(mc_date_t d);

/* 本地时间 <-> 32 位无符号时间戳（参考点 1970-01-01 00:00:00 UTC）。
 * tz_offset_s 为本地相对 UTC 的偏移，北京时间传 MC_TZ_BEIJING_S。
 * 超出 [0, 2^32-1]（即 UTC 1970-01-01 至 2106-02-07 06:28:15）返回 false。 */
bool mc_unix_from_local(const mc_datetime_t *local, int32_t tz_offset_s, uint32_t *out_ts);
void mc_local_from_unix(uint32_t ts, int32_t tz_offset_s, mc_datetime_t *out_local);

#ifdef __cplusplus
}
#endif

#endif
