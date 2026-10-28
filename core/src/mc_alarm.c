#include "mc/mc_alarm.h"

void mc_alarm_init(mc_alarm_t *al)
{
    al->hour = 0;
    al->minute = 0;
    al->weekdays = MC_ALARM_ONCE;
    al->enabled = false;
    al->fired_once = false;
    al->fired_day = 0;
}

bool mc_alarm_set(mc_alarm_t *al, uint8_t hour, uint8_t minute, uint8_t weekdays)
{
    if (hour > 23 || minute > 59 || (weekdays & ~MC_ALARM_EVERYDAY) != 0) {
        return false;
    }
    al->hour = hour;
    al->minute = minute;
    al->weekdays = weekdays;
    al->enabled = true;
    al->fired_once = false;
    return true;
}

void mc_alarm_disable(mc_alarm_t *al)
{
    al->enabled = false;
}

bool mc_alarm_poll(mc_alarm_t *al, const mc_datetime_t *now)
{
    if (!al->enabled || !mc_datetime_valid(now)) {
        return false;
    }
    if (now->hour != al->hour || now->minute != al->minute) {
        return false;
    }
    if (al->weekdays != MC_ALARM_ONCE && (al->weekdays & (1u << mc_weekday(now->date))) == 0) {
        return false;
    }
    int32_t today = mc_days_from_civil(now->date.year, now->date.month, now->date.day);
    if (al->fired_once && al->fired_day == today) {
        return false;
    }
    al->fired_once = true;
    al->fired_day = today;
    if (al->weekdays == MC_ALARM_ONCE) {
        al->enabled = false;
    }
    return true;
}
