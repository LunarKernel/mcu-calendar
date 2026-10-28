#include "check.h"
#include "mc/mc_alarm.h"

/* 星期锚点取校历：2026-10-28 周三、2026-10-29 周四、2026-11-01 周日 */
static mc_datetime_t at(int16_t y, uint8_t mo, uint8_t d, uint8_t h, uint8_t mi, uint8_t s)
{
    mc_datetime_t t = {{y, mo, d}, h, mi, s};
    return t;
}

static void test_set_validation(void)
{
    mc_alarm_t al;
    mc_alarm_init(&al);
    CHECK(!al.enabled);
    CHECK(!mc_alarm_set(&al, 24, 0, MC_ALARM_ONCE));
    CHECK(!mc_alarm_set(&al, 7, 60, MC_ALARM_ONCE));
    CHECK(!mc_alarm_set(&al, 7, 0, 0x80u)); /* 星期位图只有 7 位 */
    CHECK(!al.enabled);                     /* 失败不改状态 */

    CHECK(mc_alarm_set(&al, 7, 30, MC_ALARM_EVERYDAY));
    CHECK(al.enabled);
    mc_datetime_t bad = at(2026, 2, 30, 7, 30, 0); /* 非法日期不触发 */
    CHECK(!mc_alarm_poll(&al, &bad));
}

static void test_once(void)
{
    mc_alarm_t al;
    mc_alarm_init(&al);
    CHECK(mc_alarm_set(&al, 7, 30, MC_ALARM_ONCE));

    mc_datetime_t t = at(2026, 10, 28, 7, 29, 59);
    CHECK(!mc_alarm_poll(&al, &t));
    t = at(2026, 10, 28, 7, 30, 0);
    CHECK(mc_alarm_poll(&al, &t));
    CHECK(!al.enabled); /* 单次：响后自动关闭 */
    t = at(2026, 10, 29, 7, 30, 0);
    CHECK(!mc_alarm_poll(&al, &t));
}

static void test_same_minute_once(void)
{
    mc_alarm_t al;
    mc_alarm_init(&al);
    CHECK(mc_alarm_set(&al, 12, 0, MC_ALARM_EVERYDAY));

    mc_datetime_t t = at(2026, 10, 28, 12, 0, 3); /* 主循环迟了 3 s 才轮询到，也要响 */
    CHECK(mc_alarm_poll(&al, &t));
    t = at(2026, 10, 28, 12, 0, 40); /* 同一分钟再轮询不重复 */
    CHECK(!mc_alarm_poll(&al, &t));
    t = at(2026, 10, 28, 12, 1, 0);
    CHECK(!mc_alarm_poll(&al, &t));
    t = at(2026, 10, 29, 12, 0, 0); /* 第二天照响 */
    CHECK(mc_alarm_poll(&al, &t));
    CHECK(al.enabled);
}

static void test_weekdays(void)
{
    mc_alarm_t al;
    mc_alarm_init(&al);
    /* 只在周三、周日响 */
    CHECK(mc_alarm_set(&al, 8, 0, (uint8_t)((1u << 3) | (1u << 0))));

    mc_datetime_t t = at(2026, 10, 28, 8, 0, 0); /* 周三 */
    CHECK(mc_alarm_poll(&al, &t));
    t = at(2026, 10, 29, 8, 0, 0); /* 周四 */
    CHECK(!mc_alarm_poll(&al, &t));
    t = at(2026, 11, 1, 8, 0, 0); /* 周日 */
    CHECK(mc_alarm_poll(&al, &t));
}

static void test_reset_and_disable(void)
{
    mc_alarm_t al;
    mc_alarm_init(&al);
    CHECK(mc_alarm_set(&al, 6, 0, MC_ALARM_EVERYDAY));
    mc_datetime_t t = at(2026, 10, 28, 6, 0, 0);
    CHECK(mc_alarm_poll(&al, &t));

    /* 重新设定清除「今天已响」，同一分钟内改设即可再响一次（演示「设 1 min」时用得上） */
    CHECK(mc_alarm_set(&al, 6, 0, MC_ALARM_EVERYDAY));
    CHECK(mc_alarm_poll(&al, &t));

    mc_alarm_disable(&al);
    t = at(2026, 10, 29, 6, 0, 0);
    CHECK(!mc_alarm_poll(&al, &t));
}

int main(void)
{
    test_set_validation();
    test_once();
    test_same_minute_once();
    test_weekdays();
    test_reset_and_disable();
    CHECK_REPORT();
}
