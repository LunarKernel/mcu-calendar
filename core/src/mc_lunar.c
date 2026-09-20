#include "mc/mc_lunar.h"

#include <stddef.h>

#define LUNAR_FIRST_YEAR 1900
#define LUNAR_LAST_YEAR 2100

/* 每年一个常量：
 *   bit 16      闰月大小（1=30 天，0=29 天），仅在有闰月时有意义
 *   bit 15..4   正月…腊月大小（bit15=正月；1=30 天，0=29 天）
 *   bit 3..0    闰哪个月，0=无闰月
 * 基准：农历 1900 年正月初一 = 公历 1900-01-31。 */
static const uint32_t k_lunar_info[LUNAR_LAST_YEAR - LUNAR_FIRST_YEAR + 1] = {
    0x04bd8, 0x04ae0, 0x0a570, 0x054d5, 0x0d260, 0x0d950, 0x16554, 0x056a0, 0x09ad0, 0x055d2, /* 1900 */
    0x04ae0, 0x0a5b6, 0x0a4d0, 0x0d250, 0x1d255, 0x0b540, 0x0d6a0, 0x0ada2, 0x095b0, 0x14977, /* 1910 */
    0x04970, 0x0a4b0, 0x0b4b5, 0x06a50, 0x06d40, 0x1ab54, 0x02b60, 0x09570, 0x052f2, 0x04970, /* 1920 */
    0x06566, 0x0d4a0, 0x0ea50, 0x16a95, 0x05ad0, 0x02b60, 0x186e3, 0x092e0, 0x1c8d7, 0x0c950, /* 1930 */
    0x0d4a0, 0x1d8a6, 0x0b550, 0x056a0, 0x1a5b4, 0x025d0, 0x092d0, 0x0d2b2, 0x0a950, 0x0b557, /* 1940 */
    0x06ca0, 0x0b550, 0x15355, 0x04da0, 0x0a5b0, 0x14573, 0x052b0, 0x0a9a8, 0x0e950, 0x06aa0, /* 1950 */
    0x0aea6, 0x0ab50, 0x04b60, 0x0aae4, 0x0a570, 0x05260, 0x0f263, 0x0d950, 0x05b57, 0x056a0, /* 1960 */
    0x096d0, 0x04dd5, 0x04ad0, 0x0a4d0, 0x0d4d4, 0x0d250, 0x0d558, 0x0b540, 0x0b6a0, 0x195a6, /* 1970 */
    0x095b0, 0x049b0, 0x0a974, 0x0a4b0, 0x0b27a, 0x06a50, 0x06d40, 0x0af46, 0x0ab60, 0x09570, /* 1980 */
    0x04af5, 0x04970, 0x064b0, 0x074a3, 0x0ea50, 0x06b58, 0x05ac0, 0x0ab60, 0x096d5, 0x092e0, /* 1990 */
    0x0c960, 0x0d954, 0x0d4a0, 0x0da50, 0x07552, 0x056a0, 0x0abb7, 0x025d0, 0x092d0, 0x0cab5, /* 2000 */
    0x0a950, 0x0b4a0, 0x0baa4, 0x0ad50, 0x055d9, 0x04ba0, 0x0a5b0, 0x15176, 0x052b0, 0x0a930, /* 2010 */
    0x07954, 0x06aa0, 0x0ad50, 0x05b52, 0x04b60, 0x0a6e6, 0x0a4e0, 0x0d260, 0x0ea65, 0x0d530, /* 2020 */
    0x05aa0, 0x076a3, 0x096d0, 0x04afb, 0x04ad0, 0x0a4d0, 0x1d0b6, 0x0d250, 0x0d520, 0x0dd45, /* 2030 */
    0x0b5a0, 0x056d0, 0x055b2, 0x049b0, 0x0a577, 0x0a4b0, 0x0aa50, 0x1b255, 0x06d20, 0x0ada0, /* 2040 */
    0x14b63, 0x09370, 0x049f8, 0x04970, 0x064b0, 0x168a6, 0x0ea50, 0x06b20, 0x1a6c4, 0x0aae0, /* 2050 */
    0x092e0, 0x0d2e3, 0x0c960, 0x0d557, 0x0d4a0, 0x0da50, 0x05d55, 0x056a0, 0x0a6d0, 0x055d4, /* 2060 */
    0x052d0, 0x0a9b8, 0x0a950, 0x0b4a0, 0x0b6a6, 0x0ad50, 0x055a0, 0x0aba4, 0x0a5b0, 0x052b0, /* 2070 */
    0x0b273, 0x06930, 0x07337, 0x06aa0, 0x0ad50, 0x14b55, 0x04b60, 0x0a570, 0x054e4, 0x0d160, /* 2080 */
    0x0e968, 0x0d520, 0x0daa0, 0x16aa6, 0x056d0, 0x04ae0, 0x0a9d4, 0x0a2d0, 0x0d150, 0x0f252, /* 2090 */
    0x0d520,                                                                                  /* 2100 */
};

