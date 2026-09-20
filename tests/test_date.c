#include "check.h"
#include "mc/mc_calview.h"
#include "mc/mc_date.h"

static mc_datetime_t make_dt(int y, int mo, int d, int h, int mi, int s)
{
    mc_datetime_t dt = {{(int16_t)y, (uint8_t)mo, (uint8_t)d}, (uint8_t)h, (uint8_t)mi, (uint8_t)s};
    return dt;
}

static void test_days(void)
{
    /* 参考值由 Python datetime 算出 */
    CHECK_EQ(mc_days_from_civil(1970, 1, 1), 0);
    CHECK_EQ(mc_days_from_civil(2000, 3, 1), 11017);
    CHECK_EQ(mc_days_from_civil(1900, 1, 31), -25537);
    CHECK_EQ(mc_days_from_civil(2100, 12, 31), 47846);

    /* 全范围往返：逐日递增、日期连续、星期连续 */
    int32_t start = mc_days_from_civil(1900, 1, 1);
    int32_t end = mc_days_from_civil(2100, 12, 31);
    mc_date_t prev = mc_civil_from_days(start);
    uint8_t prev_wd = mc_weekday(prev);
    CHECK_EQ(prev_wd, 1); /* 1900-01-01 周一 */
    int bad = 0;
    for (int32_t z = start + 1; z <= end; ++z) {
        mc_date_t d = mc_civil_from_days(z);
        if (mc_days_from_civil(d.year, d.month, d.day) != z || !mc_date_valid(d)) {
            bad++;
        }
        uint8_t wd = mc_weekday(d);
        if (wd != (prev_wd + 1) % 7) {
            bad++;
        }
        bool next_day = d.year == prev.year && d.month == prev.month && d.day == prev.day + 1;
        bool next_month = d.day == 1 && prev.day == mc_days_in_month(prev.year, prev.month);
        if (!next_day && !next_month) {
            bad++;
        }
        prev = d;
        prev_wd = wd;
    }
    CHECK_EQ(bad, 0);
}

static void test_weekday_and_leap(void)
{
    mc_date_t a = {2000, 2, 29}, b = {1900, 3, 1}, c = {2026, 9, 20}, d = {2099, 12, 31};
    CHECK_EQ(mc_weekday(a), 2);
    CHECK_EQ(mc_weekday(b), 4);
    CHECK_EQ(mc_weekday(c), 0);
    CHECK_EQ(mc_weekday(d), 4);

    CHECK(mc_is_leap_year(2000));
    CHECK(!mc_is_leap_year(1900));
    CHECK(!mc_is_leap_year(2100));
    CHECK(mc_is_leap_year(2024));
    CHECK_EQ(mc_days_in_month(1900, 2), 28);
    CHECK_EQ(mc_days_in_month(2000, 2), 29);
    CHECK_EQ(mc_days_in_month(2026, 13), 0);

    mc_date_t bad1 = {1900, 2, 29}, bad2 = {2026, 4, 31}, bad3 = {2026, 0, 1};
    CHECK(!mc_date_valid(bad1));
    CHECK(!mc_date_valid(bad2));
    CHECK(!mc_date_valid(bad3));
}

