/* 按键消抖与长按判定。每个键一份状态，主循环里拿原始电平轮询。
 *
 * 机械按键按下与松开时会抖几毫秒到十几毫秒；原始电平连续稳定 MC_KEY_DEBOUNCE_MS 才算变了，
 * 抖动期间任何一次翻转都重新计时。轮询周期不限，但要明显短于消抖时长，判定才及时。
 * 长按从「按下被确认」的时刻算起，每次按下最多报一次。
 * 与 mc_timers 一样显式收 now_ms（uint32 回绕安全），自己不读时钟、不碰引脚。 */
#ifndef MC_KEY_H
#define MC_KEY_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MC_KEY_DEBOUNCE_MS 20u
#define MC_KEY_LONG_MS 800u

typedef enum {
    MC_KEY_EV_NONE,
    MC_KEY_EV_PRESS,
    MC_KEY_EV_RELEASE,
    MC_KEY_EV_LONG,
} mc_key_event_t;

typedef struct {
    bool stable;    /* 已确认的状态：true＝按下 */
    bool candidate; /* 最近一次读到的原始状态 */
    bool long_sent;
    uint32_t since_ms; /* candidate 从这一刻起没变过 */
    uint32_t press_ms; /* 按下被确认的时刻 */
} mc_key_t;

void mc_key_init(mc_key_t *k); /* 初始状态为松开 */
/* raw_pressed 由调用者按电路换算好（高有效还是低有效与本模块无关）。一次只报一个事件 */
mc_key_event_t mc_key_poll(mc_key_t *k, bool raw_pressed, uint32_t now_ms);

#ifdef __cplusplus
}
#endif

#endif