static uint8_t leap_month_of(uint32_t info)
{
    return (uint8_t)(info & 0xFu);
}

static uint8_t leap_days_of(uint32_t info)
{
    if (leap_month_of(info) == 0) {
        return 0;
    }
    return (info & 0x10000u) ? 30 : 29;
}

static uint8_t month_days_of(uint32_t info, uint8_t month)
{
    return (info & (0x10000u >> month)) ? 30 : 29;
}

static uint16_t year_days_of(uint32_t info)
{
    uint16_t sum = 0;
    for (uint8_t m = 1; m <= 12; ++m) {
        sum = (uint16_t)(sum + month_days_of(info, m));
    }
    return (uint16_t)(sum + leap_days_of(info));
}

uint8_t mc_lunar_leap_month(int32_t lunar_year)
{
    if (lunar_year < LUNAR_FIRST_YEAR || lunar_year > LUNAR_LAST_YEAR) {
        return 0;
    }
    return leap_month_of(k_lunar_info[lunar_year - LUNAR_FIRST_YEAR]);
}

bool mc_lunar_from_solar(mc_date_t solar, mc_lunar_t *out)
{
    if (!mc_date_valid(solar)) {
        return false;
    }
    int32_t offset = mc_days_from_civil(solar.year, solar.month, solar.day) -
                     mc_days_from_civil(1900, 1, 31);

    if (offset < 0) {
        /* 1900-01-01…01-30 落在农历己亥年（1899）腊月，表里没有这一年；
         * 该月初一恰为 1900-01-01、月大 30 天，故日序即公历日。 */
        if (solar.year != 1900) {
            return false;
        }
        out->year = 1899;
        out->month = 12;
        out->day = solar.day;
        out->is_leap = false;
        return true;
    }

    int32_t year = LUNAR_FIRST_YEAR;
    for (; year <= LUNAR_LAST_YEAR; ++year) {
        int32_t yd = year_days_of(k_lunar_info[year - LUNAR_FIRST_YEAR]);
        if (offset < yd) {
            break;
        }
        offset -= yd;
    }
    if (year > LUNAR_LAST_YEAR) {
        return false;
    }

    uint32_t info = k_lunar_info[year - LUNAR_FIRST_YEAR];
    uint8_t leap = leap_month_of(info);
    for (uint8_t m = 1; m <= 12; ++m) {
        int32_t md = month_days_of(info, m);
        if (offset < md) {
            out->year = (int16_t)year;
            out->month = m;
            out->day = (uint8_t)(offset + 1);
            out->is_leap = false;
            return true;
        }
        offset -= md;
        if (m == leap) {
            int32_t ld = leap_days_of(info);
            if (offset < ld) {
                out->year = (int16_t)year;
                out->month = m;
                out->day = (uint8_t)(offset + 1);
                out->is_leap = true;
                return true;
            }
            offset -= ld;
        }
    }
    return false; /* 不可达：offset < year_days 已保证落在某月内 */
}

static uint8_t mod_floor(int32_t a, int32_t n)
{
    int32_t r = a % n;
    return (uint8_t)(r < 0 ? r + n : r);
}

uint8_t mc_lunar_stem(int32_t lunar_year)
{
    return mod_floor(lunar_year - 4, 10); /* 公元 4 年为甲子 */
}

uint8_t mc_lunar_branch(int32_t lunar_year)
{
    return mod_floor(lunar_year - 4, 12);
}

const char *mc_lunar_stem_name(uint8_t stem)
{
    static const char *const k[10] = {"甲", "乙", "丙", "丁", "戊", "己", "庚", "辛", "壬", "癸"};
    return stem < 10 ? k[stem] : "";
}

const char *mc_lunar_branch_name(uint8_t branch)
{
    static const char *const k[12] = {"子", "丑", "寅", "卯", "辰", "巳",
                                      "午", "未", "申", "酉", "戌", "亥"};
    return branch < 12 ? k[branch] : "";
}

const char *mc_lunar_zodiac_name(uint8_t branch)
{
    static const char *const k[12] = {"鼠", "牛", "虎", "兔", "龙", "蛇",
                                      "马", "羊", "猴", "鸡", "狗", "猪"};
    return branch < 12 ? k[branch] : "";
}

const char *mc_lunar_month_name(uint8_t month)
{
    static const char *const k[12] = {"正月", "二月", "三月", "四月", "五月", "六月",
                                      "七月", "八月", "九月", "十月", "冬月", "腊月"};
    return (month >= 1 && month <= 12) ? k[month - 1] : "";
}

const char *mc_lunar_day_name(uint8_t day)
{
    static const char *const k[30] = {
        "初一", "初二", "初三", "初四", "初五", "初六", "初七", "初八", "初九", "初十",
        "十一", "十二", "十三", "十四", "十五", "十六", "十七", "十八", "十九", "二十",
        "廿一", "廿二", "廿三", "廿四", "廿五", "廿六", "廿七", "廿八", "廿九", "三十"};
    return (day >= 1 && day <= 30) ? k[day - 1] : "";
}
