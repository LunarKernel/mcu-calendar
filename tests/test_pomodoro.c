#include "check.h"
#include "mc/mc_pomodoro.h"

#define MIN_MS 60000u

static void test_init(void)
{
    mc_pomodoro_t p;
    CHECK(mc_pomodoro_init(&p, MC_POMODORO_WORK_MS_DEFAULT, MC_POMODORO_BREAK_MS_DEFAULT));
    CHECK_EQ(p.work_ms, 25u * MIN_MS);
    CHECK_EQ(p.break_ms, 5u * MIN_MS);
    CHECK(!mc_pomodoro_init(&p, 0, MIN_MS)); /* 失败不改状态 */
    CHECK(!mc_pomodoro_init(&p, MIN_MS, 0));
    CHECK_EQ(p.work_ms, 25u * MIN_MS);
    CHECK_EQ(p.phase, MC_POMODORO_IDLE);
    CHECK_EQ(mc_pomodoro_poll(&p, 123456), MC_POMODORO_EV_NONE);
}

static void test_cycle(void)
{
    mc_pomodoro_t p;
    CHECK(mc_pomodoro_init(&p, MC_POMODORO_WORK_MS_DEFAULT, MC_POMODORO_BREAK_MS_DEFAULT));
    mc_pomodoro_start(&p, 1000);
    CHECK_EQ(p.phase, MC_POMODORO_WORK);
    CHECK_EQ(mc_pomodoro_remaining(&p, 1000 + 10u * MIN_MS), 15u * MIN_MS);

    /* 工作段期限在 1000 + 25 min */
    CHECK_EQ(mc_pomodoro_poll(&p, 1000 + 25u * MIN_MS - 1), MC_POMODORO_EV_NONE);
    CHECK_EQ(mc_pomodoro_poll(&p, 1000 + 25u * MIN_MS), MC_POMODORO_EV_WORK_DONE);
    CHECK_EQ(p.phase, MC_POMODORO_BREAK);
    CHECK_EQ(p.completed, 1);
    CHECK_EQ(mc_pomodoro_poll(&p, 1000 + 25u * MIN_MS), MC_POMODORO_EV_NONE); /* 只报一次 */

    /* 休息段期限在 1000 + 30 min；轮询迟到 700 ms，下一段起点仍取期限，不累积滞后 */
    CHECK_EQ(mc_pomodoro_poll(&p, 1000 + 30u * MIN_MS + 700), MC_POMODORO_EV_BREAK_DONE);
    CHECK_EQ(p.phase, MC_POMODORO_WORK);
    CHECK_EQ(mc_pomodoro_remaining(&p, 1000 + 30u * MIN_MS + 700), 25u * MIN_MS - 700);
    CHECK_EQ(mc_pomodoro_poll(&p, 1000 + 55u * MIN_MS), MC_POMODORO_EV_WORK_DONE);
    CHECK_EQ(p.completed, 2);
}

static void test_late_poll_catches_up(void)
{
    mc_pomodoro_t p;
    CHECK(mc_pomodoro_init(&p, 2u * MIN_MS, 1u * MIN_MS));
    mc_pomodoro_start(&p, 0);
    /* 一口气迟到 4.5 min：期间跨了工作(0-2)、休息(2-3)、工作(3-5)中的两个段界 */
    uint32_t now = 4u * MIN_MS + 30000u;
    CHECK_EQ(mc_pomodoro_poll(&p, now), MC_POMODORO_EV_WORK_DONE);
    CHECK_EQ(mc_pomodoro_poll(&p, now), MC_POMODORO_EV_BREAK_DONE);
    CHECK_EQ(mc_pomodoro_poll(&p, now), MC_POMODORO_EV_NONE);
    CHECK_EQ(p.phase, MC_POMODORO_WORK);
    CHECK_EQ(mc_pomodoro_remaining(&p, now), 30000u); /* 第二个工作段 3-5 min */
}

static void test_pause_resume_stop(void)
{
    mc_pomodoro_t p;
    CHECK(mc_pomodoro_init(&p, 10000u, 5000u));
    mc_pomodoro_start(&p, 100);
    mc_pomodoro_start(&p, 900); /* 已在跑，不挪起点 */
    mc_pomodoro_pause(&p, 4100);
    CHECK_EQ(mc_pomodoro_remaining(&p, 999999), 6000);
    CHECK_EQ(mc_pomodoro_poll(&p, 999999), MC_POMODORO_EV_NONE); /* 暂停期间不走 */

    mc_pomodoro_start(&p, 50000);
    CHECK_EQ(mc_pomodoro_poll(&p, 55999), MC_POMODORO_EV_NONE);
    CHECK_EQ(mc_pomodoro_poll(&p, 56000), MC_POMODORO_EV_WORK_DONE);

    mc_pomodoro_stop(&p);
    CHECK_EQ(p.phase, MC_POMODORO_IDLE);
    CHECK_EQ(p.completed, 0);
    CHECK_EQ(p.work_ms, 10000u); /* 设定保留 */
}

static void test_wraparound(void)
{
    mc_pomodoro_t p;
    CHECK(mc_pomodoro_init(&p, 1000u, 500u));
    mc_pomodoro_start(&p, 0xFFFFFE00u); /* 期限跨过 2^32 */
    CHECK_EQ(mc_pomodoro_remaining(&p, 0x00000100u), 1000u - 0x300u);
    CHECK_EQ(mc_pomodoro_poll(&p, 0x000001E7u), MC_POMODORO_EV_NONE);
    CHECK_EQ(mc_pomodoro_poll(&p, 0x000001E8u), MC_POMODORO_EV_WORK_DONE);
    CHECK_EQ(mc_pomodoro_poll(&p, 0x000003DCu), MC_POMODORO_EV_BREAK_DONE);
}

int main(void)
{
    test_init();
    test_cycle();
    test_late_poll_catches_up();
    test_pause_resume_stop();
    test_wraparound();
    CHECK_REPORT();
}
