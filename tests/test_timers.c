#include "check.h"
#include "mc/mc_timers.h"

static void test_stopwatch(void)
{
    mc_stopwatch_t sw;
    mc_stopwatch_reset(&sw);
    CHECK_EQ(mc_stopwatch_elapsed(&sw, 500), 0);

    mc_stopwatch_start(&sw, 1000);
    CHECK_EQ(mc_stopwatch_elapsed(&sw, 1250), 250);
    mc_stopwatch_start(&sw, 1100); /* 重复 start 不应挪动起点 */
    CHECK_EQ(mc_stopwatch_elapsed(&sw, 1250), 250);

    mc_stopwatch_stop(&sw, 1400);
    CHECK_EQ(mc_stopwatch_elapsed(&sw, 9999), 400);
    mc_stopwatch_stop(&sw, 5000); /* 重复 stop 无动作 */
    CHECK_EQ(mc_stopwatch_elapsed(&sw, 9999), 400);

    mc_stopwatch_start(&sw, 10000); /* 续跑累计 */
    mc_stopwatch_stop(&sw, 10100);
    CHECK_EQ(mc_stopwatch_elapsed(&sw, 0), 500);

    /* 时基回绕：起点在 2^32 之前，终点在之后 */
    mc_stopwatch_reset(&sw);
    mc_stopwatch_start(&sw, 0xFFFFFF00u);
    CHECK_EQ(mc_stopwatch_elapsed(&sw, 0x00000100u), 0x200);
    mc_stopwatch_stop(&sw, 0x00000100u);
    CHECK_EQ(mc_stopwatch_elapsed(&sw, 12345), 0x200);
}

static void test_countdown(void)
{
    mc_countdown_t cd;
    mc_countdown_set(&cd, 0);
    mc_countdown_start(&cd, 0); /* 零时长不启动 */
    CHECK(!cd.running);
    CHECK(!mc_countdown_poll(&cd, 100));

    mc_countdown_set(&cd, 1000);
    mc_countdown_start(&cd, 5000);
    CHECK_EQ(mc_countdown_remaining(&cd, 5300), 700);
    CHECK(!mc_countdown_poll(&cd, 5999));

    mc_countdown_pause(&cd, 5600);
    CHECK_EQ(mc_countdown_remaining(&cd, 99999), 400); /* 暂停期间不走 */
    CHECK(!mc_countdown_poll(&cd, 99999));

    mc_countdown_start(&cd, 20000);
    CHECK(!mc_countdown_poll(&cd, 20399));
    CHECK(mc_countdown_poll(&cd, 20400));
    CHECK(cd.fired);
    CHECK(!mc_countdown_poll(&cd, 20500)); /* 每次到点只报一次 */
    CHECK_EQ(mc_countdown_remaining(&cd, 20500), 0);
    mc_countdown_ack(&cd);
    CHECK(!cd.fired);

    mc_countdown_reset(&cd);
    CHECK_EQ(mc_countdown_remaining(&cd, 0), 1000);

    /* 回绕 */
    mc_countdown_set(&cd, 0x200);
    mc_countdown_start(&cd, 0xFFFFFF00u);
    CHECK(!mc_countdown_poll(&cd, 0x000000FFu));
    CHECK(mc_countdown_poll(&cd, 0x00000100u));
}

static void test_bank(void)
{
    mc_timer_bank_t bank;
    mc_timer_bank_init(&bank);

    /* 两路秒表错开启动，互不干扰 */
    mc_stopwatch_start(&bank.stopwatch[0], 0);
    mc_stopwatch_start(&bank.stopwatch[2], 300);
    CHECK_EQ(mc_stopwatch_elapsed(&bank.stopwatch[0], 1000), 1000);
    CHECK_EQ(mc_stopwatch_elapsed(&bank.stopwatch[2], 1000), 700);
    CHECK_EQ(mc_stopwatch_elapsed(&bank.stopwatch[1], 1000), 0);

    mc_countdown_set(&bank.countdown[1], 500);
    mc_countdown_set(&bank.countdown[3], 500);
    mc_countdown_set(&bank.countdown[0], 2000);
    mc_countdown_start(&bank.countdown[1], 0);
    mc_countdown_start(&bank.countdown[3], 0);
    mc_countdown_start(&bank.countdown[0], 0);
    CHECK_EQ(mc_timer_bank_poll(&bank, 499), 0);
    CHECK_EQ(mc_timer_bank_poll(&bank, 500), 0x0A); /* 通道 1 与 3 同时到点 */
    CHECK_EQ(mc_timer_bank_poll(&bank, 501), 0);
    CHECK_EQ(mc_timer_bank_poll(&bank, 2000), 0x01);
}

int main(void)
{
    test_stopwatch();
    test_countdown();
    test_bank();
    CHECK_REPORT();
}
