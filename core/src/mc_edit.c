#include "mc/mc_edit.h"

/* v 在 [lo, hi] 内加 delta 后回绕；delta 可以远大于区间长度 */
static int32_t wrap(int32_t v, int32_t delta, int32_t lo, int32_t hi)
{
    const int32_t span = hi - lo + 1;
    int32_t off = (v - lo + delta) % span;
    if (off < 0) {
        off += span;
    }
    return lo + off;
}

static void clamp_day(mc_datetime_t *dt)
{
    const uint8_t dim = mc_days_in_month(dt->date.year, dt->date.month);
    if (dt->date.day > dim) {
        dt->date.day = dim;
    }
}

void mc_edit_begin(mc_edit_t *e, const mc_datetime_t *now)
{
    static const mc_datetime_t k_fallback = {{MC_EDIT_MIN_YEAR, 1, 1}, 0, 0, 0};
    const bool usable =
        mc_datetime_valid(now) && now->date.year >= MC_EDIT_MIN_YEAR && now->date.year <= MC_EDIT_MAX_YEAR;
    e->dt = usable ? *now : k_fallback;
    e->field = MC_EDIT_YEAR;
}

void mc_edit_next_field(mc_edit_t *e)
{
    e->field = (mc_edit_field_t)(((int)e->field + 1) % (int)MC_EDIT_FIELDS);
}

void mc_edit_step(mc_edit_t *e, int32_t delta)
{
    mc_datetime_t *dt = &e->dt;
    switch (e->field) {
    case MC_EDIT_YEAR:
        dt->date.year = (int16_t)wrap(dt->date.year, delta, MC_EDIT_MIN_YEAR, MC_EDIT_MAX_YEAR);
        clamp_day(dt);
        break;
    case MC_EDIT_MONTH:
        dt->date.month = (uint8_t)wrap(dt->date.month, delta, 1, 12);
        clamp_day(dt);
        break;
    case MC_EDIT_DAY:
        dt->date.day = (uint8_t)wrap(dt->date.day, delta, 1, mc_days_in_month(dt->date.year, dt->date.month));
        break;
    case MC_EDIT_HOUR:
        dt->hour = (uint8_t)wrap(dt->hour, delta, 0, 23);
        break;
    case MC_EDIT_MINUTE:
        dt->minute = (uint8_t)wrap(dt->minute, delta, 0, 59);
        break;
    case MC_EDIT_SECOND:
        dt->second = (uint8_t)wrap(dt->second, delta, 0, 59);
        break;
    default:
        break;
    }
}