static void test_unix(void)
{
    uint32_t ts = 0;
    mc_datetime_t dt;

    dt = make_dt(1970, 1, 1, 8, 0, 0);
    CHECK(mc_unix_from_local(&dt, MC_TZ_BEIJING_S, &ts));
    CHECK_EQ(ts, 0);

    dt = make_dt(2026, 9, 20, 0, 0, 0);
    CHECK(mc_unix_from_local(&dt, MC_TZ_BEIJING_S, &ts));
    CHECK_EQ(ts, 1789833600u);

    /* 越过有符号 32 位上限（2038 问题）：无符号时间戳应照常工作 */
    dt = make_dt(2038, 1, 19, 11, 14, 8);
    CHECK(mc_unix_from_local(&dt, MC_TZ_BEIJING_S, &ts));
    CHECK_EQ(ts, 2147483648u);

    dt = make_dt(2099, 12, 31, 23, 59, 59);
    CHECK(mc_unix_from_local(&dt, MC_TZ_BEIJING_S, &ts));
    CHECK_EQ(ts, 4102415999u);

    /* 北京时间 1970-01-01 07:59:59 早于参考点 */
    dt = make_dt(1970, 1, 1, 7, 59, 59);
    CHECK(!mc_unix_from_local(&dt, MC_TZ_BEIJING_S, &ts));

    /* 上限 UTC 2106-02-07 06:28:15 */
    dt = make_dt(2106, 2, 7, 6, 28, 15);
    CHECK(mc_unix_from_local(&dt, 0, &ts));
    CHECK_EQ(ts, 4294967295u);
    dt = make_dt(2106, 2, 7, 6, 28, 16);
    CHECK(!mc_unix_from_local(&dt, 0, &ts));

    dt = make_dt(2026, 2, 30, 0, 0, 0);
    CHECK(!mc_unix_from_local(&dt, MC_TZ_BEIJING_S, &ts));

    /* 往返：步长取与 86400 互质的数，扫到各种时分秒 */
    int bad = 0;
    for (uint64_t t = 0; t <= 4102415999u; t += 1000003u) {
        mc_datetime_t local;
        uint32_t back = 0;
        mc_local_from_unix((uint32_t)t, MC_TZ_BEIJING_S, &local);
        if (!mc_unix_from_local(&local, MC_TZ_BEIJING_S, &back) || back != (uint32_t)t) {
            bad++;
        }
    }
    CHECK_EQ(bad, 0);

    mc_local_from_unix(1789833600u, MC_TZ_BEIJING_S, &dt);
    CHECK_EQ(dt.date.year, 2026);
    CHECK_EQ(dt.date.month, 9);
    CHECK_EQ(dt.date.day, 20);
    CHECK_EQ(dt.hour, 0);
}

static void test_calview(void)
{
    mc_calview_t v;
    uint8_t grid[42];

    mc_calview_init(&v, 2026, 9);
    mc_month_info_t info = mc_calview_month_info(&v);
    CHECK_EQ(info.first_weekday, 2); /* 2026-09-01 周二 */
    CHECK_EQ(info.days, 30);
    mc_calview_fill_grid(&v, grid);
    CHECK_EQ(grid[0], 0);
    CHECK_EQ(grid[2], 1);
    CHECK_EQ(grid[21], 20); /* 9/20 周日，第 4 行行首 */
    CHECK_EQ(grid[31], 30);
    CHECK_EQ(grid[32], 0);

    CHECK(mc_calview_step_month(&v, 4));
    CHECK_EQ(v.year, 2027);
    CHECK_EQ(v.month, 1);
    CHECK(mc_calview_step_month(&v, -1));
    CHECK_EQ(v.year, 2026);
    CHECK_EQ(v.month, 12);

    /* 边界不回绕 */
    mc_calview_init(&v, 1900, 1);
    CHECK(!mc_calview_step_month(&v, -1));
    CHECK(!mc_calview_step_year(&v, -1));
    CHECK_EQ(v.year, 1900);
    CHECK_EQ(v.month, 1);
    mc_calview_init(&v, 2099, 6);
    CHECK(mc_calview_step_year(&v, 1)); /* 钳到 2099-12，游标动了 */
    CHECK_EQ(v.year, 2099);
    CHECK_EQ(v.month, 12);
    CHECK(!mc_calview_step_month(&v, 1));

    mc_calview_init(&v, 3000, 5);
    CHECK_EQ(v.year, 2099);
    CHECK_EQ(v.month, 12);

    /* 2000-02：闰二月 29 天，1 日周二 */
    mc_calview_init(&v, 2000, 2);
    info = mc_calview_month_info(&v);
    CHECK_EQ(info.days, 29);
    CHECK_EQ(info.first_weekday, 2);
}

int main(void)
{
    test_days();
    test_weekday_and_leap();
    test_unix();
    test_calview();
    CHECK_REPORT();
}
