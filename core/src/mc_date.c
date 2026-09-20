#include "mc/mc_date.h"

bool mc_is_leap_year(int32_t year)
{
    return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
}

uint8_t mc_days_in_month(int32_t year, uint8_t month)
{
    static const uint8_t k_days[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month < 1 || month > 12) {
        return 0;
    }
    if (month == 2 && mc_is_leap_year(year)) {
        return 29;
    }
    return k_days[month - 1];
}

bool mc_date_valid(mc_date_t d)
{
    uint8_t dim = mc_days_in_month(d.year, d.month);
    return dim != 0 && d.day >= 1 && d.day <= dim;
}

bool mc_datetime_valid(const mc_datetime_t *dt)
{
    return mc_date_valid(dt->date) && dt->hour < 24 && dt->minute < 60 && dt->second < 60;
}

/* days-from-civil（Howard Hinnant）：把年份起点挪到 3 月 1 日，闰日落在年末，
 * 于是「年内第几天」对月份是线性的 (153*mp+2)/5，不需要查月份表。 */
int32_t mc_days_from_civil(int32_t year, uint8_t month, uint8_t day)
{
    int32_t y = year - (month <= 2 ? 1 : 0);
    int32_t era = (y >= 0 ? y : y - 399) / 400;
    uint32_t yoe = (uint32_t)(y - era * 400);                                   /* [0, 399] */
    uint32_t mp = (uint32_t)(month > 2 ? month - 3 : month + 9);                /* 3 月=0 */
    uint32_t doy = (153u * mp + 2u) / 5u + (uint32_t)day - 1u;                  /* [0, 365] */
    uint32_t doe = yoe * 365u + yoe / 4u - yoe / 100u + doy;                    /* [0, 146096] */
    return era * 146097 + (int32_t)doe - 719468;
}

mc_date_t mc_civil_from_days(int32_t days)
{
    int32_t z = days + 719468;
    int32_t era = (z >= 0 ? z : z - 146096) / 146097;
    uint32_t doe = (uint32_t)(z - era * 146097);                                /* [0, 146096] */
    uint32_t yoe = (doe - doe / 1460u + doe / 36524u - doe / 146096u) / 365u;   /* [0, 399] */
    int32_t y = (int32_t)yoe + era * 400;
    uint32_t doy = doe - (365u * yoe + yoe / 4u - yoe / 100u);                  /* [0, 365] */
    uint32_t mp = (5u * doy + 2u) / 153u;                                       /* [0, 11] */
    uint32_t d = doy - (153u * mp + 2u) / 5u + 1u;
    uint32_t m = mp < 10u ? mp + 3u : mp - 9u;
    mc_date_t out;
    out.year = (int16_t)(y + (m <= 2u ? 1 : 0));
    out.month = (uint8_t)m;
    out.day = (uint8_t)d;
    return out;
}

uint8_t mc_weekday(mc_date_t d)
{
    /* 1970-01-01 是周四 */
    int32_t z = mc_days_from_civil(d.year, d.month, d.day);
    return (uint8_t)(z >= -4 ? (z + 4) % 7 : (z + 5) % 7 + 6);
}

bool mc_unix_from_local(const mc_datetime_t *local, int32_t tz_offset_s, uint32_t *out_ts)
{
    if (!mc_datetime_valid(local)) {
        return false;
    }
    int64_t days = mc_days_from_civil(local->date.year, local->date.month, local->date.day);
    int64_t secs = days * 86400 + (int64_t)local->hour * 3600 + (int64_t)local->minute * 60 +
                   (int64_t)local->second - (int64_t)tz_offset_s;
    if (secs < 0 || secs > (int64_t)UINT32_MAX) {
        return false;
    }
    *out_ts = (uint32_t)secs;
    return true;
}

void mc_local_from_unix(uint32_t ts, int32_t tz_offset_s, mc_datetime_t *out_local)
{
    int64_t secs = (int64_t)ts + (int64_t)tz_offset_s;
    int64_t days = secs / 86400;
    int64_t rem = secs % 86400;
    if (rem < 0) {
        rem += 86400;
        days -= 1;
    }
    out_local->date = mc_civil_from_days((int32_t)days);
    out_local->hour = (uint8_t)(rem / 3600);
    out_local->minute = (uint8_t)(rem % 3600 / 60);
    out_local->second = (uint8_t)(rem % 60);
}
