#include "check.h"
#include "mc/mc_key.h"

static void test_bounce_then_press_release(void)
{
    mc_key_t k;
    mc_key_init(&k);
    /* 按下时抖三次：每次翻转都重新计时，抖动期间不报 */
    CHECK_EQ(mc_key_poll(&k, true, 100), MC_KEY_EV_NONE);
    CHECK_EQ(mc_key_poll(&k, false, 103), MC_KEY_EV_NONE);
    CHECK_EQ(mc_key_poll(&k, true, 106), MC_KEY_EV_NONE);
    CHECK_EQ(mc_key_poll(&k, true, 106 + MC_KEY_DEBOUNCE_MS - 1), MC_KEY_EV_NONE);
    CHECK_EQ(mc_key_poll(&k, true, 106 + MC_KEY_DEBOUNCE_MS), MC_KEY_EV_PRESS);
    CHECK_EQ(mc_key_poll(&k, true, 200), MC_KEY_EV_NONE); /* 按住不重复报 */

    CHECK_EQ(mc_key_poll(&k, false, 300), MC_KEY_EV_NONE);
    CHECK_EQ(mc_key_poll(&k, true, 302), MC_KEY_EV_NONE); /* 松开时的抖动 */
    CHECK_EQ(mc_key_poll(&k, false, 304), MC_KEY_EV_NONE);
    CHECK_EQ(mc_key_poll(&k, false, 304 + MC_KEY_DEBOUNCE_MS), MC_KEY_EV_RELEASE);
    CHECK_EQ(mc_key_poll(&k, false, 1000), MC_KEY_EV_NONE);
}

static void test_glitch_ignored(void)
{
    mc_key_t k;
    mc_key_init(&k);
    /* 短于消抖时长的毛刺不报任何事件 */
    CHECK_EQ(mc_key_poll(&k, true, 0), MC_KEY_EV_NONE);
    CHECK_EQ(mc_key_poll(&k, true, MC_KEY_DEBOUNCE_MS - 5), MC_KEY_EV_NONE);
    CHECK_EQ(mc_key_poll(&k, false, MC_KEY_DEBOUNCE_MS - 1), MC_KEY_EV_NONE);
    CHECK_EQ(mc_key_poll(&k, false, 500), MC_KEY_EV_NONE);
    CHECK(!k.stable);
}

static void test_long_press_once(void)
{
    mc_key_t k;
    mc_key_init(&k);
    CHECK_EQ(mc_key_poll(&k, true, 1000), MC_KEY_EV_NONE);
    CHECK_EQ(mc_key_poll(&k, true, 1000 + MC_KEY_DEBOUNCE_MS), MC_KEY_EV_PRESS);
    const uint32_t confirmed = 1000 + MC_KEY_DEBOUNCE_MS;
    CHECK_EQ(mc_key_poll(&k, true, confirmed + MC_KEY_LONG_MS - 1), MC_KEY_EV_NONE);
    CHECK_EQ(mc_key_poll(&k, true, confirmed + MC_KEY_LONG_MS), MC_KEY_EV_LONG);
    CHECK_EQ(mc_key_poll(&k, true, confirmed + 5u * MC_KEY_LONG_MS), MC_KEY_EV_NONE); /* 只报一次 */
    CHECK_EQ(mc_key_poll(&k, false, 9000), MC_KEY_EV_NONE);
    CHECK_EQ(mc_key_poll(&k, false, 9000 + MC_KEY_DEBOUNCE_MS), MC_KEY_EV_RELEASE);

    /* 下一次按下重新计长按 */
    CHECK_EQ(mc_key_poll(&k, true, 10000), MC_KEY_EV_NONE);
    CHECK_EQ(mc_key_poll(&k, true, 10000 + MC_KEY_DEBOUNCE_MS), MC_KEY_EV_PRESS);
    CHECK_EQ(mc_key_poll(&k, true, 10000 + MC_KEY_DEBOUNCE_MS + MC_KEY_LONG_MS), MC_KEY_EV_LONG);
}

static void test_wraparound(void)
{
    mc_key_t k;
    mc_key_init(&k);
    CHECK_EQ(mc_key_poll(&k, true, 0xFFFFFFF0u), MC_KEY_EV_NONE);
    CHECK_EQ(mc_key_poll(&k, true, 0x00000003u), MC_KEY_EV_NONE);  /* 跨 2^32 只过了 19 ms */
    CHECK_EQ(mc_key_poll(&k, true, 0x00000004u), MC_KEY_EV_PRESS); /* 满 20 ms */
    CHECK_EQ(mc_key_poll(&k, true, 0x00000004u + MC_KEY_LONG_MS), MC_KEY_EV_LONG);
}

int main(void)
{
    test_bounce_then_press_release();
    test_glitch_ignored();
    test_long_press_once();
    test_wraparound();
    CHECK_REPORT();
}
