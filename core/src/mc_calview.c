#include "mc/mc_calview.h"

#define MIN_INDEX (MC_CALVIEW_MIN_YEAR * 12)
#define MAX_INDEX (MC_CALVIEW_MAX_YEAR * 12 + 11)

static int32_t to_index(const mc_calview_t *v)
{
    return (int32_t)v->year * 12 + (v->month - 1);
}

static bool set_index(mc_calview_t *v, int32_t index)
{
    if (index < MIN_INDEX) {
        index = MIN_INDEX;
    }
    if (index > MAX_INDEX) {
        index = MAX_INDEX;
    }
    bool moved = index != to_index(v);
    v->year = (int16_t)(index / 12);
    v->month = (uint8_t)(index % 12 + 1);
    return moved;
}

void mc_calview_init(mc_calview_t *v, int32_t year, uint8_t month)
{
    if (month < 1) {
        month = 1;
    }
    if (month > 12) {
        month = 12;
    }
    if (year < MC_CALVIEW_MIN_YEAR) {
        year = MC_CALVIEW_MIN_YEAR;
        month = 1;
    }
    if (year > MC_CALVIEW_MAX_YEAR) {
        year = MC_CALVIEW_MAX_YEAR;
        month = 12;
    }
    v->year = (int16_t)year;
    v->month = month;
}

bool mc_calview_step_month(mc_calview_t *v, int32_t delta)
{
    return set_index(v, to_index(v) + delta);
}

bool mc_calview_step_year(mc_calview_t *v, int32_t delta)
{
    return set_index(v, to_index(v) + delta * 12);
}

mc_month_info_t mc_calview_month_info(const mc_calview_t *v)
{
    mc_date_t first = {v->year, v->month, 1};
    mc_month_info_t info;
    info.first_weekday = mc_weekday(first);
    info.days = mc_days_in_month(v->year, v->month);
    return info;
}

void mc_calview_fill_grid(const mc_calview_t *v, uint8_t grid[42])
{
    mc_month_info_t info = mc_calview_month_info(v);
    for (uint8_t i = 0; i < 42; ++i) {
        grid[i] = 0;
    }
    for (uint8_t d = 1; d <= info.days; ++d) {
        grid[info.first_weekday + d - 1] = d;
    }
}
