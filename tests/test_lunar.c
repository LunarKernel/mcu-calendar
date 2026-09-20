#include "check.h"
#include "mc/mc_lunar.h"

#include <string.h>

typedef struct {
    int16_t sy;
    uint8_t sm, sd;
    int16_t ly;
    uint8_t lm, ld;
    bool leap;
} lunar_case_t;

/* 这里只放少量人工可核的锚点（春节、中秋、闰月首日、表的两端）。
 * 全范围 73 414 天的逐日比对由 tools/verify_lunar.py 对照独立实现完成。 */
static const lunar_case_t k_cases[] = {
    {1900, 1, 1, 1899, 12, 1, false},   {1900, 1, 30, 1899, 12, 30, false},
    {1900, 1, 31, 1900, 1, 1, false},   {1949, 10, 1, 1949, 8, 10, false},
    {2000, 2, 5, 2000, 1, 1, false},    {2008, 2, 7, 2008, 1, 1, false},
    {2019, 2, 5, 2019, 1, 1, false},    {2020, 1, 25, 2020, 1, 1, false},
    {2020, 5, 23, 2020, 4, 1, true},    {2023, 1, 22, 2023, 1, 1, false},
    {2023, 3, 22, 2023, 2, 1, true},    {2024, 2, 10, 2024, 1, 1, false},
    {2024, 9, 17, 2024, 8, 15, false},  {2025, 1, 29, 2025, 1, 1, false},
    {2025, 7, 25, 2025, 6, 1, true},    {2025, 10, 6, 2025, 8, 15, false},
    {2026, 2, 17, 2026, 1, 1, false},   {2026, 2, 16, 2025, 12, 29, false},
    {2026, 9, 25, 2026, 8, 15, false},  {2027, 2, 6, 2027, 1, 1, false},
    {2033, 1, 31, 2033, 1, 1, false},
};

static void test_cases(void)
{
    for (size_t i = 0; i < sizeof k_cases / sizeof k_cases[0]; ++i) {
        const lunar_case_t *c = &k_cases[i];
        mc_date_t solar = {c->sy, c->sm, c->sd};
        mc_lunar_t l;
        memset(&l, 0, sizeof l);
        bool ok = mc_lunar_from_solar(solar, &l);
        if (!ok || l.year != c->ly || l.month != c->lm || l.day != c->ld || l.is_leap != c->leap) {
            printf("case %d-%02d-%02d: got ok=%d %d-%d-%d leap=%d\n", c->sy, c->sm, c->sd, ok,
                   l.year, l.month, l.day, l.is_leap);
            g_check_failures++;
        }
    }
}

static void test_range_and_continuity(void)
{
    mc_lunar_t l;
    mc_date_t before = {1899, 12, 31}, after = {2101, 1, 1}, invalid = {2026, 2, 30};
    CHECK(!mc_lunar_from_solar(before, &l));
    CHECK(!mc_lunar_from_solar(invalid, &l));
    (void)after; /* 2101 年初仍在农历 2100 年内，表能覆盖，不作断言 */

    /* 逐日连续性：每过一天，要么日 +1，要么进入新的一月（日归 1）。
     * 查表实现若年总天数与月份位对不上，这里会断。 */
    int32_t start = mc_days_from_civil(1900, 1, 1);
    int32_t end = mc_days_from_civil(2100, 12, 31);
    mc_lunar_t prev;
    CHECK(mc_lunar_from_solar(mc_civil_from_days(start), &prev));
    int bad = 0;
    for (int32_t z = start + 1; z <= end; ++z) {
        if (!mc_lunar_from_solar(mc_civil_from_days(z), &l)) {
            bad++;
            continue;
        }
        bool same_month = l.year == prev.year && l.month == prev.month && l.is_leap == prev.is_leap;
        if (same_month) {
            if (l.day != prev.day + 1) {
                bad++;
            }
        } else if (l.day != 1 || prev.day < 29) {
            bad++;
        }
        prev = l;
    }
    CHECK_EQ(bad, 0);
}

static void test_names(void)
{
    /* 2026 丙午马年；1899 己亥；1984 甲子鼠年 */
    CHECK_EQ(mc_lunar_stem(2026), 2);
    CHECK_EQ(mc_lunar_branch(2026), 6);
    CHECK_EQ(mc_lunar_stem(1899), 5);
    CHECK_EQ(mc_lunar_branch(1899), 11);
    CHECK_EQ(mc_lunar_stem(1984), 0);
    CHECK_EQ(mc_lunar_branch(1984), 0);
    CHECK(strcmp(mc_lunar_zodiac_name(6), "马") == 0);
    CHECK(strcmp(mc_lunar_month_name(12), "腊月") == 0);
    CHECK(strcmp(mc_lunar_day_name(21), "廿一") == 0);
    CHECK(strcmp(mc_lunar_day_name(31), "") == 0);

    CHECK_EQ(mc_lunar_leap_month(2025), 6);
    CHECK_EQ(mc_lunar_leap_month(2026), 0);
    CHECK_EQ(mc_lunar_leap_month(2033), 11);
    CHECK_EQ(mc_lunar_leap_month(1899), 0);
}

int main(void)
{
    test_cases();
    test_range_and_continuity();
    test_names();
    CHECK_REPORT();
}
