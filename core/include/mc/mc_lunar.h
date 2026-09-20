/* 公历 -> 农历。查表法，覆盖公历 1900-01-01 至 2100-12-31。
 * 对应选题表：扩展功能 1（农历计算显示）。 */
#ifndef MC_LUNAR_H
#define MC_LUNAR_H

#include "mc/mc_date.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int16_t year;  /* 农历年，以正月初一所在公历年记 */
    uint8_t month; /* 1..12 */
    uint8_t day;   /* 1..30 */
    bool is_leap;  /* 是否闰月 */
} mc_lunar_t;

/* 超出覆盖范围或日期非法返回 false */
bool mc_lunar_from_solar(mc_date_t solar, mc_lunar_t *out);

/* 某农历年的闰月月份，无闰月返回 0；年份超表返回 0 */
uint8_t mc_lunar_leap_month(int32_t lunar_year);

/* 干支与生肖下标：天干 0=甲，地支 0=子，生肖 0=鼠（与地支同下标） */
uint8_t mc_lunar_stem(int32_t lunar_year);
uint8_t mc_lunar_branch(int32_t lunar_year);

/* UTF-8 名称表。ST7920 一类自带 GB2312 字库的屏需要在显示层转码，这里不管。 */
const char *mc_lunar_stem_name(uint8_t stem);     /* 甲乙丙丁… */
const char *mc_lunar_branch_name(uint8_t branch); /* 子丑寅卯… */
const char *mc_lunar_zodiac_name(uint8_t branch); /* 鼠牛虎兔… */
const char *mc_lunar_month_name(uint8_t month);   /* 正月…腊月，不含「闰」字 */
const char *mc_lunar_day_name(uint8_t day);       /* 初一…三十 */

#ifdef __cplusplus
}
#endif

#endif
