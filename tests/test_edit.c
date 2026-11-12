#include "check.h"
#include "mc/mc_edit.h"

static mc_edit_t begin(int16_t y, uint8_t mo, uint8_t d, uint8_t h, uint8_t mi, uint8_t s)
{
    const mc_datetime_t now = {{y, mo, d}, h, mi, s};
    mc_edit_t e;
    mc_edit_begin(&e, &now);
    return e;
}

static void select_field(mc_edit_t *e, mc_edit_field_t f)
{
    while (e->field != f) {
        mc_edit_next_field(e);
    }
}

static void test_begin(void)
{
    mc_edit_t e = begin(2026, 11, 12, 16, 30, 45);
    CHECK_EQ(e.field, MC_EDIT_YEAR);
    CHECK_EQ(e.dt.date.year, 2026);
    CHECK_EQ(e.dt.second, 45);

    /* 非法日期、越界年份都落到起点 */
    e = begin(2026, 2, 30, 0, 0, 0);
    CHECK_EQ(e.dt.date.year, MC_EDIT_MIN_YEAR);
    CHECK_EQ(e.dt.date.month, 1);
    CHECK_EQ(e.dt.date.day, 1);
    e = begin(1999, 12, 31, 23, 59, 59);
    CHECK_EQ(e.dt.date.year, MC_EDIT_MIN_YEAR);
    CHECK_EQ(e.dt.hour, 0);
    e = begin(2100, 1, 1, 0, 0, 0);
    CHECK_EQ(e.dt.date.year, MC_EDIT_MIN_YEAR);
}

static void test_field_cycle(void)
{
    mc_edit_t e = begin(2026, 11, 12, 0, 0, 0);
    const mc_edit_field_t order[] = {MC_EDIT_MONTH, MC_EDIT_DAY,    MC_EDIT_HOUR,
                                     MC_EDIT_MINUTE, MC_EDIT_SECOND, MC_EDIT_YEAR};
    for (unsigned i = 0; i < sizeof order / sizeof order[0]; ++i) {
        mc_edit_next_field(&e);
        CHECK_EQ(e.field, order[i]);
    }
}

static void test_wrap_each_field(void)
{
    mc_edit_t e = begin(2099, 12, 31, 23, 59, 59);
    mc_edit_step(&e, 1); /* 年 2099 → 2000 */
    CHECK_EQ(e.dt.date.year, 2000);
    mc_edit_step(&e, -1);
    CHECK_EQ(e.dt.date.year, 2099);

    select_field(&e, MC_EDIT_MONTH);
    mc_edit_step(&e, 1); /* 12 → 1，不进位到年 */
    CHECK_EQ(e.dt.date.month, 1);
    CHECK_EQ(e.dt.date.year, 2099);
    mc_edit_step(&e, -1);
    CHECK_EQ(e.dt.date.month, 12);

    select_field(&e, MC_EDIT_DAY);
    mc_edit_step(&e, 1); /* 12 月 31 → 1 */
    CHECK_EQ(e.dt.date.day, 1);
    CHECK_EQ(e.dt.date.month, 12);
    mc_edit_step(&e, -1);
    CHECK_EQ(e.dt.date.day, 31);

    select_field(&e, MC_EDIT_HOUR);
    mc_edit_step(&e, 1);
    CHECK_EQ(e.dt.hour, 0);
    CHECK_EQ(e.dt.date.day, 31); /* 不进位到日 */
    mc_edit_step(&e, -1);
    CHECK_EQ(e.dt.hour, 23);

    select_field(&e, MC_EDIT_MINUTE);
    mc_edit_step(&e, 1);
    CHECK_EQ(e.dt.minute, 0);
    CHECK_EQ(e.dt.hour, 23);

    select_field(&e, MC_EDIT_SECOND);
    mc_edit_step(&e, 1);
    CHECK_EQ(e.dt.second, 0);
    mc_edit_step(&e, -61); /* 步长超过一圈 */
    CHECK_EQ(e.dt.second, 59);
    mc_edit_step(&e, 125);
    CHECK_EQ(e.dt.second, 4);
}

static void test_day_clamp(void)
{
    /* 1 月 31 日拨到 2 月：平年 28，闰年 29 */
    mc_edit_t e = begin(2026, 1, 31, 0, 0, 0);
    select_field(&e, MC_EDIT_MONTH);
    mc_edit_step(&e, 1);
    CHECK_EQ(e.dt.date.month, 2);
    CHECK_EQ(e.dt.date.day, 28);

    e = begin(2024, 1, 31, 0, 0, 0);
    select_field(&e, MC_EDIT_MONTH);
    mc_edit_step(&e, 1);
    CHECK_EQ(e.dt.date.day, 29);

    /* 闰日拨年份：2024-02-29 → 2025-02-28 */
    select_field(&e, MC_EDIT_YEAR);
    mc_edit_step(&e, 1);
    CHECK_EQ(e.dt.date.year, 2025);
    CHECK_EQ(e.dt.date.day, 28);

    /* 2 月里拨日：平年 28 → 1，闰年 29 → 1 */
    e = begin(2026, 2, 28, 0, 0, 0);
    select_field(&e, MC_EDIT_DAY);
    mc_edit_step(&e, 1);
    CHECK_EQ(e.dt.date.day, 1);
    mc_edit_step(&e, -1);
    CHECK_EQ(e.dt.date.day, 28);
    e = begin(2000, 2, 28, 0, 0, 0); /* 2000 是闰年（能被 400 整除） */
    select_field(&e, MC_EDIT_DAY);
    mc_edit_step(&e, 1);
    CHECK_EQ(e.dt.date.day, 29);

    /* 31 日拨到小月 */
    e = begin(2026, 3, 31, 0, 0, 0);
    select_field(&e, MC_EDIT_MONTH);
    mc_edit_step(&e, 1);
    CHECK_EQ(e.dt.date.month, 4);
    CHECK_EQ(e.dt.date.day, 30);
}

static void test_always_valid(void)
{
    /* 每个字段来回乱拨，任何时刻都必须是合法日期时间 */
    mc_edit_t e = begin(2024, 2, 29, 23, 59, 59);
    static const int32_t k_steps[] = {1, -1, 7, -13, 100, -100, 31, 365};
    for (unsigned round = 0; round < 200; ++round) {
        mc_edit_step(&e, k_steps[round % (sizeof k_steps / sizeof k_steps[0])]);
        CHECK(mc_datetime_valid(&e.dt));
        CHECK(e.dt.date.year >= MC_EDIT_MIN_YEAR && e.dt.date.year <= MC_EDIT_MAX_YEAR);
        if (round % 3u == 0u) {
            mc_edit_next_field(&e);
        }
    }
}

int main(void)
{
    test_begin();
    test_field_cycle();
    test_wrap_each_field();
    test_day_clamp();
    test_always_valid();
    CHECK_REPORT();
}
